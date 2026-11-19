import sys; sys.path.insert(0,__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'work/ui/run/prim'))
from mem import M
m=M(sys.argv[1])
app=m.u32(0x14ee34); G=m.u32(app+0x48)
db=m.u32(G+0x1cc); n=m.u32(db); recs=m.u32(db+8); pool=m.u32(db+0x14)
dpool=m.u32(m.u32(db+0x10)+0x318)
print('G',hex(G),'db',hex(db),'n',n, 'VM',hex(m.u32(G+0x170)))
vm=m.u32(G+0x170); vars_=m.u32(vm+0x44); print('vars',[m.s32(vars_+4*i) for i in range(16)])
print('type',m.u32(G+0x55c),'num',m.u32(G+0x560))
for i in range(n):
    r=recs+32*i; raw=m.raw(r,32)
    name=m.cstr(pool+m.u16(r+4)); desc=m.cstr(dpool+m.u16(r+6))
    print(f"{i:3d} cat{raw[0]} b1={raw[1]:02x} icon{raw[2]:3d} shop={raw[3]:08b} price{m.u32(r+8):7d} eq={m.u32(r+12):08x} s10={m.u16(r+16)} s12={raw[18]} s13={raw[19]} rest={raw[20:].hex()} {name} | {desc}")
