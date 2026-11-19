import sys,struct
sys.path.insert(0,sys.argv[0].rsplit('/',1)[0]); from img import b
G=0x4e624
def s(o): e=b.find(b'\0',o); return b[o:e].decode('cp949','replace')
for a in sys.argv[1:]:
    p=a.split(':'); slot=int(p[0],16); p0=struct.unpack_from('<I',b,G+slot)[0]
    if len(p)==1: print(hex(slot),hex(p0),repr(s(p0)))
    elif p[1]=='p': n=int(p[2]); print(hex(slot),hex(p0),[s(struct.unpack_from('<I',b,p0+4*i)[0]) for i in range(n)])
    else: n=int(p[1]); st=int(p[2]); print(hex(slot),hex(p0),[s(p0+st*i) for i in range(n)])
