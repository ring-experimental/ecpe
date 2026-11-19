import sys,struct
sys.path.insert(0,sys.argv[0].rsplit('/',1)[0]); from img import b,B,R,g0,g1
print('base off %x relocs %d got %x-%x'%(B,R,g0,g1))
for a in sys.argv[1:]:
    v=int(a,16)
    for t in (v+1,v):
        p=struct.pack('<I',t); i=b.find(p)
        while i>=0: print(a,'ref at %x'%i); i=b.find(p,i+1)
