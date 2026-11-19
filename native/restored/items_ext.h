// 확장 기능 공용 계약(원작에 없다, 사용자 요청 2026-11-10). 여러 작업이 병렬로 이 헤더에 맞춰 짠다.
// 구현은 restored/items_ext.c(기반 담당). 바꿀 때는 기존 이름·뜻을 깨지 말고 덧붙인다.
//
// 새 아이템은 원작 아이템 표의 빈 번호 246~255에 넣는다(사용자 결정). 보유 개수는 원작과 같은 자리
// (인벤토리 P = [game+0x1c0]+0xa8, P+0x50+id 바이트, 최대 99)라 원래 아이템 창·저장에 그대로 실린다.
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum {
    IT_EXT_FIRST    = 246,
    IT_STAT_RESET   = 246, // 망각의 물약: 한 캐릭터의 GP 분배를 되돌린다
    IT_REVIVE       = 247, // 부활석: 전멸 때 묻고 그 자리에서 전원 완전 회복·전투 계속(지니고만 있으면 됨)
    IT_RANDOM_BOX   = 248, // 랜덤 상자: 열면 무작위 아이템
    IT_WARP         = 249, // 귀환 주문서: 가 본 마을로 이동(필드)
    IT_INCENSE_LOW  = 250, // 정적의 향: 한동안 조우율을 낮춘다
    IT_INCENSE_HIGH = 251, // 유혹의 향: 한동안 조우율을 높인다
    IT_ENHANCE      = 252, // 강화석: 장비 강화 재료
    IT_LEVEL_UP     = 253, // 성장의 물약: 한 캐릭터의 레벨을 1 올린다(statreset.c의 창을 같이 쓴다)
    IT_EXT_LAST     = 255, // 254~255 예비
    IT_MAX_COUNT    = 99,
};

// ---- 인벤토리 ----
uint32_t ext_game(void);            // 게임 객체 G([[0x14ee34]+0x48]), 아직 없으면 0
uint32_t ext_inv(void);             // 인벤토리 P([G+0x1c0]+0xa8), 없으면 0
int  ext_count(int id);             // 보유 개수(0~99)
int  ext_give(int id, int n);       // 더한다(99에서 멈춤). 실제로 더한 개수
int  ext_take(int id, int n);       // 뺀다. 모자라면 아무것도 안 빼고 0, 뺐으면 1
const char *ext_item_name(int id);  // UTF-8 이름(새 아이템은 이 파일의 표, 원작은 아이템 표에서 CP949→UTF-8). 없으면 ""

// ---- 아이템 사용 ----
// 새 아이템(246~255)을 원래 아이템 창에서 '사용'하면 기반 쪽이 등록된 처리기를 부른다.
// where: EXT_FIELD(필드 아이템 창), EXT_SHOP(프리미엄 상점의 보관함). 반환 1이면 1개 소모, 0이면 소모 안 함.
// 처리기는 자기 파일에서 __attribute__((constructor)) 함수로 등록한다. 결과 문구는 ext_toast로 알린다.
enum { EXT_FIELD = 0, EXT_SHOP = 1 };
typedef int (*ext_use_fn)(int id, int where);
void ext_register_use(int id, ext_use_fn fn);
int  ext_use(int id, int where);    // 기반·상점이 부른다: 처리기가 1을 돌려주면 1개 뺀다. 처리기가 없으면 0

// ---- 슬롯별 확장 저장(원작 저장 형식 밖) ----
// 저장 슬롯을 따라가는 정수 키-값. 게임이 슬롯 i에 저장하면 저장 폴더의 ext<i>.ini에 쓰고, 불러오면 읽고,
// 새 게임이면 비운다. 키 이름은 기능별 접두어를 붙인다(예: "enh.<캐릭터id>.<칸>", "warp.<맵>", "incense.left").
int  ext_get(const char *key, int def);
void ext_set(const char *key, int v);

// ---- 화면 연결 ----

// ---- 알림 ----
// 화면 가운데 위에 잠깐 뜨는 한 줄(UTF-8). 새 UI가 꺼져 있어도 보인다.
void ext_toast(const char *utf8);

// ---- 기반이 더 내놓는 것(2026-11-10 덧붙임) ----
const char *ext_item_desc(int id);       // UTF-8 설명(새 아이템은 이 파일의 글, 원작은 설명 풀에서 CP949→UTF-8). 없으면 ""
const char *ext_item_desc_g(int id);     // 새 아이템 설명(게스트 CP949, 호스트 포인터). 원작 번호면 NULL. 설명 풀을 직접 읽는 UI가 쓴다
uint32_t ext_item_count(void);           // 아이템 표 개수(246 + 새 아이템 수)
int  ext_can_use(int id);                // 사용 처리기가 등록돼 있나
void ext_slot_save(int slot);            // 슬롯 slot(0..2)에 저장했을 때 ui_dialog.c의 f_110d4 래퍼가 부른다(ext<slot>.ini에 쓴다)
void ext_slot_load(int slot);            // 불러왔을 때 f_111b8 래퍼가 부른다(ext<slot>.ini를 읽는다, 없으면 비움)
void ext_toast_frame(void);              // host/main.c video_present가 매 프레임 부른다(토스트 문서를 채운다)

// ---- 랜덤 상자(restored/ext_box.c) ----
// 마지막으로 연 랜덤 상자의 결과. 상점 보관함이 ext_use(IT_RANDOM_BOX, EXT_SHOP) 직후 읽어 결과 카드를 보인다.
typedef struct { int id, n, rarity; } ext_reward_t;   // rarity 0 일반, 1 고급, 2 희귀, 3 전설
ext_reward_t ext_box_last(void);
const char *ext_rarity_name(int rarity);               // "일반" "고급" "희귀" "전설"

// ---- 프리미엄 상점(restored/ui_premium.c; 옵션 목록의 마지막 줄에서 ui_system.c가 연다) ----
bool ext_shop_active(void);          // 상점이 떠 있나(옵션 화면의 키·그리기를 대신 받는다)
void ext_shop_open(void);
void ext_shop_close(void);
void ext_shop_input(uint32_t input); // 옵션 화면 키 처리기의 키 이벤트 객체(r1). 이번 틱의 키와 터치(pick)를 처리한다
void ext_shop_doc(uint32_t menu);    // 옵션 화면 그리기 틱마다: premium 문서를 채운다(menu = 필드 메뉴 객체)

// ---- 부활석(restored/revive.c) ----
// 전멸 뒤 "부활석을 사용하시겠습니까?" 창이 떠 있나. 자동 전투는 이 동안 키를 보내지 않는다(사람이 고른다).
bool ext_revive_asking(void);
