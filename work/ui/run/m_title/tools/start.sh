#!/bin/zsh
# start.sh <rundir> [args]
source ${0:A:h}/../../../../config.sh
R=$1; shift; E=$ECPE_ROOT
ECPE_ARGLOG=$R/arg.log ECPE_ARGFN=f580,54f0,54dc,1415c,14184,37a24,56ec,4798,47e8,4838,4888,3a1c,3b48,3bf0,261d4,14240,5ae8,3298,4714,467c ECPE_HEADLESS=1 ECPE_BIN=$ECPE_ROOT/work/ui/run/m_title/tools/bld/ecpe-at $E/work/tools/nctl.sh start $R --width 512 "$@"
