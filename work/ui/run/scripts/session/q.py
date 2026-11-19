import sys; sys.path.insert(0,sys.argv[1]); from mem import M
m=M(sys.argv[2])
app=m.u32(0x14ee34); W=m.u32(app+0x48)
print('app %x W %x'%(app,W))
for o in [0xc,0x10,0x14c,0x15c,0x160,0x164,0x168,0x16c,0x49c,0x540,0x45c]: print(hex(o),hex(m.u32(W+o)))
for off in range(0x194,0x284,4):
    try: print(hex(off), repr(m.gs(off))[:300])
    except Exception as e: print(hex(off),'?',e)
