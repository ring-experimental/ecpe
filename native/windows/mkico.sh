#!/bin/zsh
# mkico.sh <맥 빌드 폴더> <ecpe.ico>: Android 아이콘과 같은 그림(타이틀 로고)으로 Windows 아이콘을 만든다.
# 타이틀 프레임은 맥 빌드(build/ecpe)를 헤드리스로 두 번 띄워 받는다(첫 실행은 속도최적화 뒤 끝난다). 원본 파생물이라 build에만 둔다.
set -eu
B=${1:A}; OUT=${2:A}; HERE=${0:A:h}; T=$B/win-icon
rm -rf $T; mkdir -p $T/f/save
print '500 quit' > $T/f/script.txt
ECPE_HEADLESS=1 ECPE_SCRIPT=$T/f/script.txt $B/ecpe $B/game --windowed --width 240 --save $T/f/save >/dev/null 2>&1 || true
print '300 quit' > $T/f/script.txt
ECPE_HEADLESS=1 ECPE_FRAMELOG=$T/f ECPE_SCRIPT=$T/f/script.txt $B/ecpe $B/game --windowed --width 240 --save $T/f/save >/dev/null 2>&1
python3 $HERE/../android/mkicon.py $T/f/$(awk '$1==299{print $2}' $T/f/log.txt).raw $T/res >/dev/null
# mkicon.py가 남긴 둥근 마스크용 안쪽 그림(legacy.png)을 크기별 PNG로 줄여 ICO에 넣는다(Vista 이후 PNG 항목 허용).
for n in 16 24 32 48 64 128 256; do sips -z $n $n $T/res.tmp/legacy.png --out $T/$n.png >/dev/null; done
python3 - $OUT $T/{16,24,32,48,64,128,256}.png <<'PY'
import struct, sys
out, files = sys.argv[1], sys.argv[2:]
data = [open(f, 'rb').read() for f in files]
sizes = [int(f.rsplit('/', 1)[1].split('.')[0]) for f in files]
off = 6 + 16 * len(files); head = struct.pack('<HHH', 0, 1, len(files)); body = b''
for s, d in zip(sizes, data):
    head += struct.pack('<BBBBHHII', s % 256, s % 256, 0, 0, 1, 32, len(d), off + len(body)); body += d
open(out, 'wb').write(head + body)
PY
