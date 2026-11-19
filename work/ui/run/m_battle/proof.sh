#!/bin/zsh
# proof.sh <dir> <접두어>: 전투 중인 <dir>에서 화면마다 modern/classic 쌍을 남긴다(poke로 3인, 스킬, 필살, 적 정보, 단축, 아이템, 조합 상태를 만든 저장 사본 기준).
source ${0:A:h}/../../../config.sh
HERE=${0:A:h}; T=$ECPE_ROOT/work/tools/nctl.sh; D=${1:A}; X=$2; P=$HERE/pair.sh; PY=$ECPE_PY
k() { $T $D "$@" >/dev/null; }
$P $D ${X}cmd
k key 7 4000 wait1.2; $T $D wshot ${X}tags classic wait0.8 wshot ${X}tags_c modern wait2.5 >/dev/null
k ok wait0.5; $P $D ${X}sub
k right wait0.4 ok wait0.6; $P $D ${X}skill
k down wait0.4 down wait0.4; $P $D ${X}skill2
k clear wait0.5 down wait0.4 ok wait0.6; $P $D ${X}special
k clear wait0.5 clear wait0.5 down wait0.4 ok wait0.6; $P $D ${X}item
k right wait0.4; $P $D ${X}item_r
k down wait0.4; $P $D ${X}item_tab
k clear wait0.5
# 조합 목록: 원본 키 경로(Combi → 0x1d500 동료 고르기)는 같은 캐릭터 셋이라 동료를 못 골라 무반응이므로 그 결과 상태를 직접 만든다
$T $D memdump $D/cm.bin >/dev/null; for i in 1 2 3 4 5; do [[ -s $D/cm.bin ]] && break; sleep 0.5; done
CM=$($PY -c "import struct;b=open('$D/cm.bin','rb').read();u=lambda a:struct.unpack_from('<I',b,a-0x100000)[0];print(hex(u(0x42d3f8+0x5c)))"); rm -f $D/cm.bin
LW=$(( CM + 0x94 ))
k poke32 0x42d6a0 0x42c24c poke32 $(printf 0x%x $((LW+0x14))) 2 poke32 $(printf 0x%x $((LW+0x1c))) 75 poke32 $(printf 0x%x $((LW+0x18))) 76 poke32 $(printf 0x%x $((CM+0x24))) 1 poke32 0x42d590 2 wait0.5
$P $D ${X}combi
k poke32 $(printf 0x%x $((LW+0x1c))) -1 wait0.4
$P $D ${X}combi_none
k poke32 $(printf 0x%x $((CM+0x24))) 0 poke32 0x42d590 0 poke32 0x42d6a0 0 poke32 $(printf 0x%x $((LW+0x14))) 1 wait0.5
k ok wait0.5 ok wait0.6; $P $D ${X}tgt_enemy
k clear wait0.5 clear wait0.5 down wait0.4 ok wait0.6 ok wait0.6; $P $D ${X}tgt_ally
k clear wait0.5 clear wait0.5 up wait0.4
k poke32 0x42c320 2 wait0.3 key 3 wait0.3; $P $D ${X}msg_mp
k poke32 0x42c320 25 wait1.5
k ok wait0.4; $P $D ${X}msg_flee
