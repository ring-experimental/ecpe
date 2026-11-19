#!/bin/zsh
# nctl.sh: 네이티브 빌드를 띄워 둔 채 한 단계씩 조작한다.
#   nctl.sh start <dir> [ecpe 인자...]   (저장 폴더는 <dir>/save, 없으면 새로)
#   nctl.sh <dir> <명령...>              (key <이름> [ms] | waitN | shot <이름> | framelog <폴더>|off | poison | tap | fing | source | raw) 차례로 보내고 마지막에 <dir>/now.png를 남긴다.
#   nctl.sh stop <dir>
source ${0:A:h}/../../../config.sh
ROOT=$ECPE_ROOT; BIN=${ECPE_BIN:-$ROOT/native/build/ecpe}
if [[ $1 == start ]]; then
  D=${2:A}; shift 2; mkdir -p $D/save; rm -f $D/ctrl.txt
  $BIN ${BIN:h}/game --ctrl $D/ctrl.txt --save $D/save "$@" >$D/out.log 2>&1 &
  echo $! > $D/pid; exit 0
fi
if [[ $1 == stop ]]; then D=${2:A}; print quit >> $D/ctrl.txt; perl -e 'select(undef,undef,undef,0.5)'; kill $(cat $D/pid) 2>/dev/null; exit 0; fi
D=${1:A}; shift
snap() { rm -f $D/$1.ppm; print "shot $D/$1.ppm" >> $D/ctrl.txt; for i in {1..50}; do [[ -s $D/$1.ppm ]] && break; perl -e 'select(undef,undef,undef,0.05)'; done; perl -e 'select(undef,undef,undef,0.1)'; sips -s format png $D/$1.ppm --out $D/$1.png >/dev/null 2>&1; }
while [[ $# -gt 0 ]]; do
  case $1 in
    wait*) perl -e "select(undef,undef,undef,${1#wait})";;
    shot) snap $2; shift;;
    memdump) print "memdump $2" >> $D/ctrl.txt; shift; perl -e 'select(undef,undef,undef,1.5)';;
    framelog) print "framelog $2" >> $D/ctrl.txt; shift;;
    poison) print poison >> $D/ctrl.txt;;
    poke8|poke16|poke32) print "$1 $2 $3" >> $D/ctrl.txt; shift 2;;
    wshot) rm -f $D/$2.png; print "wshot $D/$2.png" >> $D/ctrl.txt; shift; perl -e "select(undef,undef,undef,0.4)";;
    uireload) print uireload >> $D/ctrl.txt;;
    # 터치·패드 시험: tap <선택자>(문서 요소 누르기), fing <x> <y>(0~1 비율로 짧게 누르기), source touch|pad|keyboard,
    # raw '<ctrl 줄>'(finger/mouse/vpad/vbtn/vaxis/settings 등 그대로)
    tap) print "tap $2" >> $D/ctrl.txt; shift; perl -e 'select(undef,undef,undef,0.3)';;
    fing) print "finger down 0 $2 $3" >> $D/ctrl.txt; perl -e 'select(undef,undef,undef,0.08)'; print "finger up 0 $2 $3" >> $D/ctrl.txt; shift 2; perl -e 'select(undef,undef,undef,0.3)';;
    source) print "source $2" >> $D/ctrl.txt; shift; perl -e 'select(undef,undef,undef,0.1)';;
    raw) print -r -- "$2" >> $D/ctrl.txt; shift; perl -e 'select(undef,undef,undef,0.1)';;
    classic|modern) print "ui $1" >> $D/ctrl.txt;;
    calltree) rm -f $2; print "calltree $2" >> $D/ctrl.txt; shift; perl -e "select(undef,undef,undef,0.6)";;
    key) if [[ $3 == <-> ]]; then print "key $2 $3" >> $D/ctrl.txt; shift; else print "key $2" >> $D/ctrl.txt; fi; shift; perl -e 'select(undef,undef,undef,0.25)';;
    *) print "key $1" >> $D/ctrl.txt; perl -e 'select(undef,undef,undef,0.25)';;
  esac; shift
done
snap now
