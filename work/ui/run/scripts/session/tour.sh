#!/bin/zsh
# tour.sh <폭> : 모든 화면을 돌며 wshot을 남긴다
source ${0:A:h}/../../../../config.sh
source $ECPE_ROOT/work/ui/run/scripts/session/env.sh
W=$1; X=$2
$N stop $R 2>/dev/null
work/tools/tofield.sh $R --width $W --scale 3
party5; skills
for i in 1 2 3 4 5 6 7; do $N $R poke16 $(printf 0x%x $((0x42d638 + 8*i))) $((i+1)) wait0.15 >/dev/null; done
$N $R clear wait0.4 right right right ok ok wait0.4 wshot st_$W classic wait0.3 wshot st_${W}_orig modern down wait0.2 wshot stdex_$W right wait0.3 wshot st2_$W clear wait0.3 >/dev/null
$N $R right ok wait0.4 wshot qu_$W right wait0.2 wshot qu2_$W classic wait0.3 wshot qu2_${W}_orig modern clear wait0.3 >/dev/null
$N $R right ok wait0.4 wshot pt_$W ok right right ok wait0.3 wshot pt2_$W classic wait0.3 wshot pt2_${W}_orig modern clear clear clear wait0.3 >/dev/null
$N $R left left left left ok ok wait0.4 wshot s0_$W ok wait0.3 wshot s1_$W ok down down down down down down down down down down wait0.3 wshot s2_$W classic wait0.3 wshot s2_${W}_orig modern ok wait0.3 wshot s4_$W classic wait0.3 wshot s4_${W}_orig modern clear wait0.3 right right ok ok wait0.3 wshot s6_$W >/dev/null
grep "rml [0-3]" $R/out.log | sort | uniq -c
