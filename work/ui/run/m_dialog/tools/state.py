# state.py <memdump>: 장면 번호와 스크립트 VM 대기 상태를 찍는다
import struct, sys
m = open(sys.argv[1], 'rb').read()
u = lambda a: struct.unpack_from('<I', m, a - 0x100000)[0]
G = u(u(0x14ee34) + 0x48); VM = u(G + 0x170)
print(u(G + 0xc), m[VM + 0x25 - 0x100000])
