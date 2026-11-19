#!/usr/bin/env python3
"""함수 하나를 Thumb 역어셈블로 보인다(파일 오프셋 기준, 번역 C인 native/build/tr.c의 f_<오프셋>과 같은 함수).
tdis.py <오프셋(hex)> [...]      리터럴 풀 값과 bl 대상을 주석으로 붙인다. 리터럴이 이미지 안 CP949 문자열이면 글도 보인다.
tdis.py --callers <오프셋>       이 함수를 직접 부르는(bl) 함수 목록.
tdis.py --callees <오프셋>       이 함수가 직접 부르는 함수 목록.
tdis.py --str <글(UTF-8)>        이미지에서 CP949 문자열을 찾아 오프셋을 보인다.
간접 호출(vtable, 0x44f1c~0x44f30 계열 `bx rN` 베니어)은 정적으로 안 잡힌다. 그런 연결은 ecpe-trace의 calltree로 본다."""
import json, struct, sys, os
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
data = open(f'{ROOT}/native/build/game/client.bin', 'rb').read()
cm = json.load(open(f'{ROOT}/native/build/codemap.json'))
funcs = {int(k, 16): v for k, v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
def calls(e):
    out = []
    for a in funcs[e]['addrs']:
        i = next(md.disasm(data[a:a + 4], a, 1))
        if i.mnemonic == 'bl' and a not in funcs[e]['far']: out.append(int(i.op_str[1:], 0))
    return out
def cstr(o):
    if not (0 < o < len(data)): return None
    e = data.find(b'\0', o)
    if e < 0 or e - o < 2 or e - o > 200: return None
    try: s = data[o:e].decode('cp949')
    except Exception: return None
    return s if all(ch.isprintable() or ch in '\n' for ch in s) else None
a = sys.argv[1:]
if a[0] == '--callers':
    t = int(a[1], 16); print(' '.join(f'{e:x}' for e in sorted(funcs) if t in calls(e)))
elif a[0] == '--callees':
    print(' '.join(f'{x:x}' for x in sorted(set(calls(int(a[1], 16))))))
elif a[0] == '--str':
    b = a[1].encode('cp949'); o = -1
    while (o := data.find(b, o + 1)) >= 0: print(f'{o:x}')
else:
    for x in a:
        e = int(x, 16)
        print(f'; ---- f_{e:x} ----')
        for ad in funcs[e]['addrs']:
            i = next(md.disasm(data[ad:ad + 4], ad, 1)); note = ''
            if i.mnemonic == 'ldr' and '[pc' in i.op_str:
                off = int(i.op_str.split('#')[1].rstrip(']'), 0) if '#' in i.op_str else 0
                la = ((ad + 4) & ~3) + off; v = u32(la); note = f'; ={v:#x}'
                s = cstr(v - 0x100000) if v >= 0x100000 else None
                if s: note += f' "{s}"'
            if i.mnemonic == 'bl': note = '; far jump' if ad in funcs[e]['far'] else ''
            print(f'{ad:6x}: {i.mnemonic:6} {i.op_str:28} {note}')
