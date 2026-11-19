import struct,sys
class M:
    def __init__(s,p): s.b=open(p,'rb').read()
    def r32(s,a): return struct.unpack_from('<I',s.b,a-0x100000)[0]
    def r16(s,a): return struct.unpack_from('<H',s.b,a-0x100000)[0]
    def r8(s,a): return s.b[a-0x100000]
    def s(s,a):
        o=a-0x100000; e=s.b.index(b'\0',o); return s.b[o:e].decode('cp949','replace')
