import struct,sys
b=open(sys.argv[1],'rb').read()
def u(x): return struct.unpack_from('<I',b,x-0x100000)[0]
A=u(0x14ee34); G=u(A+0x48); VM=u(G+0x170)
print('G',hex(G),'scene',u(G+0xc),'VM',hex(VM))
print('VM words',[hex(u(VM+i)) for i in range(0,0x50,4)])
base=u(VM+0x10); off=u(VM+0x2c); pc=base+off+8
print('base',hex(base),'off',hex(off),'pc',hex(pc),'state25',b[VM+0x25-0x100000],'26',b[VM+0x26-0x100000])
p=pc
for k in range(int(sys.argv[2]) if len(sys.argv)>2 else 8):
    op,fl=struct.unpack_from('<hH',b,p-0x100000); n=fl&0xf
    print(hex(p),hex(p-base),'op %x fl %x'%(op,fl),b[p-0x100000+4:p-0x100000+4*max(n,1)].hex())
    if n==0: break
    p+=4*n
