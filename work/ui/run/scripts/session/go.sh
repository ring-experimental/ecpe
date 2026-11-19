#!/bin/zsh
# go.sh <dir>: restart with fresh save copy and arg-trace binary, reach field
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; cd $R; D=$R/work/ui/run/$1
$R/work/tools/nctl.sh stop $D 2>/dev/null; rm -rf $D; mkdir -p $D; cp -R "$ECPE_USER_SAVE" $D/save
ECPE_HEADLESS=1 ECPE_BIN=${XB:-$ECPE_ROOT/work/ui/run/scripts/session/bld/ecpe-arg} $R/work/tools/nctl.sh start $D --width ${W:-512}; sleep 3
$R/work/tools/nctl.sh $D ok wait1 ok wait2 ok wait2 key ok 30 wait1.2 key ok 30 wait2.5 key ok 30 wait2.5 key ok 30 wait2.5 >/dev/null
