#!/usr/bin/env python3
"""codemap.json의 Thumb 함수를 C로 기계 번역한다(방식 시험용).

레지스터와 플래그는 함수 안에서 지역 변수로 두고 호출과 복귀 때만 cpu 구조체와 맞춘다. 메모리는 m(게스트 주소 + 베이스)로 접근한다.
thumb2c.py <client.bin> <codemap.json> <out.c>
"""
import json, re, struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

data = open(sys.argv[1], 'rb').read()
cm = json.load(open(sys.argv[2]))
LOAD = 0x100000
GOT0 = cm['summary']['GOT'][0]
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
funcs = {int(k, 16): v for k, v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
RN = {'sb': 'r9', 'sl': 'r10', 'fp': 'r11', 'ip': 'r12'}
def R(n):
    return RN.get(n, n)
COND = {'eq':'fl.z','ne':'!fl.z','hs':'fl.cf','cs':'fl.cf','lo':'!fl.cf','cc':'!fl.cf','mi':'fl.n','pl':'!fl.n','vs':'fl.v','vc':'!fl.v',
        'hi':'(fl.cf&&!fl.z)','ls':'(!fl.cf||fl.z)','ge':'(fl.n==fl.v)','lt':'(fl.n!=fl.v)','gt':'(!fl.z&&fl.n==fl.v)','le':'(fl.z||fl.n!=fl.v)'}
LOADS = {'ldr':'R32','ldrb':'R8','ldrh':'R16','ldrsb':'RS8','ldrsh':'RS16'}
STORES = {'str':'W32','strb':'W8','strh':'W16'}
ALU = {'ands':'{d}={a}&{b};NZ({d});','and':'{d}={a}&{b};NZ({d});','eors':'{d}={a}^{b};NZ({d});','orrs':'{d}={a}|{b};NZ({d});',
       'bics':'{d}={a}&~{b};NZ({d});','mvns':'{d}=~{b};NZ({d});','muls':'{d}={a}*{b};NZ({d});',
       'adcs':'{d}=addc(&fl,{a},{b},fl.cf);','sbcs':'{d}=addc(&fl,{a},~{b},fl.cf);',
       'lsls':'{d}=lsl_(&fl,{a},{b});','lsrs':'{d}=lsr_(&fl,{a},{b});','asrs':'{d}=asr_(&fl,{a},{b});','rors':'{d}=ror_(&fl,{a},{b});'}

def tr(ins, lits_pc, targets, far, fn):
    m, ops = ins.mnemonic, ins.op_str
    pc = ins.address
    p = [x.strip() for x in re.sub(r'[\[\]!]', '', ops.split('{')[0]).split(',') if x.strip()]
    def val(x):
        if x.startswith('#'):
            return f'{int(x[1:], 0) & 0xffffffff}u'
        if x == 'pc':
            return f'{(pc + 4 + LOAD) & 0xffffffff}u'
        return R(x)
    d = R(p[0]) if p and p[0] != 'pc' else ''
    if m in LOADS or m in STORES:
        base = p[1]
        if base == 'pc':
            ea = f'{((pc + 4) & ~3) + LOAD + int(p[2][1:], 0)}u'
        else:
            off = val(p[2]) if len(p) > 2 else '0u'
            ea = f'{val(base)}+{off}'
        return f'{d}={LOADS[m]}({ea});' if m in LOADS else f'{STORES[m]}({ea},{d});'
    if m in ('mov', 'movs'):
        if p[0] == 'pc':
            return None
        return f'{d}={val(p[1])};' + ('NZ(%s);' % d if m == 'movs' else '')
    if m in ('add', 'adds', 'sub', 'subs'):
        if len(p) == 2: a, b = p[0], p[1]
        else: a, b = p[1], p[2]
        sub = m.startswith('sub')
        if m.endswith('s'):
            return f'{d}=addc(&fl,{val(a)},{"~" if sub else ""}{val(b)},{1 if sub else 0});'
        if p[0] == 'pc':
            return None
        # add rd, pc, #i는 워드 정렬된 pc를 쓴다.
        av = f'{(pc + 4 + LOAD) & ~3 & 0xffffffff}u' if a == 'pc' and len(p) == 3 else val(a)
        return f'{d}={av}{"-" if sub else "+"}{val(b)};'
    if m == 'rsbs':
        return f'{d}=addc(&fl,~{val(p[1])},0u,1);'
    if m == 'negs':
        return f'{d}=addc(&fl,~{val(p[1])},0u,1);'
    if m == 'cmp':
        return f'addc(&fl,{val(p[0])},~{val(p[1])},1);'
    if m == 'cmn':
        return f'addc(&fl,{val(p[0])},{val(p[1])},0);'
    if m == 'tst':
        return f'{{uint32_t t={val(p[0])}&{val(p[1])};NZ(t);}}'
    if m in ALU:
        a, b = (p[0], p[1]) if len(p) == 2 else (p[1], p[2])
        return ALU[m].format(d=d, a=val(a), b=val(b))
    if m in ('push', 'pop', 'ldm', 'stm', 'ldmia', 'stmia'):
        regs = [x.strip() for x in ops.split('{')[1].rstrip('}').split(',')]
        if m == 'push':
            s = f'sp-={4 * len(regs)};{{uint32_t a=sp;'
            for r in regs: s += f'W32(a,{R(r)});a+=4;'
            return s + '}'
        if m == 'pop':
            s = '{uint32_t a=sp;'
            for r in regs:
                if r == 'pc': s += 'a+=4;'
                else: s += f'{R(r)}=R32(a);a+=4;'
            return s + f'sp=a;}}'
        s = f'{{uint32_t a={R(p[0])};'
        wb = p[0] not in regs
        for r in regs:
            s += (f'{R(r)}=R32(a);a+=4;' if m.startswith('ldm') else f'W32(a,{R(r)});a+=4;')
        return s + (f'{R(p[0])}=a;' if wb or not m.startswith('ldm') else '') + '}'
    raise ValueError(f'{pc:#x} {m} {ops}')

def popped_return(decoded, addrs, idx, reg):
    """pop {rN}; (add sp 등); bx rN 형태의 복귀인지 본다."""
    for k in range(idx - 1, max(idx - 5, -1), -1):
        i = decoded[addrs[k]]
        if i.mnemonic == 'pop' and re.search(rf'\b{reg}\b', i.op_str):
            return True
        if re.match(rf'{reg}\b', i.op_str) and i.mnemonic not in ('cmp', 'tst', 'str'):
            return False
    return False

SYNC, RELOAD = '@SYNC@', '@RELOAD@'
REG = r'\b(r1[0-2]|r[0-9]|sp|lr)\b'
SLOT = {**{f'r{i}': i for i in range(13)}, 'sp': 13, 'lr': 14}
NZ_ONLY = {'ands', 'eors', 'orrs', 'bics', 'mvns', 'muls', 'movs', 'tst', 'and'}
ALL_FLAGS = {'adds', 'subs', 'adcs', 'sbcs', 'rsbs', 'negs', 'cmp', 'cmn'}
SHIFTS = {'lsls', 'lsrs', 'asrs', 'rors'}
FLAG_USE = {'eq':'z','ne':'z','hs':'c','cs':'c','lo':'c','cc':'c','mi':'n','pl':'n','vs':'v','vc':'v','hi':'cz','ls':'cz','ge':'nv','lt':'nv','gt':'znv','le':'znv'}
flag_leaks = []

def check_flags(entry, decoded, addrs, f, labels, tabs):
    """플래그를 지역 변수로 두려면 함수 입구와 호출 직후의 값을 읽는 코드가 없어야 한다. 있으면 번역을 멈춘다."""
    aset = set(addrs)
    succs = {}
    for idx, a in enumerate(addrs):
        i = decoded[a]; m = i.mnemonic; nxt = a + i.size; s = []
        if m == 'b': s = [int(i.op_str.split('#')[1], 0)]
        elif m[0] == 'b' and m[1:] in COND: s = [int(i.op_str.split('#')[1], 0), nxt]
        elif m == 'bl' and a in f['far']: s = [int(i.op_str.split('#')[1], 0)]
        elif m == 'bx' or (m == 'pop' and 'pc' in i.op_str): s = []
        elif m == 'mov' and i.op_str.startswith('pc,'): s = sorted(l for l in labels if l in aset)
        else: s = [nxt]
        succs[a] = [x for x in s if x in aset]
    undef = {entry: set('nzcv')}
    work = [entry]
    while work:
        a = work.pop()
        i = decoded[a]; m = i.mnemonic; u = set(undef[a])
        use = set()
        if m[0] == 'b' and m[1:] in COND: use = set(FLAG_USE[m[1:]])
        if m in ('adcs', 'sbcs'): use = {'c'}
        if use & u: flag_leaks.append((hex(entry), hex(a), m))
        if m in ALL_FLAGS: u = set()
        elif m in NZ_ONLY: u -= {'n', 'z'}
        elif m in SHIFTS:
            u -= {'n', 'z'}
            if '#' in i.op_str.split(',')[-1] and int(i.op_str.split('#')[-1], 0) != 0: u -= {'c'}
        if m == 'bl' and a not in f['far']: u = set('nzcv')
        for s in succs[a]:
            old = undef.get(s)
            new = u if old is None else old | u
            if old != new:
                undef[s] = new
                work.append(s)

def gen(entry, f):
    tabs = {int(t): n for t, n in f['tables']}
    addrs = f['addrs']
    aset = set(addrs)
    out = [f'DEF({entry:x}){{']
    decoded = {}
    for a in addrs:
        decoded[a] = next(md.disasm(data[a:a + 4], a, 1))
    # 라벨
    labels = set()
    for a, i in decoded.items():
        if i.mnemonic == 'b' or (i.mnemonic[0] == 'b' and i.mnemonic[1:] in COND):
            labels.add(int(i.op_str.split('#')[1], 0))
        if i.mnemonic == 'bl':
            if a in f['far']: labels.add(int(i.op_str.split('#')[1], 0))
    for t, n in tabs.items():
        for k in range(n):
            v = u32(t + k * 4)
            labels.add((t + (v - (1 << 32) if v >> 31 else v)) & 0xffffffff)
    popped = None
    body = []
    if addrs[0] != entry:
        body.append(f'goto L{entry:x};')
    for idx, a in enumerate(addrs):
        i = decoded[a]
        m, ops = i.mnemonic, i.op_str
        if a in labels or a == entry:
            body.append(f'L{a:x}:;')
        body.append(f'TICK(0x{a:x});')
        nxt = a + i.size
        fall = True
        if m == 'b':
            body.append(f'goto L{int(ops.split("#")[1], 0):x};'); fall = False
        elif m[0] == 'b' and m[1:] in COND:
            body.append(f'if({COND[m[1:]]})goto L{int(ops.split("#")[1], 0):x};')
        elif m == 'bl':
            t = int(ops.split('#')[1], 0)
            if a in f['far']:
                body.append(f'lr={(nxt + LOAD) | 1}u;goto L{t:x};'); fall = False
            else:
                body.append(f'lr={(nxt + LOAD) | 1}u;{SYNC}f_{t:x}(c);{RELOAD}' if t in funcs else f'lr={(nxt + LOAD) | 1}u;{SYNC}call_ind(c,{t + LOAD | 1}u);{RELOAD}')
        elif m == 'bx':
            if ops == 'lr':
                body.append(f'{SYNC}return;')
            elif ops == 'pc':
                body.append(f'{SYNC}call_ind(c,{(a + 4 & ~3) + LOAD}u);return;')
            elif popped_return(decoded, addrs, idx, ops):
                body.append(f'{SYNC}return;')
            else:
                body.append(f'{SYNC}call_ind(c,{R(ops)});return;')
            fall = False
        elif m == 'pop' and 'pc' in ops:
            body.append(tr(i, None, None, None, f)); body.append(f'{SYNC}return;'); fall = False
        elif m == 'mov' and ops.startswith('pc,'):
            src = ops.split(',')[1].strip()
            tab = None
            for t in tabs:
                # 이 함수의 점프표: 값이 라벨 중 하나면 switch로 보낸다.
                tab = t
            prev = decoded.get(addrs[idx - 1]) if idx else None
            if tabs and src != 'lr' and not popped_return(decoded, addrs, idx, src):
                cases = ''.join(f'case {(l + LOAD) & 0xffffffff}u:goto L{l:x};' for l in sorted(labels) if l in aset)
                body.append(f'switch({R(src)}){{{cases}default:{SYNC}call_ind(c,{R(src)});return;}}')
            elif src == 'lr' or popped_return(decoded, addrs, idx, src):
                body.append(f'{SYNC}return;')
            else:
                body.append(f'{SYNC}call_ind(c,{R(src)});return;')
            fall = False
        else:
            body.append(tr(i, None, None, None, f))
        if fall and idx + 1 < len(addrs) and addrs[idx + 1] != nxt:
            body.append(f'goto L{nxt:x};')
        elif fall and idx + 1 == len(addrs):
            body.append(f'{SYNC}return;')
        if a == addrs[-1] and fall and nxt not in aset:
            pass
    check_flags(entry, decoded, addrs, f, labels, tabs)
    text = '\n'.join(body)
    # 함수에 나오는 레지스터만 들여오고, 쓴 레지스터만 내보낸다.
    used = sorted(set(re.findall(REG, text)))
    written = sorted(set(re.findall(REG + r'[-+]?=(?!=)', text)))
    reload = ''.join(f'{x}=c->r[{SLOT[x]}];' for x in used)
    sync = ''.join(f'c->r[{SLOT[x]}]={x};' for x in written)
    out[0] += (f'uint32_t {",".join(used)};' if used else '') + 'uint8_t*const m=c->m;flags_t fl={0};(void)m;(void)fl;' + reload
    out.append(text.replace(SYNC, sync).replace(RELOAD, reload))
    # 쓰이지 않은 라벨 경고 방지용 끝맺음
    out.append('}')
    return '\n'.join(out)

fails = []
with open(sys.argv[3], 'w') as w:
    w.write('#define TR_LOCAL_REGS\n#include "thumbrt.h"\n')
    # TRACE_STACK: 함수마다 드나듦을 알려 호출 나무를 남긴다(runtime.c ts_enter, 진단용 빌드 ecpe-trace).
    w.write('#ifdef TRACE_STACK\nvoid ts_enter(uint32_t a);void ts_leave(void);\n'
            '#define DEF(x) static void b_##x(cpu_t*c);void f_##x(cpu_t*c){ts_enter(0x##x);b_##x(c);ts_leave();}static void b_##x(cpu_t*c)\n'
            '#define ODEF(x) static void ob_##x(cpu_t*c);void orig_f_##x(cpu_t*c){ts_enter(0x##x);ob_##x(c);ts_leave();}static void ob_##x(cpu_t*c)\n'
            '#else\n#define DEF(x) void f_##x(cpu_t*c)\n#define ODEF(x) void orig_f_##x(cpu_t*c)\n#endif\n')
    for e in funcs:
        w.write(f'void f_{e:x}(cpu_t*c);\n')
    # 사람이 옮긴 함수(파일 오프셋 목록)는 건너뛴다. 정의는 native/restored/에 있다.
    restored = {int(l.split()[0], 16) for l in open(sys.argv[4]) if l.strip() and not l.startswith('#')} if len(sys.argv) > 4 else set()
    # `wrap`이 붙은 함수는 orig_f_<오프셋>으로 남겨 읽는 C가 원본을 호출할 수 있게 한다.
    rows = [l.split() for l in open(sys.argv[4]) if l.strip() and not l.startswith('#')] if len(sys.argv) > 4 else []
    wrapped = {int(x[0], 16) for x in rows if x[-1] == 'wrap'}
    restored -= wrapped
    for e, f in sorted(funcs.items()):
        if e in restored:
            continue
        try:
            code = gen(e, f)
            if e in wrapped:
                code = code.replace(f'DEF({e:x}){{', f'ODEF({e:x}){{', 1)
            w.write(code + '\n')
        except Exception as ex:
            fails.append((hex(e), str(ex)))
            w.write(f'void f_{e:x}(cpu_t*c){{c->fault=1;}}\n')
    w.write('const fn_entry_t fn_table[]={\n')
    for e in sorted(funcs):
        w.write(f'{{{e + LOAD}u,f_{e:x}}},\n')
    w.write('{0,0}};\n')
print('functions', len(funcs), 'translate failures', len(fails), fails[:20])
print('flag leaks', len(flag_leaks), flag_leaks[:40])
