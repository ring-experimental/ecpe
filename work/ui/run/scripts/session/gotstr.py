import json, struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
R=__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..'))
code=open(R+'/native/build/game/client.bin','rb').read()
mem=open(R+'/work/ui/run/battle1/m_start.bin','rb').read()
cm=json.load(open(R+'/native/build/codemap.json'))
funcs={int(k,16):v for k,v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
def s(a):
  o=a-0x100000
  if not (0<=o<len(mem)): return None
  e=mem.find(b'\0',o)
  if e-o<1 or e-o>120: return None
  try: t=mem[o:e].decode('cp949')
  except: return None
  return t if all(c.isprintable() for c in t) else None
lo,hi=[int(x,16) for x in sys.argv[1:3]]
for f in sorted(funcs):
  if not lo<=f<hi: continue
  out=[]
  for a in funcs[f]['addrs']:
    i=next(md.disasm(code[a:a+4],a,1))
    if i.mnemonic=='ldr' and '[pc' in i.op_str:
      off=int(i.op_str.split('#')[1].rstrip(']'),0) if '#' in i.op_str else 0
      v=struct.unpack_from('<I',code,((a+4)&~3)+off)[0]
      if v<0x1000 and v%4==0:
        p=struct.unpack_from('<I',mem,0x4e624+v)[0]
        t=s(p)
        if t: out.append(f'{a:x}:GOT+{v:x}->{t!r}')
  if out: print(f'{f:x}', ' '.join(out))
