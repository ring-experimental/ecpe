#!/bin/zsh
# 강화 시험용: 내 빌드로 사본 저장 필드에 선다(저장은 사본). 강화석 99개를 쥐어 준다. 화면 폭은 W(기본 568).
source ${0:A:h}/../../../config.sh
ROOT=$ECPE_ROOT
export ECPE_HEADLESS=1 ECPE_BIN=$ROOT/native/build_enhance/ecpe ECPE_UI_DIR=$ROOT/native/ui
D=$ROOT/work/ui/run/enhance; N=$ROOT/work/tools/nctl.sh
$N stop $D 2>/dev/null; sleep 1
$ROOT/work/tools/tofield.sh $D --width ${W:-568} --scale 2
$N $D poke8 42d7f0 99
