# fit.py <정리된 그림 RGBA png> <원본 초상 프레임 png(자홍 = 투명)> <출력 png> [배율=8]
# 원본 도트 초상은 이 일러스트를 줄여 찍은 것이다. 일러스트를 줄인 격자 위에서 도트 그림과 가장 맞는 배율·자리(·좌우 뒤집기)를 찾아,
# 도트 프레임 테두리와 꼭 맞는 자리를 잘라 (프레임 크기 × 배율)로 남긴다. host/ui.cpp illust_sprite가 이 맞춤을 전제로 한다.
import sys
import numpy as np
from PIL import Image

src, spr, out = sys.argv[1:4]
OUT_SCALE = int(sys.argv[4]) if len(sys.argv) > 4 else 8
G = Image.open(src).convert('RGBA')
S = np.asarray(Image.open(spr).convert('RGB')).astype(np.float32)
h, w = S.shape[:2]
m = ~((S[..., 0] > 240) & (S[..., 1] < 20) & (S[..., 2] > 240))
PAD = 12

def grid(k, flip):
    g = G.transpose(Image.FLIP_LEFT_RIGHT) if flip else G
    gw, gh = max(1, round(g.width / k)), max(1, round(g.height / k))
    a = np.asarray(g.resize((gw, gh), Image.BOX)).astype(np.float32)
    p = np.zeros((gh + 2 * PAD, gw + 2 * PAD, 4), np.float32)
    p[PAD:PAD + gh, PAD:PAD + gw] = a
    return p, g.width / gw, g.height / gh

def best_offset(p):
    al = p[..., 3] / 255
    best = (1e9, 0, 0)
    if p.shape[0] < h or p.shape[1] < w: return best
    for oy in range(0, p.shape[0] - h + 1):
        rows = p[oy:oy + h]
        win = np.lib.stride_tricks.sliding_window_view(rows, (h, w, 4))[0, :, 0]  # (nx, h, w, 4)
        a = win[..., 3] / 255
        mis = np.abs(a - m).mean(axis=(1, 2))
        both = (a > 0.5) & m
        cd = (np.abs(win[..., :3] - S).mean(-1) * both).sum(axis=(1, 2)) / np.maximum(both.sum(axis=(1, 2)), 1) / 255
        sc = mis + cd
        i = int(sc.argmin())
        if sc[i] < best[0]: best = (float(sc[i]), i, oy)
    return best

k0 = G.height / h
res = []
for flip in (False, True):
    for r in np.arange(0.35, 1.25, 0.03):
        p, kx, ky = grid(k0 * r, flip)
        sc, ox, oy = best_offset(p)
        res.append((sc, k0 * r, flip, ox, oy))
res.sort()
sc, k, flip, _, _ = res[0]
fine = []
for kk in np.arange(k * 0.97, k * 1.03, k * 0.004):
    p, kx, ky = grid(kk, flip)
    s2, ox, oy = best_offset(p)
    fine.append((s2, kk, kx, ky, ox, oy))
fine.sort()
sc, k, kx, ky, ox, oy = fine[0]
g = G.transpose(Image.FLIP_LEFT_RIGHT) if flip else G
box = ((ox - PAD) * kx, (oy - PAD) * ky, (ox - PAD + w) * kx, (oy - PAD + h) * ky)
# 둘레를 가장자리 화소로 늘인다: 상반신이 잘린 아래쪽 등 그림 끝이 프레임 끝에 조금 못 미쳐도 틈이 없게(투명한 가장자리는 투명 그대로)
off = (int(2 * PAD * kx), int(2 * PAD * ky))
canvas = Image.fromarray(np.pad(np.asarray(g), ((off[1], off[1]), (off[0], off[0]), (0, 0)), mode='edge'))
crop = canvas.crop((round(box[0] + off[0]), round(box[1] + off[1]), round(box[2] + off[0]), round(box[3] + off[1])))
crop = crop.resize((w * OUT_SCALE, h * OUT_SCALE), Image.LANCZOS)
crop.save(out)
print(f'{out}: score {sc:.3f} k {k:.2f} flip {flip} box {tuple(round(v) for v in box)} -> {crop.size}')
