#!/bin/zsh
# run_win.sh <dir>: 전투까지 간 뒤 프레임 기록을 켜고 fight.sh로 이긴다.
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; S=${0:A:h}; D=${1:A}
export ECPE_HEADLESS=1 ECPE_BIN=$R/native/build/ecpe-trace
$S/tob.sh $D $R/work/shots/b512/save --width 512 || exit 1
mkdir -p $D/fl; $R/work/tools/nctl.sh $D framelog $D/fl memdump $D/m_start.bin
zsh $R/work/tools/fight.sh $D
$R/work/tools/nctl.sh $D wait3 shot after1 key ok 30 wait2 shot after2 key ok 30 wait2 shot after3 key ok 30 wait2 shot after4 framelog off
echo done
