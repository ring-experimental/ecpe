#!/usr/bin/env python3
"""인터프리터 빌드의 연결 파일을 만든다. 원본 파일은 읽지 않는다.

interpgen.py <restored/list.txt> <out.c> <소스...>
- 교체 표: list.txt(ECPE는 list.d를 합친 build/list.txt)의 함수 주소 → restored/의 f_<오프셋>.
- 읽는 C가 이름으로 부르는 원본 함수(f_<오프셋>, orig_f_<오프셋>)는 인터프리터로 넘기는 한 줄 함수로 만든다.
"""
import re, sys

rows = [l.split() for l in open(sys.argv[1]) if l.strip() and not l.startswith('#')]
hooks = sorted({int(x[0], 16) for x in rows})
names = set()
# `wrap` 줄은 원본을 orig_f_<오프셋>으로 부른다. 매크로(orig_f_##off)로 쓰면 소스에서 이름이 안 보이므로 목록에서 만든다.
names |= {f'orig_f_{int(x[0], 16):x}' for x in rows if 'wrap' in x[2:]}
for p in sys.argv[3:]:
    names |= set(re.findall(r'\b((?:orig_)?f_[0-9a-f]{3,6})\b', open(p, encoding='utf-8', errors='replace').read()))
LOAD = 0x100000
with open(sys.argv[2], 'w') as w:
    w.write('#include "host.h"\nvoid interp_call(cpu_t*c,uint32_t t,bool skip_hook);\nvoid interp_exec(cpu_t*c,uint32_t t);\nextern void (*guest_exec)(cpu_t*,uint32_t);\n')
    for h in hooks:
        w.write(f'void f_{h:x}(cpu_t*c);\n')
    for n in sorted(names):
        off = int(n.split('_')[-1], 16)
        if n.startswith('orig_'):
            w.write(f'void {n}(cpu_t*c){{interp_call(c,{off + LOAD | 1}u,true);}}\n')
        elif off not in hooks:
            w.write(f'void {n}(cpu_t*c){{interp_call(c,{off + LOAD | 1}u,false);}}\n')
    w.write('const fn_entry_t interp_hooks[]={\n' + ''.join(f'{{{h + LOAD}u,f_{h:x}}},\n' for h in hooks) + '{0,0}};\n')
    w.write('const fn_entry_t fn_table[]={{0,0}};\n')
    w.write('__attribute__((constructor)) static void interp_install(void){guest_exec=interp_exec;}\n')
print('hooks', len(hooks), 'stubs', len(names))
