# npc.py <정리된 그림 RGBA png> <출력 png> [mirror]
# 원작에 초상이 없는 인물의 대사창 그림. 투명 테두리를 잘라 높이 64dp × 8배(512px)로 맞춘다(폭은 100dp까지).
# 그림은 오른쪽을 보게 둔다(왼쪽을 보는 원화는 mirror). 대사창이 오른쪽 칸이면 뒤집어 안쪽을 보게 한다(restored/ui_dialog.c).
import sys
from PIL import Image
src, out = sys.argv[1:3]
mirror = len(sys.argv) > 3 and sys.argv[3] == 'mirror'
SCALE, H, WMAX = 8, 64, 100
g = Image.open(src).convert('RGBA')
# 정리본 배경에 옅은 알파 안개가 남는 경우가 있다(겐나드: 배경 전체가 알파 20~36). 알파 40 미만은 지운다.
g.putalpha(g.getchannel('A').point(lambda v: 0 if v < 40 else v))
a = g.getchannel('A').point(lambda v: 255 if v > 8 else 0)
g = g.crop(a.getbbox())
if mirror: g = g.transpose(Image.FLIP_LEFT_RIGHT)
s = H * SCALE / g.height
if g.width * s > WMAX * SCALE: s = WMAX * SCALE / g.width
w, h = round(g.width * s / SCALE) * SCALE, round(g.height * s / SCALE) * SCALE
g = g.resize((w, h), Image.LANCZOS)
g.save(out)
print(out, g.size, (w // SCALE, h // SCALE))
