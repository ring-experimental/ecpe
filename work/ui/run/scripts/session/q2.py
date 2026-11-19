import sys; sys.path.insert(0,sys.argv[1]); from mem import M
m=M(sys.argv[2])
P=0x437cb0
print('hdr',hex(m.u32(P)),'frames',hex(m.u32(P+4)))
fa=m.u32(P+4)
for i in range(26):
    f=m.u32(fa+4*i); n=m.u32(f)
    parts=[(m.b[f-0x100000+4+4*k:f-0x100000+8+4*k]).hex() for k in range(min(n,4))]
    print(i,hex(f),n,parts)
