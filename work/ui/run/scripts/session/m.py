import sys,struct
class M:
    def __init__(s,f): s.b=open(f,'rb').read()
    def u32(s,a): return struct.unpack_from('<I',s.b,a-0x100000)[0]
    def u16(s,a): return struct.unpack_from('<H',s.b,a-0x100000)[0]
    def u8(s,a): return s.b[a-0x100000]
    def s(s_,a,n=200):
        o=a-0x100000; e=s_.b.index(b'\0',o); return s_.b[o:min(e,o+n)].decode('cp949','replace')
    def hx(s,a,n): return s.b[a-0x100000:a-0x100000+n].hex(' ')
m=M(sys.argv[1])
app=m.u32(0x14ee34); game=m.u32(app+0x48); menu=m.u32(game+0x1d4); pd=m.u32(game+0x1c0)
print('app %x game %x menu %x pd %x'%(app,game,menu,pd))
print('game+0x194..', [hex(m.u32(game+0x194+4*i)) for i in range(8)])
print('pd+0x84..', [hex(m.u32(pd+0x84+4*i)) for i in range(5)])
for i in range(8):
    ch=m.u32(game+0x194+4*i)
    if ch<0x400000: continue
    st=m.u32(ch+0x64)
    print(i,hex(ch),'id',m.u8(ch+4),m.u8(ch+5),'lv',m.u8(st+0x18) if st>0x400000 else None,'hp',m.u32(ch+0xcc),m.u32(ch+0xd0),'mp',m.u32(ch+0xd4),m.u32(ch+0xf4),'cls',hex(m.u32(ch+0xb4)),'nsk',m.u8(ch+0x174),'name',hex(m.u32(ch+0x19c)))
print('quests',m.hx(pd+0x3c,64))
for off in [0x460,0x464,0x468,0x46c,0x4b4,0x4b8,0x4c8,0x4cc,0x4d0,0x4d4,0x4d8,0x4dc,0x4e8,0x4f0,0x2c0]:
    v=m.u32(0x14e624+off); a=v
    print(hex(off),hex(v),hex(a),repr(m.s(a,60)))
