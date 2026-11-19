import json,struct,sys,re
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
R=__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..'))
data=open(R+'/native/build/game/client.bin','rb').read()
cm=json.load(open(R+'/native/build/codemap.json'))
funcs={int(k,16):v for k,v in cm['roots'].items() if v['thumb'] and v.get('addrs')}
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
u32=lambda o: struct.unpack_from('<I',data,o)[0]
GOT=0x4e624
strs={}
for m in re.finditer(rb'[\x20-\x7e\xa1-\xfe]{2,}\x00',data[0x46878:]):
    s=m.group()[:-1]
    try: t=s.decode('cp949')
    except: continue
    strs[0x46878+m.start()]=t
# substring refs: also allow pointing into middle? keep start only
ref={}
for e,f in funcs.items():
    for ad in f['addrs']:
        i=next(md.disasm(data[ad:ad+4],ad,1))
        if i.mnemonic=='ldr' and '[pc' in i.op_str:
            off=int(i.op_str.split('#')[1].rstrip(']'),0) if '#' in i.op_str else 0
            la=((ad+4)&~3)+off
            if la+4>len(data): continue
            v=u32(la)
            for c in ((GOT+v)&0xffffffff, (v+0x1254)&0xffffffff):
                if c in strs: ref.setdefault(c,set()).add((e,ad))
            g=(GOT+v)&0xffffffff
            if g+4<=len(data) and GOT<=g<len(data):
                p=(u32(g)+0x1254)&0xffffffff
                if p in strs: ref.setdefault(p,set()).add((e,ad))
import pickle
pickle.dump((strs,ref),open(sys.argv[1],'wb'))
