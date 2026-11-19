#!/bin/zsh
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; S=${0:A:h}; D=$R/work/ui/run/battle2; K=$R/work/ui/run/battle_keep/lose
export ECPE_HEADLESS=1 ECPE_BIN=$R/native/build/ecpe-trace
$S/walk.sh $D || exit 1
rm -rf $K; mkdir -p $K; $R/work/tools/nctl.sh $D framelog $K memdump $R/work/ui/run/battle_keep/m_active.bin wait200 framelog off
echo done
