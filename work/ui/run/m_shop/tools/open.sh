#!/bin/zsh
# open.sh <type> <num> [width] [scale] : 저장 사본으로 필드까지 간 뒤 문 앞 대사에 상점 명령(9d TT II 00, 9b 02)을 주입해 상점을 연다.
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; D=$R/work/ui/run/m_shop; T=$R/work/tools
export ECPE_HEADLESS=1 ECPE_BIN=${ECPE_BIN:-$R/native/build/ecpe} ECPE_UI_DIR=$R/native/ui
$T/tofield.sh $D --width ${3:-568} --scale ${4:-3} >/dev/null
print "poke32 0x42c254 410\npoke32 0x42c258 340" >> $D/ctrl.txt; sleep 0.5
$T/nctl.sh $D key up 600 wait2 >/dev/null
[[ -n $PRE ]] && print "$PRE" >> $D/ctrl.txt
w1=$(( $1 | ($2 << 8) ))
print "poke32 0x452354 0x0022009d\npoke32 0x452358 $w1\npoke32 0x45235c 0x0022009b\npoke32 0x452360 2\npoke32 0x452364 0x0021001c" >> $D/ctrl.txt; sleep 0.5
$T/nctl.sh $D ok wait2.5 >/dev/null
