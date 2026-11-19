#!/bin/zsh
# proof.sh <width> <full 0|1>: 상점 모든 상태를 현대/원작 쌍으로 work/ui/modern/shop/<폭>/에 남긴다.
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; D=$R/work/ui/run/m_shop; T=$R/work/tools; S=${0:A:h}; W=$1; O=$R/work/ui/modern/shop/w$W; mkdir -p $O
n() { $T/nctl.sh $D "$@" >/dev/null; }
p() { n wait0.4 wshot m; cp $D/m.png $O/$1.png; n classic wait0.4 wshot c modern wait0.3; cp $D/c.png $O/$1_classic.png; }
party5="poke32 0x42d684 0x42c3f0 poke32 0x42d688 0x42c594 poke32 0x42d68c 0x42c738 poke32 0x42d690 0x42c8dc"
# 장비 상점(첫 마을 비트 0x80), 파티 5명
$S/open.sh 2 128 $W 3; n ${=party5} wait0.3
p 01_top; n ok; p 02_category; n ok wait0.3; p 03_weapons
n down down down down down down down; p 04_weapons_scrolled
[[ $2 == 1 ]] && { n clear down ok wait0.3 down down down down; p 05_armor; n clear down ok wait0.3; p 06_accessory; n clear up up ok wait0.3; }
n ok; p 07_qty; n right right; p 08_qty3; n ok; p 09_confirm; n right; p 10_confirm_no; n left ok wait0.4; p 11_bought
n poke32 0x42d6a4 50 up up; p 12_poor; n ok; p 13_nomoney; n ok
[[ $2 == 1 ]] || exit 0
# 소비물 상점, 수량 MAX
$S/open.sh 1 128 $W 3; n poke8 0x42d6f4 99; n ok; p 14_consumables; n ok ok; p 15_qtymax
# 코인 상점(종류 3, 변수11 = 코인 품목 비트 1)
PRE="poke32 0x42bc90 1" $S/open.sh 3 0 $W 3; n poke32 0x42d6a8 3500; p 16_coinshop; n ok; p 17_coin_short; n ok down ok right right; p 18_coinshop_qty
# 코인 구매(종류 4)
$S/open.sh 4 4 $W 3; p 19_coinbuy; n up right; p 20_coinbuy_120; n ok; p 21_coinbuy_confirm; n ok; p 22_coinbuy_done
n poke32 0x42d6a8 99999990 ok ok; p 23_coin_max
$T/nctl.sh stop $D
