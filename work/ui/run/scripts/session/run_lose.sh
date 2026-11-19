#!/bin/zsh
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; S=${0:A:h}; D=${1:A}
export ECPE_HEADLESS=1 ECPE_BIN=$R/native/build/ecpe-trace
$S/tob.sh $D $R/work/shots/b512/save --width 512 || exit 1
mkdir -p $D/fl; $R/work/tools/nctl.sh $D framelog $D/fl memdump $D/m_start.bin wait25 framelog off
echo done
