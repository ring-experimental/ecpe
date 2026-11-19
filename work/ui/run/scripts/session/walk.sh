#!/bin/zsh
# walk.sh <dir>: 필드에서 좌우로 걸어 전투를 찾는다
source ${0:A:h}/../../../../config.sh
T=$ECPE_ROOT/work/tools; D=${1:A}
for i in {1..20}; do
  $T/nctl.sh $D key right 1500 wait1.6 key left 1500 wait1.6
  $ECPE_PY -c "
from PIL import Image; import sys
im=Image.open('$D/now.png').convert('RGB'); w,h=im.size; p=im.getpixel((w//2,h-20))
sys.exit(0 if p[2]>70 and p[0]<70 and p[1]<70 else 1)" && { echo battle; exit 0; }
done
echo nobattle
