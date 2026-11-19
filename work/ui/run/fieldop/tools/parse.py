import struct,sys,collections
def parse(b):
    # header? try from 0
    i=0; out=[]
    while i+4<=len(b):
        op,fl=struct.unpack_from('<hH',b,i)
        n=fl&0xf
        if n==0: return out,i
        out.append((i,op,fl,b[i+4:i+4*n]))
        i+=4*n
    return out,i
if __name__=='__main__':
    cnt=collections.Counter()
    for k in range(2,39):
        b=open(f'scr/{k:02d}.bin','rb').read()
        ins,end=parse(b)
        for x in ins: cnt[x[1]]+=1
        print(k,len(b),end,len(ins))
    print(sorted((hex(o),c) for o,c in cnt.items()))
