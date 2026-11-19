import sys,struct
b=open(__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'native/build/game/client.bin'),'rb').read()
G=0x4e624
off=int(sys.argv[1],16); n=int(sys.argv[2],0); base=int(sys.argv[3],16) if len(sys.argv)>3 else 0
t=(G+off)&0xffffffff
for i in range(n):
    e=struct.unpack_from('<i',b,t+4*i)[0]
    print('%3x -> %x'%(base+i,(t+e)&~1))
