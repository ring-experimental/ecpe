#!/bin/zsh
# fmcore 화면을 차례로 띄워 wshot을 남긴다: m_fmcore_tour.sh <폭> <접두사>. 저장 사본은 m_fmcore_setup.sh가 만든다.
source ${0:A:h}/../../config.sh
source $ECPE_ROOT/work/ui/run/m_fmcore.env
W=$1; P=$2
$ECPE_ROOT/work/ui/run/m_fmcore_setup.sh $W
c() { $N $R "$@" >/dev/null; }
c clear wait0.6 wshot ${P}_main classic wait0.2 wshot ${P}_main_c modern
c right ok wait0.3 right wait0.2 wshot ${P}_main_pick classic wait0.2 wshot ${P}_main_pick_c modern
c key clear wait0.3 left wait0.2 ok wait0.4 wshot ${P}_item classic wait0.2 wshot ${P}_item_c modern
c ok wait0.3 down wait0.2 wshot ${P}_item_target classic wait0.2 wshot ${P}_item_target_c modern
c key clear wait0.3 star wait0.2 ok wait0.2 down ok wait0.2 wshot ${P}_item_hot classic wait0.2 wshot ${P}_item_hot_c modern
c ok wait0.1 wshot ${P}_item_hotdup classic wait0.05 wshot ${P}_item_hotdup_c modern
c wait0.8 key clear wait0.4 right wait0.2 down down ok wait0.3 wshot ${P}_item_info classic wait0.2 wshot ${P}_item_info_c modern
c key clear wait0.3 left wait0.2 poke32 0x48e15c 1 wait0.2 ok wait0.2 right wait0.2 wshot ${P}_sell_qty classic wait0.2 wshot ${P}_sell_qty_c modern
c ok wait0.2 wshot ${P}_sell_ask classic wait0.2 wshot ${P}_sell_ask_c modern
c key clear wait0.2 key clear wait0.2 poke32 0x48e15c 0 wait0.2 key clear wait0.4 right right ok wait0.2 ok wait0.4 wshot ${P}_equip classic wait0.2 wshot ${P}_equip_c modern
c ok wait0.3 down wait0.2 wshot ${P}_equip_cand classic wait0.2 wshot ${P}_equip_cand_c modern
c key 7 wait0.3 wshot ${P}_equip_info classic wait0.2 wshot ${P}_equip_info_c modern
c key clear wait0.3 poke8 0x42d72e 1 wait0.1 down down down down down down down down down down wait0.2 ok wait0.15 wshot ${P}_equip_lv classic wait0.05 wshot ${P}_equip_lv_c modern
c wait1.0 key clear wait0.3 down down wait0.3 wshot ${P}_equip_slot3
grep -v "Loaded font\|이미지" $R/out.log
