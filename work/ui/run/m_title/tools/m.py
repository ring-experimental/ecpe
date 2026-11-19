import struct,sys
class M:
    def __init__(s,f): s.d=open(f,'rb').read()
    def w(s,a): return struct.unpack_from('<I',s.d,a-0x100000)[0]
    def h(s,a): return struct.unpack_from('<h',s.d,a-0x100000)[0]
    def b(s,a): return s.d[a-0x100000]
    def words(s,a,n): return [s.w(a+4*i) for i in range(n)]
    def str(s,a,n=80):
        o=a-0x100000; e=s.d.find(b'\0',o); return s.d[o:min(e,o+n)].decode('cp949','replace')
GOT=0x14e624
