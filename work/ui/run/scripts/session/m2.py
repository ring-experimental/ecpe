import sys; sys.path.insert(0,sys.argv[2]); 
exec(open(sys.argv[2]+'/m.py').read().split("app=m.u32")[0])
m=M(sys.argv[1])
a=m.u32(0x14e624+0x4a0); print(hex(a))
for i in range(6): print(i, repr(m.s(a+12*i,12)))
