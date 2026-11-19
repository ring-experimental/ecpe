#!/usr/bin/env python3
"""calltree 덤프(nctl.sh calltree)를 줄인다. 그리기 잎(API)마다 뿌리부터의 함수 경로를 한 줄로 적는다.
treepath.py <덤프> [--all]   --all이면 API 잎이 없는 가지도 깊이 제한(-d N, 기본 6)까지 보인다."""
import sys
lines = open(sys.argv[1], encoding='cp949', errors='replace').read().split('\n')
allb = '--all' in sys.argv
maxd = int(sys.argv[sys.argv.index('-d') + 1]) if '-d' in sys.argv else 6
stack = []; last = None
for l in lines:
    if not l.strip(): continue
    d = (len(l) - len(l.lstrip())) // 2
    t = l.strip()
    stack = stack[:d]
    if t.startswith('*'):
        p = '>'.join(s.split()[0] for s in stack)
        if p != last: print(p); last = p
        print('    ' + t)
    else:
        stack.append(t)
        if allb and d < maxd: print('  ' * d + t)
