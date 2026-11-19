#!/bin/zsh
# cap.sh <name> "<words>" "<nctl cmds before capture>"
source ${0:A:h}/../../../../config.sh
S=$ECPE_ROOT/work/ui/run/scripts/session; R=$ECPE_ROOT; D=$R/work/ui/run/field1; O=$R/work/ui/run/fieldop
$S/go.sh field1
$S/run9.sh ${=2}
$R/work/tools/nctl.sh $D ${=3} wait0.3 shot cap calltree $O/ct/$1.txt wait1 memdump $O/m_$1.bin >/dev/null
cp $D/cap.png $O/$1.png; echo "$1: $(tail -1 $D/out.log | cut -c1-50)"
