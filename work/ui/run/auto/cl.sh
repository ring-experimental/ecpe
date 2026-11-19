#!/bin/zsh
source ${0:A:h}/../../../config.sh
HERE=${0:A:h}; D=$HERE/r9; . $HERE/env.sh; unset ECPE_ABDBG
UIINI='autobattle 1\n' $HERE/tob.sh $D --width 568 --scale 2 --classic | tail -1
PY=$ECPE_PY
for i in {1..30}; do $HERE/nctl.sh $D memdump $D/sc.bin >/dev/null; s=$($PY $ECPE_ROOT/work/ui/run/m_battle/scene.py $D/sc.bin); rm -f $D/sc.bin; [[ $s != 13 ]] && { echo "classic scene $s after ${i}"; exit; }; sleep 1; done; echo still
