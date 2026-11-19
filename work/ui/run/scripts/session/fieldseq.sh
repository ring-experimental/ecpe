#!/bin/zsh
# fieldseq.sh <dir> <width>: 필드 → 옵션 각 화면을 차례로 찍는다(modern + classic)
source ${0:A:h}/../../../../config.sh
ROOT=$ECPE_ROOT; T=$ROOT/work/tools; D=${1:A}; W=$2
$T/tofield.sh $D --width $W --scale 3
c() { $T/nctl.sh $D "$@" >/dev/null; }
pair() { c wait0.3 wshot $1 classic wait0.3 wshot ${1}_classic modern wait0.2; }
c clear wait0.6 left wait0.4 ok wait0.5; pair list
c down; pair pv_cfg; c down; pair pv_help; c down; pair pv_quit
c ok; pair quit_q; c right; pair quit_no; c clear wait0.3 down ok; pair choice
c ok; pair save_slots; c down; c ok; pair save_q; c right ok
c clear wait0.3 ok down; pair choice_load; c ok; pair load_slots; c ok; pair load_q; c right ok wait0.2
c clear wait0.3 down ok; pair cfg; c right; pair cfg_changed; c down down down; pair cfg_speed; c clear
c down ok wait0.4; pair help1; c right right; pair help3
