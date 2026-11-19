#!/bin/zsh
# hp.sh <dir>: 전투 중인 <dir>의 적 HP와 아군 필드(+0x124)를 한 줄로 찍는다
source ${0:A:h}/../../../config.sh
D=${1:A}; N=$ECPE_ROOT/work/tools/nctl.sh; PY=$ECPE_PY
$N $D memdump $D/q.bin >/dev/null; for i in 1 2 3 4 5 6; do [[ -s $D/q.bin ]] && break; sleep 0.5; done
$PY $ECPE_ROOT/work/ui/run/m_battle/q.py $D/q.bin | grep -E " X| M"
rm -f $D/q.bin
