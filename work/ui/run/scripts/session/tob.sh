#!/bin/zsh
# tob.sh <dir> <savesrc> [args]: tobattle.sh 사본(저장 원본을 바꿀 수 있게)
source ${0:A:h}/../../../../config.sh
ROOT=$ECPE_ROOT; T=$ROOT/work/tools; D=${1:A}; S=${2:A}; shift 2
$T/nctl.sh stop $D 2>/dev/null; rm -rf $D; mkdir -p $D; cp -R $S $D/save
$T/nctl.sh start $D "$@"; perl -e 'select(undef,undef,undef,3)'
$T/nctl.sh $D ok wait1 ok wait2 ok wait2 key ok 30 wait1.2
for i in 1 2 3; do $T/nctl.sh $D key ok 30 wait2.5; done
$T/nctl.sh $D shot loaded key up 2500 wait2.6 key left 1500 wait1.6 key right 600 wait1
for i in {1..12}; do
  $T/nctl.sh $D key right 1500 wait1.6 key left 1500 wait1.6
  $ECPE_PY -c "
from PIL import Image; import sys
im=Image.open('$D/now.png').convert('RGB'); w,h=im.size; p=im.getpixel((w//2,h-20))
sys.exit(0 if p[2]>70 and p[0]<70 and p[1]<70 else 1)" && { echo battle; exit 0; }
done
echo nobattle
