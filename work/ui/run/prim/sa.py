#!/usr/bin/env python3
"""정적 분석: 함수마다 직접 호출, API 호출(표,번호), GOT/데이터 참조(문자열, vtable), vtable 저장을 뽑아 sa.json으로 남긴다."""
import json, struct, sys, os, collections
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
ROOT = __import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../..'))
data = open(f'{ROOT}/native/build/game/client.bin', 'rb').read()
cm = json.load(open(f'{ROOT}/native/build/codemap.json'))
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
RELOC_N = u32(0x70); BASE = 0x74 + RELOC_N * 4
GOT0, GOT1 = BASE + u32(BASE), BASE + u32(BASE + 4)
relocs = sorted(BASE + u32(0x74 + i * 4) for i in range(RELOC_N))
TEXT_MAX = 0x46878
funcs = {int(k, 16): v for k, v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
fstarts = set(funcs)
TABLES = {0x4dec0: 0, 0x4de40: 1, 0x4dda0: 2, 0x4dce0: 3, 0x4dea0: 4, 0x4df00: 5, 0x4dc80: 6, 0x4dd80: 7, 0x4dd60: 8,
          0x4de60: 9, 0x4dca0: 10, 0x4dd20: 11, 0x4de20: 12, 0x4dd40: 13, 0x4ddc0: 14, 0x4dc60: 15, 0x4dee0: 16, 0x4dc20: 17}
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
REG = {f'r{i}': i for i in range(13)}; REG.update(sb=9, sl=10, fp=11, ip=12)
VENEER = {0x44f10 + 4 * k: k for k in range(12)}  # bx rK

def cstr(o):
    if not (BASE <= o < len(data)): return None
    e = data.find(b'\0', o)
    if e < 0 or e - o < 1 or e - o > 400: return None
    b = data[o:e]
    try: s = b.decode('cp949')
    except Exception: return None
    if not all(ch.isprintable() or ch in '\n\r\t' for ch in s): return None
    return s

def analyze(e):
    f = funcs[e]
    regs = {}
    res = dict(calls=[], api=[], refs=[], vstores=[], fptrs=[], icalls=[])
    targets = set()
    ins = []
    for a in f['addrs']:
        i = next(md.disasm(data[a:a + 4], a, 1)); ins.append(i)
        if i.mnemonic.startswith('b') and i.mnemonic not in ('bl', 'bx', 'bic', 'bics') and '#' in i.op_str:
            targets.add(int(i.op_str[1:], 0))
    for i in ins:
        a = i.address; m = i.mnemonic; ops = [x.strip() for x in i.op_str.replace('[', '').replace(']', '').split(',')]
        if a in targets: regs = {}
        def val(r): return regs.get(REG.get(r))
        def setr(r, v):
            k = REG.get(r)
            if k is None: return
            if v is None: regs.pop(k, None)
            else: regs[k] = v
        if m == 'ldr' and 'pc' in i.op_str:
            off = int(i.op_str.split('#')[1].rstrip(']'), 0) if '#' in i.op_str else 0
            setr(ops[0], ('c', u32(((a + 4) & ~3) + off)))
        elif m in ('add',) and len(ops) == 2 and ops[1] == 'sl' and val(ops[0]) and val(ops[0])[0] == 'c':
            ad = (GOT0 + val(ops[0])[1]) & 0xffffffff
            setr(ops[0], ('addr', ad))
            if not (GOT0 <= ad < GOT1):
                s = cstr(ad)
                if ad < TEXT_MAX and (ad & ~1) in fstarts: res['fptrs'].append(ad & ~1)
                else: res['refs'].append(ad)
        elif m in ('adds', 'mov', 'movs') and len(ops) >= 2 and ops[1] in REG and (len(ops) == 2 or ops[2] == '#0'):
            setr(ops[0], val(ops[1]))
        elif m in ('ldr',) and len(ops) >= 2 and ops[1] in REG:
            base = val(ops[1]); imm = int(ops[2].lstrip('#'), 0) if len(ops) > 2 and ops[2].startswith('#') else (0 if len(ops) == 2 else None)
            nv = None
            if base and imm is not None:
                if base[0] == 'addr' and GOT0 <= base[1] + imm < GOT1:
                    v = u32(base[1] + imm); p = BASE + v
                    nv = ('ptr', p)
                    if (p & ~1) in fstarts and v & 1: res['fptrs'].append(p & ~1)
                    else: res['refs'].append(p)
                elif base[0] == 'ptr' and imm == 0 and (base[1] - BASE) in TABLES:
                    nv = ('tbl', TABLES[base[1] - BASE])
                elif base[0] == 'tbl':
                    nv = ('api', base[1], imm // 4)
            setr(ops[0], nv)
        elif m == 'str' and len(ops) >= 2 and val(ops[0]) and val(ops[0])[0] in ('ptr', 'addr'):
            res['vstores'].append(val(ops[0])[1])
            if ops[0] in REG: pass
        elif m == 'bl':
            t = int(i.op_str[1:], 0)
            if a in f['far']: pass
            elif t in VENEER:
                v = regs.get(VENEER[t])
                if v and v[0] == 'api': res['api'].append([v[1], v[2]])
                else: res['icalls'].append(a)
            else: res['calls'].append(t)
            for k in range(4): regs.pop(k, None)
            regs.pop(12, None)
        elif m in ('pop', 'push', 'b', 'bx'):
            pass
        else:
            if ops and ops[0] in REG and m not in ('cmp', 'cmn', 'tst', 'str', 'strb', 'strh', 'stm', 'stmia'):
                setr(ops[0], None)
            if m in ('ldm', 'ldmia'):
                for r in i.op_str.split('{')[1].rstrip('}').split(','): setr(r.strip(), None)
                setr(ops[0].rstrip('!'), None)
    return res

out = {}
for e in sorted(funcs):
    r = analyze(e)
    fo = funcs[e]
    out[e] = dict(size=fo['end'] - e, why=fo['why'], **r)
# vtables: 재배치된 데이터 워드 중 함수 시작을 가리키는 연속 묶음
relocset = set(relocs)
vt = {}
o = 0
rl = [x for x in relocs if x >= TEXT_MAX]
i = 0
while i < len(rl):
    j = i
    run = []
    while j < len(rl) and (j == i or rl[j] == rl[j - 1] + 4):
        v = u32(rl[j])
        run.append((rl[j], BASE + (v & ~1) if v & 1 and BASE + (v & ~1) in fstarts else None, v))
        j += 1
    i = j
    # 함수 포인터가 섞인 묶음만
    if any(x[1] for x in run):
        vt[run[0][0]] = [[x[0], x[1], x[2]] for x in run]
json.dump(dict(funcs={f'{k:x}': v for k, v in out.items()}, vtables={f'{k:x}': v for k, v in vt.items()},
               BASE=BASE, GOT=[GOT0, GOT1]), open(f'{ROOT}/work/ui/run/prim/sa.json', 'w'))
print(len(out), 'funcs', len(vt), 'reloc runs with fptrs')
c = collections.Counter(tuple(x) for v in out.values() for x in v['api'])
for k, n in sorted(c.items()): print(k, n)
