#!/bin/zsh
# banner.sh <width>: 지역 이름 띠만 다시 찍는다(proof.sh 3절 뒷부분)
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; T=$R/work/ui/run/m_dialog/tools; I=b$1; D=$R/work/ui/run/m_dialog/$I; N=$R/work/tools/nctl.sh
O=$R/work/ui/modern/dialog/$1; mkdir -p $O
ws() { $N $D wshot $1 >/dev/null; for i in {1..30}; do [[ -s $D/$1.png ]] && break; perl -e 'select(undef,undef,undef,0.1)'; done; cp $D/$1.png $O/ 2>/dev/null || echo "없음 $1"; }
$T/go.sh $I $1 3
$T/door.sh $I 0x00220099 0x0007000a 0x0021001c
$N $D ok wait1.3 >/dev/null; ws banner; $N $D classic wait0.35 >/dev/null; ws banner_classic; $N $D modern >/dev/null
$N stop $D
