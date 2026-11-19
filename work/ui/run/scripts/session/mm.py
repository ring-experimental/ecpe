import sys,struct
class M:
    def __init__(s,f): s.b=open(f,'rb').read()
    def u32(s,a): return struct.unpack_from('<I',s.b,a-0x100000)[0]
    def u16(s,a): return struct.unpack_from('<H',s.b,a-0x100000)[0]
    def u8(s,a): return s.b[a-0x100000]
    def s(s_,a,n=200):
        o=a-0x100000; e=s_.b.index(b'\0',o); return s_.b[o:min(e,o+n)].decode('cp949','replace')
    def hx(s,a,n): return s.b[a-0x100000:a-0x100000+n].hex(' ')
