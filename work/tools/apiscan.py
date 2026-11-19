#!/usr/bin/env python3
"""게임의 WIPI 호출 지점을 정적으로 찾는다.

형태: ldr rA,=전역(재배치값) ; ldr rA,[rA] ; ldr rB,[rA,#오프셋] ; bl bx_rB 썽크.
결과: (전역, 오프셋)별 호출 지점과 감싸는 함수. codemap.json이 먼저 있어야 한다.
"""
import bisect, collections, json, struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

data = open(sys.argv[1], 'rb').read()
m = json.load(open(sys.argv[2]))
kind = open(sys.argv[2] + '.kind', 'rb').read()
S = m['summary']; BASE = S['BASE']; TEXT = S['last_code']
GOT0, GOT1 = S['GOT']
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
roots = sorted(int(a, 16) for a in m['roots'])
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
# bx rN; nop 썽크 표
thunks = {}
for o in range(BASE, TEXT, 2):
    h = struct.unpack_from('<H', data, o)[0]
    if h & 0xff87 == 0x4700 and data[o + 2:o + 4] == b'\xc0\x46':
        thunks[o] = (h >> 3) & 15
insns = []
o = BASE
while o < TEXT:
    if kind[o] == 1:
        i = next(md.disasm(data[o:o + 4], o, 1), None)
        insns.append((o, i.mnemonic, i.op_str)); o += i.size
    else:
        o += 2
idx = {x[0]: n for n, x in enumerate(insns)}
sites = collections.defaultdict(list)

def slice_value(hist, want):
    """함수 머리부터 직선으로 돌리는 작은 추상 해석. 분기 합류는 무시하므로 결과는 WIE 로그로 검증한다.
    값: ('c',상수) ('sl',) ('a',이미지 주소) ('m',주소의 내용) ('field',주소,오프셋)."""
    regs, stack = {}, {}
    def addr(x, y):
        if x and y and x[0] == 'sl' and y[0] == 'c':
            return ('a', (GOT0 - BASE + (y[1] - (1 << 32) if y[1] >> 31 else y[1])) & 0xffffffff)
        return None
    for pc, mn, ops in hist:
        p = [x.strip() for x in ops.replace('[', '').replace(']', '').split(',')]
        d = p[0] if p else ''
        if mn == 'ldr' and p[1] == 'pc':
            regs[d] = ('c', u32(((pc + 4) & ~3) + int(p[2].lstrip('#'), 0)))
        elif mn == 'ldr' and p[1] == 'sp':
            regs[d] = stack.get(p[2] if len(p) > 2 else '#0')
        elif mn == 'str' and p[1] == 'sp':
            stack[p[2] if len(p) > 2 else '#0'] = regs.get(d)
        elif mn == 'mov' and len(p) == 2 and p[1] == 'sl':
            regs[d] = ('sl',)
        elif mn in ('mov', 'adds', 'movs') and len(p) >= 2 and not p[1].startswith('#') and (len(p) == 2 or p[2] == '#0'):
            regs[d] = regs.get(p[1])
        elif mn in ('add', 'adds'):
            if len(p) == 2:
                other = ('sl',) if p[1] == 'sl' else regs.get(p[1])
                regs[d] = addr(other, regs.get(d)) or addr(regs.get(d), other)
            elif len(p) == 3 and not p[2].startswith('#'):
                x, y = regs.get(p[1]), ('sl',) if p[2] == 'sl' else regs.get(p[2])
                regs[d] = addr(x, y) or addr(y, x)
            else:
                regs[d] = None
        elif mn == 'ldr':
            x = regs.get(p[1])
            off = 0 if len(p) == 2 else int(p[2].lstrip('#'), 0) if p[2].startswith('#') else None
            if not x or off is None:
                regs[d] = None
            elif x[0] == 'c':
                regs[d] = ('m', x[1] + off)
            elif x[0] == 'a':
                fo = BASE + x[1] + off
                regs[d] = ('c', u32(fo)) if GOT0 <= fo < GOT1 else ('m', x[1] + off)
            elif x[0] == 'm':
                regs[d] = ('field', x[1], off)
            else:
                regs[d] = None
        elif mn not in ('str', 'strb', 'strh', 'cmp', 'cmn', 'tst', 'push', 'pop', 'b', 'bl', 'bx') and not (mn[0] == 'b' and len(mn) == 3):
            regs[d] = None
        elif mn == 'bl':
            for r in ('r0', 'r1', 'r2', 'r3'):
                regs.pop(r, None)
    return regs.get(want)

indirect = 0
for n, (pc, mn, ops) in enumerate(insns):
    if mn != 'bl':
        continue
    t = int(ops.split('#')[1], 0)
    if t not in thunks:
        continue
    indirect += 1
    r = f'r{thunks[t]}'
    f0 = roots[bisect.bisect_right(roots, pc) - 1]
    v = slice_value(insns[idx.get(f0, max(0, n - 40)):n], r)
    if v and v[0] == 'field':
        sites[(v[1], v[2])].append(pc)
    else:
        sites['indirect'].append(pc)
fn = lambda pc: roots[bisect.bisect_right(roots, pc) - 1]
out = []
for k, v in sites.items():
    if k == 'indirect':
        continue
    out.append({'global': k[0], 'offset': k[1], 'sites': v, 'funcs': sorted({fn(x) for x in v})})
json.dump({'thunks': {hex(k): v for k, v in thunks.items()}, 'api': out,
           'indirect_sites': [(x, fn(x)) for x in sites.get('indirect', [])]}, open(sys.argv[3], 'w'))
g = collections.Counter(); gs = collections.Counter()
for a in out:
    g[a['global']] += 1; gs[a['global']] += len(a['sites'])
print('thunks', len(thunks), 'indirect calls', indirect, 'resolved', sum(len(a['sites']) for a in out), 'unresolved', len(sites.get('indirect', [])))
for x, c in g.most_common(30):
    print(f'global {x:#x}: slots {c} sites {gs[x]}')
