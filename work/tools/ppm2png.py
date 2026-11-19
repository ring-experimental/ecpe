# ppm2png.py <in.ppm> <out.png>; also prints the largest all-black rectangle's column range per row band.
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
im.save(sys.argv[2])
w, h = im.size
px = im.load()
print('size', w, h)
for y in range(0, h, 40):
    black = [x for x in range(w) if px[x, y] == (0, 0, 0)]
    print('y', y, 'black', len(black), (black[0], black[-1]) if black else None)
