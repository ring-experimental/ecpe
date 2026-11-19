#!/bin/zsh
# tobattle.sh <dir> [ecpe 인자...]: 사용자 저장 사본을 불러와 첫 필드에서 왔다 갔다 하며 전투가 날 때까지 걷는다. 전투면 <dir>/now.png.
source ${0:A:h}/../config.sh
ROOT=${0:A:h:h:h}; T=$ROOT/work/tools; D=${1:A}; shift
$T/nctl.sh stop $D 2>/dev/null; rm -rf $D; mkdir -p $D; cp -R "$ECPE_USER_SAVE" $D/save
$T/nctl.sh start $D "$@"; perl -e 'select(undef,undef,undef,3)'
$T/nctl.sh $D ok wait1 ok wait2 ok wait2 key ok 30 wait1.2
for i in 1 2 3; do $T/nctl.sh $D key ok 30 wait2.5; done
$T/nctl.sh $D key up 2500 wait2.6 key left 1500 wait1.6 key right 600 wait1
for i in {1..12}; do
  $T/nctl.sh $D key right 1500 wait1.6 key left 1500 wait1.6
  # 전투 아래 상태창: 맨 아래 줄 가운데가 짙은 남색(약 0x303060)
  $ECPE_PY -c "
from PIL import Image; import sys
im=Image.open('$D/now.png').convert('RGB'); w,h=im.size; p=im.getpixel((w//2,h-20))
sys.exit(0 if p[2]>70 and p[0]<70 and p[1]<70 else 1)" && { echo battle; exit 0; }
done
echo nobattle
