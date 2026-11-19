import struct,sys
b=open(sys.argv[1],'rb').read()
u=lambda a: struct.unpack_from('<I',b,a-0x100000)[0]
A=u(0x14ee34); G=u(A+0x48); E=u(G+0x1c4)
def cs(a):
    e=b.index(b'\0',a-0x100000); return b[a-0x100000:e].decode('cp949','replace')
print('G',hex(G),'scene',u(G+0xc),'flags14',hex(u(G+0x14)),'flags20',hex(u(G+0x20)),'map',b[G+0x2d-0x100000],b[G+0x2e-0x100000],b[G+0x2f-0x100000],'name4dc',cs(G+0x4dc),'name4fc',cs(G+0x4fc))
print('E',b[E-0x100000:E-0x100000+0x40].hex(' '))
hero=u(G+0x1b4); print('hero',hex(hero),u(hero+8),u(hero+0xc),'tile',u(hero+8)//14,u(hero+0xc)//14)
