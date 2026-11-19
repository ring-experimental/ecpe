#!/usr/bin/env python3
"""두 판의 client.bin을 함수 단위로 맞대어 주소 대응표를 만든다. 원본 내용은 이 도구에 없다.

vermap.py <기준 client.bin> <기준 codemap.json> <대상 client.bin> <대상 codemap.json> <out.json> [<out.map> <대상 코드 끝(codemap.py의 셋째 인자)>]
- 함수 몸을 정규화(BL 대상, pc 상대 리터럴 위치, 리터럴 값은 가림)해 같은 것끼리 짝짓고,
  짝이 없는 구간은 순서 정렬(difflib) 뒤 비슷한 정도로 짝짓는다. 짝지은 함수의 BL 대상끼리도 짝으로 확인한다.
  GOT의 함수 포인터 칸끼리, ARM 도우미(같은 바이트)끼리도 짝짓는다.
- GOT 칸: 가리키는 대상(함수 짝, 같은 내용의 데이터, BSS)이 같은 칸끼리 짝짓는다. 내용이 바뀐 데이터 칸은 자리로 맞추고,
  자리가 빈 칸은 같은 구간 안에서 남은 칸끼리 차례로 채운다.
- 데이터 주소: GOT 값과 재배치 대상 값의 짝.
- 결과: out.json(조사용), out.map(호스트가 읽는 표, native/ver/<AID>.map). 손으로 정한 규칙(gofs, bss)은 그 파일 끝
  '# ---- 손으로 정한 규칙' 아래에 적고, 다시 만들어도 남는다.
"""
import json, struct, sys, difflib, collections

def load(binp, cmp):
    d = open(binp, 'rb').read()
    cm = json.load(open(cmp))
    funcs = {}
    for k, v in cm['roots'].items():
        if not v.get('thumb'): continue
        funcs[int(k, 16)] = v['addrs']
    return d, funcs

def norm(d, addrs):
    toks, bls = [], []
    i = 0
    s = set(addrs)
    for a in addrs:
        h = d[a] | d[a + 1] << 8
        if (h >> 11) == 0x1e and a + 2 < len(d):
            h2 = d[a + 2] | d[a + 3] << 8
            if (h2 >> 11) == 0x1f:
                off = ((h & 0x7ff) << 12 | (h2 & 0x7ff) << 1)
                if off & 0x400000: off -= 0x800000
                bls.append(a + 4 + off)
                toks.append('BL'); continue
        if (h >> 11) == 0x1f and (a - 2) in s and ((d[a - 2] | d[a - 1] << 8) >> 11) == 0x1e:
            continue
        if (h >> 11) == 9: toks.append('LP%d' % ((h >> 8) & 7)); continue      # ldr rd, [pc, #]
        if (h >> 11) == 0x14: toks.append('AP%d' % ((h >> 8) & 7)); continue   # add rd, pc, #
        toks.append('%04x' % h)
    return tuple(toks), bls

def main():
    da, fa = load(sys.argv[1], sys.argv[2])
    db, fb = load(sys.argv[3], sys.argv[4])
    na = {k: norm(da, v) for k, v in fa.items()}
    nb = {k: norm(db, v) for k, v in fb.items()}
    A, B = sorted(fa), sorted(fb)
    m = {}
    # 1) 순서 정렬: 같은 정규형을 앞뒤 순서대로 맞춘다.
    sm = difflib.SequenceMatcher(None, [na[k][0] for k in A], [nb[k][0] for k in B], autojunk=False)
    gaps = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            for x, y in zip(range(i1, i2), range(j1, j2)): m[A[x]] = (B[y], True, 1.0)
        else:
            gaps.append((A[i1:i2], B[j1:j2]))
    # 2) 구간 안에서 비슷한 것끼리 순서대로(욕심) 짝짓는다.
    for ga, gb in gaps:
        used = set()
        for x in ga:
            best, br = None, 0.0
            for y in gb:
                if y in used: continue
                r = difflib.SequenceMatcher(None, na[x][0], nb[y][0], autojunk=False).ratio()
                if r > br: best, br = y, r
            if best is not None and br >= 0.6:
                m[x] = (best, False, round(br, 3)); used.add(best)
    # 3) BL 대상으로 확인·보충: 짝지은 두 함수의 BL 목록이 같은 길이면 대상끼리도 짝이다.
    added = conflicts = 0
    for x, (y, same, r) in list(m.items()):
        ba, bb = na[x][1], nb[y][1]
        if len(ba) != len(bb): continue
        for p, q in zip(ba, bb):
            if p in m:
                if m[p][0] != q: conflicts += 1
            elif p in na and q in nb:
                m[p] = (q, na[p][0] == nb[q][0], 0.0); added += 1
    # 4) GOT 칸: 함수 포인터 칸은 함수 짝, 그 밖은 내용.
    LOAD = 0x100000
    def info(d):
        u = lambda o: struct.unpack_from('<I', d, o)[0]
        n = u(0x70); base = 0x74 + n * 4
        return u, n, base, base + u(base), base + u(base + 4), [base + u(0x74 + i * 4) for i in range(n)]
    ua, nra, ba, ga0, ga1, ra = info(da); ub, nrb, bb, gb0, gb1, rb = info(db)
    assert ba == bb and nra == nrb and ga1 - ga0 == gb1 - gb0
    GA = [ua(o) for o in range(ga0, ga1, 4)]; GB = [ub(o) for o in range(gb0, gb1, 4)]
    text_b = max(fb) + 0x1000
    def tgt(d, v, other):
        o = v + ba
        if v & 1 and (o & ~1) in (fa if other == 'a' else fb): return ('F', o & ~1)
        if o < len(d):
            e = d.find(b'\0', o, o + 64)   # 문자열이면 NUL까지, 아니면 24바이트
            if e > o + 1 and all(c >= 0x20 or c in (9, 10, 13) for c in d[o:e]): return ('D', d[o:e])
            return ('D', d[o:o + 24])
        return ('B', o - len(d))
    for i, (v, w) in enumerate(zip(GA, GB)):
        x, y = tgt(da, v, 'a'), tgt(db, w, 'b')
        if x[0] == 'F' and y[0] == 'F' and x[1] not in m: m[x[1]] = (y[1], na[x[1]][0] == nb[y[1]][0], 0.0); added += 1
    slot = {}
    for i, v in enumerate(GA):
        x = tgt(da, v, 'a'); c = []
        for j, w in enumerate(GB):
            y = tgt(db, w, 'b')
            if x[0] == y[0] == 'F' and m.get(x[1], (None,))[0] == y[1]: c.append(j)
            elif x[0] == y[0] == 'D' and x[1] == y[1]: c.append(j)
            elif x[0] == y[0] == 'B' and x[1] == y[1]: c.append(j)
        if c: slot[i] = min(c, key=lambda j: abs(j - i))
    taken = set(slot.values())
    for i in range(len(GA)):        # 내용이 바뀐 칸: 같은 자리가 비었으면 그 자리
        if i not in slot and i not in taken: slot[i] = i; taken.add(i)
    free_b = sorted(set(range(len(GB))) - taken)
    for i, j in zip(sorted(set(range(len(GA))) - set(slot)), free_b):   # 남은 칸: 남은 빈 칸에 순서대로
        slot[i] = j
    # 5) 데이터 주소 짝(절대 주소): GOT 값, 재배치 자리와 그 값
    data = {}
    for i, j in slot.items():
        x = tgt(da, GA[i], 'a')
        if x[0] != 'F': data.setdefault(LOAD + ba + GA[i], set()).add(LOAD + ba + GB[j])
    for p, q in zip(ra, rb):
        data.setdefault(LOAD + p, set()).add(LOAD + q)
    data = {k: min(v) for k, v in data.items() if len(v) == 1}
    # 6) ARM 도우미(같은 바이트를 찾는다)
    arm = {}
    for x in (0x40024, 0x400d8, 0x40210):
        pat = da[x:x + 32]; j = db.find(pat)
        if j >= 0 and db.find(pat, j + 1) < 0: arm[x] = j
    if len(sys.argv) > 6:
        keep = []   # 손으로 정한 규칙(gofs, bss와 그 설명)은 남긴다
        try:
            lines = open(sys.argv[6]).read().split('\n')
            if '# ---- 손으로 정한 규칙' in '\n'.join(lines):
                keep = lines[next(i for i, l in enumerate(lines) if l.startswith('# ---- 손으로 정한 규칙')):]
        except FileNotFoundError: pass
        with open(sys.argv[6], 'w') as w:
            w.write('# 판 대응표(기준 010100D5 → 대상). work/tools/vermap.py가 두 client.bin에서 만든다. 원본 내용은 없고 주소만 있다.\n')
            w.write('# fn <기준 함수> <대상 함수> <몸이 같으면 1>, arm <기준> <대상>, got <기준 칸> <대상 칸>(다른 것만), data <기준 주소> <대상 주소>\n')
            w.write('image %d %d\n' % (len(db), ub(0x70)))
            w.write('text %s\n' % sys.argv[7])
            for k, v in sorted(m.items()): w.write('fn %x %x %d\n' % (k + LOAD, v[0] + LOAD, 1 if v[1] else 0))
            for k, v in sorted(arm.items()): w.write('arm %x %x\n' % (k + LOAD, v + LOAD))
            for i, j in sorted(slot.items()):
                if i != j: w.write('got %x %x\n' % (i * 4, j * 4))
            for k, v in sorted(data.items()): w.write('data %x %x\n' % (k, v))
            if keep: w.write('\n'.join(keep).rstrip('\n') + '\n')
    out = {'functions': {hex(k): {'to': hex(v[0]), 'same': v[1], 'ratio': v[2]} for k, v in sorted(m.items())},
           'summary': {'base_functions': len(A), 'target_functions': len(B), 'matched': len(m),
                       'same': sum(1 for v in m.values() if v[1]), 'from_bl': added, 'bl_conflicts': conflicts}}
    json.dump(out, open(sys.argv[5], 'w'), indent=0)
    print(out['summary'])

main()
