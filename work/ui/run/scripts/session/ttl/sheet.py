import sys
from PIL import Image
out=sys.argv[1]; fs=sys.argv[2:]
ims=[Image.open(f).convert('RGB') for f in fs]
w=max(i.width for i in ims); h=max(i.height for i in ims)
cols=2; rows=(len(ims)+1)//2
S=Image.new('RGB',(cols*w,rows*h),(255,0,255))
for k,i in enumerate(ims): S.paste(i,((k%cols)*w,(k//cols)*h))
S.save(out)
