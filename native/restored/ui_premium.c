// 프리미엄 상점(원작에 없다, 사용자 요청 2026-11-10). 인게임 옵션 목록의 마지막 줄 「프리미엄 상점」에서 들어간다(restored/ui_system.c).
// 문서 ui/premium.rml. 테스트판이라 모든 품목은 무료다. 가격표는 아래 SHOP[].price로 둔다(0이면 「무료」).
//
// 구성: 탭 「상점」(새 아이템 카드 목록 + 고른 품목 자세히 + 수량 ◀ ▶ MAX + 받기), 탭 「보관함」(가진 새 아이템 + 사용),
//       랜덤 상자를 열면 결과 카드(등급 색)를 보인다. 강화석은 받기만 하고 강화는 장비 화면에서만 한다(사용자 결정 2026-11-11).
//       받기·사용은 버튼(「결정」·패널 단추·OK)으로만: 목록 줄은 몇 번 눌러도 고르기만 한다(사용자 결정 2026-11-11).
// 다른 모바일 게임 상점을 따랐다: 품목은 카드(아이콘·이름·가격·보유), 수량은 스테퍼와 MAX, 구매는 확인 창 없이 즉시 + 토스트
//   (무료라 되돌릴 일이 없다), 보유 상한(99)은 상세와 수량에 계속 보인다, 상자는 결과 카드로 한 번 보여 준다.
//
// 키(필드 메뉴의 옵션 화면 키 처리기가 이 파일로 넘긴다; # 은 분배기가 메뉴 전체를 닫는다):
//   ▲▼ 품목, ◀▶ 수량(상점 탭), OK 받기/사용, * 상점↔보관함, CLR 닫기.
// 터치·마우스: 탭·강화 버튼은 pick("prem"), 목록 줄은 pick("pl", 줄 번호), 수량은 key 속성(원본 키), 받기·사용·닫기는 필드 메뉴 틀의
//   고정 버튼 띠(「결정」「이전」, restored/fm_shell.c). 게임패드 LB/RB는 틀의 레일이라 탭은 X(*)로 바꾼다.
//   목록은 원본 커서가 없는 이 파일만의 상태라 ui_nav(원본 키를 한 칸씩 흉내)를 쓰지 않고 누른 줄로 커서를 바로 옮긴다.
//   목록은 scroll="plist" 상자에 다 그린다: 손가락으로 끌기(줄 위에서 시작해도), 휠, 게임패드 오른쪽 스틱으로 넘긴다(호스트).
//   키로 커서를 옮기면 ui_scroll_show로 그 줄만 보이게 넘긴다(터치로 고른 줄은 이미 보이므로 넘기지 않는다).
//   pick idx: 0 상점 탭, 1 보관함 탭, 3 수량 MAX, 4 결과 카드 닫기
#include "restored.h"
#include "items_ext.h"
#include "fm_shell.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// ---- 품목표(가격은 코인 단위가 아니라 표시용. 0이면 무료) ----
static const struct { int id, price; } SHOP[] = {
    {IT_STAT_RESET, 0}, {IT_REVIVE, 0}, {IT_RANDOM_BOX, 0}, {IT_WARP, 0}, {IT_INCENSE_LOW, 0}, {IT_INCENSE_HIGH, 0}, {IT_ENHANCE, 0}, {IT_LEVEL_UP, 0},
};
#define NSHOP ((int)(sizeof SHOP / sizeof SHOP[0]))

enum { K_UP = 0x1f, K_DOWN = 0x1e, K_LEFT = 0x1d, K_RIGHT = 0x1c, K_OK = 0x1b, K_CLR = 0x10, K_2 = 0x52, K_4 = 0x54, K_6 = 0x56, K_8 = 0x58, K_5 = 0x55,
       K_STAR = 0x4a, K_1 = 0x51,
       M_SPR = 0x04 }; // 필드 메뉴 객체의 메뉴 스프라이트 래퍼(아이콘 그리기)

static bool open_;
static int tab;                 // 0 상점, 1 보관함
static int cur[2], qty = 1;
static bool show_cur;           // 다음 문서에서 커서 줄이 보이게 넘긴다(키 이동·탭 전환)
static int card_id, card_n, card_rar; // 랜덤 상자 결과 카드(card_id > 0이면 보인다)
static uint64_t seen;

static uint64_t mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

bool ext_shop_active(void) { return open_ && mono_ms() - seen < 400; } // 메뉴가 #으로 닫히면 그리기가 끊겨 저절로 꺼진다
void ext_shop_open(void) { open_ = true; tab = 0; cur[0] = cur[1] = 0; show_cur = true; qty = 1; card_id = 0; seen = mono_ms(); }
void ext_shop_close(void) { open_ = false; card_id = 0; }

// 보관함에 보이는 번호(보유 > 0인 새 아이템)
static int vault_ids(int *out) {
    int n = 0;
    for (int id = IT_EXT_FIRST; id < (int)ext_item_count(); id++) if (ext_count(id) > 0) out[n++] = id;
    return n;
}
static int list_count(void) { int v[16]; return tab == 0 ? NSHOP : vault_ids(v); }
static int sel_id(void) {
    int v[16], n = vault_ids(v);
    if (tab == 0) return SHOP[cur[0]].id;
    return n ? v[cur[1] < n ? cur[1] : n - 1] : -1;
}
static void clamp(void) {
    int n = list_count();
    if (cur[tab] >= n) cur[tab] = n - 1;
    if (cur[tab] < 0) cur[tab] = 0;
    int id = sel_id(), room = id >= 0 ? IT_MAX_COUNT - ext_count(id) : 0;
    if (qty > room) qty = room;
    if (qty < 1) qty = 1;
}
static void set_tab(int t) { tab = t; qty = 1; clamp(); show_cur = true; }

static void buy(void) {
    int id = sel_id();
    if (id < 0) return;
    int own = ext_count(id);
    if (own >= IT_MAX_COUNT) { ext_toast("이미 99개를 가지고 있습니다"); return; }
    int got = ext_give(id, qty);
    char b[160];
    snprintf(b, sizeof b, "%s ×%d 받음 (보유 %d)", ext_item_name(id), got, own + got);
    ext_toast(b);
    qty = 1;
    clamp();
}
static void use(void) {
    int id = sel_id();
    if (id < 0) return;
    if (!ext_can_use(id)) { // 부활석처럼 지니고만 있는 것은 설명만
        char b[160];
        snprintf(b, sizeof b, "%s: %s", ext_item_name(id), ext_item_desc(id));
        ext_toast(b);
        return;
    }
    if (ext_use(id, EXT_SHOP) && id == IT_RANDOM_BOX) {
        ext_reward_t r = ext_box_last();
        card_id = r.id; card_n = r.n; card_rar = r.rarity;
    }
    clamp();
}

// 이번 틱의 키(0x61d4와 같은 판정)
static int key_now(uint32_t in) { return R32_(&cpu, in + 0xc) && R16_(&cpu, in + 2) == 1 ? (int)R16_(&cpu, in) : 0; }

void ext_shop_input(uint32_t in) {
    int k = key_now(in), pk = ui_pick("prem");
    if (card_id) { // 결과 카드: 아무 확인 키나 누르면 닫는다(카드 뒤 목록을 누른 것은 버린다)
        ui_pick("pl");
        if (pk == 4 || k == K_OK || k == K_5 || k == K_CLR) card_id = 0;
        return;
    }
    if (pk == 0) { set_tab(0); return; }
    if (pk == 1) { set_tab(1); return; }
    if (pk == 3) { qty = IT_MAX_COUNT; clamp(); return; }
    int pl = ui_pick("pl");
    if (pl >= 0 && pl < list_count()) {
        if (pl != cur[tab]) { cur[tab] = pl; qty = 1; clamp(); } // 고르기만(같은 줄을 다시 눌러도 받기·사용은 하지 않는다)
        return;
    }
    bool up = k == K_UP || k == K_2, down = k == K_DOWN || k == K_8, ok = k == K_OK || k == K_5;
    bool left = k == K_LEFT || k == K_4, right = k == K_RIGHT || k == K_6;
    int n = list_count();
    if (k == K_CLR) { ext_shop_close(); return; }
    if (k == K_STAR) { set_tab(!tab); return; }
    if (k == 0x43 && tab == 0) { qty = IT_MAX_COUNT; clamp(); return; } // #: 최대 수량(MAX 버튼과 같다. 버튼은 누르기 전용이라 키보드·게임패드로 닿을 길이 없었다)
    if (up && n) { cur[tab] = (cur[tab] + n - 1) % n; qty = 1; clamp(); show_cur = true; return; }
    if (down && n) { cur[tab] = (cur[tab] + 1) % n; qty = 1; clamp(); show_cur = true; return; }
    if (tab == 0 && left) { qty = qty > 1 ? qty - 1 : 1; return; }
    if (tab == 0 && right) { qty++; clamp(); return; }
    if (ok) { if (tab == 0) buy(); else use(); }
}

// ---- 문서 ----
static int cell_x, cell_y, cell_h;
static void cell(int w, int h, int *x, int *y) {
    int sw = ui_screen_w();
    if (cell_x + w > sw) { cell_x = 0; cell_y += cell_h; cell_h = 0; }
    if (cell_y + h > 320) { ui_scratch_clear(); cell_x = cell_y = cell_h = 0; }
    *x = cell_x; *y = cell_y;
    cell_x += w;
    if (h > cell_h) cell_h = h;
}
static void put_icon(const char *key, uint32_t menu, int id) {
    int x, y;
    cell(20, 24, &x, &y);
    uint32_t db = R32_(&cpu, ext_game() + 0x1cc);
    uint32_t icon = R8_(&cpu, R32_(&cpu, db + 8) + 32u * (uint32_t)id + 2);
    CALL(0x1301f4, 0, menu + M_SPR, (uint32_t)(x + 2), (uint32_t)(y + 2), icon);
    ui_sprite_trim(key, x, y, 20, 24);
}

static int rarity_of(int id) { // 상점 카드 테두리 색(등급): 부활석·성장의 물약 희귀, 강화석·향 고급, 나머지 일반
    return id == IT_REVIVE || id == IT_LEVEL_UP ? 2 : id == IT_RANDOM_BOX || id == IT_ENHANCE || id == IT_INCENSE_LOW || id == IT_INCENSE_HIGH ? 1 : 0;
}

void ext_shop_doc(uint32_t menu) {
    seen = mono_ms();
    clamp();
    int v[16], vn = vault_ids(v), n = tab == 0 ? NSHOP : vn, id = sel_id();
    ui_begin("premium");
    // 필드 메뉴 틀 안(모드 1의 옵션 화면, 상점은 여기서만 열린다): 레일 오른쪽 내용 자리에 두고 받기·사용·닫기는 고정 버튼 띠(아래)
    int shell = ui_modern() && R32_(&cpu, menu + 0x1c) == 1;
    fm_doc_frame(menu);
    ui_scratch_begin();
    cell_x = cell_y = cell_h = 0;
    ui_bool("narrow", ui_screen_w() < 400);
    ui_int("tab", tab);
    ui_int("cur", cur[tab]);
    ui_int("count", n);
    ui_int("vault_n", vn);
    ui_list("items");
    for (int i = 0; i < n; i++) {
        int it = tab == 0 ? SHOP[i].id : v[i];
        ui_item();
        ui_int("i", i);
        ui_text("name", ext_item_name(it));
        ui_int("own", ext_count(it));
        ui_int("rar", rarity_of(it));
        ui_bool("free", tab == 0 && SHOP[i].price == 0);
        ui_int("price", tab == 0 ? SHOP[i].price : 0);
        put_icon("icon", menu, it);
        ui_close();
    }
    ui_close();
    if (id >= 0) {
        int own = ext_count(id), room = IT_MAX_COUNT - own;
        ui_bool("has", true);
        ui_text("sname", ext_item_name(id));
        ui_text("sdesc", ext_item_desc(id));
        ui_int("sown", own);
        ui_int("rar", rarity_of(id));
        put_icon("sicon", menu, id);
        ui_int("qty", qty);
        ui_int("room", room);
        ui_bool("full", room <= 0);
        ui_bool("box", id == IT_RANDOM_BOX);
        // 보관함의 단추: 사용 가능한 것은 「사용」, 나머지(부활석·강화석처럼 지니고만 있는 것)는 「설명 보기」
        ui_text("act", tab == 0 ? "받기" : ext_can_use(id) ? "사용" : "설명 보기");
        ui_int("price", tab == 0 ? SHOP[cur[0]].price : 0);
    } else {
        ui_bool("has", false);
        ui_text("sname", ""); ui_text("sdesc", ""); ui_int("sown", 0); ui_int("qty", 1); ui_int("room", 0);
        ui_bool("full", false); ui_bool("box", false); ui_text("act", ""); ui_int("price", 0); ui_text("sicon", "");
    }
    ui_bool("card", card_id != 0);
    if (card_id) {
        ui_text("card_name", ext_item_name(card_id));
        ui_text("card_desc", ext_item_desc(card_id));
        ui_int("card_n", card_n);
        ui_int("card_rar", card_rar);
        ui_text("card_rar_name", ext_rarity_name(card_rar));
        put_icon("card_icon", menu, card_id);
    }
    ui_scratch_end();
    (void)shell; {
        // 결정: 결과 카드면 「확인」(카드 닫기), 상점 탭 「받기」, 보관함 「사용」/「설명 보기」. 이전(CLR): 카드 → 상점 닫기(옵션 목록).
        fm_bar_begin();
        if (!card_id) fm_bar_hint(tab == 0 ? "↑↓ 품목   ←→ 수량" : "↑↓ 품목");
        if (card_id) fm_bar_ok("확인", 1);
        else if (id < 0) fm_bar_ok(tab == 0 ? "받기" : "사용", 0);
        else fm_bar_ok(tab == 0 ? "받기" : ext_can_use(id) ? "사용" : "설명 보기",
                       !(tab == 0 && IT_MAX_COUNT - ext_count(id) <= 0));
        fm_bar_emit(menu);
    }
    ui_end();
    // 문서가 이미 그 줄을 가지고 있어야 넘길 수 있다(새 탭은 다음 틱에 줄이 생긴다). 못 넘겼으면 다음 틱에 다시.
    if (show_cur && !card_id && n) show_cur = !ui_scroll_show("plist", cur[tab]);
}
