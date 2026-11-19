import struct,sys
class M:
    def __init__(s,f): s.b=open(f,'rb').read()
    def u32(s,a): a-=0x100000; return struct.unpack_from('<I',s.b,a)[0]
    def s32(s,a): a-=0x100000; return struct.unpack_from('<i',s.b,a)[0]
    def u8(s,a): return s.b[a-0x100000]
    def u16(s,a): a-=0x100000; return struct.unpack_from('<H',s.b,a)[0]
    def cstr(s,a):
        a-=0x100000; e=s.b.index(b'\0',a); return s.b[a:e].decode('cp949','replace')
