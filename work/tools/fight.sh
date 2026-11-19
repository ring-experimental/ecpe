#!/bin/zsh
# fight.sh <dir>: nctl로 띄운 게임이 전투 중이면 OK를 눌러 이길 때까지 싸운다(최대 약 60초). 끝나면 field, 아니면 battle을 찍는다.
source ${0:A:h}/../config.sh
T=${0:A:h}; D=${1:A}
inbattle() { $ECPE_PY -c "
from PIL import Image; import sys
im=Image.open('$D/now.png').convert('RGB'); w,h=im.size; p=im.getpixel((w//2,h-20))
sys.exit(0 if p[2]>70 and p[0]<70 and p[1]<70 else 1)"; }
$T/nctl.sh $D >/dev/null
for i in {1..25}; do
  inbattle || { echo field; exit 0; }
  $T/nctl.sh $D key ok 30 wait0.4 key ok 30 wait0.4 key ok 30 wait0.6 >/dev/null
done
for i in 1 2 3; do $T/nctl.sh $D key ok 30 wait1 >/dev/null; done
inbattle && echo battle || echo field
