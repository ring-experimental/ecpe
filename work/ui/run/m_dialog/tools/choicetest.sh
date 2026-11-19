#!/bin/zsh
# choicetest.sh <inst> <width> <prefix>: 선택창 9가지(여관, 저장, 꽃, 헌화, 도움말, 카지노, 전투 확인, 슬롯 메뉴, 레버)를 차례로 띄워 찍는다
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; T=$R/work/ui/run/m_dialog/tools; D=$R/work/ui/run/m_dialog/$1; N=$R/work/tools/nctl.sh; p=$3
$T/go.sh $1 $2 3
$T/door.sh $1 0x0022009d 0x00000a00 0x002100a2 0x002100a3 0x002100a4 0x002100d0 0x002200a5 0x0acc0005 0x002200a7 0x00000000 0x002200a7 0x00000003 0x002100c8 0x002200d6 0x01010009 0x0021001c
$N $D ok wait1.2 wshot ${p}1 ok wait1.2 wshot ${p}2 ok wait1.2 wshot ${p}3 ok wait1.2 wshot ${p}4 ok wait1.2 wshot ${p}5 ok wait1.2 wshot ${p}6 down wait0.3 wshot ${p}6b ok wait1.2 wshot ${p}7 ok wait1.2 wshot ${p}8 down down down wait0.3 wshot ${p}8b ok wait1.2 wshot ${p}9 ok wait1.2 wshot ${p}10 >/dev/null
