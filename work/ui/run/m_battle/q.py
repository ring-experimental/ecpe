import sys; sys.path.insert(0,__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../..')), 'work/ui/run/m_battle'))
from mem import M
m=M(sys.argv[1])
g=m.u32(m.u32(0x14edd4)+0x54) if False else None
app=m.u32(0x14edd4); game=m.u32(app+0x54); B=m.u32(game+0x1c8)
print('app %x game %x B %x state %d sub %d'%(app,game,B,m.u32(B+0x30),m.u32(B+0x34)))
P=m.u32(B+4);E=m.u32(B+8)
print('P %x n %d  E %x n %d'%(P,m.u32(P+0xc),E,m.u32(E+0xc)))
for i in range(m.u32(P+0xc)):
    Mm=m.u32(P+0x98+4*i); S=m.u32(Mm+0x64)
    print(' M%d %x id5=%d flags58=%x 5c=%x 60=%x S=%s mpmax=%d mg=%d/%d 1a0=%d quick=%s skills=%d sp=%d'%(i,Mm,m.u8(Mm+5),m.u32(Mm+0x58),m.u32(Mm+0x5c),m.u32(Mm+0x60),[m.s32(S+4*k) for k in range(5)],m.u32(Mm+0xf4),m.u32(Mm+0xf8),m.u32(Mm+0xfc),m.u32(Mm+0x1a0),[m.u8(Mm+0x13d+k) for k in range(3)],m.u32(Mm+0x174),m.u32(Mm+0x17c)))
for i in range(m.u32(E+0xc)):
    X=m.u32(E+0x3d0+4*i); print(' X%d %x f4=%x xy=%d,%d hp=%d/%d'%(i,X,m.u32(X+4),m.s32(X+0x28),m.s32(X+0x2c),m.s32(m.u32(X+0x10)),m.s32(m.u32(X+0x10)+4)))
CM=m.u32(B+0x5c)
print('CM', ['%x'%m.u32(CM+o) for o in range(0,0x50,4)])
print('B 28=%x 2c=%d 174=%d 198=%d 19c=%d 1c0=%d 1b4=%x P1c=%x Pa4=%x'%(m.u32(B+0x28),m.u32(B+0x2c),m.u32(B+0x174),m.u32(B+0x198),m.u32(B+0x19c),m.u32(B+0x1c0),m.u32(B+0x1b4),m.u32(P+0x1c),m.u32(P+0xa4)))
print('game 14c=%x 160=%x 36=%d'%(m.u32(game+0x14c),m.u32(game+0x160),m.u8(game+0x36)))
