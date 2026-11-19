import json,struct,sys
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
R=__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..'))
data=open(R+'/native/build/game/client.bin','rb').read()
cm=json.load(open(R+'/native/build/codemap.json'))
funcs={int(k,16):v for k,v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
u32=lambda o: struct.unpack_from('<I',data,o)[0]
targets=[int(x,16) for x in sys.argv[1:]]
for e,f in funcs.items():
    for ad in f['addrs']:
        i=next(md.disasm(data[ad:ad+4],ad,1))
        if i.mnemonic=='ldr' and '[pc' in i.op_str:
            off=int(i.op_str.split('#')[1].rstrip(']'),0) if '#' in i.op_str else 0
            la=((ad+4)&~3)+off
            if la+4>len(data): continue
            v=u32(la)
            for t in targets:
                if (0x4e624+v)&0xffffffff==t or (v+0x1254)&0xffffffff==t or v==t: print('%x'%t,'fn %x at %x'%(e,ad))
