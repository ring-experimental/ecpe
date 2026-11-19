// 상점(필드 메뉴 화면 8) 그리기를 RmlUi 문서 ui/shop.rml로 바꾼다. 원본 그리기 0x360b0을 감쌌다.
//
// 원본은 240 기준 120폭 창(가운데 196..316)에 한 번에 한 가지만 보인다: 처음 메뉴, 장비 분류, 4줄 목록 + 고른 품목의 능력치·가격,
// 그리고 목록을 통째로 가리는 개수/확인/부족 창(0x367bc, 머리는 0x36a84). 넓은 화면에서는 목록(가격·보유 수 포함), 고른 품목의 설명과
// 능력치, 파티의 장착 가능 여부(원본 오른쪽 얼굴 칸의 어둡게 표시)를 한 화면에 함께 두고, 개수/확인 창은 목록 위에 뜨는 팝업으로 보인다.
//
// 키와 상태는 원본(0x35d00)이 그대로 처리한다. 원본 그리기는 부수 효과(파티 얼굴 애니메이션 0x5f84의 프레임 진행, 글자색 0x3f14)를
// 지키려고 ui_scratch 안에서 그대로 부르고 화면은 버린다. 판매(하위 화면 3)는 아이템 화면(0x31b18, 영역 fmcore)을 그대로 쓰므로 원본에 맡긴다.
//
// 상점 객체 S = [메뉴+0x44] (생성 0x35c74, 설정 0x35cbc). 필드:
//   +0x04 메뉴, +0x08 메뉴 스프라이트 래퍼(= 메뉴+4), +0x0c 하위 화면(0 처음, 1 장비 분류, 2 구입 목록, 3 판매, 4 코인 구매),
//   +0x1c 종류(1 소비물, 2 장비, 3 코인 상점, 4 코인 구매), +0x20 상점 번호(품목 레코드 +3과 겹치는 비트 마스크),
//   +0x24 처음 커서, +0x28 분류 커서(0 무기류, 1 방어구류, 2 액세사리류), +0x2c 팝업 상태(0 없음, 1 개수, 2 소지금/코인 부족,
//   3 코인 MAX 초과, 4 구입 확인, 5 수량 MAX 초과), +0x30 개수(코인 구매는 코인 수), +0x34 예/아니오 커서,
//   +0x38.. 목록 도우미(0x2fbdc): +0x44 보이는 첫 품목 번호, +0x48 고른 품목 번호(-1 없음), +0x4c 커서 줄(0~3), +0x58 분류 마스크.
//
// 상점 재고: 따로 표가 없다. 아이템 DB(246개)를 처음부터 훑어 `(1 << rec[0]) & 분류 마스크`이고 `rec[3] & 상점 번호`인 것을 차례로 보인다
// (0x36250, 0x2fbdc). 상점 번호는 스크립트 명령 0x9c/0x9d가 정하는 비트 하나다(rec[3] 비트 7 = 첫 마을 … 비트 1 = 후반, 비트 0 = 코인 상점 품목).
// 분류 마스크: 소비물 1, 무기류 2, 방어구류 0x1c(장갑·방패/갑옷/투구), 액세사리류 0x20 (0x35d00의 OK 처리).
//
// 코인 상점(종류 3): 명령 0x9c/0x9d는 종류 3이면 상점 번호를 스크립트 변수 11(VM+0x44 배열 +0x2c)에서 가져온다. 이 값이 0이면
// 재고가 비어 고른 품목이 -1이 되고, 0x36534가 0x240dc(db, -1) = 0을 레코드로 써서 `rec+0xc`(게스트 주소 0xc)를 읽다 죽는다
// (조사 때 주입이 죽은 원인. 원본 결함이고 실제 게임은 변수 11을 세워 두고 연다). 변수 11 = 1(코인 품목 비트)이면 정상으로 열린다.
#include "restored.h"
#include "fm_shell.h"
#include <stdio.h>
#include <string.h>

enum {
    S_MENU = 0x04, S_WRAP = 0x08, S_STATE = 0x0c, S_TYPE = 0x1c, S_SHOP = 0x20, S_TOPCUR = 0x24, S_CATCUR = 0x28,
    S_SUB = 0x2c, S_QTY = 0x30, S_YESNO = 0x34, S_LTOP = 0x44, S_SEL = 0x48, S_CATMASK = 0x58,
    GAME_PARTY = 0x1c0, PARTY_INV = 0xa8, INV_GOLD = 0, INV_COIN = 4, INV_DB = 0x160, PARTY_CHARS = 0x84,
    CHAR_ID = 5, CHAR_EQUIP = 0x140,
    REC_CAT = 0, REC_ICON = 2, REC_SHOP = 3, REC_PRICE = 8, REC_EQMASK = 0xc, REC_S10 = 0x10, REC_S12 = 0x12, REC_S13 = 0x13,
};

// 방어구 능력치 줄 세 번째 라벨. 원본은 메뉴 스프라이트 그림 글자다(프레임 7 P.ATT, 8 M.ATT, 9 P.DEF, 10 M.DEF, 0xb HIT, 0xc AVO를 떼어 보고 확인).
#define ST3_DEF_LABEL "AVO"

void orig_f_360b0(cpu_t *c);

// 원본 메뉴 글(GOT 칸에 든 CP949 문자열 포인터)
static const char *gs(cpu_t *c, uint32_t off) { return gstr(R32(GOT_ADDR(off))); }

static int cmask_of(int cur) { return cur == 0 ? 2 : cur == 1 ? 0x1c : 0x20; }

// 줄 아이콘: 소비물·코인 상점은 병 바탕(0x301f4), 장비는 장비 아이콘(0x301c4). 0x36250과 같은 선택.
static void draw_icon(cpu_t *c, uint32_t S, int type, int icon, int x, int y) {
    uint32_t fn = (type == 1 || type == 3) ? 0x1301f4 : 0x1301c4;
    CALL(fn, S, R32(S + S_WRAP), (uint32_t)x, (uint32_t)y, (uint32_t)icon);
}

// 떼어 낼 그림을 한 번 지운 화면에 칸을 나눠 그린다(칸마다 지우면 화면 전체를 여러 번 채워야 해서).
enum { CELL = 48, COLS = 10, BATCH_ROWS = 6 };
static int cell_x(int i) { return 8 + (i % COLS) * CELL; }
static int cell_y(int i) { return 8 + (i / COLS) * CELL; }

typedef struct { uint32_t rec; int id; } item_t;

// ---- 터치로 고른 줄을 원본 커서에 직접 쓴다(UI_NAV_DIRECT) ----
// 원본은 위/아래 키마다 목록 도우미(0x2fc90)가 보이는 4줄 창을 한 칸씩 민다: +0x44 창 맨 위 품목 번호, +0x48 고른 품목 번호(= S_SEL), +0x4c 창 안 줄(0~3).
// 고른 줄이 늘 창의 (맨 위 + 줄)이고 창은 4줄이 다 찬다. 키로 닿을 수 있는 같은 규칙으로 쓰면 F2 원작 그리기(4줄 창)도 그대로 맞는다.
//   창 안이면 줄만 바꾸고, 아래로 벗어나면 맨 아래 줄(3)에, 위로 벗어나면 맨 위 줄(0)에 둔다(키로 한 칸씩 밀어도 닿는 자리).
enum { S_LROW = 0x4c, WIN4 = 4 };
static void direct_pick(cpu_t *c, uint32_t S, const item_t *items, int n, int t) {
    if (t < 0 || t >= n) return;
    int top = 0, topid = (int)(int32_t)R32(S + S_LTOP);
    for (int i = 0; i < n; i++) if (items[i].id == topid) top = i;
    int row;
    if (t < top) { top = t; row = 0; }
    else if (t >= top + WIN4) { top = t - (WIN4 - 1); row = WIN4 - 1; }
    else row = t - top;
    W32(S + S_LTOP, (uint32_t)items[top].id);
    W32(S + S_SEL, (uint32_t)items[t].id);
    W32(S + S_LROW, (uint32_t)row);
}

static int collect(cpu_t *c, uint32_t db, int cmask, int shop, item_t *out, int max) {
    int n = (int)R32(db), k = 0;
    uint32_t recs = R32(db + 8);
    for (int i = 0; i < n; i++) {
        uint32_t r = recs + 32u * (uint32_t)i;
        if (!((1u << R8(r + REC_CAT)) & (uint32_t)cmask)) continue;
        if (!(R8(r + REC_SHOP) & (uint32_t)shop)) continue;
        if (k < max) out[k] = (item_t){r, i};
        k++;
    }
    return k;
}

static void put_num_fmt(cpu_t *c, const char *key, uint32_t fmt_off, int v) {
    char b[64];
    snprintf(b, sizeof b, gs(c, fmt_off), v);
    ui_str(key, b);
}

// 목록 한 줄(목록과 미리보기가 같이 쓴다). 그림은 칸 번호 cell에 미리 그려 둔 것을 뗀다.
// idx: 전체 목록에서의 번호(터치 nav의 idx. 미리보기는 누를 수 없어 쓰지 않는다)
static void put_row(cpu_t *c, uint32_t db, uint32_t party, item_t it, int x, int y, int money, int idx) {
    ui_item();
    ui_int("i", idx);
    ui_sprite_trim("icon", x - 8, y - 8, 40, 40);
    ui_str("name", gstr(CALL(0x124090, db, (uint32_t)it.id)));
    int price = (int)R32(it.rec + REC_PRICE);
    ui_int("price", price);
    ui_int("owned", (int)CALL(0x11a864, party, (uint32_t)it.id)); // 가방 + 장착 중인 수(0x36534가 "N개"로 보이는 값)
    ui_bool("poor", (unsigned)price > (unsigned)money);
    ui_close();
}

void f_360b0(cpu_t *c) {
    if (!ui_modern()) { orig_f_360b0(c); return; }
    uint32_t S = c->r[0];
    int state = (int)R32(S + S_STATE);
    if (state == 3) { orig_f_360b0(c); return; } // 판매: 아이템 화면(fmcore)이 그린다

    // 터치 직접 쓰기(UI_NAV_DIRECT): 처음 메뉴·분류·구입 목록의 줄을 누르면 번호가 넘어온다. 원본 커서를 쓰고, 누르면 바로 결정이라 OK도 보낸다.
    // 원본 상태를 쓰므로 아래 읽기와 원본 그리기보다 앞에서 한다. 처음·분류의 커서는 S_TOPCUR/S_CATCUR 하나뿐이다(위아래 키로 ±1, 순환 없음).
    int tsub = (int)R32(S + S_SUB);
    int p_top = ui_pick("top"), p_cat = ui_pick("cat"), p_items = ui_pick("items");
    int want_ok = 0;
    if (tsub == 0) {
        if (state == 0 && p_top >= 0 && p_top < 3) { W32(S + S_TOPCUR, (uint32_t)p_top); want_ok = 1; }
        // 처음 화면의 「나가기」 버튼: 목록의 나가기 줄(2)을 고른 것과 같다
        if (state == 0 && ui_pick("sexit") >= 0) { W32(S + S_TOPCUR, 2); want_ok = 1; }
        else if (state == 1 && p_cat >= 0 && p_cat < 3) { W32(S + S_CATCUR, (uint32_t)p_cat); want_ok = 1; }
    }
    // 장비 상점 구입 목록 위의 분류 탭(무기류/방어구류/액세사리류): 보이기만 하고 누를 수 없었다(원본은 CLR로 분류 고르기에 돌아가 다시 골라야 한다).
    // 누르면 원본 키를 같은 틱에 동기로 불러 CLR(분류 고르기) → 커서 쓰기 → OK(그 분류의 목록)까지 한 번에 간다.
    int p_stab = ui_pick("stab");
    if (tsub == 0 && state == 2 && R32(S + S_TYPE) == 2 && p_stab >= 0 && p_stab < 3 && p_stab != (int)R32(S + S_CATCUR)) {
        cpu_t sv = *c;
        uint32_t m0 = R32(S + S_MENU);
        fm_key_raw(m0, FMK_CLR);
        if (R32(S + S_STATE) == 1) { W32(S + S_CATCUR, (uint32_t)p_stab); fm_key_raw(m0, FMK_OK); }
        *c = sv;
        state = (int)R32(S + S_STATE);
        if (state == 3) { orig_f_360b0(c); return; }
    }
    static int ok_pend; // 줄을 옮긴 뒤 아직 못 보낸 OK(대기열이 차 있으면 다음 틱에 다시)
    // 구입 목록의 줄은 아래에서 목록을 만든 뒤 쓴다(목록이 필요하다).
    if (want_ok) ok_pend = 1;

    // 원본을 그대로 돌려 부수 효과만 남기고 화면은 버린다.
    ui_scratch_begin();
    orig_f_360b0(c);
    ui_scratch_end();

    uint32_t menu = R32(S + S_MENU), wrap = R32(S + S_WRAP), game = R32(menu);
    uint32_t party = R32(game + GAME_PARTY), inv = party + PARTY_INV, db = R32(inv + INV_DB);
    int type = (int)R32(S + S_TYPE), sub = (int)R32(S + S_SUB), shop = (int)R32(S + S_SHOP);
    int gold = (int)R32(inv + INV_GOLD), coin = (int)R32(inv + INV_COIN);
    int money = type == 3 ? coin : gold;
    int narrow = ui_screen_w() < 400;
    // 보일 품목 목록: 구입 목록이면 지금 분류, 분류 화면이면 커서가 가리키는 분류의 미리보기, 처음 화면이면 이 상점 재고 전체의 미리보기.
    enum { MAXI = 64 };
    static item_t items[MAXI];
    int cmask = 0, nitems = 0, preview = 0;
    if (state == 2) cmask = (int)R32(S + S_CATMASK);
    else if (state == 1) { cmask = cmask_of((int)R32(S + S_CATCUR)); preview = 1; }
    else if (state == 0 && (type == 1 || type == 2)) { cmask = type == 1 ? 1 : 0x3e; preview = 1; }
    if (cmask) nitems = collect(c, db, cmask, shop, items, MAXI);
    if (nitems > MAXI) nitems = MAXI;

    // 구입 목록의 줄(UI_NAV_DIRECT): 누른 줄을 원본 커서에 쓴다. 줄은 몇 번을 눌러도 고르기만 한다(사용자 결정, 프리미엄 상점과 같다).
    // 구입(개수 창)은 아래 「구입」 버튼이나 OK 키로만 연다. 돈이 드는 동작이 스크롤하다 스친 터치로 시작되지 않게.
    if (state == 2 && tsub == 0 && p_items >= 0 && p_items < nitems) direct_pick(c, S, items, nitems, p_items);
    else if (p_items >= 0) { /* 팝업이 떠 있는 동안 눌린 줄은 버린다 */ }
    if (ok_pend && (tsub != 0 || ui_key_tap(UI_KEY_OK))) ok_pend = 0;
    int sel = (int)(int32_t)R32(S + S_SEL);
    uint32_t selrec = (state == 2 && sel >= 0) ? CALL(0x1240dc, db, (uint32_t)sel) : 0;

    // 목록은 문서에 다 그리고 scroll 상자(shop.rml의 slist)가 넘긴다. 미리보기(처음·분류 화면)는 상자 크기만큼만 보여 앞 13줄만 그린다.
    int cur = -1;
    if (state == 2) for (int i = 0; i < nitems; i++) if (items[i].id == sel) cur = i;
    int vis = preview ? (narrow ? 0 : (nitems < 13 ? nitems : 13)) : nitems;

    // 파티(원본 오른쪽 얼굴 칸 5개, 0x36534)
    uint32_t chars[5]; int nch = 0;
    for (int i = 0; i < 5; i++) { uint32_t ch = R32(party + PARTY_CHARS + 4 * i); if (ch) chars[nch++] = ch; }

    // ---- 그림 떼기 ---- 줄 아이콘은 목록을 만들며 묶음으로(아래), 고른 품목 아이콘과 얼굴은 그 뒤에 한 번 지운 화면에 그린다.
    ui_scratch_begin();

    ui_begin("shop");
    ui_int("narrow", narrow);
    ui_int("type", type);
    ui_int("state", state);
    ui_int("sub", sub);
    ui_int("gold", gold);
    ui_int("coin", coin);
    ui_bool("coins", type == 3 || type == 4);
    put_num_fmt(c, "coin_text", 0x570, coin); // "코인: %dC"(0x36534가 코인 상점에서 소지금 대신 보이는 글)
    // 키 안내: 원본은 처음 화면에서 0x30884 종류 6(OK:확인), 그 밖은 종류 5(OK:확인 CLR:이전)
    ui_bool("key_back", state != 0);
    // 아래 고정 버튼(필드 메뉴 틀의 버튼 띠와 같은 약속): 오른쪽 끝 「이전」(처음 화면에서는 「나가기」), 그 왼쪽 「결정」.
    // 두 칸은 어느 단계·팝업에서도 자리와 크기가 같고, 쓸 수 없으면 사라지지 않고 흐려진다.
    ui_text("ok_t", sub ? "확인" : state == 2 ? "구입" : "선택");
    ui_bool("ok_on", sub != 0 || state != 2 || nitems > 0);
    ui_text("back_t", state == 0 && sub == 0 ? "나가기" : "이전");
    ui_bool("exit", state == 0 && sub == 0);

    // 제목
    if (state == 0) ui_text("title", "상점");
    else if (state == 1) ui_str("title", gs(c, 0x550));
    else if (state == 4) ui_str("title", gs(c, 0x5a0));
    else if (type == 2) ui_str("title", gs(c, 0x55c + 4 * R32(S + S_CATCUR)));
    else ui_str("title", gs(c, 0x550));

    // 처음 메뉴 / 분류
    ui_list("menu");
    if (state == 0) for (int i = 0; i < 3; i++) { ui_item(); ui_str("t", gs(c, 0x550 + 4 * i)); ui_close(); }
    if (state == 1) for (int i = 0; i < 3; i++) { ui_item(); ui_str("t", gs(c, 0x55c + 4 * i)); ui_close(); }
    ui_close();
    ui_int("mcur", state == 0 ? (int)R32(S + S_TOPCUR) : state == 1 ? (int)R32(S + S_CATCUR) : -1);
    // 장비 상점 목록 위 분류 탭
    ui_bool("tabs", state == 2 && type == 2);
    ui_list("tablist");
    for (int i = 0; i < 3; i++) { ui_item(); ui_str("t", gs(c, 0x55c + 4 * i)); ui_close(); }
    ui_close();
    ui_int("tcur", (int)R32(S + S_CATCUR));
    // 게임패드 LB/RB: 분류 탭 이전/다음(문서의 shoulder 요소가 pick "stab"으로 넘긴다)
    ui_int("tprev", ((int)R32(S + S_CATCUR) + 2) % 3);
    ui_int("tnext", ((int)R32(S + S_CATCUR) + 1) % 3);

    // 품목 목록(구입 목록이면 고르는 목록, 아니면 미리보기)
    ui_bool("preview", preview);
    ui_int("total", nitems);
    ui_int("cur", cur);
    ui_int("pos", cur + 1);
    ui_list("items");
    {
        // 줄 아이콘: 한 번에 한 화면 가득(칸 48px) 그려 떼고, 다음 묶음은 화면을 지우고 다시. 줄이 많아도 칸 번호가 겹치지 않는다.
        int cols = (ui_screen_w() - 8) / CELL; if (cols > COLS) cols = COLS; if (cols < 1) cols = 1;
        int per = cols * BATCH_ROWS;
        for (int b0 = 0; b0 < vis; b0 += per) {
            ui_scratch_clear();
            int e = b0 + per < vis ? b0 + per : vis;
            for (int i = b0; i < e; i++) {
                int k = i - b0, icon = (int)R8(items[i].rec + REC_ICON);
                draw_icon(c, S, type, icon, 8 + (k % cols) * CELL, 8 + (k / cols) * CELL);
            }
            for (int i = b0; i < e; i++) { int k = i - b0; put_row(c, db, party, items[i], 8 + (k % cols) * CELL, 8 + (k / cols) * CELL, money, i); }
        }
    }
    ui_close();
    ui_str("unit", type == 3 ? "C" : "G");

    // ---- 터치(키 처리 0x35d00): 처음 메뉴·분류·품목 목록은 위아래 키, 개수는 좌우(문서의 ◀ ▶ 버튼), 구입 확인 예/아니오는 좌우 ----
    // 원본이 순환하지 않으므로 UI_NAV_WRAP은 쓰지 않는다. 메시지 팝업(sub 2·3·5)은 문서 전체가 OK를 받는다(tapall).
    if (sub == 0) {
        if (state == 0) ui_nav("top", (int)R32(S + S_TOPCUR), 3, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);
        else if (state == 1) ui_nav("cat", (int)R32(S + S_CATCUR), 3, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);
        else if (state == 2) ui_nav("items", cur, nitems, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);
    } else if (sub == 4) ui_nav("yn", (int)R32(S + S_YESNO), 2, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_ACT);

    // 고른 품목 아이콘(칸 20)과 파티 얼굴(칸 30..)은 줄 아이콘을 다 뗀 뒤 지운 화면에 그린다.
    ui_scratch_clear();
    if (selrec) draw_icon(c, S, type, (int)R8(selrec + REC_ICON), cell_x(20), cell_y(20));
    // 얼굴: 0x2fe50으로 애니메이션을 고르고 0x5f84(진행 없이 그리기 = r3 1)로 그린다. 래퍼의 애니 상태는 원본 그리기가 남긴 대로 되돌린다.
    uint8_t anim[0x14];
    for (int k = 0; k < 0x14; k++) anim[k] = (uint8_t)R8(wrap + k);
    for (int i = 0; i < nch; i++) {
        CALL(0x12fe50, menu, chars[i]);
        CALL(0x105f84, wrap, (uint32_t)(cell_x(30 + i) + 16), (uint32_t)(cell_y(30 + i) + 40), 1);
    }
    for (int k = 0; k < 0x14; k++) W8(wrap + k, anim[k]);

    // 고른 품목 자세히(0x36250 아래쪽 능력치·가격, 0x36534 "N개", 0x36a84 설명)
    ui_bool("has_sel", selrec != 0);
    if (selrec) {
        ui_sprite_trim("sicon", cell_x(20) - 8, cell_y(20) - 8, 40, 40);
        ui_str("sname", gstr(CALL(0x124090, db, (uint32_t)sel)));
        uint32_t d = CALL(0x1240b0, db, (uint32_t)sel);
        ui_str("sdesc", d ? gstr(d) : "");
        ui_str("price_label", gs(c, 0x568));
        ui_int("sprice", (int)R32(selrec + REC_PRICE));
        int owned = (int)CALL(0x11a864, party, (uint32_t)sel);
        put_num_fmt(c, "sowned", 0x56c, owned);
        // 능력치 줄: 분류 커서 0이면 P.ATT/M.ATT/HIT, 1이면 P.DEF/M.DEF/(프레임 0xc), 2(액세사리)면 없음. 소비물·코인 상점은 커서가 0이라 첫 줄을 보인다.
        int cat = (int)R32(S + S_CATCUR);
        ui_int("statkind", cat <= 1 ? cat + 1 : 0);
        ui_int("st1", (int)R16(selrec + REC_S10));
        ui_int("st2", (int)R8(selrec + REC_S12));
        ui_int("st3", (int)R8(selrec + REC_S13));
        ui_text("st3_label", ST3_DEF_LABEL);
    } else {
        ui_text("sicon", ""); ui_int("sicon_w", 0); ui_int("sicon_h", 0); ui_text("sname", ""); ui_text("sdesc", ""); ui_text("price_label", "");
        ui_int("sprice", 0); ui_text("sowned", ""); ui_int("statkind", 0); ui_text("st3_label", ""); ui_int("st1", 0); ui_int("st2", 0); ui_int("st3", 0);
    }

    // 파티: 장착 가능(원본은 목록 화면에서 rec+0xc & (1 << char[5])가 0이면 그 얼굴을 반투명 검정으로 덮는다)
    // 장비면 그 부위(rec[0]-1)에 지금 장착한 것과 품목 능력치(+0x10, +0x12) 차이를 함께 보인다(원본 장비 화면 0x32f48과 같은 두 값).
    int slot = selrec ? (int)R8(selrec + REC_CAT) - 1 : -1;
    ui_bool("cmp", slot >= 0 && slot <= 3);  // 능력치 차이는 무기(공격)와 방어구 3부위(방어)만. 액세사리는 원본 장비 화면도 비교하지 않는다.
    ui_bool("isequip", slot >= 0 && slot <= 4);
    ui_list("party");
    for (int i = 0; i < nch; i++) {
        uint32_t ch = chars[i];
        ui_item();
        ui_sprite_trim("face", cell_x(30 + i) - 8, cell_y(30 + i) - 8, CELL, CELL);
        ui_str("name", gstr(CALL(0x1252f8, ch)));
        int can = 1;
        if (selrec) can = (R32(selrec + REC_EQMASK) & (1u << R8(ch + CHAR_ID))) != 0;
        ui_bool("can", can);
        uint32_t eq = (slot >= 0 && slot <= 4) ? R32(ch + CHAR_EQUIP + 4 * slot) : 0;
        if (eq) {
            int eid = (int)((eq - R32(db + 8)) / 32);
            ui_str("eq", gstr(CALL(0x124090, db, (uint32_t)eid)));
        } else ui_text("eq", "");
        int d1 = 0, d2 = 0;
        if (selrec && slot >= 0 && slot <= 3) {
            d1 = (int)R16(selrec + REC_S10) - (eq ? (int)R16(eq + REC_S10) : 0);
            d2 = (int)R8(selrec + REC_S12) - (eq ? (int)R8(eq + REC_S12) : 0);
        }
        ui_int("d1", d1);
        ui_int("d2", d2);
        ui_close();
    }
    ui_close();
    ui_text("l1", slot == 0 ? "P.ATT" : "P.DEF");
    ui_text("l2", slot == 0 ? "M.ATT" : "M.DEF");

    // 팝업(0x367bc + 머리 0x36a84). 코인 구매(종류 4)는 머리가 "코인구매"이고 개수는 10씩.
    ui_bool("pop", sub != 0);
    ui_int("qty", (int)R32(S + S_QTY));
    ui_int("yesno", (int)R32(S + S_YESNO));
    if (sub != 0) {
        if (type == 4) {
            ui_str("ptitle", gs(c, 0x5a0));
            ui_str("pqty_label", gs(c, 0x598));
            char b[64];
            snprintf(b, sizeof b, gs(c, 0x59c), (int)R32(S + S_QTY) / 10); // 0x40324(개수, 10)
            ui_str("pcost", b);
        } else {
            ui_str("ptitle", selrec ? gstr(CALL(0x124090, db, (uint32_t)sel)) : "");
            ui_str("pqty_label", gs(c, 0x574));
            ui_text("pcost", "");
        }
        ui_str("pcost_label", gs(c, type == 3 ? 0x57c : 0x578));
        ui_int("ptotal", selrec ? (int)(R32(selrec + REC_PRICE) * R32(S + S_QTY)) : 0);
        ui_str("hold_coin_label", gs(c, 0x580));
        // 구입 뒤 남는 돈(원본에 없는 칸이지만 소지금 - 구매금액으로 계산만 한 값)
        ui_text("after_label", "구입 후");
        ui_int("after", money - (selrec ? (int)(R32(selrec + REC_PRICE) * R32(S + S_QTY)) : 0));
        ui_str("msg", sub == 2 ? gs(c, type == 3 ? 0x588 : 0x584) : sub == 3 ? gs(c, 0x58c) : sub == 4 ? gs(c, 0x590) : sub == 5 ? gs(c, 0x594) : "");
        ui_str("yes", gs(c, 0x48c));
        ui_str("no", gs(c, 0x490));
    } else {
        ui_text("ptitle", ""); ui_text("pqty_label", ""); ui_text("pcost", ""); ui_text("pcost_label", ""); ui_int("ptotal", 0);
        ui_text("hold_coin_label", ""); ui_text("after_label", ""); ui_int("after", 0); ui_text("msg", ""); ui_text("yes", ""); ui_text("no", "");
    }
    ui_end();
    ui_scratch_end();
}
