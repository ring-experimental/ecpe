import sys
exec(open(sys.argv[2]+'/m.py').read().split("app=m.u32")[0])
m=M(sys.argv[1])
app=m.u32(0x14ee34); game=m.u32(app+0x48); sdb=m.u32(game+0x1d0)
n=m.u32(sdb+4); recs=m.u32(sdb+8); pool=m.u32(sdb+0xc)
print('sdb',hex(sdb),n,hex(recs),hex(pool))
for i in range(n):
    r=recs+28*i; f=m.u32(r+8); nm=m.s(pool+m.u16(r+4),30)
    tags=[]
    if f>>28==1: tags.append('FIELD')
    if f&0x800: tags.append('ALL')
    if f&0x400000: tags.append('COMBO%02x'%(f&0xff))
    print(i, m.u8(r), m.u8(r+3), hex(f), nm, ' '.join(tags))
