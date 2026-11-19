# raw2png.py <raw> <width> <out.png>: 프레임 기록의 RGB565 raw를 png로 바꾼다.
import sys
from PIL import Image
d = open(sys.argv[1], 'rb').read(); w = int(sys.argv[2]); h = len(d) // 2 // w
im = Image.new('RGB', (w, h)); px = im.load()
for i in range(w * h):
    p = d[2 * i] | d[2 * i + 1] << 8
    px[i % w, i // w] = (((p >> 11 & 31) * 255 + 15) // 31, ((p >> 5 & 63) * 255 + 31) // 63, ((p & 31) * 255 + 15) // 31)
im.save(sys.argv[3])
