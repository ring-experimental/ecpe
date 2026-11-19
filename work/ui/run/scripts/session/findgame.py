import struct,sys,array
m=open(sys.argv[1],'rb').read()
a=array.array('I'); a.frombytes(m[:len(m)//4*4])
BASE=0x100000; L=len(m)
def rd(addr):
    i=addr-BASE
    if i<0 or i+4>L or i&3: return None
    return a[i>>2]
seen=set()
for i,G in enumerate(a):
    if G<BASE or G>=BASE+L-0x200 or G&3 or G in seen: continue
    seen.add(G)
    B=rd(G+0x1c8)
    if B and rd(B)==G and B!=G:
        print(hex(G),hex(B),'state',rd(B+0x30),'ref at',hex(BASE+4*i))
