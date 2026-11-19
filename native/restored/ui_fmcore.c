// 필드 메뉴 바탕(분배 0x2faa4)과 메인(화면 0), 아이템(화면 1), 장비(화면 3)를 RmlUi로 다시 그린다(영역 fmcore).
//
// 원작 필드 메뉴는 화면 가운데 120x146 창 하나에 모든 화면을 욱여넣었다. 넓은 화면(높이 320dp, 폭 240~640dp)에서는
// 목록, 설명, 대상 캐릭터를 한 화면에 나란히 놓는다. 입력과 상태는 원본이 그대로 처리하고, 여기서는 그리기만 바꾼다.
//
// 그리는 방법은 셋 모두 같다.
//   1. 원본 그리기를 ui_scratch 안에서 그대로 부른다. 원본 그리기에는 부수 효과가 있다(메시지 띠 타이머 0x30cac가
//      메시지를 지우고, 장비 그리기가 커서 후보 레코드 +0x40을 갱신하고, 메인 그리기가 화살표 깜빡임과 치트 알림 프레임을 센다).
//      화면만 버리면 이 효과는 원작과 똑같이 남는다.
//   2. 화면에 보일 값은 구조체에서 읽는다(원본 그리기가 읽는 곳과 같은 곳. 아래 각 함수 설명).
//   3. 초상화, 아이템 아이콘, 메뉴 아이콘은 같은 scratch 화면에 원본 스프라이트 그리기를 하나씩 불러 떼어 낸다.
//
// 주소는 파일 오프셋이다(게스트 주소는 +0x100000). 조사 내용은 work/ui/survey/fieldmenu.md.
#include "restored.h"
#include "items_ext.h"
#include "enhance.h"
#include "fm_shell.h"
#include <stdio.h>
#include <string.h>

#define G32(a) R32_(&cpu, (a))
#define G16(a) R16_(&cpu, (a))
#define G8(a) R8_(&cpu, (a))
#define FN(x) (0x100000u + (x))

#define GAME_CHEAT VG(GAME_CHEAT_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
#define GAME_PLACE VG(GAME_PLACE_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
enum {
    GOT_FACE_DX = 0x458, GOT_FACE_DY = 0x45c, // 메인 카드 초상화 오프셋 표(9바이트, 캐릭터 id별, 0x43 그림은 8번)
    GOT_MSG_NOUSE = 0x460, GOT_MSG_NOSELL = 0x464, GOT_MSG_HOTSAVE = 0x468, GOT_MSG_DUP = 0x46c,
    GOT_CHEAT_ON = 0x474, GOT_CHEAT_OFF = 0x478,
    GOT_TAB_ICON_X = 0x47c,     // 메인 탭 아이콘: 스프라이트 프레임 0을 (표[탭], 0, 11, 13)으로 자른다
    GOT_ITEM_CATS = 0x484,      // 10바이트씩 "소비물품" "장비물품" "장신구류" "키아이템"
    GOT_SELL_ASK = 0x488, GOT_YES = 0x48c, GOT_NO = 0x490, GOT_SELL_N = 0x494, GOT_SELL_GOLD = 0x498,
    GOT_SLOT_NAMES = 0x4a0,     // 12바이트씩 장비 칸 이름(무기류, 장갑/방패류, ...)
    GOT_EQUIP_LV = 0x4a4,       // "장착불가 Lv%d"
    GOT_NO_EQUIP = 0x4a8,       // "소지한 장비가 없습니다."

    // 메뉴 객체(game + 0x1d4)
    MENU_GAME = 0x00, MENU_SPR = 0x04, MENU_SHEET = 0x14, MENU_SCREEN = 0x18, MENU_MODE = 0x1c,
    MENU_SCREENS = 0x24, MENU_CLOSE = 0x60, MENU_X = 0x64, MENU_Y = 0x68,
    MODE_NO_BACK = 2,           // 타이틀의 이어하기: 바탕 창 없음
    SCR_OPTION = 7, OPT_SUB = 0x0c, OPT_HELP = 3,
    // 게임 객체
    GAME_GFX = 0x08, GAME_FLAGS = 0x14, GAME_PLAY_T0 = 0x24, GAME_PLAY_ACC = 0x28,
    GAME_PARTY = 0x1c0, GAME_ITEMDB = 0x1cc, GAME_CLASS_NAMES = 0x314, GAME_CHEAT_D5 = 0x4bc, GAME_PLACE_D5 = 0x4dc,
    FLAG_PLACE_TABLE = 0x08,
    // 파티와 인벤토리(파티 + 0xa8)
    PARTY_CHARS = 0x84, PARTY_INV = 0xa8, PARTY_MAX = 5,
    INV_GOLD = 0x00, INV_COUNT = 0x50, INV_HOT = 0x150, INV_HOT_N = 0x158, HOT_MAX = 8,
    // 아이템 DB(game + 0x1cc)와 32바이트 레코드
    IDB_N = 0x00, IDB_RECS = 0x08, IDB_GAME = 0x10, IDB_NAMES = 0x14, GAME_DESCS = 0x318,
    REC_KIND = 0x00, REC_ICON = 0x02, REC_NAME = 0x04, REC_DESC = 0x06, REC_PRICE = 0x08, REC_FLAGS = 0x0c,
    REC_PATK = 0x10, REC_MATK = 0x12,
    RECF_ALL_PARTY = 1 << 13,
    ITEM_CONSUMABLE_LAST = 0x1b, // 이 id까지는 소모품 아이콘(0x301f4)
    // 캐릭터
    CH_ID = 0x04, CH_MASK_BIT = 0x05, CH_STATE = 0x58, CH_STATS = 0x64, CH_CLASS = 0xb4, CH_MP_MAX = 0xf4,
    CH_STR = 0x118, CH_DEX = 0x11a, CH_INT = 0x11c, CH_EQUIP = 0x140, CH_NAME = 0x19c,
    CH_DOWN = 0x8000,           // 전투불능
    ST_HP = 0x00, ST_HP_MAX = 0x04, ST_MP = 0x08, ST_LV = 0x18,
    CLS_W_STR = 0x0c, CLS_W_DEX = 0x0d, CLS_W_INT = 0x0e, CLS_PATK = 0x16, CLS_MATK = 0x17, CLS_PDEF = 0x18, CLS_MDEF = 0x19,
    CLS_NAME = 0x1e,

    FRAME_FACE_ALT = 0x43,      // 이 초상 프레임이면 오프셋 표 8번
    NARROW_W = 400, WIDE3_W = 520,
};

// ---- 공통 도우미 ----

static const char *got_str(int off) { return gstr(G32(GOT_ADDR(off))); }

static uint32_t item_rec(uint32_t db, int id) { return G32(db + IDB_RECS) + 32u * (uint32_t)id; }
static const char *item_name(uint32_t db, int id) { return gstr(G32(db + IDB_NAMES) + G16(item_rec(db, id) + REC_NAME)); }
static const char *item_desc(uint32_t db, int id) {
    const char *ext = ext_item_desc_g(id); // 새 아이템(246~)의 설명은 설명 풀 밖에 있다(restored/items_ext.c)
    if (ext) return ext;
    return gstr(G32(G32(db + IDB_GAME) + GAME_DESCS) + G16(item_rec(db, id) + REC_DESC));
}
static int item_id_of(uint32_t db, uint32_t rec) { return rec ? (int)((rec - G32(db + IDB_RECS)) / 32u) : -1; }
static int inv_count(uint32_t inv, int id) { return (int)G8(inv + INV_COUNT + (uint32_t)id); }

// 캐릭터 이름(0x252f8과 같은 규칙): 사용자 이름이 있으면 그것, 없으면 직업 표의 이름
static const char *char_name(uint32_t game, uint32_t ch) {
    uint32_t user = G32(ch + CH_NAME);
    if (user) return gstr(user);
    return gstr(G32(game + GAME_CLASS_NAMES) + G16(G32(ch + CH_CLASS) + CLS_NAME));
}

static int pct(int v, int max) {
    if (max <= 0 || v <= 0) return 0;
    return v >= max ? 100 : v * 100 / max;
}

// 게스트 쪽 임시 사각형(0x60dc의 원본 잘라내기 인자). 한 번만 잡는다.
static uint32_t guest_rect(int x, int y, int w, int h) {
    static uint32_t buf;
    if (!buf) buf = heap_alloc(16);
    W32_(&cpu, buf, (uint32_t)x); W32_(&cpu, buf + 4, (uint32_t)y);
    W32_(&cpu, buf + 8, (uint32_t)w); W32_(&cpu, buf + 12, (uint32_t)h);
    return buf;
}

// scratch 화면 위의 빈 칸을 차례로 나눠 준다(그림마다 화면 전체를 다시 칠하지 않게 한 번 지우고 칸을 바꿔 가며 그린다).
static int cell_x, cell_y, cell_h;
static void cells_reset(void) { ui_scratch_clear(); cell_x = 0; cell_y = 0; cell_h = 0; }
static void cell(int w, int h, int *x, int *y) {
    int sw = ui_screen_w();
    if (cell_x + w > sw) { cell_x = 0; cell_y += cell_h; cell_h = 0; }
    if (cell_y + h > 320) cells_reset(); // 다 썼으면(그림이 아주 많을 때) 처음부터. 이미 뗀 그림은 텍스처로 남는다.
    *x = cell_x; *y = cell_y;
    cell_x += w;
    if (h > cell_h) cell_h = h;
}

// 초상화. 메인 카드(0x30a38)와 같은 프레임과 오프셋으로, 40x40 칸에 잘라 그린다.
static void put_face(const char *key, uint32_t game, uint32_t menu, uint32_t ch) {
    int id = (int)G8(ch + CH_ID);
    uint32_t frame = CALL(FN(0x12fe8), game, (uint32_t)id, 0);
    int k = frame == FRAME_FACE_ALT ? 8 : id;
    int dx = (int8_t)G8(G32(GOT_ADDR(GOT_FACE_DX)) + (uint32_t)k), dy = (int8_t)G8(G32(GOT_ADDR(GOT_FACE_DY)) + (uint32_t)k);
    uint32_t gfx = G32(game + GAME_GFX);
    int x, y;
    cell(40, 40, &x, &y);
    CALL(FN(0x3900), gfx, (uint32_t)x, (uint32_t)y, 40, 40);
    CALL(FN(0x56ec), G32(menu + MENU_SHEET), frame, (uint32_t)(x + 8), (uint32_t)(y + 2), (uint32_t)dx, (uint32_t)dy, 0);
    CALL(FN(0x3994), gfx);
    ui_sprite(key, x, y, 40, 40);
}

// 큰 초상화(상반신 전체, 64x60 정도)
static void put_bust(const char *key, uint32_t game, uint32_t menu, uint32_t ch) {
    uint32_t frame = CALL(FN(0x12fe8), game, (uint32_t)G8(ch + CH_ID), 0);
    int x, y;
    cell(112, 112, &x, &y);
    CALL(FN(0x56ec), G32(menu + MENU_SHEET), frame, (uint32_t)(x + 24), (uint32_t)(y + 24), 0, 0, 0);
    ui_sprite_trim(key, x, y, 112, 112);
}

// 아이템 아이콘. 소모품(id ≤ 0x1b)은 0x301f4(바탕 + 그림), 나머지는 0x301c4(11x11 한 칸).
static void put_icon(const char *key, uint32_t menu, uint32_t db, int id) {
    int x, y;
    cell(20, 24, &x, &y);
    uint32_t icon = G8(item_rec(db, id) + REC_ICON);
    CALL(FN(id <= ITEM_CONSUMABLE_LAST || id >= IT_EXT_FIRST ? 0x301f4 : 0x301c4), 0, menu + MENU_SPR, (uint32_t)(x + 2), (uint32_t)(y + 2), icon);
    ui_sprite_trim(key, x, y, 20, 24);
}

// 아이템 아이콘(0x301c4에 그림 번호를 바로 준다. 빈 장비 칸 그림 13)
static void put_icon_no(const char *key, uint32_t menu, int icon) {
    int x, y;
    cell(16, 16, &x, &y);
    CALL(FN(0x301c4), 0, menu + MENU_SPR, (uint32_t)(x + 2), (uint32_t)(y + 2), (uint32_t)icon);
    ui_sprite_trim(key, x, y, 16, 16);
}

// 캐릭터 한 명의 공통 값(초상, 이름, 레벨, HP/MP)
static void put_char(uint32_t game, uint32_t menu, uint32_t ch) {
    uint32_t st = G32(ch + CH_STATS);
    int hp = (int)G32(st + ST_HP), hpm = (int)G32(st + ST_HP_MAX), mp = (int)G32(st + ST_MP), mpm = (int)G32(ch + CH_MP_MAX);
    put_face("face", game, menu, ch);
    ui_str("name", char_name(game, ch));
    ui_int("lv", (int)G8(st + ST_LV));
    ui_int("hp", hp); ui_int("hp_max", hpm); ui_int("hp_pct", pct(hp, hpm));
    ui_int("mp", mp); ui_int("mp_max", mpm); ui_int("mp_pct", pct(mp, mpm));
    ui_bool("down", (G32(ch + CH_STATE) & CH_DOWN) != 0);
}

static uint32_t party_char(uint32_t game, int i) { return G32(G32(game + GAME_PARTY) + PARTY_CHARS + 4u * (uint32_t)i); }

// 목록 길이를 늘 같게 채운다. RmlUi는 배열이 줄어드는 틱에 사라질 줄의 식(d.items[3].sel 같은)을 한 번 더 읽어
// 경고를 내므로, 줄 수가 바뀌는 목록은 빈 칸(on = 0, 문서에서 감춘다)으로 끝까지 채워 길이를 고정한다.
static void pad_list(int have, int want) {
    // 게이지 폭 식(p.hp_pct + '%')이 빈 값이면 "width: %"가 되어 경고가 나므로 0을 넣어 둔다
    // i = -1: 빈 칸은 터치로 고를 수 없다(nav idx)
    for (int i = have; i < want; i++) { ui_item(); ui_int("on", 0); ui_int("i", -1); ui_int("hp_pct", 0); ui_int("mp_pct", 0); ui_close(); }
}

// 키 안내 한 쌍(목록 "keys"는 KEYS_MAX칸으로 채운다)
enum { KEYS_MAX = 7 };
static int keys_n;
static void keys_begin(void) { ui_list("keys"); keys_n = 0; }
// k는 키 이름(ok, clear, star, 0~9: 문서가 장치별 이름표로 바꾸고 터치에서는 그 키를 누르는 버튼이 된다) 또는 방향 안내 글(◀▶, ▲▼).
static void key_hint(const char *k, const char *label) {
    // 방향키 안내(nav)는 원작 안내줄에 없던 것이라 좁은 화면에서는 접고, 터치에서는 감춘다(항목을 바로 누른다)
    int nav = !strncmp(k, "◀", strlen("◀")) || !strncmp(k, "▲", strlen("▲"));
    ui_item(); ui_int("on", 1); ui_int("nav", nav); ui_text("k", k); ui_text("key", nav ? "" : k); ui_text("t", label); ui_close();
    keys_n++;
}
static void keys_end(void) { pad_list(keys_n, KEYS_MAX); ui_close(); }

// ---- 분배 0x2faa4(메뉴 객체) ----
// 원본: 닫기 요청(+0x60)이 없으면, 모드가 2가 아닐 때 바탕 창 0x1415c(x, y, 120, 146, 남색)를 칠하고 현재 화면의 그리기(vtable+4)를 부른다.
// 화면 7(옵션)의 도움말(+0xc == 3)이면 아무것도 그리지 않는다.
// 새 UI에서는 바탕 창을 칠하지 않는다. 아홉 화면 모두 각자 문서로 그리므로 원작 창 자리가 남으면 안 된다.
// 화면 그리기는 그대로 부른다(각 영역이 자기 그리기 함수를 감싼다).
// 모드 1의 화면 0~7은 화면 그리기 앞에서 필드 메뉴 틀(레일·캐릭터 띠, restored/fm_shell.c)을 채운다. 옵션의 도움말도 틀은 그린다.
void orig_f_2faa4(cpu_t *c);
void f_2faa4(cpu_t *c) {
    if (!ui_modern()) { orig_f_2faa4(c); return; }
    uint32_t menu = c->r[0];
    if (G32(menu + MENU_CLOSE)) return;
    fm_shell_emit(menu);
    uint32_t n = G32(menu + MENU_SCREEN);
    if (n == SCR_OPTION && G32(G32(menu + MENU_SCREENS + 4 * SCR_OPTION) + OPT_SUB) == OPT_HELP) { fm_shell_end(); return; }
    uint32_t scr = G32(menu + MENU_SCREENS + 4 * n);
    CALLP(G32(G32(scr) + 4), scr, G32(G32(menu + MENU_GAME) + GAME_GFX));
    fm_shell_end();
}

// ---- 화면 0: 메인 0x310e8(화면 객체) ----
// 화면 객체: +0xc 탭(0~6), +0x20 캐릭터 고르는 중, +0x28 맨 위 파티 칸, +0x2c 고른 파티 칸, +0x30 보이는 칸 안 커서,
// +0x50 알림(낮은 바이트 종류 1/4/8, 그 위 프레임 수).
// 필드 메뉴 틀(fm_shell.c)에서 메뉴 7개와 소지금·시간·장소는 왼쪽 레일에 있다. 홈은 파티 5칸(누르면 그 캐릭터의 스테이터스)이다.
// 원본의 '대상 선택' 단계(+0x20)는 틀이 같은 틱에 기억한 캐릭터로 마저 넘기므로 화면에 나오지 않는다.
// 좁은 화면(레일이 아이콘만)에서는 소지금·시간·장소를 홈에 함께 보인다(원작 메인의 정보).
enum { MAIN_TAB = 0x0c, MAIN_PICK = 0x20, MAIN_SEL = 0x2c, MAIN_TOAST = 0x50, TABS = 7 };
static const char *const tab_names[TABS] = {"아이템", "스킬", "장비", "스테이터스", "퀘스트", "전투파티", "옵션"};

void orig_f_310e8(cpu_t *c);
void f_310e8(cpu_t *c) {
    if (!ui_modern()) { orig_f_310e8(c); return; }
    uint32_t scr = c->r[0], menu = G32(scr + 4), game = G32(menu + MENU_GAME);
    uint32_t toast = G32(scr + MAIN_TOAST);
    ui_scratch_begin();
    orig_f_310e8(c);
    cells_reset();

    int tab = (int)G32(scr + MAIN_TAB);
    uint32_t party = G32(game + GAME_PARTY);
    ui_begin("fm_main");
    fm_doc_frame(menu);
    ui_int("narrow", ui_screen_w() < NARROW_W);
    ui_int("tab", tab);
    ui_text("tab_name", tab >= 0 && tab < TABS ? tab_names[tab] : "");
    ui_list("party");
    int members = 0;
    for (int i = 0; i < PARTY_MAX; i++) {
        uint32_t ch = party_char(game, i);
        if (!ch) continue;
        ui_item();
        ui_int("on", 1);
        put_char(game, menu, ch);
        ui_int("slot", i);
        ui_close();
        members++;
    }
    pad_list(members, PARTY_MAX);
    ui_close();
    ui_int("members", members);
    ui_int("last", fm_last_char(game));
    ui_int("gold", (int)G32(party + PARTY_INV + INV_GOLD));
    // 플레이 시간(0x314d0 → 0x11ed0): 현재 ms - game+0x24 + game+0x28. 시·분·초 모두 60으로 나눈 나머지를 두 자리로 보인다.
    uint32_t ms = CALL(FN(0x3234)) - G32(game + GAME_PLAY_T0) + G32(game + GAME_PLAY_ACC);
    int s = (int)(ms / 1000u);
    ui_fmt("time", "%02d:%02d:%02d", s / 3600 % 60, s / 60 % 60, s % 60);
    // 장소(0x31508): 게임 플래그 비트 3이면 지역 표(0x11ff4가 돌려주는 28바이트 칸의 앞 글), 아니면 game+0x4dc
    uint32_t place = 0;
    if (G32(game + GAME_FLAGS) & FLAG_PLACE_TABLE) place = CALL(FN(0x11ff4), game);
    if (!place) place = game + GAME_PLACE;
    ui_str("place", gstr(place));
    // 치트 알림(0x310e8 끝): 종류 1/4/8이 game+0x4bc의 비트 1/2/0x20을 본다. 켜졌으면 "Cheat Enabled.."
    int kind = (int)(toast & 0xff);
    const char *msg = "";
    if (kind) {
        uint32_t bits = G32(game + GAME_CHEAT);
        int on = kind & 1 ? bits & 1 : kind & 4 ? bits & 2 : kind & 8 ? bits & 0x20 : 1;
        msg = got_str(on ? GOT_CHEAT_ON : GOT_CHEAT_OFF);
    }
    ui_str("toast", msg);
    // 레일(세로)의 커서는 원본 탭이고 원본(0x30ef0)은 좌우로 넘긴다. 위아래 키도 좌우로 받는다(원본은 위아래를 쓰지 않는다).
    ui_key_alias_v2h();
    fm_bar_begin();
    fm_bar_hint("↑↓ 선택");
    fm_bar_ok("열기", 1);
    fm_bar_emit(menu);
    ui_end();
    ui_scratch_end();
}

// ---- 화면 1: 아이템 0x31b18(화면 객체) ----
// 화면 객체(survey "아이템"): +0x10 상태(0 목록, 1 사용 대상, 2 설명, 3 판매 개수, 4 판매 확인, 5 핫슬롯 등록),
// +0x14 판매 확인 커서(0 예, 1 아니오), +0x2c 판매 개수, +0x30 판매 모드(상점이 1로), +0x34 메시지, +0x38 탭(쪽 안 0/1),
// +0x3c 쪽(0/1), +0x44/+0x48/+0x4c 대상 맨 위/고른 파티 칸/줄, +0x50 목록 맨 위 id, +0x54 고른 id(-1 없음), +0x64 종류 마스크.
// 목록(0x31cec): id 0부터 보유 > 0이고 (1 << 레코드[0]) & 마스크인 것. 원작은 5줄을 보이고 맨 위 id부터 그린다.
// 새 UI는 더 많은 줄을 보이되 원작의 맨 위 줄을 기준으로 창을 잡아 커서가 늘 보이게 한다.
enum {
    IT_STATE = 0x10, IT_YESNO = 0x14, IT_SELL_N = 0x2c, IT_SELL = 0x30, IT_MSG = 0x34, IT_TAB = 0x38, IT_PAGE = 0x3c,
    IT_TGT_SEL = 0x48, IT_TOP = 0x50, IT_CUR = 0x54, IT_ROW = 0x58, IT_MASK = 0x64,
    IS_LIST = 0, IS_TARGET = 1, IS_INFO = 2, IS_SELL_N = 3, IS_SELL_ASK = 4, IS_HOT = 5,
    MSG_NOUSE = 0xa, MSG_DUP = 0xc, MSG_HOTSAVE = 0xd, MSG_NOSELL = 0xe,
    CATS = 4, KEY_CAT = 3,
};


// 목록 직접 쓰기(UI_NAV_DIRECT): 누른 줄 t(걸러 낸 목록의 번호)로 원본 커서를 옮긴다. 원본 위아래 키(30e54/2fc90, 장비 32e14)가 바꾸는
// 세 칸 - 맨 위 id(topf), 고른 id(curf), 화면 줄(rowf, 0..maxrow) - 을 같은 규칙으로 맞춘다: 맨 위 번호 s에서 t - s가 0..maxrow.
// 현재 맨 위가 t를 품으면 그대로 두고, 아니면 가장 적게(위로 벗어났으면 t가 맨 위, 아래면 t가 맨 아래 줄).
static void list_direct(uint32_t scr, uint32_t topf, uint32_t curf, uint32_t rowf, const int *ids, int top_i, int t, int maxrow) {
    int s = top_i < 0 ? 0 : top_i;
    if (t < s) s = t;
    else if (t > s + maxrow) s = t - maxrow;
    W32_(&cpu, scr + topf, (uint32_t)ids[s]);
    W32_(&cpu, scr + curf, (uint32_t)ids[t]);
    W32_(&cpu, scr + rowf, (uint32_t)(t - s));
}

// 눌러서 고른 줄을 바로 결정할 때 보낼 OK(대기열이 차 있으면 다음 틱에 다시 해 본다)
static int pend_ok;
static void tap_ok_soon(void) { pend_ok = ui_key_tap(UI_KEY_OK) ? 0 : 8; }
static void retry_ok(void) { if (pend_ok > 0 && (--pend_ok, ui_key_tap(UI_KEY_OK))) pend_ok = 0; }

static const char *item_msg(int code) {
    switch (code) {
    case MSG_NOUSE: return got_str(GOT_MSG_NOUSE);
    case MSG_DUP: return got_str(GOT_MSG_DUP);
    case MSG_HOTSAVE: return got_str(GOT_MSG_HOTSAVE);
    case MSG_NOSELL: return got_str(GOT_MSG_NOSELL);
    }
    return "";
}

// 걸러 낸 목록(0x31cec와 같은 조건)
static int item_ids(uint32_t scr, uint32_t game, int *ids, int max, int *top_i, int *cur_i) {
    uint32_t inv = G32(game + GAME_PARTY) + PARTY_INV, db = G32(game + GAME_ITEMDB);
    uint32_t mask = G32(scr + IT_MASK);
    int n = (int)G32(db + IDB_N), total = 0, top = (int)G32(scr + IT_TOP), cur = (int)G32(scr + IT_CUR);
    *top_i = *cur_i = -1;
    for (int id = 0; id < n && total < max; id++) {
        if (!inv_count(inv, id)) continue;
        if (!((1u << G8(item_rec(db, id) + REC_KIND)) & mask)) continue;
        if (id >= top && *top_i < 0) *top_i = total;
        if (id == cur) *cur_i = total;
        ids[total++] = id;
    }
    return total;
}

void orig_f_31b18(cpu_t *c);
void f_31b18(cpu_t *c) {
    if (!ui_modern()) { orig_f_31b18(c); return; }
    uint32_t scr = c->r[0], menu = G32(scr + 4), game = G32(menu + MENU_GAME);
    // 목록 줄을 눌렀다(UI_NAV_DIRECT): 원본이 그리기 전에 커서를 직접 옮긴다(상태 0·5, 메시지 띠가 없을 때).
    // 원본 위아래 키가 바꾸는 필드는 +0x50 맨 위 id, +0x54 고른 id, +0x58 화면 줄(0..3)뿐이다(0x2fc90). 사용 대상 파티는 원본 도우미라 키 흉내를 쓴다.
    retry_ok();
    int ist = (int)G32(scr + IT_STATE), shell = fm_shell_on();
    // 필드 메뉴 틀: 소비물품이 아닌 분류의 '설명' 단계(상태 2)는 없앤다(설명은 늘 오른쪽에 보인다). 들어왔으면 같은 틱에 목록으로.
    if (shell && ist == IS_INFO && !G32(scr + IT_SELL)) {
        cpu_t sv = *c;
        fm_key_raw(menu, FMK_CLR);
        *c = sv;
        ist = (int)G32(scr + IT_STATE);
    }
    // 고른 줄을 다시 누르면 소비물품(사용)·핫슬롯 등록은 실행, 다른 분류와 판매는 고르기만(아래).
    int icat = (int)(G32(scr + IT_PAGE) * 2 + G32(scr + IT_TAB));
    int act = icat == 0 || ist == IS_HOT || G32(scr + IT_SELL);
    int ipk = ui_pick("items");
    if (ipk >= 0 && (ist == IS_LIST || ist == IS_HOT) && !G32(scr + IT_MSG)) {
        static int pids[512];
        int pt, pc, ptot = item_ids(scr, game, pids, 512, &pt, &pc);
        if (ipk < ptot) {
            if (ipk != pc) list_direct(scr, IT_TOP, IT_CUR, IT_ROW, pids, pt, ipk, 3);
            // 틀 안에서는 고르지 않은 줄을 누르면 커서만 옮긴다(설명이 바뀐다). 사용 대상 단계·핫슬롯 등록은 「사용」/「등록」, OK,
            // 이미 고른 줄을 다시 누를 때만(사용자 결정 2026-11-11). 마우스는 올리면 커서가 가므로 클릭이 곧 고른 줄 누르기다.
            // 상점 판매(틀 없음)는 구입 목록과 같이 줄을 몇 번 눌러도 고르기만 한다(사용자 결정 2026-11-11). 판매 개수 창은
            // 아래 「판매」 버튼이나 OK 키로만 연다.
            if (act && shell && ipk == pc) tap_ok_soon();
        }
    }
    // 메시지는 원본 그리기(0x30cac)가 시간이 다 되면 지운다. 원작이 이번 틱에 그린 메시지를 보이려고 먼저 읽는다.
    int msg = (int)G32(scr + IT_MSG);
    ui_scratch_begin();
    orig_f_31b18(c);
    cells_reset();

    uint32_t party = G32(game + GAME_PARTY), inv = party + PARTY_INV, db = G32(game + GAME_ITEMDB);
    int state = (int)G32(scr + IT_STATE), sell = G32(scr + IT_SELL) != 0;
    int page = (int)G32(scr + IT_PAGE), tab = (int)G32(scr + IT_TAB), cat = page * 2 + tab;
    int cur = (int)G32(scr + IT_CUR), n = (int)G32(db + IDB_N);
    int narrow = ui_screen_w() < NARROW_W;

    ui_begin("fm_item");
    fm_doc_frame(menu);
    ui_int("narrow", narrow);
    // 목록 | 설명 | 파티 세 단을 둘 만큼 넓다. 틀 안에서는 레일 몫만큼 좁아 두 단(목록 | 설명·핫슬롯, 사용 대상은 고를 때 오른쪽 단)으로 둔다.
    ui_int("wide3", !shell && ui_screen_w() >= WIDE3_W);
    ui_int("state", state);
    ui_int("sell", sell);
    ui_int("cat", cat);
    ui_list("cats");
    for (int i = 0; i < CATS; i++) {
        ui_item();
        ui_str("name", gstr(G32(GOT_ADDR(GOT_ITEM_CATS)) + 10u * (uint32_t)i));
        ui_int("dis", sell && i == KEY_CAT); // 판매 모드에서 키아이템 탭은 회색(0x31b18)
        ui_close();
    }
    ui_close();

    // 목록: 걸러 낸 전체를 문서에 그린다(scroll="ilist" 상자가 넘긴다. 호스트가 키로 옮긴 커서 줄을 보이게 한다)
    static int ids[512];
    int top_i, cur_i, total = item_ids(scr, game, ids, 512, &top_i, &cur_i);
    ui_int("total", total);
    ui_list("items");
    for (int i = 0; i < total; i++) {
        ui_item();
        ui_int("on", 1);
        ui_str("name", item_name(db, ids[i]));
        ui_int("count", inv_count(inv, ids[i]));
        ui_int("sel", i == cur_i);
        ui_int("i", i);
        put_icon("icon", menu, db, ids[i]);
        ui_close();
    }
    ui_close();

    // 고른 아이템(설명, 사용 대상, 판매에서 같이 쓴다)
    ui_int("has_cur", cur >= 0 && cur < n);
    if (cur >= 0 && cur < n) {
        uint32_t rec = item_rec(db, cur);
        ui_str("cur_name", item_name(db, cur));
        ui_str("cur_desc", item_desc(db, cur));
        ui_int("cur_count", inv_count(inv, cur));
        put_icon("cur_icon", menu, db, cur);
        ui_int("all", (G32(rec + REC_FLAGS) & RECF_ALL_PARTY) != 0);
        ui_int("price", (int)(G32(rec + REC_PRICE) >> 1) * (int)G32(scr + IT_SELL_N));
    } else {
        ui_text("cur_name", ""); ui_text("cur_desc", ""); ui_int("cur_count", 0); ui_text("cur_icon", "");
        ui_int("all", 0); ui_int("price", 0);
    }

    // 핫슬롯(인벤토리 +0x150, 수 +0x158). 소비물품 탭에서 늘 보이고 등록 중(상태 5)에는 강조한다.
    int hot_n = (int)G32(inv + INV_HOT_N);
    ui_int("hot_n", hot_n);
    ui_list("hot");
    for (int i = 0; i < HOT_MAX; i++) {
        ui_item();
        int id = i < hot_n ? (int)G8(inv + INV_HOT + (uint32_t)i) : 0xff;
        ui_int("used", id != 0xff && id < n);
        ui_int("next", i == hot_n);
        if (id != 0xff && id < n) { put_icon("icon", menu, db, id); ui_str("name", item_name(db, id)); }
        else { ui_text("icon", ""); ui_text("name", ""); }
        ui_close();
    }
    ui_close();

    // 사용 대상(상태 1, 0x32148): 파티 5칸. 고른 칸(+0x48) 또는 전원 대상 아이템이면 모두 강조, 전투불능은 회색.
    ui_int("tgt", (int)G32(scr + IT_TGT_SEL));
    ui_list("party");
    int members = 0;
    for (int i = 0; i < PARTY_MAX; i++) {
        uint32_t ch = party_char(game, i);
        if (!ch) continue;
        ui_item();
        ui_int("on", 1);
        put_char(game, menu, ch);
        ui_int("slot", i);
        ui_close();
        members++;
    }
    pad_list(members, PARTY_MAX);
    ui_close();

    // 판매(상태 3·4, 0x31e74)
    ui_int("gold", (int)G32(inv + INV_GOLD));
    ui_int("sell_n", (int)G32(scr + IT_SELL_N));
    ui_int("yes", G32(scr + IT_YESNO) == 0);
    ui_str("t_ask", got_str(GOT_SELL_ASK));
    ui_str("t_yes", got_str(GOT_YES));
    ui_str("t_no", got_str(GOT_NO));
    ui_str("t_sell_n", got_str(GOT_SELL_N));
    ui_str("t_sell_gold", got_str(GOT_SELL_GOLD));

    // 메시지 띠(0x30cac): 사용불가/판매불가는 0x31b18이, 중복/핫슬롯 저장은 0x32338이 그린다(상태 0·5에서만).
    int show_msg = msg && (msg == MSG_NOUSE || msg == MSG_NOSELL || state == IS_LIST || state == IS_HOT);
    ui_str("msg", show_msg ? item_msg(msg) : "");

    // 키 안내(0x30884 종류): 소비물품 1 "OK:사용 *:핫슬롯", 다른 탭 9 "OK:설명 CLR:이전", 핫슬롯 8 "*:저장 CLR:이전",
    // 대상 3 "OK:사용 CLR:이전", 설명 2 "CLR:이전", 판매 목록 2, 판매 개수·확인 5 "OK:확인 CLR:이전".
    // 터치(0x315f8의 키): 목록 위아래, 분류 좌우(상태 0), 대상 위아래(상태 1), 판매 예/아니오 좌우(상태 4).
    if (state == IS_LIST || state == IS_HOT) ui_nav("items", cur_i, total, UI_KEY_UP, UI_KEY_DOWN, (act ? UI_NAV_ACT : 0) | UI_NAV_DIRECT);
    if (state == IS_LIST) ui_nav("cats", cat, CATS, UI_KEY_LEFT, UI_KEY_RIGHT, 0);
    if (state == IS_TARGET) ui_nav("party", (int)G32(scr + IT_TGT_SEL), PARTY_MAX, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT);
    if (state == IS_SELL_ASK) ui_nav("yn", (int)G32(scr + IT_YESNO), 2, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_ACT);
    keys_begin();
    switch (state) {
    case IS_LIST:
        key_hint("◀▶", "분류"); key_hint("▲▼", "선택");
        if (sell) { key_hint("ok", "판매"); key_hint("clear", "이전"); }
        else if (cat == 0) { key_hint("ok", "사용"); key_hint("star", "핫슬롯"); key_hint("clear", "이전"); }
        else { key_hint("ok", "설명"); key_hint("clear", "이전"); }
        break;
    case IS_TARGET: key_hint("▲▼", "대상"); key_hint("ok", "사용"); key_hint("clear", "이전"); break;
    case IS_INFO: key_hint("clear", "이전"); break;
    case IS_SELL_N: key_hint("◀▶", "개수"); key_hint("ok", "확인"); key_hint("clear", "이전"); break;
    case IS_SELL_ASK: key_hint("◀▶", "예/아니오"); key_hint("ok", "확인"); key_hint("clear", "이전"); break;
    case IS_HOT: key_hint("▲▼", "선택"); key_hint("ok", "등록"); key_hint("0", "지우기"); key_hint("star", "저장"); key_hint("clear", "이전"); break;
    }
    keys_end();
    // 고정 버튼 띠(틀): 보조 [핫슬롯/저장 *] [지우기 0], 결정은 사용/등록. 패드는 LB가 레일이라 0(지우기)을 RS(원본 9, 이 상태에서 안 쓰는 키)로 받는다.
    int pad = ui_input_source() == UI_SRC_PAD;
    if (shell && state == IS_HOT && pad) ui_key_alias('9', '0');
    fm_bar_begin();
    fm_bar_ex(0, state == IS_HOT ? "저장" : "핫슬롯", !show_msg && ((state == IS_LIST && cat == 0) || state == IS_HOT), '*', 0, "star");
    fm_bar_ex(1, "지우기", !show_msg && state == IS_HOT, '0', 0, pad ? "9" : "0");
    if (!show_msg) fm_bar_hint(state == IS_LIST ? "←→ 분류" : state == IS_TARGET ? "←→ 대상" : "");
    if (state == IS_HOT) fm_bar_ok("등록", !show_msg && total > 0);
    else fm_bar_ok("사용", !show_msg && ((state == IS_LIST && cat == 0 && total > 0) || state == IS_TARGET));
    fm_bar_emit(menu);
    ui_end();
    ui_scratch_end();
}

// ---- 화면 3: 장비 0x32778(화면 객체) ----
// 화면 객체: +0xc 파티 칸, +0x10 상태(0 칸 목록, 1 후보 목록, 2 상세), +0x24 칸 커서, +0x2c 칸 스크롤(늘 0),
// +0x1c 후보 맨 위 id(-1 없음), +0x20 후보 고른 id, +0x28 후보 줄, +0x30 상세에서 돌아갈 상태, +0x34 메시지(10 = 장착불가),
// +0x38 요구 레벨, +0x3c 지금 장착한 레코드, +0x40 커서 후보 레코드(원본 그리기가 갱신한다).
enum {
    EQ_SLOT_CHAR = 0x0c, EQ_STATE = 0x10, EQ_CAND_TOP = 0x1c, EQ_CAND = 0x20, EQ_CAND_ROW = 0x28, EQ_SLOT = 0x24, EQ_SLOT_TOP = 0x2c,
    EQ_BACK = 0x30, EQ_MSG = 0x34, EQ_REQ_LV = 0x38, EQ_CUR_REC = 0x3c, EQ_CAND_REC = 0x40,
    ES_SLOTS = 0, ES_CANDS = 1, ES_INFO = 2, EQ_MSG_LV = 10, SLOTS = 5, STAT_MAX = 999,
    ICON_EMPTY_SLOT = 13,
};

static int clamp999(int v) { return v > STAT_MAX ? STAT_MAX : v; }

// 후보(상태 1, 0x32e7c와 같은 조건): 보유 > 0, 이 캐릭터가 낄 수 있고(레코드 +0xc의 1 << 캐릭터[5]), 칸 종류가 맞는 것.
// 맨 위·고른 id는 top0/cand0(상태 0에서는 지난 값이라 -1을 준다).
static int cand_ids(uint32_t game, uint32_t ch, int slot, int top0, int cand0, int *ids, int max, int *top_i, int *cur_i) {
    uint32_t inv = G32(game + GAME_PARTY) + PARTY_INV, db = G32(game + GAME_ITEMDB);
    int n = (int)G32(db + IDB_N), total = 0;
    *top_i = *cur_i = -1;
    for (int id = 0; id < n && total < max; id++) {
        if (!inv_count(inv, id)) continue;
        uint32_t rec = item_rec(db, id);
        if (!(G32(rec + REC_FLAGS) & (1u << G8(ch + CH_MASK_BIT)))) continue;
        if ((int)G8(rec + REC_KIND) != slot + 1) continue;
        if (top0 >= 0 && id >= top0 && *top_i < 0) *top_i = total;
        if (id == cand0) *cur_i = total;
        ids[total++] = id;
    }
    return total;
}

// ---- 필드 메뉴 틀의 장비 화면 몰기(restored/fm_shell.c) ----
// 보이는 것은 바로 눌린다: 칸(상태 0의 목록)과 후보(늘 보이는 커서 칸의 후보)를 어느 상태에서 눌러도 래퍼가 원본 상태를 옮긴 뒤 처리한다.
//   칸을 누르면: 후보·상세·메시지에서는 CLR로 칸 상태로 → 그 칸으로(이미 고른 칸이면 OK로 후보에 들어간다).
//   후보를 누르면: 칸 상태면 OK(후보 만들기) → 커서를 그 줄로 → OK(장착)까지 같은 틱에. 장착불가면 원본 메시지가 그대로 뜬다.
static void eq_to_slots(uint32_t menu) {
    uint32_t scr = G32(menu + MENU_SCREENS + 4 * 3);
    for (int i = 0; i < 4 && G32(menu + MENU_SCREEN) == 3 && (G32(scr + EQ_STATE) != ES_SLOTS || G32(scr + EQ_MSG)); i++) fm_key_raw(menu, FMK_CLR);
}
static void eq_unequip(uint32_t menu) { eq_to_slots(menu); if (G32(G32(menu + MENU_SCREENS + 4 * 3) + EQ_STATE) == ES_SLOTS) fm_key_raw(menu, FMK_0); }
static void eq_enhance(uint32_t menu) { eq_to_slots(menu); if (G32(G32(menu + MENU_SCREENS + 4 * 3) + EQ_STATE) == ES_SLOTS) fm_key_raw(menu, FMK_STAR); }
static void eq_picks(cpu_t *c, uint32_t menu, uint32_t scr, uint32_t game, int spk, int cpk) {
    if (spk < 0 && cpk < 0) return;
    cpu_t sv = *c;
    if (spk >= 0 && spk < SLOTS) {
        int was = G32(scr + EQ_STATE) == ES_SLOTS && !G32(scr + EQ_MSG);
        eq_to_slots(menu);
        if (G32(scr + EQ_STATE) == ES_SLOTS && !G32(scr + EQ_MSG)) {
            if (was && (int)G32(scr + EQ_SLOT) == spk) fm_key_raw(menu, FMK_OK); // 고른 칸을 다시 눌렀다: 후보로
            else { W32_(&cpu, scr + EQ_SLOT, (uint32_t)spk); W32_(&cpu, scr + EQ_SLOT_TOP, 0); }
        }
    } else if (cpk >= 0) {
        if (G32(scr + EQ_MSG)) fm_key_raw(menu, FMK_CLR);            // 메시지는 아무 키나 닫는다
        if (G32(scr + EQ_STATE) == ES_INFO) fm_key_raw(menu, FMK_CLR); // 상세 → 돌아갈 상태
        if (G32(scr + EQ_STATE) == ES_SLOTS) fm_key_raw(menu, FMK_OK); // 칸 → 후보(원본이 후보 목록을 만든다)
        if (G32(scr + EQ_STATE) == ES_CANDS && !G32(scr + EQ_MSG)) {
            static int pids[512];
            uint32_t ch = party_char(game, (int)G32(scr + EQ_SLOT_CHAR)), db = G32(game + GAME_ITEMDB);
            int pt, pc, ptot = ch ? cand_ids(game, ch, (int)(G32(scr + EQ_SLOT) + G32(scr + EQ_SLOT_TOP)), (int)G32(scr + EQ_CAND_TOP),
                                             (int)G32(scr + EQ_CAND), pids, 512, &pt, &pc) : 0;
            if (cpk < ptot) {
                if (cpk != pc) list_direct(scr, EQ_CAND_TOP, EQ_CAND, EQ_CAND_ROW, pids, pt, cpk, 4);
                W32_(&cpu, scr + EQ_CAND_REC, item_rec(db, pids[cpk])); // 커서 후보 레코드(원본 그리기가 갱신하는 칸)도 맞춰 둔다
                fm_key_raw(menu, FMK_OK);
            }
        }
    }
    *c = sv;
}

void orig_f_32778(cpu_t *c);
void f_32778(cpu_t *c) {
    if (!ui_modern()) { // 원작 그리기: 그리는 동안만 장착 레코드(와 후보)에 칸 강화를 얹는다(restored/enhance.c)
        uint32_t s0 = c->r[0];
        uint32_t game0 = G32(G32(s0 + 4) + MENU_GAME);
        enh_patch_begin(party_char(game0, (int)G32(s0 + EQ_SLOT_CHAR)), G32(s0 + EQ_CAND_REC), (int)G32(s0 + EQ_SLOT));
        orig_f_32778(c);
        enh_patch_end();
        return;
    }
    uint32_t scr = c->r[0], menu = G32(scr + 4), game = G32(menu + MENU_GAME);
    int msg = (int)G32(scr + EQ_MSG);
    // 줄을 눌렀다(UI_NAV_DIRECT): 원본이 그리기 전에 커서를 직접 옮긴다(장착불가 메시지가 떠 있으면 원본처럼 키만 받으므로 버린다).
    // 칸(상태 0): 원본 위아래 키는 +0x24만 바꾼다(0 아래로는 안 돈다, +0x2c는 늘 0). 후보(상태 1): +0x1c 맨 위 id, +0x20 고른 id, +0x28 줄(0..4)(0x32e14).
    retry_ok();
    int spk = ui_pick("slots"), cpk = ui_pick("cands"), est = (int)G32(scr + EQ_STATE);
    int spk2 = ui_pick("spick"), cpk2 = ui_pick("cpick"), shell = fm_shell_on();
    if (shell) { eq_picks(c, menu, scr, game, spk >= 0 ? spk : spk2, cpk >= 0 ? cpk : cpk2); msg = (int)G32(scr + EQ_MSG); }
    else if (spk >= 0 && est == ES_SLOTS && msg != EQ_MSG_LV && spk < SLOTS) {
        W32_(&cpu, scr + EQ_SLOT, (uint32_t)spk);
        W32_(&cpu, scr + EQ_SLOT_TOP, 0);
        tap_ok_soon();
    }
    if (!shell && cpk >= 0 && est == ES_CANDS && msg != EQ_MSG_LV) {
        static int pids[512];
        uint32_t pch = party_char(game, (int)G32(scr + EQ_SLOT_CHAR));
        int pt, pc, ptot = pch ? cand_ids(game, pch, (int)(G32(scr + EQ_SLOT) + G32(scr + EQ_SLOT_TOP)), (int)G32(scr + EQ_CAND_TOP),
                                          (int)G32(scr + EQ_CAND), pids, 512, &pt, &pc) : 0;
        if (cpk < ptot) {
            if (cpk != pc) list_direct(scr, EQ_CAND_TOP, EQ_CAND, EQ_CAND_ROW, pids, pt, cpk, 4);
            tap_ok_soon();
        }
    }
    ui_scratch_begin();
    orig_f_32778(c);
    cells_reset();

    uint32_t party = G32(game + GAME_PARTY), inv = party + PARTY_INV, db = G32(game + GAME_ITEMDB);
    uint32_t ch = party_char(game, (int)G32(scr + EQ_SLOT_CHAR));
    int state = (int)G32(scr + EQ_STATE), slot = (int)(G32(scr + EQ_SLOT) + G32(scr + EQ_SLOT_TOP));
    int n = (int)G32(db + IDB_N);
    int narrow = ui_screen_w() < NARROW_W;

    ui_begin("fm_equip");
    fm_doc_frame(menu);
    ui_int("narrow", narrow);
    ui_int("state", state);
    ui_int("slot", slot);
    if (!ch) { ui_end(); ui_scratch_end(); return; }
    put_char(game, menu, ch);
    put_bust("bust", game, menu, ch);

    // 능력치(0x32778 앞부분과 같은 식). b = STR*w0 + DEX*w1 + INT*w2,
    // P.ATT = (b+2550)*cls[0x16]/2560 + 무기 +0x10, M.ATT = (b+2550)*cls[0x17]/2560 + 무기 +0x12,
    // P.DEF = b*12/1000 + cls[0x18] + 방어구 3칸 +0x10, M.DEF = b*12/1000 + cls[0x19] + 방어구 3칸 +0x12. 표시는 999까지.
    uint32_t cls = G32(ch + CH_CLASS);
    int b = (int)G16(ch + CH_STR) * (int)G8(cls + CLS_W_STR) + (int)G16(ch + CH_DEX) * (int)G8(cls + CLS_W_DEX) +
            (int)G16(ch + CH_INT) * (int)G8(cls + CLS_W_INT);
    int patk = (b + 2550) * (int)G8(cls + CLS_PATK) / 2560, matk = (b + 2550) * (int)G8(cls + CLS_MATK) / 2560;
    uint32_t weapon = G32(ch + CH_EQUIP);
    int ebp, ebm; // 칸 강화 보너스(restored/enhance.c). 장비 화면의 능력치와 전투 필드가 같은 값이다
    if (weapon) {
        enh_slot_bonus(ch, 0, weapon, &ebp, &ebm);
        patk += (int)G16(weapon + REC_PATK) + ebp; matk += (int)G8(weapon + REC_MATK) + ebm;
    }
    int pdef = b * 12 / 1000 + (int)G8(cls + CLS_PDEF), mdef = b * 12 / 1000 + (int)G8(cls + CLS_MDEF);
    for (int k = 1; k <= 3; k++) {
        uint32_t a = G32(ch + CH_EQUIP + 4u * (uint32_t)k);
        if (a) { enh_slot_bonus(ch, k, a, &ebp, &ebm); pdef += (int)G16(a + REC_PATK) + ebp; mdef += (int)G8(a + REC_MATK) + ebm; }
    }
    // 후보 비교(상태 1, 0x32f48): 무기 칸이면 공격 줄, 장갑·갑옷·투구 칸이면 방어 줄에 (후보 - 지금) 차이를 더해 보인다. 장신구는 비교 없음.
    int cmp_row = -1, d0 = 0, d1 = 0;
    if (state == ES_CANDS && slot <= 3 && (int)G32(scr + EQ_CAND) >= 0) { // 후보가 없으면(+0x20 = -1) 비교할 것이 없다
        uint32_t cand = G32(scr + EQ_CAND_REC), now = G32(scr + EQ_CUR_REC);
        int cbp = 0, cbm = 0, nbp = 0, nbm = 0; // 후보도 이 칸의 강화를 받는다
        if (cand) enh_slot_bonus(ch, slot, cand, &cbp, &cbm);
        if (now) enh_slot_bonus(ch, slot, now, &nbp, &nbm);
        d0 = (cand ? (int)G16(cand + REC_PATK) + cbp : 0) - (now ? (int)G16(now + REC_PATK) + nbp : 0);
        d1 = (cand ? (int)G8(cand + REC_MATK) + cbm : 0) - (now ? (int)G8(now + REC_MATK) + nbm : 0);
        cmp_row = slot == 0 ? 0 : 1;
    }
    struct { const char *name; int v, row, d; } stats[4] = {
        {"P.ATT", patk, 0, d0}, {"M.ATT", matk, 0, d1}, {"P.DEF", pdef, 1, d0}, {"M.DEF", mdef, 1, d1},
    };
    ui_list("stats");
    for (int i = 0; i < 4; i++) {
        ui_item();
        ui_text("name", stats[i].name);
        ui_int("v", clamp999(stats[i].v));
        int cmp = stats[i].row == cmp_row;
        int nv = stats[i].v + stats[i].d;
        if (nv < 0) nv = -nv; // 원본(0x32f48)도 음수면 부호를 떼고 보인다
        ui_int("cmp", cmp);
        ui_int("nv", clamp999(nv));
        ui_int("dir", !cmp ? 0 : stats[i].d > 0 ? 1 : stats[i].d < 0 ? -1 : 0);
        ui_close();
    }
    ui_close();

    // 장비 칸 5개(상태 0의 목록). 비었으면 빈 칸 그림 13 + 칸 이름(회색).
    ui_list("slots");
    for (int i = 0; i < SLOTS; i++) {
        uint32_t rec = G32(ch + CH_EQUIP + 4u * (uint32_t)i);
        ui_item();
        ui_str("kind", gstr(G32(GOT_ADDR(GOT_SLOT_NAMES)) + 12u * (uint32_t)i));
        ui_int("empty", rec == 0);
        ui_int("enh", rec ? enh_step(ch, i) : 0); // 칸 강화 단계(restored/enhance.c)
        if (rec) { int id = item_id_of(db, rec); ui_str("name", item_name(db, id)); put_icon("icon", menu, db, id); }
        else { ui_text("name", ""); put_icon_no("icon", menu, ICON_EMPTY_SLOT); }
        ui_close();
    }
    ui_close();

    // 후보: 걸러 낸 전체를 문서에 그린다(scroll="clist" 상자가 넘긴다)
    static int ids[512];
    int top = (int)G32(scr + EQ_CAND_TOP), cand = (int)G32(scr + EQ_CAND);
    // 칸 목록 상태(0)에서도 커서 칸의 후보를 미리 보인다(그때 +0x1c/+0x20은 지난 값이라 쓰지 않는다).
    if (state == ES_SLOTS) top = cand = -1;
    int top_i, cur_i, total = cand_ids(game, ch, slot, top, cand, ids, 512, &top_i, &cur_i);
    ui_int("total", total);
    ui_str("t_none", got_str(GOT_NO_EQUIP));
    // 후보 줄마다 지금 장비와의 차이(무기 칸 P.ATT/M.ATT, 방어구 칸 P.DEF/M.DEF, 강화 보너스 포함, 위 cmp와 같은 식). 장신구는 원본도 비교가 없다.
    // 그래서 후보를 미리 고르지 않아도 비교가 보인다.
    uint32_t now_rec = slot < SLOTS ? G32(ch + CH_EQUIP + 4u * (uint32_t)slot) : 0;
    int nbp0 = 0, nbm0 = 0;
    if (now_rec) enh_slot_bonus(ch, slot, now_rec, &nbp0, &nbm0);
    int now_p = now_rec ? (int)G16(now_rec + REC_PATK) + nbp0 : 0, now_m = now_rec ? (int)G8(now_rec + REC_MATK) + nbm0 : 0;
    ui_int("cmp", slot <= 3);
    ui_text("cmp_p", slot == 0 ? "P.ATT" : "P.DEF");
    ui_text("cmp_m", slot == 0 ? "M.ATT" : "M.DEF");
    ui_list("cands");
    for (int i = 0; i < total; i++) {
        ui_item();
        ui_int("on", 1);
        uint32_t rec = item_rec(db, ids[i]);
        int cbp = 0, cbm = 0;
        enh_slot_bonus(ch, slot, rec, &cbp, &cbm);
        int dd[2] = {(int)G16(rec + REC_PATK) + cbp - now_p, (int)G8(rec + REC_MATK) + cbm - now_m};
        static const char *const dk[2] = {"dp", "dm"};
        for (int k = 0; k < 2; k++) {
            char t[16], key[8];
            if (dd[k] > 0) snprintf(t, sizeof t, "▲%d", dd[k]); else if (dd[k] < 0) snprintf(t, sizeof t, "▼%d", -dd[k]); else snprintf(t, sizeof t, "-");
            ui_text(dk[k], t);
            snprintf(key, sizeof key, "%s_s", dk[k]);
            ui_int(key, dd[k] > 0 ? 1 : dd[k] < 0 ? -1 : 0);
        }
        ui_str("name", item_name(db, ids[i]));
        ui_int("count", inv_count(inv, ids[i]));
        ui_int("sel", i == cur_i);
        ui_int("i", i);
        put_icon("icon", menu, db, ids[i]);
        ui_close();
    }
    ui_close();

    // 설명(상태 2): 돌아갈 상태가 0이면 장착한 아이템, 아니면 후보. 상태 0·1에서도 커서가 가리키는 것을 미리 보인다.
    int info = -1;
    if (state == ES_INFO) info = G32(scr + EQ_BACK) == 0 ? item_id_of(db, G32(ch + CH_EQUIP + 4u * (uint32_t)slot)) : cand;
    else if (state == ES_SLOTS) info = item_id_of(db, G32(ch + CH_EQUIP + 4u * (uint32_t)slot));
    else if (cur_i >= 0) info = cand;
    if (info >= n) info = -1;
    ui_int("has_info", info >= 0);
    if (info >= 0) { ui_str("info_name", item_name(db, info)); ui_str("info_desc", item_desc(db, info)); put_icon("info_icon", menu, db, info); }
    else { ui_text("info_name", ""); ui_text("info_desc", ""); ui_text("info_icon", ""); }

    // 메시지 10 "장착불가 Lv%d"(요구 레벨 +0x38)
    char m[64] = "";
    if (msg == EQ_MSG_LV && state == ES_CANDS) snprintf(m, sizeof m, got_str(GOT_EQUIP_LV), (int)G32(scr + EQ_REQ_LV));
    ui_str("msg", m);

    // 키 안내: 상태 0은 종류 4 "0:해제 7:상세설명", 1·2는 종류 2 "CLR:이전"
    // 터치(0x32548의 키): 칸 위아래(상태 0, 캐릭터는 좌우라 문서의 ◀▶ 버튼), 후보 위아래(상태 1).
    if (state == ES_SLOTS && msg != EQ_MSG_LV) ui_nav("slots", slot, SLOTS, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_VERT | UI_NAV_DIRECT);
    if (state == ES_CANDS && msg != EQ_MSG_LV) ui_nav("cands", cur_i, total, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);
    // 틀: 고정 버튼 띠. 보조 [해제 0] [강화 *], 결정은 칸에서 「후보」, 후보에서 「장착」. 상세설명(7)은 설명이 늘 보여 버튼을 두지 않는다(키는 그대로).
    // 패드는 LB(원본 0)가 레일이라 해제를 RS(원본 9, 장비 화면이 안 쓰는 키)로 받는다.
    int pad = ui_input_source() == UI_SRC_PAD;
    if (shell && state == ES_SLOTS && pad) ui_key_alias('9', '0');
    uint32_t cur_rec = G32(ch + CH_EQUIP + 4u * (uint32_t)slot);
    fm_bar_begin();
    fm_bar_ex(0, "해제", cur_rec != 0, '0', eq_unequip, pad ? "9" : "0");
    fm_bar_ex(1, "강화", 1, '*', eq_enhance, "star");
    fm_bar_hint(state == ES_SLOTS ? "←→ 캐릭터" : state == ES_CANDS ? "← 칸" : "");
    if (state == ES_SLOTS) fm_bar_ok("후보", 1);
    else fm_bar_ok("장착", state == ES_CANDS && total > 0 && cur_i >= 0);
    fm_bar_emit(menu);
    keys_begin();
    if (state == ES_SLOTS) { key_hint("◀▶", "캐릭터"); key_hint("▲▼", "선택"); key_hint("ok", "변경"); key_hint("0", "해제"); key_hint("7", "상세설명"); key_hint("star", "강화"); key_hint("clear", "이전"); }
    else if (state == ES_CANDS) { key_hint("▲▼", "선택"); key_hint("ok", "장착"); if (total) key_hint("7", "상세설명"); key_hint("clear", "이전"); }
    else key_hint("clear", "이전");
    keys_end();
    ui_end();
    ui_scratch_end();
}
