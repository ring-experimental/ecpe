# flsheet.py <fl dir> <out prefix> [step] : 처음 나온 순서대로 고유 프레임을 축소 모음으로
import sys, os
from PIL import Image
d=sys.argv[1]; out=sys.argv[2]; scale=0.5
order=[]; seen=set(); first={}
for l in open(d+'/log.txt'):
    f,h,w,hh=l.split(); 
    if h not in seen: seen.add(h); order.append((int(f),h,int(w),int(hh)))
def load(h,w):
    data=open(f'{d}/{h}.raw','rb').read(); hh=len(data)//2//w
    im=Image.frombuffer('RGB',(w,hh),bytes(b for i in range(0,len(data),2) for b in (( (data[i+1]>>3)<<3), ((((data[i+1]&7)<<3)|(data[i]>>5))<<2), ((data[i]&31)<<3))),'raw','RGB',0,1)
    return im
cols=4; per=24
for p in range(0,len(order),per):
    chunk=order[p:p+per]; W=chunk[0][2]//2; H=chunk[0][3]//2
    sheet=Image.new('RGB',(cols*W,((len(chunk)+cols-1)//cols)*(H+12)),'white')
    from PIL import ImageDraw
    dr=ImageDraw.Draw(sheet)
    for i,(f,h,w,hh) in enumerate(chunk):
        im=load(h,w).resize((W,H))
        x=(i%cols)*W; y=(i//cols)*(H+12)
        sheet.paste(im,(x,y+12)); dr.text((x+2,y),f'{f} {h}',fill='black')
    sheet.save(f'{out}_{p//per:02d}.png')
print(len(order))
