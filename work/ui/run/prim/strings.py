#!/usr/bin/env python3
"""바이너리 데이터 구역의 문자열(CP949/ASCII)을 모두 뽑고, 참조하는 함수(직접 GOT/GOTOFF 참조, 포인터 배열 경유)를 붙인다."""
import json, struct, collections, bisect
ROOT = __import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../..'))
J = json.load(open(f'{ROOT}/work/ui/run/prim/sa.json')); F = {int(k, 16): v for k, v in J['funcs'].items()}
d = open(f'{ROOT}/native/build/game/client.bin', 'rb').read(); B = 0x1254
u32 = lambda o: struct.unpack_from('<I', d, o)[0]
N = u32(0x70); REL = sorted(B + u32(0x74 + 4 * i) for i in range(N)); RELS = set(REL)
GOT0 = 0x4e624
strs = []
i = 0x46878
while i < GOT0:
    j = d.find(b'\0', i)
    b = d[i:j]
    if 2 <= len(b) <= 300:
        try:
            s = b.decode('cp949')
            hang = any('가' <= c <= '힣' for c in s)
            if all(c.isprintable() or c in '\n\r' for c in s) and (hang or (sum(c.isalpha() for c in s) >= 2 and all(ord(c) < 128 for c in s))):
                strs.append((i, s))
        except Exception: pass
    i = j + 1
starts = [a for a, s in strs]; smap = dict(strs)
# 참조: 함수 refs가 문자열 안을 가리키면(정확히 시작 또는 중간) 그 문자열
refby = collections.defaultdict(set)
ptrarr = collections.defaultdict(set)
def str_at(a):
    k = bisect.bisect_right(starts, a) - 1
    if k >= 0 and starts[k] <= a < starts[k] + len(smap[starts[k]].encode('cp949')) + 1: return starts[k]
for f, v in F.items():
    for r in v['refs']:
        s = str_at(r)
        if s is not None: refby[s].add(f); continue
        a = r; n = 0
        while a in RELS and n < 128:
            t = B + u32(a); s2 = str_at(t)
            if s2 is None: break
            refby[s2].add(f); ptrarr[s2].add(r); a += 4; n += 1
# 재배치 워드가 가리키는 문자열(어떤 배열에서 쓰는지)
relref = collections.defaultdict(set)
for a in REL:
    if a >= 0x46878:
        s = str_at(B + u32(a))
        if s is not None: relref[s].add(a)
out = []
for a, s in strs:
    out.append(dict(off=f'{a:x}', s=s, funcs=[f'{x:x}' for x in sorted(refby.get(a, ()))], arrays=[f'{x:x}' for x in sorted(ptrarr.get(a, ()))],
                    relocs=[f'{x:x}' for x in sorted(relref.get(a, ()))]))
json.dump(out, open(f'{ROOT}/work/ui/run/prim/strings.json', 'w'), ensure_ascii=False, indent=0)
print(len(out), 'strings;', sum(1 for o in out if o['funcs']), 'with func refs;', sum(1 for o in out if not o['funcs'] and o['relocs']), 'only via relocs;', sum(1 for o in out if not o['funcs'] and not o['relocs']), 'unreferenced')
