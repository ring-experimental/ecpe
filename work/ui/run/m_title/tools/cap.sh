#!/bin/zsh
# cap.sh <rundir> <name> : one-frame calltree + arg log + shot + scene index
source ${0:A:h}/../../../../config.sh
R=$1; N=$2; E=$ECPE_ROOT; P=$ECPE_PY
: > $R/arg.log.cut; L=$(wc -c < $R/arg.log)
touch $R/arg.log.on; $E/work/tools/nctl.sh $R calltree $R/ct_$N.txt >/dev/null; rm -f $R/arg.log.on; perl -e 'select(undef,undef,undef,0.2)'
tail -c +$((L+1)) $R/arg.log | awk "/^--frame/{n++; next} n==1" > $R/args_$N.txt
$E/work/tools/nctl.sh $R shot $N memdump $R/mem_$N.bin >/dev/null
$P -c "
import sys; sys.path.insert(0,'$ECPE_ROOT/work/ui/run/m_title/tools')
from m import *; m=M('$R/mem_$N.bin'); g=m.w(m.w(m.w(GOT+0x294))+0x48); print('game',hex(g),'scene',m.w(g+0xc),'sub',m.w(g+0x10),'lvl',m.w(g+0x164),'cur',m.w(g+0x15c))"
$P $E/work/tools/treepath.py $R/ct_$N.txt | grep -v GetStringWidth | grep -v "PutPixel\|FlushLcd\|^6604>6720>40c4" > $R/tp_$N.txt
