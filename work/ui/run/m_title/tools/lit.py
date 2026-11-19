import json,struct,sys
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
ROOT=__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..'))
data=open(f'{ROOT}/native/build/game/client.bin','rb').read()
cm=json.load(open(f'{ROOT}/native/build/codemap.json'))
funcs={int(k,16):v for k,v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
want=set(int(x,16) for x in sys.argv[1:])
for e,v in sorted(funcs.items()):
  for ad in v['addrs']:
    i=next(md.disasm(data[ad:ad+4],ad,1))
    if i.mnemonic=='ldr' and '[pc' in i.op_str:
      off=int(i.op_str.split('#')[1].rstrip(']'),0) if '#' in i.op_str else 0
      la=((ad+4)&~3)+off; val=struct.unpack_from('<I',data,la)[0]
      if val in want: print(f'{e:x} {ad:x} {val:#x}')
