import struct,lzma,sys
d=open(__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'work/game/script.bin'),'rb').read()
n=struct.unpack_from('<I',d,8)[0]
for i in range(n):
    off,mg,x=struct.unpack_from('<III',d,0xc+i*12)
    usz=struct.unpack_from("<I",d,off+4)[0]
    p=d[off+16]; ds=struct.unpack_from('<I',d,off+17)[0]
    lc=p%9; r=p//9; lp=r%5; pb=r//5
    dec=lzma.LZMADecompressor(lzma.FORMAT_RAW,filters=[{'id':lzma.FILTER_LZMA1,"dict_size":1<<20,'lc':lc,'lp':lp,'pb':pb}])
    try: out=dec.decompress(d[off+21:],max_length=usz)
    except Exception as e: out=b''; print(i,e)
    open(f'scr/{i:02d}.bin','wb').write(out)
    print(i,hex(off),hex(x),usz,len(out),d[off:off+16].hex())
