import sys
from PIL import Image
d=sys.argv[1]; out=sys.argv[2]; names=sys.argv[3:]
ims=[Image.open(f'{d}/{n}.png').convert('RGB') for n in names]
W=852; ims=[im.resize((W,int(im.height*W/im.width))) for im in ims]
H=max(im.height for im in ims)
cols=2; rows=(len(ims)+1)//2
o=Image.new('RGB',(W*cols,H*rows))
for i,im in enumerate(ims): o.paste(im,((i%cols)*W,(i//cols)*H))
o.save(out)
