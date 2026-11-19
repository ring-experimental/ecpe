import struct,sys
class M:
    def __init__(s,f): s.d=open(f,'rb').read()
    def u32(s,a): return struct.unpack_from('<I',s.d,a-0x100000)[0]
    def s32(s,a): return struct.unpack_from('<i',s.d,a-0x100000)[0]
    def u16(s,a): return struct.unpack_from('<H',s.d,a-0x100000)[0]
    def u8(s,a): return s.d[a-0x100000]
    def raw(s,a,n): return s.d[a-0x100000:a-0x100000+n]
    def cstr(s,a,n=200):
        b=s.raw(a,n); b=b.split(b'\0')[0]
        return b.decode('cp949','replace')
GAMEPTR=0x14ee34
def code(v): return v-0x100000
