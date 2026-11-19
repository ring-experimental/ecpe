#!/bin/zsh
# pair.sh <dir> <name>: 합친 화면(modern)과 원작 그리기(classic)를 <name>.png, <name>_c.png로 남긴다.
source ${0:A:h}/../../../config.sh
T=$ECPE_ROOT/work/tools/nctl.sh
$T $1 wshot $2 classic wait0.8 wshot ${2}_c modern wait0.6 >/dev/null
