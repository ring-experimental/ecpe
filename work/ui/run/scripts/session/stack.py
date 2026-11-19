# stack.py <dir> <out> <y0> <y1> <scale> names...
import sys
from PIL import Image
d,out,y0,y1,sc=sys.argv[1],sys.argv[2],int(sys.argv[3]),int(sys.argv[4]),int(sys.argv[5]); ns=sys.argv[6:]
ims=[Image.open(f'{d}/{n}.png').convert('RGB') for n in ns]; w=ims[0].size[0]
h=(y1-y0)*sc; c=Image.new('RGB',(w*sc,h*len(ims)+2*len(ims)),'white')
for i,im in enumerate(ims): c.paste(im.crop((0,y0,w,y1)).resize((w*sc,h),Image.NEAREST),(0,i*(h+2)))
c.save(out)
