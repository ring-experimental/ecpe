#!/bin/zsh
# walk.sh <dir>: 필드에서 좌우로 걸어 전투까지 간다.
source ${0:A:h}/../../../config.sh
HERE=${0:A:h}; T=$ECPE_ROOT/work/tools/nctl.sh; D=${1:A}; PY=$ECPE_PY
inb() { $T $D memdump $D/st.bin >/dev/null; for k in 1 2 3 4 5; do [[ -s $D/st.bin ]] && break; perl -e "select(undef,undef,undef,0.5)"; done; s=$($PY $HERE/bstate.py $D/st.bin); rm -f $D/st.bin; [[ ${s%% *} == 13 ]]; }
for i in {1..30}; do
  $T $D key right 1500 wait1.6 >/dev/null; inb && { $T $D wait2 >/dev/null; echo battle; exit 0; }
  $T $D key left 1500 wait1.6 >/dev/null; inb && { $T $D wait2 >/dev/null; echo battle; exit 0; }
done
echo nobattle
