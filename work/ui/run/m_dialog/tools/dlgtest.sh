#!/bin/zsh
# dlgtest.sh <inst> <width> <prefix>: 대사창 변형 4가지(얼굴 왼쪽 긴 글 2쪽, 얼굴 오른쪽, 얼굴 없음 0x8d, 주인공 0x89)를 띄워 찍는다
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; T=$R/work/ui/run/m_dialog/tools; D=$R/work/ui/run/m_dialog/$1; N=$R/work/tools/nctl.sh; p=$3
$T/go.sh $1 $2 3
$T/door.sh $1 0x00320085 0x02aa0101 0x00320085 0x01490401 0x0032008d 0x0267ff01 0x00320089 0x0180ff01 0x0021001c
$N $D ok wait0.25 wshot ${p}1 wait2 wshot ${p}2 ok wait2 wshot ${p}3 ok wait2 wshot ${p}4 ok wait2 wshot ${p}5 ok wait2 wshot ${p}6 >/dev/null
