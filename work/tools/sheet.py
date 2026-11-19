# sheet.py <out.png> <png...>: 스크린샷 여러 장을 두 줄로 붙여 한 장으로 본다(Pillow 필요: work/tools/py sheet.py ...).
import sys
from PIL import Image
out=sys.argv[1]; ims=[Image.open(f).convert('RGB') for f in sys.argv[2:]]
w=852; ims=[i.resize((w,int(i.height*w/i.width))) for i in ims]
cols=2; rows=(len(ims)+1)//2; h=max(i.height for i in ims)
s=Image.new('RGB',(cols*w+6,rows*h+(rows-1)*6),(255,0,255))
for k,i in enumerate(ims): s.paste(i,((k%cols)*(w+6),(k//cols)*(h+6)))
s.save(out)
