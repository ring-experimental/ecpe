#!/bin/zsh
# win.sh <dir> <UIINI> [ACTIVE=1]: 저장 사본으로 전투까지 간 뒤 자동 전투가 필드로 돌아올 때까지 시간을 잰다.
source ${0:A:h}/../../../config.sh
HERE=${0:A:h}; D=${1:A}; . $HERE/env.sh; unset ECPE_ABDBG
PY=$ECPE_PY
UIINI=$2 $HERE/tob.sh $D --width 568 --scale 2 | tail -1
t0=$(date +%s)
for i in {1..60}; do
  $HERE/nctl.sh $D memdump $D/sc.bin >/dev/null; s=$($PY $ECPE_ROOT/work/ui/run/m_battle/scene.py $D/sc.bin); rm -f $D/sc.bin
  [[ $s != 13 ]] && { echo "scene $s after $(( $(date +%s) - t0 ))s"; exit 0; }
  [[ $i == 5 ]] && $HERE/nctl.sh $D wshot mid >/dev/null
  sleep 1
done
echo "still battle"
