import sys,struct
sys.path.insert(0,sys.argv[0].rsplit('/',1)[0]); from img import b
F=set(int(l,16) for l in open(sys.argv[0].rsplit('/',1)[0]+'/funcs.txt'))
s,e=int(sys.argv[1],16),int(sys.argv[2],16)
for o in range(s,e,4):
    v=struct.unpack_from('<I',b,o)[0]
    t='F' if (v&~1) in F else ''
    print('%x: %08x %s'%(o,v,t))
