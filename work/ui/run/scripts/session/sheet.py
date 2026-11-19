# 여러 wshot을 한 장으로: sheet.py out.png cols scale img...
import sys
from PIL import Image
out, cols, sc = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]); fs = sys.argv[4:]
ims = [Image.open(f).convert('RGB') for f in fs]
ims = [im.resize((int(im.width*sc), int(im.height*sc)), Image.LANCZOS) for im in ims]
w = max(i.width for i in ims); h = max(i.height for i in ims); rows = (len(ims)+cols-1)//cols
S = Image.new('RGB', (cols*(w+4), rows*(h+4)), (255,255,255))
for k, im in enumerate(ims): S.paste(im, ((k%cols)*(w+4), (k//cols)*(h+4)))
S.save(out)
