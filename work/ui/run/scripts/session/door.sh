#!/bin/zsh
# door.sh <dir>: teleport below the banquet door and trigger dialog; then poke extra lines from stdin-args
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; D=$R/work/ui/run/$1; shift
print "poke32 0x42c254 410\npoke32 0x42c258 340" >> $D/ctrl.txt; sleep 0.5
$R/work/tools/nctl.sh $D key up 600 wait2 >/dev/null
for l in "$@"; do print "$l" >> $D/ctrl.txt; done; sleep 0.5
