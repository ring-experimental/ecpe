#!/bin/zsh
# proof.sh: 568 폭에서 모든 상태를 돌며 modern/classic 짝을 남긴다
source ${0:A:h}/../../../../config.sh
source $ECPE_ROOT/work/ui/run/scripts/session/env.sh
W=568
$N stop $R 2>/dev/null
work/tools/tofield.sh $R --width $W --scale 3
pair() { $N $R wshot $1 classic wait0.35 wshot $1_classic modern wait0.2 >/dev/null; }
# 1인 파티(저장 그대로): 전투파티 첫 화면과 고른 뒤
$N $R clear wait0.4 left left ok wait0.4 >/dev/null; pair party_1member
$N $R ok wait0.3 >/dev/null; pair party_1member_done
$N $R clear clear wait0.3 clear wait0.4 >/dev/null
# 퀘스트 없음
$N $R poke16 0x42d638 0 wait0.2 clear wait0.4 right right right right ok wait0.4 >/dev/null; pair quest_empty
$N $R clear wait0.2 clear wait0.4 >/dev/null
party5; skills
for i in 0 1 2 3 4 5 6 7; do $N $R poke16 $(printf 0x%x $((0x42d638 + 8*i))) $((i+2)) wait0.15 >/dev/null; done
# 스테이터스
$N $R clear wait0.4 right right right ok ok wait0.4 >/dev/null; pair status_gp
$N $R down wait0.2 >/dev/null; pair status_cursor_dex
$N $R ok wait0.3 >/dev/null; pair status_after_alloc
$N $R right right wait0.3 >/dev/null; pair status_member3
$N $R clear wait0.3 >/dev/null
# 퀘스트 8개
$N $R right ok wait0.4 right right right wait0.3 >/dev/null; pair quest_list
$N $R clear wait0.3 >/dev/null
# 전투파티 5명
$N $R right ok wait0.4 right wait0.2 >/dev/null; pair party_cursor
$N $R ok right right ok wait0.3 >/dev/null; pair party_picking
$N $R right ok wait0.3 >/dev/null; pair party_done
$N $R clear clear clear clear wait0.3 >/dev/null
# 스킬
$N $R left left left left ok ok wait0.4 >/dev/null; pair skill_s0_category
$N $R down wait0.2 >/dev/null; pair skill_s0_combo_preview
$N $R ok wait0.3 down wait0.2 >/dev/null; pair skill_s2_combo_list
$N $R clear wait0.3 down ok wait0.3 down wait0.2 >/dev/null; pair skill_s2_special_list
$N $R clear wait0.3 down ok wait0.3 >/dev/null; pair skill_s1_action
$N $R ok wait0.3 ok wshot skill_msg10_cant >/dev/null
$N $R wait1 down down down down down down down down down down wait0.3 >/dev/null; pair skill_s2_scrolled
$N $R ok wait0.3 >/dev/null; pair skill_s4_target
$N $R ok wait0.3 ok wait0.3 ok wshot skill_s4_msg10_nomp >/dev/null
$N $R wait1 clear wait0.3 right ok wait0.3 >/dev/null; pair skill_s5_hotkey_wait
$N $R 6 wait0.3 >/dev/null; pair skill_s5_hotkey_set
$N $R poke32 0x48e25c 0 poke32 0x48e258 11 wshot skill_msg11_hotkey >/dev/null
$N $R wait0.5 clear wait0.3 right ok wait0.3 ok wait0.3 >/dev/null; pair skill_s6_info
$N $R ok wait0.3 clear wait0.3 poke32 0x42c2a4 0x8000 wait0.2 poke32 0x48e238 3 wait0.3 >/dev/null; pair skill_s3_browse_sealed
$N $R poke32 0x42c2a4 0 wait0.2 poke32 0x48e238 1 wait0.2 >/dev/null
# 레벨업 알림(영역 dialog의 상자를 원본 순서로 부른다): 게임 화면(shot)과 합친 화면
$N $R clear clear clear wait0.3 left left ok ok wait0.4 poke32 0x48e1fc 2 wait0.2 poke32 0x48e1f8 0x148a0c wait0.4 shot status_levelup_gamelayer wshot status_levelup classic wait0.35 wshot status_levelup_classic modern clear wait0.3 >/dev/null
$N $R poison wait0.3 shot poison_gamelayer >/dev/null
grep "rml [0-3]" $R/out.log | sort | uniq -c
