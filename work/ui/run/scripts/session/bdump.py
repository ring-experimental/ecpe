import struct,sys
m=open(sys.argv[1],'rb').read()
def rd(a,n=4):
  return struct.unpack_from({4:'<i',2:'<h',1:'<b'}[n],m,a-0x100000)[0]
def u(a): return rd(a)&0xffffffff
def hx(a,n=0x80,step=4):
  return ' '.join(f'{k:x}:{u(a+k):x}' for k in range(0,n,step))
G=u(u(0x14edd4)+0x54); B=u(G+0x1c8)
print('game',hex(G),'battle',hex(B))
print('game+14c',hex(u(G+0x14c)),'game+160',u(G+0x160),'game+1d4',hex(u(G+0x1d4)))
print('B:',hx(B,0x1d0))
P=u(B+4); E=u(B+8)
print('party',hex(P),hx(P,0xc0))
print('enemyside',hex(E),'count',u(E+0xc))
for i in range(u(P+0xc)):
  M=u(P+0x98+4*i); S=u(M+0x64)
  print(' member',i,hex(M),'vt',hex(u(M+0x54)),'stats',hex(S),[rd(S+k) for k in range(0,0x30,4)])
  print('   m:',hx(M,0x1b0))
for i in range(u(E+0xc)):
  X=u(E+0x3d0+4*i)
  if X: S=u(X+0x10); print(' enemy',i,hex(X),'flags',hex(u(X+4)),'x,y',rd(X+0x28),rd(X+0x2c),'stats',hex(S),[rd(S+k) for k in range(0,0x30,4)]); print('   e:',hx(X,0x80))
for nm,off in (('SP',0x58),('CM',0x5c),('EP',0x60),('MSG',0x64),('NM',0x120)):
  O=u(B+off); print(nm,hex(O),hx(O,0xd8 if nm=='CM' else 0x70))
