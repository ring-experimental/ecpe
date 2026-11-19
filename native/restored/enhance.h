// 장비 강화 +1~+10(restored/enhance.c, 원작에 없다. 사용자 요청 2026-11-10). 화면에서 쓰는 도우미.
// 강화는 개체가 아니라 '캐릭터의 장비칸'에 붙는다: 키 ext "enh.<캐릭터 id>.<칸>" = 단계(0~10). 같은 아이템 레코드를 여럿이 나눠 쓰는
// 원작 구조라 아이템에는 저장할 곳이 없고, 장비를 바꿔도 칸의 단계가 남는다(모바일 RPG의 장비칸 강화와 같다).
#pragma once
#include <stdint.h>

#define ENH_MAX 10
#define ENH_SLOTS 5

int enh_step(uint32_t ch, int slot);                                  // 캐릭터 객체의 칸 단계(0~10)
// 칸 단계가 step일 때 장비 레코드 rec가 더하는 값. 무기면 (물공, 마공), 방어구면 (물방, 마방)에 들어간다.
void enh_bonus(uint32_t rec, int step, int *p, int *m);
int  enh_rec_is_def(uint32_t rec);                                    // 레코드 플래그 비트16: 원본 0x167a0이 방어 필드로 보내는지
// 이 캐릭터의 칸에 rec를 끼웠을 때(장착 중이든 후보든) 더해지는 값
void enh_slot_bonus(uint32_t ch, int slot, uint32_t rec, int *p, int *m);
// 캐릭터의 전투용 필드(+0x124 물공, +0x128 마공, +0x12c 물방, +0x130 마방)를 직업 기본값 + 장비 + 강화로 맞춘다.
void enh_sync(uint32_t ch);
void enh_sync_all(void);
// 원작 그리기(F2)가 장비 화면·스테이터스 화면에서 강화를 보이게: 그리기 동안만 그 캐릭터의 장착 레코드 능력치를 보정해 두었다가 되돌린다.
// cand_rec는 장비 화면의 후보 비교용(0이면 없음), cand_slot은 후보가 들어갈 칸.
void enh_patch_begin(uint32_t ch, uint32_t cand_rec, int cand_slot);
void enh_patch_end(void);
