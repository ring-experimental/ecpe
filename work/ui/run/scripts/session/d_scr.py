import struct,lzma,re,sys
b=open(__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'native/build/game/script.bin'),'rb').read()
cnt=struct.unpack_from('<I',b,8)[0]
def dec(i):
    off=struct.unpack_from('<I',b,12+12*i)[0]; p=off+16
    props=b[p]; d=struct.unpack_from('<I',b,p+1)[0]
    lc=props%9; r=props//9; lp=r%5; pb=r//5
    f=[{'id':lzma.FILTER_LZMA1,'dict_size':d,'lc':lc,'lp':lp,'pb':pb}]
    dc=lzma.LZMADecompressor(lzma.FORMAT_RAW,filters=f)
    try: return dc.decompress(b[p+5:])
    except Exception as e: return dc.unused_data if False else b''
for i in range(cnt):
    x=dec(i)
    hits=[m.start() for m in re.finditer(b'\x9c\x00',x) if m.start()%2==0]
    hits2=[m.start() for m in re.finditer(b'\x9d\x00',x) if m.start()%2==0]
    print(i,len(x),len(hits),len(hits2))
