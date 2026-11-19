#!/bin/zsh
# go.sh [dir]: (re)start an instance with the arg-logging binary in <dir> (default: fieldmenu) and walk to the field.
source ${0:A:h}/../../../config.sh
R=$ECPE_ROOT; D=${1:-$R/work/ui/run/fieldmenu}; D=${D:A}; T=$R/work/tools
mkdir -p $D; $T/nctl.sh stop $D 2>/dev/null; sleep 0.5
rm -rf $D/save; cp -R "$ECPE_USER_SAVE" $D/save
export ECPE_HEADLESS=1 ECPE_BIN=$R/work/ui/run/fieldmenu/bld/ecpe-args
export ECPE_ARGFN=${ECPE_ARGFN:-60dc,6104,5f84,5fdc,56ec,1415c,14184,3c0c,3b48,3a1c,2ff80,2fffc,4760,48d8,37a78,310e8,31b18,33d94,32778,3312c,338b8,34cb8,35440,360b0}
$T/nctl.sh start $D --width 512; sleep 3
$T/nctl.sh $D ok wait1 ok wait2 ok wait2 key ok 30 wait1.2
for i in 1 2 3; do $T/nctl.sh $D key ok 30 wait2.5; done
