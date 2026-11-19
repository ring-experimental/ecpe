# relocated image view: returns guest-memory-like bytes for file offsets
import struct
b=bytearray(open(__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'native/build/game/client.bin'),'rb').read())
R=struct.unpack_from('<I',b,0x70)[0]; B=0x74+R*4   # base file offset
for i in range(R):
    o=B+struct.unpack_from('<I',b,0x74+i*4)[0]
    v=struct.unpack_from('<I',b,o)[0]; struct.pack_into('<I',b,o,(v+B)&0xffffffff)
g0=struct.unpack_from('<I',b,B)[0]-0; g1=struct.unpack_from('<I',b,B+4)[0]
# g0,g1 now = file offsets (since relocated by B, values are file offsets)
for o in range(g0,g1,4):
    v=struct.unpack_from('<I',b,o)[0]; struct.pack_into('<I',b,o,(v+B)&0xffffffff)
