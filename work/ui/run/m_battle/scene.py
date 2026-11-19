import struct,sys
b=open(sys.argv[1],'rb').read()
u=lambda a: struct.unpack_from('<I',b,a-0x100000)[0]
app=u(0x14edd4); game=u(app+0x54); print(u(game+0xc))
