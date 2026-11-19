#!/bin/zsh
# tofield.sh <dir> [ecpe 인자...]: 사용자 저장 사본을 불러와 필드에 선다(창과 소리 없이 띄우려면 ECPE_HEADLESS=1, 다른 빌드는 ECPE_BIN).
source ${0:A:h}/../config.sh
ROOT=${0:A:h:h:h}; T=$ROOT/work/tools; D=${1:A}; shift
$T/nctl.sh stop $D 2>/dev/null; rm -rf $D; mkdir -p $D; cp -R "$ECPE_USER_SAVE" $D/save
$T/nctl.sh start $D "$@"; perl -e 'select(undef,undef,undef,3)'
$T/nctl.sh $D ok wait1 ok wait2 ok wait2 key ok 30 wait1.2
for i in 1 2 3; do $T/nctl.sh $D key ok 30 wait2.5; done
