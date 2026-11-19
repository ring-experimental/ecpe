// 필드 메뉴 틀(셸). 사용자 결정(2026-11-11): "포커스 단계"(보이는데 OK로 한 번 들어가야 조작되는 창)와 자리가 바뀌는 버튼 띠를 없앤다.
//
// 원인은 원본 상태 기계(메인의 대상 선택, 장비의 칸→후보, 스킬의 분류→동작→목록)를 화면에 그대로 드러낸 것이었다.
// 그래서 원본 상태 기계는 그대로 두되 래퍼가 대신 몰고(원본 키를 같은 틱 안에서 동기로 넣는다, fm_key_raw), 사용자에게는 보이는 구조만 남긴다.
//
//   - 레일(왼쪽): 메뉴 7개 + 소지금·플레이 시간·장소. 어느 화면·상태에서든 누르면 바로 그 화면으로 간다(fm_goto: CLR로 홈까지 → 탭 → OK →
//     캐릭터 화면이면 기억한 캐릭터로 맞추고 OK). 그리기 전에 한 틱 안에서 끝나 중간 화면이 보이지 않는다.
//   - 캐릭터 띠(스킬·장비·스테이터스 위): 칩을 누르면 그 캐릭터로. 원본이 캐릭터 넘김을 받는 상태로 먼저 되돌린다(fm_set_char).
//   - 「이전」(CLR): 보이는 구조에서 한 단계 위. 팝업·대상 고르기·단축키 대기·메시지면 원본에 맡기고(한 단계), 아니면 홈으로(ext_shell_key).
//   - 메인의 '대상 선택' 단계는 보이지 않는다: 키보드로 들어가도 같은 틱에 기억한 캐릭터로 마저 넘긴다(ext_shell_after).
//   - 고정 버튼 띠: 화면 래퍼가 fm_bar_*로 채우고, 누르면 여기서 받는다(pick "fmbar").
//
// 문서는 ui/fm_shell.rml(틀, 화면 문서보다 아래에 깔린다)과 각 화면 문서의 <fmbar/>(host/ui_vlist.cpp가 펼친다), ui/fmshell.rcss.
// 원본 키 이벤트는 0x2fa60이 읽는 꼴(+0 u16 코드, +2 u16 1 = 누름, +0xc 0 아님 = 유효, +8 누르고 있는 키 0)으로 게스트 메모리에 만든다.
// 효과음: 합성 키마다 원본이 효과음을 요청하지만, 요청(0x11aec)은 소리 관리자의 '지금 소리' 칸을 덮어쓰고 두 틱 뒤에 한 번만 재생하므로
// 한 틱 안에 여러 번 넣어도 마지막 하나만 난다(restored/sound.c 머리). 따로 막지 않는다.
// 주소는 파일 오프셋(게스트 주소는 +0x100000).
#include "restored.h"
#include "fm_shell.h"
#include <stdio.h>
#include <string.h>

#define G32(a) R32_(&cpu, (a))
#define G16(a) R16_(&cpu, (a))
#define G8(a) R8_(&cpu, (a))
#define FN(x) (0x100000u + (x))

#define GAME_PLACE VG(GAME_PLACE_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
enum {
    GOT_FACE_DX = 0x458, GOT_FACE_DY = 0x45c, GOT_TAB_ICON_X = 0x47c,
    APP_GAME = 0x48,
    GAME_GFX = 0x08, GAME_SCENE = 0x0c, GAME_FLAGS = 0x14, GAME_PLAY_T0 = 0x24, GAME_PLAY_ACC = 0x28, GAME_PARTY = 0x1c0,
    GAME_MENU = 0x1d4, GAME_CLASS_NAMES = 0x314, GAME_PLACE_D5 = 0x4dc, FLAG_PLACE_TABLE = 0x08, SCENE_MENU = 14,
    MENU_GAME = 0x00, MENU_SPR = 0x04, MENU_SHEET = 0x14, MENU_SCREEN = 0x18, MENU_MODE = 0x1c, MENU_SCREENS = 0x24, MENU_CLOSE = 0x60,
    MODE_FIELD = 1,
    PARTY_CHARS = 0x84, PARTY_INV = 0xa8, PARTY_MAX = 5,
    CH_ID = 0x04, CH_SEAL = 0x58, CH_STATS = 0x64, CH_CLASS = 0xb4, CH_NAME = 0x19c, CLS_NAME = 0x1e, ST_LV = 0x18, SEALED = 0x8000,
    SCR_SLOT = 0x0c, SCR_STATE = 0x10,
    // 화면 번호
    S_MAIN = 0, S_ITEM = 1, S_SKILL = 2, S_EQUIP = 3, S_STATUS = 4, S_QUEST = 5, S_PARTY = 6, S_OPTION = 7, TABS = 7,
    // 메인 +0xc 탭, +0x20 대상 고르는 중, +0x2c 고른 파티 칸
    MAIN_TAB = 0x0c, MAIN_PICK = 0x20, MAIN_SEL = 0x2c,
    // 아이템 +0x10 상태(1 사용 대상, 2 설명, 3·4 판매, 5 핫슬롯), +0x30 판매 모드, +0x34 메시지, +0x38 탭, +0x3c 쪽
    IT_SELL = 0x30, IT_MSG = 0x34, IT_TAB = 0x38, IT_PAGE = 0x3c,
    // 스킬 +0x10 상태(0 분류, 1 동작, 2 목록, 4 대상, 5 단축키 대기, 6 정보), +0x1c 동작(0 사용, 1 장착, 2 정보), +0x2c 분류, +0x30 메시지
    SK_ACT = 0x1c, SK_TOP = 0x20, SK_SEL = 0x24, SK_ROW = 0x28, SK_CAT = 0x2c, SK_MSG = 0x30,
    // 장비 +0x10 상태(0 칸, 1 후보, 2 상세), +0x34 메시지
    EQ_MSG = 0x34,
    // 스테이터스 +0x1c 레벨업 알림 글
    ST_NOTE = 0x1c,
    // 전투파티 +0x2c 다 골랐음, +0x34.. 고른 셋
    PT_DONE = 0x2c, PT_PICKS = 0x34,
    // 원본 키 이벤트(0x61d4가 읽는 꼴)
    EV_SIZE = 0x20,
    NARROW_W = 400,
    BAR_OK = 8, BAR_BACK = 9,
};

static const char *const tab_names[TABS] = {"아이템", "스킬", "장비", "스테이터스", "퀘스트", "전투파티", "옵션"};

// ---- 상태 ----
static int depth;       // fm_key_raw 안(틀 훅을 건너뛴다: 자기 훅을 다시 타지 않게)
static int shell_now;   // 이번 틱에 틀을 그렸다
static int chars_now;   // 이번 틱의 화면이 캐릭터 화면(캐릭터 띠가 있다)
static int last_slot = -1;

// 틀 안에 뜨는 자체 창(장비 강화 enhance.c, 귀환 주문서 warp.c, 망각의 물약 statreset.c, 프리미엄 상점 ui_premium.c).
// 사용자 결정(2026-11-11): 창이 떠 있어도 레일은 잠그지 않는다. 레일을 누르면 창을 그 창의 닫기 경로로 닫고 바로 그 화면으로 간다.
// 강화는 캐릭터를 바꾸는 창이라 틀의 캐릭터 띠가 강화의 파티원을 보이고 바꾼다.
__attribute__((weak)) int ext_enhance_open(void);
__attribute__((weak)) int ext_enhance_member(void);
__attribute__((weak)) void ext_enhance_set_member(int slot);
__attribute__((weak)) void ext_enhance_close(void);
__attribute__((weak)) int ext_warp_open(void);
__attribute__((weak)) void ext_warp_close(void);
__attribute__((weak)) int ext_statreset_open(void);
__attribute__((weak)) void ext_statreset_close(void);
__attribute__((weak)) bool ext_shop_active(void);
__attribute__((weak)) void ext_shop_close(void);

static int enh_open(void) { return ext_enhance_open && ext_enhance_open(); }
// 떠 있는 자체 창을 모두 닫는다(각 창의 닫기와 같은 길: 창 상태만 내리고 소모·적용은 하지 않는다. 결과 카드·결과 띠도 함께 넘어간다).
static void close_overlays(void) {
    if (enh_open() && ext_enhance_close) ext_enhance_close();
    if (ext_warp_open && ext_warp_open() && ext_warp_close) ext_warp_close();
    if (ext_statreset_open && ext_statreset_open() && ext_statreset_close) ext_statreset_close();
    if (ext_shop_active && ext_shop_active() && ext_shop_close) ext_shop_close();
}

static uint32_t scr_of(uint32_t menu, int n) { return G32(menu + MENU_SCREENS + 4u * (uint32_t)n); }
static int screen_of(uint32_t menu) { return (int)G32(menu + MENU_SCREEN); }
static uint32_t party_char(uint32_t game, int i) { return (i < 0 || i >= PARTY_MAX) ? 0 : G32(G32(game + GAME_PARTY) + PARTY_CHARS + 4u * (uint32_t)i); }
static int first_member(uint32_t game) { for (int i = 0; i < PARTY_MAX; i++) if (party_char(game, i)) return i; return 0; }
// 틀을 두는 메뉴: 모드 1(필드)의 화면 0~7. 상점(화면 8)과 타이틀의 옵션(모드 2·3)은 틀 없이 그대로다.
static int shell_menu(uint32_t menu) {
    return menu && !G32(menu + MENU_CLOSE) && G32(menu + MENU_MODE) == MODE_FIELD && screen_of(menu) <= S_OPTION;
}
static uint32_t menu_of_game(uint32_t game) {
    if (!game || G32(game + GAME_SCENE) != SCENE_MENU) return 0;
    return G32(game + GAME_MENU);
}

int fm_shell_on(void) { return shell_now; }
int fm_last_char(uint32_t game) { return last_slot >= 0 && party_char(game, last_slot) ? last_slot : first_member(game); }

// ---- 동기 키 ----
void fm_key_raw(uint32_t menu, int gcode) {
    static uint32_t ev;
    if (!ev) ev = heap_alloc(EV_SIZE);
    for (uint32_t i = 0; i < EV_SIZE; i += 4) W32_(&cpu, ev + i, 0);
    W16_(&cpu, ev, (uint32_t)gcode);
    W16_(&cpu, ev + 2, 1);
    W32_(&cpu, ev + 0xc, 1);
    depth++;
    CALL(FN(0x2fa60), menu, ev);
    depth--;
}

static int at_home(uint32_t menu) { return screen_of(menu) == S_MAIN && !G32(scr_of(menu, S_MAIN) + MAIN_PICK); }

// 홈(화면 0, 대상 고르기 아님)까지 CLR. 홈에서는 CLR이 메뉴를 닫으므로(+0x60) 닿으면 멈춘다. 저장 중 같은 대화가 키를 안 받아 못 닿으면 0.
static int go_home(uint32_t menu) {
    for (int i = 0; i < 8; i++) {
        if (G32(menu + MENU_CLOSE)) return 0;
        if (at_home(menu)) return 1;
        fm_key_raw(menu, FMK_CLR);
    }
    return at_home(menu);
}

// 메인이 대상 캐릭터를 고르는 중(+0x20)이면 칸을 slot으로 맞춰 OK. 원본 좌우 키(0x30dc0)로 옮겨 맞춘다.
static void pick_finish(uint32_t menu, int slot) {
    uint32_t m = scr_of(menu, S_MAIN);
    for (int i = 0; i < 8 && screen_of(menu) == S_MAIN && G32(m + MAIN_PICK); i++) {
        int sel = (int)G32(m + MAIN_SEL);
        if (sel == slot) break;
        fm_key_raw(menu, sel < slot ? FMK_RIGHT : FMK_LEFT);
        if ((int)G32(m + MAIN_SEL) == sel) break; // 더 못 간다(빈 칸)
    }
    if (screen_of(menu) == S_MAIN && G32(m + MAIN_PICK)) fm_key_raw(menu, FMK_OK);
}

// 레일: 탭(0~6)의 화면으로 바로 간다.
static void fm_goto(uint32_t menu, int tab) {
    if (tab < 0 || tab >= TABS || screen_of(menu) == tab + 1) return;
    uint32_t game = G32(menu + MENU_GAME);
    if (!go_home(menu)) return;
    W32_(&cpu, scr_of(menu, S_MAIN) + MAIN_TAB, (uint32_t)tab);
    fm_key_raw(menu, FMK_OK);
    pick_finish(menu, fm_last_char(game));
}

// ---- 스킬 화면 몰기 ----
static int skill_want_act(uint32_t menu) {
    uint32_t scr = scr_of(menu, S_SKILL), game = G32(menu + MENU_GAME);
    uint32_t ch = party_char(game, (int)G32(scr + SCR_SLOT));
    // 봉인이면 원본이 '사용'을 받지 않는다(동작 고르기에서 OK 무시). 그때는 '정보'로 목록에 둔다(OK는 보이지 않게 되돌린다).
    return ch && (G32(ch + CH_SEAL) & SEALED) ? 2 : 0;
}
// 목록 커서 기억: 동작 고르기(상태 1)에서 OK하면 원본이 목록을 새로 만들어 커서가 첫 줄로 간다. 단축키 대기·대상 고르기에서
// 「이전」으로 돌아올 때 원본이 동작 고르기를 거치면 고른 스킬을 잃으므로, 목록에 있던 커서(같은 캐릭터·분류)를 되돌린다.
static struct { int slot, cat; uint32_t top, sel, row; } sk_mem = {-1, -1, 0, 0, 0};
void fm_skill_normalize(uint32_t menu) {
    if (!shell_menu(menu)) return;
    uint32_t scr = scr_of(menu, S_SKILL);
    for (int i = 0; i < 6 && screen_of(menu) == S_SKILL; i++) {
        int st = (int)G32(scr + SCR_STATE), want = skill_want_act(menu);
        if (G32(scr + SK_MSG)) return; // 메시지(사용불가, 단축키 안내)가 떠 있는 동안은 건드리지 않는다
        if (st == 0) { fm_key_raw(menu, FMK_OK); continue; }                 // 분류 → (일반) 동작 / (조합·필살) 목록
        if (st == 1) {                                                          // 동작 → 목록
            W32_(&cpu, scr + SK_ACT, (uint32_t)want);
            fm_key_raw(menu, FMK_OK);
            if ((int)G32(scr + SCR_STATE) == 1) return;                        // 받지 않았다
            if (sk_mem.slot == (int)G32(scr + SCR_SLOT) && sk_mem.cat == (int)G32(scr + SK_CAT) && (int)G32(scr + SK_TOP) >= 0) {
                W32_(&cpu, scr + SK_TOP, sk_mem.top); W32_(&cpu, scr + SK_SEL, sk_mem.sel); W32_(&cpu, scr + SK_ROW, sk_mem.row);
            }
            continue;
        }
        if (st == 2 || st == 4 || st == 5) {
            sk_mem.slot = (int)G32(scr + SCR_SLOT); sk_mem.cat = (int)G32(scr + SK_CAT);
            sk_mem.top = G32(scr + SK_TOP); sk_mem.sel = G32(scr + SK_SEL); sk_mem.row = G32(scr + SK_ROW);
        }
        if (st == 6) { fm_key_raw(menu, FMK_CLR); continue; }                  // 정보 → 목록(정보는 늘 오른쪽 창에 보인다)
        if (st == 2 && (int)G32(scr + SK_ACT) != want) W32_(&cpu, scr + SK_ACT, (uint32_t)want); // 동작 칸은 OK의 뜻만 정한다
        return;
    }
}
// 분류 바꾸기: 분류 고르기(상태 0)까지 CLR(0에서 CLR은 홈이라 그 전에 멈춘다) → 분류 → 다시 목록으로.
static void skill_to_state0(uint32_t menu) {
    uint32_t scr = scr_of(menu, S_SKILL);
    for (int i = 0; i < 5 && screen_of(menu) == S_SKILL && G32(scr + SCR_STATE) != 0; i++) fm_key_raw(menu, FMK_CLR);
}
void fm_skill_set_cat(uint32_t menu, int cat) {
    uint32_t scr = scr_of(menu, S_SKILL);
    sk_mem.slot = -1;
    skill_to_state0(menu);
    if (screen_of(menu) != S_SKILL || G32(scr + SCR_STATE) != 0) return;
    W32_(&cpu, scr + SK_CAT, (uint32_t)cat);
    fm_skill_normalize(menu);
}
// 「단축키」 버튼: 목록 → 동작 '장착' → 3/6/9 대기(상태 5)까지 한 번에. 고른 스킬(+0x24)은 CLR로 동작 고르기에 갔다 와도 남는다.
void fm_skill_hotkey(uint32_t menu) {
    uint32_t scr = scr_of(menu, S_SKILL);
    if (screen_of(menu) != S_SKILL || G32(scr + SCR_STATE) != 2 || G32(scr + SK_CAT) != 0) return;
    // 동작 고르기에서 OK하면 원본이 목록을 새로 만들어(0x3478c) 커서가 첫 줄로 간다. 고른 스킬(맨 위 +0x20, 선택 +0x24, 줄 +0x28)을 되돌려 둔다.
    uint32_t top = G32(scr + SK_TOP), sel = G32(scr + SK_SEL), row = G32(scr + SK_ROW);
    fm_key_raw(menu, FMK_CLR);
    if (G32(scr + SCR_STATE) != 1) return;
    W32_(&cpu, scr + SK_ACT, 1);
    fm_key_raw(menu, FMK_OK);
    W32_(&cpu, scr + SK_TOP, top); W32_(&cpu, scr + SK_SEL, sel); W32_(&cpu, scr + SK_ROW, row);
    if (G32(scr + SCR_STATE) == 2) fm_key_raw(menu, FMK_OK); // '장착'도 목록을 거친다: 그 스킬에서 OK → 3/6/9 대기
}

// ---- 자체 창의 틀 ----
// 화면 번호별 문서(자체 창이 아래 화면 문서를 감춘다: 반투명 막 아래로 비치거나 눌리지 않게)
static const char *const screen_docs[] = {"fm_main", "fm_item", "fmchar_skill", "fm_equip", "fmchar_status", "fmchar_quest", "fmchar_party", "system"};
int fm_overlay_frame(uint32_t menu) {
    int on = ui_modern() && shell_menu(menu);
    ui_int("shell", on);
    ui_int("chars", on && enh_open()); // 강화만 캐릭터 띠를 쓴다
    if (!on) return 0;
    int n = screen_of(menu);
    if (n >= 0 && n <= S_OPTION) ui_hide(screen_docs[n]);
    if (n == S_OPTION) ui_hide("premium");
    return 1;
}

// ---- 캐릭터 바꾸기 ----
// 원본은 스테이터스(늘), 장비(칸 상태 0), 스킬(분류 상태 0)에서만 좌우 키로 캐릭터를 넘긴다. 그 상태로 먼저 되돌리고 오른쪽 키로 맞춘다.
static void fm_set_char(uint32_t menu, int slot) {
    uint32_t game = G32(menu + MENU_GAME);
    int n = screen_of(menu);
    if (enh_open()) { if (party_char(game, slot) && ext_enhance_set_member) { ext_enhance_set_member(slot); last_slot = slot; } return; }
    if (!party_char(game, slot) || (n != S_SKILL && n != S_EQUIP && n != S_STATUS)) return;
    uint32_t scr = scr_of(menu, n);
    if (n == S_STATUS && G32(scr + ST_NOTE) && G8(G32(scr + ST_NOTE))) fm_key_raw(menu, FMK_CLR); // 레벨업 알림은 키를 가로챈다
    if (n == S_EQUIP)
        for (int i = 0; i < 4 && screen_of(menu) == S_EQUIP && (G32(scr + SCR_STATE) != 0 || G32(scr + EQ_MSG)); i++) fm_key_raw(menu, FMK_CLR);
    if (n == S_SKILL) { skill_to_state0(menu); sk_mem.slot = -1; }
    for (int i = 0; i < PARTY_MAX + 1 && screen_of(menu) == n && (int)G32(scr + SCR_SLOT) != slot; i++) {
        int before = (int)G32(scr + SCR_SLOT);
        fm_key_raw(menu, FMK_RIGHT);
        if ((int)G32(scr + SCR_SLOT) == before) break; // 받지 않는 상태
    }
    if (n == S_SKILL) fm_skill_normalize(menu);
    if (screen_of(menu) == n && (int)G32(scr + SCR_SLOT) == slot) last_slot = slot;
}
static void char_step(uint32_t menu, int dir) {
    uint32_t game = G32(menu + MENU_GAME);
    int n = screen_of(menu);
    if (n != S_SKILL && n != S_EQUIP && n != S_STATUS && !enh_open()) return;
    int cur = enh_open() && ext_enhance_member ? ext_enhance_member() : (int)G32(scr_of(menu, n) + SCR_SLOT);
    for (int t = 1; t <= PARTY_MAX; t++) {
        int i = ((cur + dir * t) % PARTY_MAX + PARTY_MAX) % PARTY_MAX;
        if (party_char(game, i)) { fm_set_char(menu, i); return; }
    }
}

// ---- 「이전」 규칙 ----
// 이 화면에 지금 보이는 하위 구조(팝업·대상 고르기·핫슬롯 등록·단축키 대기·메시지·장비 후보)가 있나. 있으면 CLR은 원본이 한 단계 닫는다.
static int has_layer(uint32_t menu, int n) {
    uint32_t scr = scr_of(menu, n);
    int st = (int)G32(scr + SCR_STATE);
    switch (n) {
    case S_ITEM: return st == 1 || st == 3 || st == 4 || st == 5 || G32(scr + IT_MSG);
    case S_SKILL: return st == 4 || st == 5 || G32(scr + SK_MSG);
    case S_EQUIP: return st == 1 || st == 2 || G32(scr + EQ_MSG);
    case S_STATUS: return G32(scr + ST_NOTE) && G8(G32(scr + ST_NOTE));
    case S_PARTY: return G32(scr + PT_DONE) || G32(scr + PT_PICKS); // 고른 동료를 한 칸씩 되돌린다(원본 CLR)
    case S_OPTION: return 1; // 옵션은 ui_system.c의 키 래퍼가 하위 화면 → 목록 → 홈을 맡는다
    }
    return 0;
}

// ---- 버튼 띠의 값 ----
struct bslot { char t[40], kl[12]; int on, key; fm_fn fn; };
static struct bslot bar_cur[FMB_EX + 2], bar_live[FMB_EX + 2]; // 0..FMB_EX-1 보조, FMB_EX 결정, FMB_EX+1 이전
// 이 화면에 메시지(사용불가, 안내)가 떠 있다: 원본은 그동안 OK/CLR만 받아 메시지를 닫는다
static int has_popup_msg(uint32_t menu, int n) {
    uint32_t scr = scr_of(menu, n);
    return (n == S_ITEM && G32(scr + IT_MSG)) || (n == S_SKILL && G32(scr + SK_MSG)) || (n == S_EQUIP && G32(scr + EQ_MSG));
}

// 필드 메뉴 키 분배 0x2fa60 앞(enhance.c f_2fa60이 부른다). 1이면 키를 가져갔다.
int ext_shell_key(uint32_t menu, int k) {
    if (depth || !k || !ui_modern() || !shell_menu(menu)) return 0;
    int n = screen_of(menu);
    uint32_t scr = scr_of(menu, n);
    int st = (int)G32(scr + SCR_STATE);
    if (k == FMK_CLR) {
        if (n == S_MAIN || has_layer(menu, n)) return 0;
        go_home(menu); // 숨긴 단계(스킬 목록 → 동작 → 분류 등)를 한 칸씩 거꾸로 걷지 않고 홈으로
        return 1;
    }
    // 버튼 띠의 보조 버튼 가운데 여러 단계를 한 번에 하는 것(fn: 스킬 「단축키」, 장비 후보 상태의 「해제」「강화」)은 그 키를 눌러도
    // 버튼을 누른 것과 같은 길로 간다. 전에는 눌러야만 됐고(「단축키」는 키가 아예 없었다) 게임패드로는 닿을 수 없었다(사용자 보고 2026-11-12).
    int hk = k == FMK_STAR ? '*' : k >= FMK_0 && k <= FMK_0 + 9 ? '0' + (k - FMK_0) : 0;
    if (hk && !has_popup_msg(menu, n))
        for (int i = 0; i < FMB_EX; i++)
            if (bar_live[i].on && bar_live[i].fn && bar_live[i].key == hk) { bar_live[i].fn(menu); return 1; }
    int left = k == FMK_LEFT || k == FMK_4, right = k == FMK_RIGHT || k == FMK_6, ok = k == FMK_OK || k == FMK_5;
    if (n == S_SKILL && st == 2 && !G32(scr + SK_MSG) && (left || right)) { // ←→ 분류 탭
        int cat = ((int)G32(scr + SK_CAT) + (right ? 1 : 2)) % 3;
        fm_skill_set_cat(menu, cat);
        return 1;
    }
    if (n == S_EQUIP && !G32(scr + EQ_MSG)) {
        // 장비: 칸(상태 0)의 ←→는 원본대로 캐릭터 넘김(사용자 결정 2026-11-11: 장비 변경은 변경 의사가 있을 때만, 후보로는 OK·후보 줄·고른 칸 다시 누르기로).
        // 후보(상태 1)에서 ←는 칸으로(원본은 후보에서 좌우를 안 받으므로 캐릭터가 넘어가지 않는다).
        if (st == 1 && left) { fm_key_raw(menu, FMK_CLR); return 1; }
    }
    // 아이템: 소비물품이 아닌 분류의 OK(설명 단계)는 없앤다. 설명은 늘 보인다.
    if (n == S_ITEM && st == 0 && ok && !G32(scr + IT_SELL) && (G32(scr + IT_PAGE) * 2 + G32(scr + IT_TAB)) != 0) return 1;
    return 0;
}

// 키 처리 뒤: 메인이 대상 캐릭터 고르기에 들어갔으면(키보드로 홈에서 OK) 같은 틱에 기억한 캐릭터로 마저 넘긴다.
void ext_shell_after(uint32_t menu, int k) {
    (void)k;
    if (depth || !ui_modern() || !shell_menu(menu)) return;
    if (screen_of(menu) == S_MAIN && G32(scr_of(menu, S_MAIN) + MAIN_PICK)) pick_finish(menu, fm_last_char(G32(menu + MENU_GAME)));
}

// ---- 버튼 띠 ----
static void bset(struct bslot *b, const char *t, int on, int key, fm_fn fn, const char *kl) {
    snprintf(b->t, sizeof b->t, "%s", t ? t : "");
    snprintf(b->kl, sizeof b->kl, "%s", kl ? kl : "");
    b->on = on; b->key = key; b->fn = fn;
}
static char bar_hint[64];
void fm_bar_begin(void) { memset(bar_cur, 0, sizeof bar_cur); bar_hint[0] = 0; }
void fm_bar_hint(const char *utf8) { snprintf(bar_hint, sizeof bar_hint, "%s", utf8 ? utf8 : ""); }
void fm_bar_ex(int i, const char *label, int on, int key, fm_fn fn, const char *kl) {
    if (i >= 0 && i < FMB_EX) bset(&bar_cur[i], label, on, key, fn, kl);
}
void fm_bar_ok(const char *label, int on) { bset(&bar_cur[FMB_EX], label, on, UI_KEY_OK, 0, "ok"); }
static void bar_item(const struct bslot *b) {
    ui_text("t", b->t); ui_int("on", b->on); ui_text("kl", b->kl);
}
void fm_bar_emit(uint32_t menu) {
    // 「이전」: 홈에서는 메뉴를 닫으므로 「닫기」
    bset(&bar_cur[FMB_EX + 1], screen_of(menu) == S_MAIN ? "닫기" : "이전", 1, UI_KEY_CLEAR, 0, "clear");
    ui_obj("bar");
    ui_text("hint", bar_hint);
    ui_list("ex");
    for (int i = 0; i < FMB_EX; i++) { ui_item(); bar_item(&bar_cur[i]); ui_close(); }
    ui_close();
    ui_obj("ok"); bar_item(&bar_cur[FMB_EX]); ui_close();
    ui_obj("back"); bar_item(&bar_cur[FMB_EX + 1]); ui_close();
    ui_close();
    memcpy(bar_live, bar_cur, sizeof bar_live);
}
static void bar_press(uint32_t menu, int idx) {
    int i = idx == BAR_OK ? FMB_EX : idx == BAR_BACK ? FMB_EX + 1 : idx;
    if (i < 0 || i >= FMB_EX + 2) return;
    struct bslot *b = &bar_live[i];
    if (!b->on || !b->t[0]) return;
    if (b->fn) b->fn(menu);
    else if (b->key) ui_key_tap(b->key); // 사람이 그 키를 누른 것과 같은 길(틀 훅 포함)로 다음 틱에
}

// 문서의 틀 값은 메뉴에서 바로 정한다(옵션의 도움말은 키 처리 쪽에서 그려 틀 문서보다 먼저 채워지므로 shell_now를 쓸 수 없다).
void fm_doc_frame(uint32_t menu) {
    int on = ui_modern() && shell_menu(menu), n = menu ? screen_of(menu) : -1;
    ui_int("shell", on);
    ui_int("chars", on && (n == S_SKILL || n == S_EQUIP || n == S_STATUS));
}

// ---- 그림 ----
static uint32_t guest_rect(int x, int y, int w, int h) {
    static uint32_t buf;
    if (!buf) buf = heap_alloc(16);
    W32_(&cpu, buf, (uint32_t)x); W32_(&cpu, buf + 4, (uint32_t)y);
    W32_(&cpu, buf + 8, (uint32_t)w); W32_(&cpu, buf + 12, (uint32_t)h);
    return buf;
}
static int cell_x, cell_y, cell_h;
static void cells_reset(void) { ui_scratch_clear(); cell_x = cell_y = cell_h = 0; }
static void cell(int w, int h, int *x, int *y) {
    if (cell_x + w > ui_screen_w()) { cell_x = 0; cell_y += cell_h; cell_h = 0; }
    if (cell_y + h > 320) cells_reset();
    *x = cell_x; *y = cell_y;
    cell_x += w;
    if (h > cell_h) cell_h = h;
}
// 칩 초상: 원본 초상 상자(0x300a0)와 같은 24x23 잘라 보기(프레임과 오프셋 표는 메인 카드 0x30a38과 같다)
static void put_chip_face(const char *key, uint32_t game, uint32_t menu, uint32_t ch) {
    int id = (int)G8(ch + CH_ID);
    uint32_t frame = CALL(FN(0x12fe8), game, (uint32_t)id, 0);
    int k = frame == 0x43 ? 8 : id;
    int dx = (int8_t)G8(G32(GOT_ADDR(GOT_FACE_DX)) + (uint32_t)k), dy = (int8_t)G8(G32(GOT_ADDR(GOT_FACE_DY)) + (uint32_t)k);
    uint32_t gfx = G32(game + GAME_GFX);
    int x, y;
    cell(24, 23, &x, &y);
    CALL(FN(0x3900), gfx, (uint32_t)x, (uint32_t)y, 24, 23);
    CALL(FN(0x56ec), G32(menu + MENU_SHEET), frame, (uint32_t)x, (uint32_t)y, (uint32_t)dx, (uint32_t)dy, 0);
    CALL(FN(0x3994), gfx);
    ui_sprite(key, x, y, 24, 23);
}
static const char *char_name(uint32_t game, uint32_t ch) {
    uint32_t user = G32(ch + CH_NAME);
    if (user) return gstr(user);
    return gstr(G32(game + GAME_CLASS_NAMES) + G16(G32(ch + CH_CLASS) + CLS_NAME));
}

// ---- 틀 문서 ----
void fm_shell_emit(uint32_t menu) {
    shell_now = ui_modern() && shell_menu(menu);
    chars_now = 0;
    if (!shell_now) return;
    uint32_t game = G32(menu + MENU_GAME);
    int n = screen_of(menu), lock = 0, enh = enh_open() && ext_enhance_member;
    chars_now = enh || n == S_SKILL || n == S_EQUIP || n == S_STATUS ? 1 : n == S_MAIN ? -1 : 0;
    int cur_slot = enh ? ext_enhance_member() : chars_now > 0 ? (int)G32(scr_of(menu, n) + SCR_SLOT) : -1;
    if (chars_now > 0) last_slot = cur_slot; // 다른 화면에 갔다 와도 이 캐릭터로 연다
    cpu_t saved = cpu;
    ui_scratch_begin();
    cells_reset();
    ui_begin("fm_shell");
    ui_below(); // 화면 문서(옵션 도움말처럼 키 처리 쪽에서 먼저 채우는 것도)보다 늘 아래
    ui_int("narrow", ui_screen_w() < NARROW_W);
    ui_int("lock", lock);
    ui_int("home", n == S_MAIN);
    ui_int("cur", n == S_MAIN ? -1 : n - 1);
    ui_int("tab", n == S_MAIN ? (int)G32(scr_of(menu, S_MAIN) + MAIN_TAB) : -1);
    ui_list("rail");
    for (int i = 0; i < TABS; i++) {
        ui_item();
        ui_text("name", tab_names[i]);
        ui_int("i", i);
        int x, y;
        cell(16, 16, &x, &y);
        CALL(FN(0x60dc), menu + MENU_SPR, (uint32_t)x, (uint32_t)y, 0, 0,
             guest_rect((int)G32(G32(GOT_ADDR(GOT_TAB_ICON_X)) + 4u * (uint32_t)i), 0, 11, 13));
        ui_sprite_trim("icon", x, y, 16, 16);
        ui_close();
    }
    ui_close();
    uint32_t party = G32(game + GAME_PARTY);
    ui_int("gold", (int)G32(party + PARTY_INV));
    // 플레이 시간(0x314d0 → 0x11ed0): 현재 ms - game+0x24 + game+0x28. 시·분·초 모두 60으로 나눈 나머지를 두 자리로 보인다(메인 메뉴와 같다).
    uint32_t ms = CALL(FN(0x3234)) - G32(game + GAME_PLAY_T0) + G32(game + GAME_PLAY_ACC);
    int s = (int)(ms / 1000u);
    ui_fmt("time", "%02d:%02d:%02d", s / 3600 % 60, s / 60 % 60, s % 60);
    // 장소(0x31508): 게임 플래그 비트 3이면 지역 표(0x11ff4), 아니면 game+0x4dc
    uint32_t place = 0;
    if (G32(game + GAME_FLAGS) & FLAG_PLACE_TABLE) place = CALL(FN(0x11ff4), game);
    if (!place) place = game + GAME_PLACE;
    ui_str("place", gstr(place));

    // 캐릭터 띠(스킬·장비·스테이터스). 칸은 다섯 개로 고정(빈 칸은 on 0, 목록 길이가 바뀌면 RmlUi가 경고를 낸다).
    ui_int("chars", chars_now > 0);
    ui_list("chips");
    int members = 0;
    for (int i = 0; i < PARTY_MAX; i++) {
        uint32_t ch = chars_now > 0 ? party_char(game, i) : 0;
        ui_item();
        ui_int("on", ch != 0);
        ui_int("i", i);
        ui_int("sel", ch && i == cur_slot);
        if (ch) {
            members++;
            ui_str("name", char_name(game, ch));
            ui_int("lv", (int)G8(G32(ch + CH_STATS) + ST_LV));
            put_chip_face("face", game, menu, ch);
        } else { ui_text("name", ""); ui_int("lv", 0); ui_text("face", ""); }
        ui_close();
    }
    ui_close();
    ui_int("members", members);
    // LT/RT: 캐릭터 넘김. 스킬 단축키 대기(상태 5)에서는 RT가 원본 3번 키(단축키 3)라 띠가 버튼을 거둔다.
    int hk_wait = !enh && n == S_SKILL && G32(scr_of(menu, S_SKILL) + SCR_STATE) == 5;
    ui_int("cshoulder", chars_now > 0 && members > 1 && !hk_wait && !lock);
    ui_end();
    ui_scratch_end();
    cpu = saved;
}
void fm_shell_end(void) { shell_now = 0; }

// ---- 누름 처리(메뉴 장면 틱 0x109b8의 원본 앞, enhance.c f_109b8이 부른다) ----
// 원본 키 처리보다 먼저 동기 키로 상태를 옮겨 두면, 같은 틱의 원본 그리기가 바로 새 화면을 그린다(중간 화면이 안 보인다).
void ext_shell_tick(uint32_t game) {
    int rail = ui_pick("rail"), rstep = ui_pick("railstep"), chip = ui_pick("chip"), cstep = ui_pick("charstep");
    int hparty = ui_pick("hparty"), bar = ui_pick("fmbar");
    uint32_t menu = menu_of_game(game);
    if (!ui_modern() || !shell_menu(menu)) return;
    // 레일·홈 카드: 떠 있는 자체 창을 닫고 바로 그 화면으로(한 번 누름). 창을 연 화면을 누르면 창만 닫혀 그 화면으로 돌아간다.
    if (rail >= 0 || rstep >= 0 || hparty >= 0) close_overlays();
    int n = screen_of(menu);
    if (rail >= 0) fm_goto(menu, rail);
    else if (rstep >= 0) {
        int dir = rstep ? 1 : -1;
        if (n == S_MAIN) { // 홈에서는 커서만
            uint32_t m = scr_of(menu, S_MAIN);
            if (!G32(m + MAIN_PICK)) W32_(&cpu, m + MAIN_TAB, (uint32_t)(((int)G32(m + MAIN_TAB) + dir + TABS) % TABS));
        } else fm_goto(menu, ((n - 1) + dir + TABS) % TABS);
    }
    else if (chip >= 0) fm_set_char(menu, chip);
    else if (cstep >= 0) char_step(menu, cstep ? 1 : -1);
    else if (hparty >= 0 && party_char(game, hparty)) { last_slot = hparty; fm_goto(menu, S_STATUS - 1); }
    else if (bar >= 0) bar_press(menu, bar);
}
