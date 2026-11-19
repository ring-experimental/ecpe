import sys
from PIL import Image
out=sys.argv[1]; cols=int(sys.argv[2]); fs=sys.argv[3:]
ims=[Image.open(f).convert('RGB') for f in fs]
w=max(i.width for i in ims); h=max(i.height for i in ims)
sc=min(1.0, 1800/(w*cols))
W=int(w*sc); H=int(h*sc)
rows=(len(ims)+cols-1)//cols
M=Image.new('RGB',(W*cols+4*(cols-1),H*rows+4*(rows-1)),(255,0,255))
for k,i in enumerate(ims):
    M.paste(i.resize((int(i.width*sc),int(i.height*sc))),( (k%cols)*(W+4),(k//cols)*(H+4)))
M.save(out)
