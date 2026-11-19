#!/bin/zsh
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; D=$R/work/ui/run/$1
$R/work/tools/nctl.sh $D key right 1200 wait0.5 key up 800 wait0.5 key left 300 wait0.3 key up 1000 wait1 key down 1500 wait0.6 key left 400 wait0.6 key up 1500 wait2 >/dev/null
