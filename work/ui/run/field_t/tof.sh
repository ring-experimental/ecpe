#!/bin/zsh
# tof.sh <dir> <저장폴더> [ecpe 인자...]: 저장 사본으로 필드까지(귀환/향 시험용, 자기 build_field 사용)
source ${0:A:h}/../../../config.sh
ROOT=$ECPE_ROOT; T=$ROOT/work/tools; D=${1:A}; S=$2; shift 2
export ECPE_HEADLESS=1 ECPE_BIN=$ROOT/native/build_field/ecpe ECPE_UI_DIR=$ROOT/native/ui
$T/nctl.sh stop $D 2>/dev/null; rm -rf $D; mkdir -p $D; cp -R $S $D/save
$T/nctl.sh start $D "$@"; perl -e 'select(undef,undef,undef,3)'
$T/nctl.sh $D ok wait1 ok wait2 ok wait2 key ok 30 wait1.2
for i in 1 2 3; do $T/nctl.sh $D key ok 30 wait2.5; done
