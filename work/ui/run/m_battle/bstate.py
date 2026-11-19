import struct,sys
b=open(sys.argv[1],'rb').read()
u=lambda a: struct.unpack_from('<I',b,a-0x100000)[0]
app=u(0x14edd4); game=u(app+0x54); sc=u(game+0xc)
B=u(game+0x1c8) if sc==13 else 0
print(sc, u(B+0x30) if B else -1, u(B+0x34) if B else -1)
