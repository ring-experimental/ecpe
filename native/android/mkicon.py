# mkicon.py <타이틀 프레임.raw> <res 폴더>: 타이틀 화면(240x320 RGB565)의 흰 바탕과 로고로 런처 아이콘을 만든다.
# 적응형 아이콘은 이 그림을 배경층으로 쓰고, 로고가 원형 마스크의 보이는 영역(108dp 중 72dp) 안에 들도록 둘레를 넉넉히 잡는다.
# 화면 밖은 가장자리를 거울처럼 이어 붙인다. 크기 조정은 macOS sips를 쓴다.
import os, struct, subprocess, sys, zlib
raw, res = sys.argv[1], sys.argv[2]
W, H = 240, 320
f = open(raw, 'rb').read()
assert len(f) == W * H * 2, len(f)
px = [[0] * W for _ in range(H)]
for y in range(H):
    for x in range(W):
        p = f[(y * W + x) * 2] | f[(y * W + x) * 2 + 1] << 8
        px[y][x] = ((p >> 11 & 31) * 255 // 31, (p >> 5 & 63) * 255 // 63, (p & 31) * 255 // 31)
# 타이틀 배경은 흰색이다. 통신사 로고(KTF, magic n), 등급 표시, 버전 글자는 로고 위아래라 흰 줄로 덮는다.
for y in range(H):
    if y < 65 or y >= 224: px[y] = [px[H - 1][x] for x in range(W)]

def mirror(v, n):
    while not 0 <= v < n: v = -v - 1 if v < 0 else 2 * n - v - 1
    return v

CX, CY, C = 120, 150, 280  # 로고 중심과 배경층 한 변(화면 픽셀)
img = [[px[mirror(CY - C // 2 + y, H)][mirror(CX - C // 2 + x, W)] for x in range(C)] for y in range(C)]

def png(path, rows):
    n = len(rows); data = b''.join(b'\0' + b''.join(bytes(c) for c in r for _ in (0, 1)) for r in rows for _ in (0, 1))
    ch = lambda t, b: struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', n * 2, n * 2, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(data)) + ch(b'IEND', b''))

tmp = res + '.tmp'; os.makedirs(tmp, exist_ok=True)
png(f'{tmp}/bg.png', img)  # 2배 최근접으로 키워 두고 sips가 줄이게 한다
k = C * (72 / 108); o = round((C - k) / 2)
png(f'{tmp}/legacy.png', [r[o:C - o] for r in img[o:C - o]])
for dpi, s in (('mdpi', 1), ('hdpi', 1.5), ('xhdpi', 2), ('xxhdpi', 3), ('xxxhdpi', 4)):
    d = f'{res}/mipmap-{dpi}'; os.makedirs(d, exist_ok=True)
    for src, dst, dp in (('bg', 'ic_launcher_bg', 108), ('legacy', 'ic_launcher', 48)):
        n = str(round(dp * s))
        subprocess.run(['sips', '-z', n, n, f'{tmp}/{src}.png', '--out', f'{d}/{dst}.png'], check=True, capture_output=True)
d = f'{res}/mipmap-anydpi-v26'; os.makedirs(d, exist_ok=True)
open(f'{d}/ic_launcher.xml', 'w').write('''<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@mipmap/ic_launcher_bg" />
    <foreground android:drawable="@android:color/transparent" />
</adaptive-icon>
''')
print('icon', res)
