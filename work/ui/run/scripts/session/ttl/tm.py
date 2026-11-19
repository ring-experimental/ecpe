import struct,sys
class M:
    def __init__(s,f): s.b=open(f,'rb').read()
    def u32(s,a): return struct.unpack_from('<I',s.b,a-0x100000)[0]
    def s32(s,a): return struct.unpack_from('<i',s.b,a-0x100000)[0]
    def cstr(s,a):
        o=a-0x100000; e=s.b.index(b'\0',o); return s.b[o:e].decode('cp949','replace')
    def got(s,off): return s.u32(0x14e624+off)
    def gs(s,off): return s.cstr(s.got(off))
