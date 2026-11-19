#!/bin/zsh
# title.sh <dir> <width>: 사용자 저장 사본으로 타이틀 → 이어하기 슬롯 화면까지
source ${0:A:h}/../../../../config.sh
ROOT=$ECPE_ROOT; T=$ROOT/work/tools; D=${1:A}; W=$2
$T/nctl.sh stop $D 2>/dev/null; rm -rf $D; mkdir -p $D; cp -R "$ECPE_USER_SAVE" $D/save
$T/nctl.sh start $D --width $W --scale 3; sleep 4
$T/nctl.sh $D ok wait1.5 wshot a_menu0 ok wait1.5 wshot a_menu1 ok wait2 ok wait2 >/dev/null
