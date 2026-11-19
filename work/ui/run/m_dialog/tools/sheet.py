# sheet.py <dir> <out> <y0> name...: 현대(왼쪽)·원작(오른쪽) 쌍을 세로로 잇고 1/3로 줄인다
import sys
from PIL import Image
d, out, y0, names = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4:]
rows = []
for n in names:
    try:
        a = Image.open(f'{d}/{n}.png').convert('RGB'); b = Image.open(f'{d}/{n}_classic.png').convert('RGB')
    except FileNotFoundError:
        continue
    w, h = a.size
    r = Image.new('RGB', (w * 2 + 6, h - y0), (255, 0, 0)); r.paste(a.crop((0, y0, w, h)), (0, 0)); r.paste(b.crop((0, y0, w, h)), (w + 6, 0)); rows.append(r)
W = rows[0].size[0]; H = sum(r.size[1] + 6 for r in rows)
o = Image.new('RGB', (W, H), (255, 0, 0)); y = 0
for r in rows: o.paste(r, (0, y)); y += r.size[1] + 6
k = 3 if W > 1500 else 2
o.resize((W // k, H // k), Image.LANCZOS).save(out)
