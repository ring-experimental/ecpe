import sys
from PIL import Image
out=sys.argv[1]; fs=sys.argv[2:]
ims=[Image.open(f) for f in fs]; w,h=ims[0].size; c=min(4,len(ims)); r=(len(ims)+c-1)//c
o=Image.new('RGB',(w*c,h*r),(80,0,80))
for i,im in enumerate(ims): o.paste(im,((i%c)*w,(i//c)*h))
o.save(out)
