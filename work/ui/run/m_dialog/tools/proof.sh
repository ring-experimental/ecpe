#!/bin/zsh
# proof.sh <width>: 영역 dialog의 모든 화면을 띄워 현대(wshot)·원작(classic wshot) 쌍을 work/ui/modern/dialog/<width>/에 남긴다.
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; T=$R/work/ui/run/m_dialog/tools; I=p$1; D=$R/work/ui/run/m_dialog/$I; N=$R/work/tools/nctl.sh
O=$R/work/ui/modern/dialog/$1; mkdir -p $O
# wshot은 다음 표시 때 파일을 쓴다. 생길 때까지 기다린다(최대 3초).
ws() { $N $D wshot $1 >/dev/null; for i in {1..30}; do [[ -s $D/$1.png ]] && break; perl -e 'select(undef,undef,undef,0.1)'; done; cp $D/$1.png $O/ 2>/dev/null || { echo "없음 $1"; cp $D/now.png $O/FAIL_$1.png; }; }
pair() { ws $1; $N $D classic wait0.35 >/dev/null; ws $1_classic; $N $D modern wait0.3 >/dev/null; }
# 1. 대사창
$T/go.sh $I $1 3
$T/door.sh $I 0x00320085 0x02aa0101 0x00320085 0x01490401 0x0032008d 0x0267ff01 0x00320089 0x0180ff01 0x0021001c
$N $D ok wait0.25 >/dev/null; pair dlg1_typing
$N $D wait2 >/dev/null; pair dlg1_page1
$N $D ok wait2 >/dev/null; pair dlg1_page2
$N $D ok wait2 >/dev/null; pair dlg2
$N $D ok wait2 >/dev/null; pair dlg3_noface
$N $D ok wait2 >/dev/null; pair dlg4_hero
$N $D poison wait0.3 >/dev/null; ws dlg4_poison
# 2. 선택창
$T/go.sh $I $1 3
$T/door.sh $I 0x0022009d 0x00000a00 0x002100a2 0x002100a3 0x002100a4 0x002100d0 0x002200a5 0x0acc0005 0x002200a7 0x00000000 0x002200a7 0x00000003 0x002100c8 0x002200d6 0x01010009 0x0021001c
for n in inn save flower offer help casino battle slot lever; do
  $N $D ok wait1.2 >/dev/null; pair ch_$n
  [[ $n == slot ]] && { $N $D down down down wait0.3 >/dev/null; pair ch_slot_last; }
done
# 3. 알림, 지역 이름 띠(0x97 바로 뒤에 0x99를 이어 넣으면 원본 빌드에서도 멈춘다: 따로 넣는다)
$T/go.sh $I $1 3
$T/door.sh $I 0x00210097 0x0021001c 0x0021001c
$N $D ok wait1 >/dev/null; pair notice_item
$T/go.sh $I $1 3
$T/door.sh $I 0x00220099 0x0007000a 0x0021001c
$N $D ok wait1.3 >/dev/null; pair banner
# 4. 미니맵, 비행정 안내, 세기 띠, 말풍선, 레벨업
$T/go.sh $I $1 3
$N $D hash wait1 >/dev/null; pair minimap
$N $D poison wait0.3 >/dev/null; ws minimap_poison
$N $D clear wait1 >/dev/null
G=$((0x42bb6c)); h() { printf 0x%x $((G+$1)); }
$N $D poke32 $(h 0x20) $((0xf0ff|0x20000)) poke32 $(h 0x14) $((0x600|8)) wait0.5 >/dev/null; pair hud_airship
$N $D poke8 $(h 0x6c2) 7 poke8 $(h 0x6c3) 2 poke16 $(h 0x6be) 900 poke32 $(h 0x14) $((0x600|0x1800)) wait0.5 >/dev/null; pair counters
$N $D poke32 $(h 0x14) $((0x600)) poke32 $(h 0x20) $((0xf0ff)) wait0.4 >/dev/null
$T/door.sh $I 0x0033008b 0x0180ff00 0x00000050 0x0021001c
$N $D ok wait0.5 >/dev/null; pair balloon
$N $D wait2 clear wait1 >/dev/null
S=$((0x48e1dc)); $N $D poke32 $(printf 0x%x $((S+0x20))) 2 poke32 $(printf 0x%x $((S+0x1c))) $((0x148a0c)) poke32 0x42d934 4 wait1 >/dev/null; pair levelup
grep -a "rml [0-3]" $D/out.log | sort | uniq -c > $O/rml_warnings.txt
$N stop $D
