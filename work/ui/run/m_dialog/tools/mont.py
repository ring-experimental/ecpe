# mont.py out.png y0 files... : 각 그림의 y0(창 화소)부터 아래를 잘라 세로로 잇고 반으로 줄인다
import sys
from PIL import Image
out, y0, fs = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ims = [Image.open(f).convert('RGB') for f in fs]
w = max(i.size[0] for i in ims)
hs = [i.size[1] - y0 for i in ims]
o = Image.new('RGB', (w, sum(hs) + 4 * len(ims)), (255, 0, 0))
y = 0
for i, h in zip(ims, hs):
    o.paste(i.crop((0, y0, i.size[0], i.size[1])), (0, y)); y += h + 4
sc = float(sys.argv[0] and 0.5)
o = o.resize((int(w * sc), int(o.size[1] * sc)), Image.LANCZOS)
o.save(out)
