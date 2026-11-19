#!/usr/bin/env python3
"""native/ver/*.map(판 대응표)을 호스트가 링크하는 C 표로 바꾼다. 원본은 읽지 않는다(파이썬 표준 라이브러리만).
줄: image <크기> <재배치 수>, text <코드 끝>, fn, arm, got, data(생성), gofs <오프셋> <더할 값>, bss <시작> <끝> <더할 값>(손으로).

vergen.py <out.c> <map...>   각 표 이름은 파일 이름(AID)이다.
"""
import os, sys

out = open(sys.argv[1], 'w')
out.write('// 생성 파일(work/tools/vergen.py). 고치지 말 것.\n#include "ver.h"\n')
names = []
for p in sys.argv[2:]:
    aid = os.path.basename(p).split('.')[0]
    sec = {'fn': [], 'arm': [], 'got': [], 'data': [], 'gofs': [], 'bss': []}
    size = 0
    for l in open(p):
        t = l.split()
        if not t or t[0].startswith('#'): continue
        if t[0] == 'image': size = int(t[1]); continue
        if t[0] == 'text': continue
        if t[0] == 'gofs': sec['gofs'].append((int(t[1], 16), int(t[2], 0))); continue
        if t[0] == 'bss': sec['bss'].append((int(t[1], 16), int(t[2], 16), int(t[3], 0))); continue
        a, b = int(t[1], 16), int(t[2], 16)
        if t[0] == 'fn': sec['fn'].append((a, b, int(t[3])))
        else: sec[t[0]].append((a, b))
    v = 'v_' + aid
    out.write(f'static const ver_fn_t {v}_fn[] = {{\n' + ''.join(f'{{{a:#x}u,{b:#x}u,{s}}},\n' for a, b, s in sorted(sec['fn']) or [(0, 0, 0)]) + '};\n')
    for k in ('arm', 'got', 'data'):
        rows = sorted(sec[k]) or [(0, 0)]
        out.write(f'static const ver_pair_t {v}_{k}[] = {{\n' + ''.join(f'{{{a:#x}u,{b:#x}u}},\n' for a, b in rows) + '};\n')
    out.write(f'static const ver_gofs_t {v}_gofs[] = {{\n' + ''.join(f'{{{a:#x}u,{d}}},\n' for a, d in sorted(sec['gofs']) or [(0, 0)]) + '};\n')
    out.write(f'static const ver_bss_t {v}_bss[] = {{\n' + ''.join(f'{{{a:#x}u,{b:#x}u,{d}}},\n' for a, b, d in sorted(sec['bss']) or [(0, 0, 0)]) + '};\n')
    out.write(f'const ver_table_t ver_table_{aid} = {{"{aid}", {size}u, {v}_fn, {len(sec["fn"])}, {v}_arm, {len(sec["arm"])}, '
              f'{v}_got, {len(sec["got"])}, {v}_data, {len(sec["data"])}, {v}_gofs, {len(sec["gofs"])}, {v}_bss, {len(sec["bss"])}}};\n')
    names.append(aid)
out.write('const ver_table_t *const ver_tables[] = {' + ''.join(f'&ver_table_{n}, ' for n in names) + 'NULL};\n')
