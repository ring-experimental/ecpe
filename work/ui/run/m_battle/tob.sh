#!/bin/zsh
# tob.sh <dir> [ecpe 인자...]: b512 저장 사본(턴 모드, ACTIVE=1이면 액티브)으로 필드에 선 뒤(POKES가 있으면 그 명령을 보내고) 오가며 전투까지 간다.
source ${0:A:h}/../../../config.sh
HERE=${0:A:h}; ROOT=$ECPE_ROOT; T=$ROOT/work/tools; D=${1:A}; shift; PY=$ECPE_PY
$T/nctl.sh stop $D 2>/dev/null; rm -rf $D; mkdir -p $D; cp -R $ROOT/work/shots/b512/save $D/save
[[ -n $ACTIVE ]] || printf '\x21' | dd of=$D/save/gopt.sav bs=1 count=1 conv=notrunc 2>/dev/null
$T/nctl.sh start $D "$@"; perl -e 'select(undef,undef,undef,3)'
$T/nctl.sh $D ok wait1 ok wait2 ok wait2 key ok 30 wait1.2 >/dev/null
scene() { $T/nctl.sh $D memdump $D/sc.bin >/dev/null; $PY $HERE/scene.py $D/sc.bin; rm -f $D/sc.bin; }
for i in {1..8}; do s=$(scene); [[ $s == 7 || $s == 13 ]] && break; $T/nctl.sh $D key ok 30 wait2 >/dev/null; done
echo "scene $s"
[[ -n $POKES ]] && $T/nctl.sh $D ${=POKES} wait0.3 >/dev/null
inb() { s=$(scene); [[ $s == 13 ]]; }
inb && { echo battle; exit 0; }
$T/nctl.sh $D key up 2500 wait2.6 key left 1500 wait1.6 key right 600 wait1 >/dev/null
for i in {1..30}; do
  $T/nctl.sh $D key right 1500 wait1.6 >/dev/null; inb && { $T/nctl.sh $D wait2 >/dev/null; echo battle; exit 0; }
  $T/nctl.sh $D key left 1500 wait1.6 >/dev/null; inb && { $T/nctl.sh $D wait2 >/dev/null; echo battle; exit 0; }
done
echo nobattle
