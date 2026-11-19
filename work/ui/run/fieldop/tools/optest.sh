#!/bin/zsh
# optest.sh <name> <words...>: fresh instance, inject words after door dialog, shot+calltree
source ${0:A:h}/../../../../config.sh
S=$ECPE_ROOT/work/ui/run/scripts/session; R=$ECPE_ROOT; D=$R/work/ui/run/field1
n=$1; shift
$S/go.sh field1; mkdir -p $D/ct $D/op
$S/run9.sh "$@"
$R/work/tools/nctl.sh $D shot op_$n calltree $D/ct/op_$n.txt wait1 >/dev/null
cp $D/op_$n.png $D/ct/op_$n.txt $R/work/ui/run/fieldop/ 2>/dev/null
echo "$n: $(tail -1 $D/out.log | cut -c1-60)"
