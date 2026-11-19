#!/bin/zsh
# tomenu.sh <rundir> : restart instance and go to main menu
source ${0:A:h}/../../../../config.sh
R=$1; E=$ECPE_ROOT; T=$ECPE_ROOT/work/ui/run/m_title/tools
$E/work/tools/nctl.sh stop $R 2>/dev/null; rm -f $R/arg.log
$T/start.sh $R; sleep 7; $E/work/tools/nctl.sh $R ok wait1.5 >/dev/null
