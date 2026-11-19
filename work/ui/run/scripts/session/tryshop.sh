#!/bin/zsh
source ${0:A:h}/../../../../config.sh
S=$ECPE_ROOT/work/ui/run/scripts/session; R=$ECPE_ROOT; D=$R/work/ui/run/field1; cd $R; work/tools/nctl.sh stop $D; export ECPE_ARGFN="$1"; $S/go.sh field1; $S/door.sh field1 "poke32 0x452354 0x0022009c" "poke32 0x452358 0" "poke32 0x45235c 0x0022009b" "poke32 0x452360 2" "poke32 0x452364 0x0021001c"; work/tools/nctl.sh $D ok wait3 calltree $D/ct_shop.txt wait1 >/dev/null; ls $D | grep ct; tail -1 $D/out.log
