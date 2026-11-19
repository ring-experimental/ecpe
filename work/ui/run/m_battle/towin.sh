#!/bin/zsh
# towin.sh <dir>: 전투에서 OK(공격-기본-대상)를 눌러 결과 화면(상태 0x17)까지 간다.
source ${0:A:h}/../../../config.sh
HERE=${0:A:h}; T=$ECPE_ROOT/work/tools/nctl.sh; D=${1:A}; PY=$ECPE_PY
for i in {1..120}; do
  $T $D memdump $D/st.bin >/dev/null; for k in 1 2 3 4 5; do [[ -s $D/st.bin ]] && break; perl -e "select(undef,undef,undef,0.5)"; done; s=$($PY $HERE/bstate.py $D/st.bin); rm -f $D/st.bin
  set -- ${=s}; [[ $2 == 23 ]] && { echo result; exit 0; }; [[ $1 != 13 ]] && { echo "scene $1"; exit 1; }
  $T $D key ok 30 wait0.6 >/dev/null
done
echo timeout
