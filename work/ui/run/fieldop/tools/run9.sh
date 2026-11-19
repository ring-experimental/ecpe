#!/bin/zsh
# run9.sh <w1 w2 w3 w4 ...> : write words starting at the instruction after the door dialog (0x452354) and close the dialog
source ${0:A:h}/../../../../config.sh
S=$ECPE_ROOT/work/ui/run/scripts/session; R=$ECPE_ROOT; D=$R/work/ui/run/field1
a=$((0x452354)); L=()
for w in "$@"; do L+=("poke32 $(printf 0x%x $a) $w"); a=$((a+4)); done
$S/door.sh field1 "${L[@]}"
$R/work/tools/nctl.sh $D ok wait2.5 >/dev/null
