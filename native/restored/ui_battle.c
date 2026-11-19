// 전투 UI를 RmlUi로 다시 그린다(영역 battle). 문서: ui/battle.rml(아래 HUD: 파티 카드, 명령, 공격 하위메뉴, 아이템·조합·스킬·필살 목록, 적 정보),
// ui/battle_msg.rml(전투 메시지), ui/battle_result.rml(결과 화면).
//
// 원작 전투 그리기 0x1bcd4는 맵·캐릭터·효과를 그린 뒤 아래 띠(높이 B+0x1c0 = 48)에 패널을 그리고, 그 위에 이름표·메시지·결과를 얹는다.
// 패널 그리기 함수들은 키 처리 쪽 상태를 바꾸기도 하므로(공격 하위메뉴 0x1ec54가 사용 불가 비트를 매 프레임 다시 계산한다)
// 모두 원본을 그대로 부르되 ui_scratch로 화면만 버린다. 어떤 창이 그려졌는지는 그 함수가 불렸는지로 알고,
// 값은 0x1bcd4가 끝난 뒤 전투 객체에서 직접 읽어 한꺼번에 문서로 넘긴다(한 문서를 여러 래퍼가 나눠 채울 수 없어서).
//
// 아래 띠는 원본 패널이 매 프레임 덮어 그리던 곳이고 맵 그리기(0x3892c)는 0x3900 클립으로 그 위까지만 그린다.
// 패널을 버리면 띠에 지난 화면이 남으므로 0x1bcd4 뒤에 띠를 한 색으로 칠한다(그 위를 HUD가 덮는다).
// 이름표·메시지는 맵 위에 그려지던 것이라 버려도 남는 것이 없다(원본도 지운 자리를 갱신 격자로 다시 그리게 표시하고, 그 표시는 원본을 불러 그대로 한다).
#include "restored.h"
#include <stdio.h>
#include <string.h>

enum {
    FN_FILL = 0x11415c,        // (x, y, w, h, [sp]색 0xBBGGRR00) 기본 gfx 채우기
    FN_ITEM_REC = 0x1240dc,    // (아이템DB, id) -> 레코드(+2 아이콘 번호)
    FN_ITEM_NAME = 0x124090,   // (아이템DB, id) -> 이름
    FN_ITEM_DESC = 0x1240b0,   // (아이템DB, id) -> 설명
    FN_ITEM_ID = 0x124100,     // (아이템DB, 레코드) -> id
    FN_SKILL_REC = 0x123f50,   // (스킬DB, id) -> 레코드(+3 MP, +8 플래그)
    FN_SKILL_NAME = 0x123f78,  // (스킬DB, id) -> 이름
    FN_SKILL_DESC = 0x123f98,  // (스킬DB, id) -> 설명
    FN_ICON = 0x11d650,        // (B, x, y, id, [sp]아이콘) 아이템 칸 + 아이콘
    GOT_CAT = 0x2ec,           // 아이템 분류별 id 끝(5칸: 8,12,19,22,28)
    GOT_EP_COL = 0x2e0,        // 적 정보 막대 색 4개
    GOT_NO_ITEM = 0x2f4, GOT_NO_HOT = 0x2f8, GOT_NO_COMBI = 0x2fc, GOT_NO_SPECIAL = 0x30c, GOT_NO_SKILL = 0x310,
    GOT_SKILL_GET = 0x2bc,     // "%s스킬획득"

    // 게임 객체(B+0)
    G_GFX = 0x08, G_OPT = 0x14c, G_BIG = 0x160,
    GFX_CLIP_W = 0xe4, GFX_CLIP_H = 0xe8,
    // 전투 객체 B
    B_GAME = 0x00, B_P = 0x04, B_E = 0x08, B_FLAGS = 0x28, B_TGT = 0x2c, B_STATE = 0x30, B_PAGE = 0x34,
    B_SKILLDB = 0x50, B_ITEMDB = 0x54, B_SP = 0x58, B_CM = 0x5c, B_EP = 0x60, B_MSG = 0x64, B_NM = 0x120,
    B_TARGETING = 0x174, B_LISTMODE = 0x198, B_COMBI = 0x19c, B_SPECIAL_M = 0x1b4, B_PANEL_H = 0x1c0,
    // 진영
    P_N = 0x0c, P_ATB = 0x1c, P_EXP = 0x30, P_GOLD = 0x34, P_DROPN = 0x38, P_DROP = 0x39, P_MEM = 0x98, P_PARTNER = 0xa4, P_INV = 0xa8,
    E_ENEMY = 0x3d0,
    INV_LEARNED = 0x10, INV_COUNT = 0x50, INV_HOT = 0x150, INV_HOTN = 0x158, INV_DB = 0x160,
    // 멤버 M
    M_ID = 0x05, M_ACT = 0x54, M_FLAGS = 0x58, M_ABIL = 0x5c, M_STATS = 0x64, M_MPMAX = 0xf4, M_MG = 0xf8, M_MGMAX = 0xfc,
    M_QUICK = 0x13d, M_LVUP = 0x154, M_SKILLGET = 0x158, M_SKILLS = 0x160, M_SKILLN = 0x174, M_SPECIALS = 0x178, M_SPECIALN = 0x17c, M_EPON = 0x1a0,
    MF_KO = 0x8000, UF_TARGET = 0x100000,
    // 적 X
    X_FLAGS = 0x04, X_STATS = 0x10, X_X = 0x28, X_Y = 0x2c, X_RES = 0x64, XF_HIDDEN = 0x10000,
    // 명령 메뉴 CM
    CM_CUR = 0x14, CM_SCROLL = 0x1c, CM_SUB = 0x20, CM_LIST = 0x24, CM_SUBCUR = 0x3c, CM_SUBBITS = 0x44, CM_SL = 0x48, CM_LW = 0x94,
    // 목록 창 LW(아이템/조합), SL(스킬/필살)
    LW_MODE = 0x14, LW_SEL = 0x18, LW_FIRST = 0x1c, LW_HAS = 0x24, LW_TAB = 0x3c,
    SL_MODE = 0x14, SL_M = 0x40, SL_CUR = 0x44, SL_SCROLL = 0x48,
    // 메시지 상자(필드 대사 상자와 같은 클래스)
    MSG_TEXT = 0x14, MSG_RECT = 0x4c, MSG_FLAGS = 0x60,
    SC_GRID = 0xb8,
};

// R32 등은 지역 변수 c를 쓴다. 래퍼 밖의 도우미는 전역 cpu를 본다(래퍼의 c도 같은 &cpu다).
void ab_hud(void);   // restored/autobattle.c
static cpu_t *const c = &cpu;
#define GOTP(o) R32(GOT_ADDR((o)))

// 이번 프레임에 원본이 그리려던 것
static struct {
    int in;                 // 0x1bcd4 안
    int sp, sp_names;       // 상태 패널(세 번째 인자 0이면 이름 열)
    int cm, sub, list, skill, ep, result;
    uint32_t ep_m;
} fr;

static inline uint32_t vfn(uint32_t obj, int off) { return R32(R32(obj) + off); }

static void save_regs(uint32_t *r) { memcpy(r, cpu.r, sizeof cpu.r); }
static void load_regs(const uint32_t *r) { memcpy(cpu.r, r, sizeof cpu.r); }

// 원본 그리기를 부르되 화면은 버린다(부수 효과는 남는다).
#define SCRATCH(call) do { ui_scratch_begin(); call; ui_scratch_end(); } while (0)

// ---- 아이템 아이콘: 원본 칸 그리기(0x1d650)를 떼어 낸다 ----
// 문서를 채우는 동안 ui_scratch를 한 번 열어 두고(icons_begin/end), 아이콘마다 겹치지 않는 칸에 그려 그 자리를 뗀다.
// 같은 그림은 호스트가 해시로 한 번만 텍스처로 만든다.
static int icon_slot;
static void icons_begin(void) { ui_scratch_begin(); icon_slot = 0; }
static void icons_end(void) { ui_scratch_end(); }
static void item_icon(uint32_t B, int id) {
    uint32_t db = R32(B + B_ITEMDB);
    uint32_t rec = CALL(FN_ITEM_REC, db, (uint32_t)id);
    int cols = ui_screen_w() / 24, x = 4 + 24 * (icon_slot % cols), y = 4 + 24 * (icon_slot / cols);
    icon_slot++;
    if (!rec || y + 24 > 320) { ui_text("icon", ""); return; }
    CALL(FN_ICON, B, (uint32_t)x, (uint32_t)y, CALL(FN_ITEM_ID, db, rec), (uint32_t)R8(rec + 2));
    ui_sprite_trim("icon", x - 2, y - 2, 22, 22);
}

static int pct(int v, int max) {
    if (max <= 0) return 0;
    if (v <= 0) return 0;
    if (v >= max) return 100;
    return (int)((int64_t)v * 100 / max);
}

static void str_or_empty(const char *key, uint32_t p) { ui_str(key, p ? gstr(p) : ""); }

// ---- 배열 ----
// RmlUi는 data-for 배열이 줄어드는 프레임에 사라질 자식의 값을 먼저 읽어 경고를 낸다(범위 밖 자식이 스칼라로 온다).
// 그래서 배열은 줄지 않게 정해 둔 길이까지 빈 항목(v = 0)으로 채우고, 문서는 v가 0인 항목을 감춘다.
static int lst_n[4], lst_top;
static void L_begin(const char *k) { ui_list(k); lst_n[lst_top++] = 0; }
static void L_item(void) { ui_item(); ui_bool("v", 1); lst_n[lst_top - 1]++; }
static void L_end(int pad, void (*fill)(void)) {
    // 빈 칸도 문서가 스타일 식에 쓰는 수치(위치, 게이지 %)는 채운다(빈 값이면 'width: %;' 구문 오류)
    static const char *const nums[] = {"x", "y", "hp_pct", "mp_pct", "atb_pct", "mg_pct", "h"};
    for (int i = lst_n[lst_top - 1]; i < pad; i++) {
        ui_item(); ui_bool("v", 0);
        ui_int("i", -1);   // 빈 칸은 터치로 고를 수 없다(nav idx)
        for (size_t k = 0; k < sizeof nums / sizeof *nums; k++) ui_int(nums[k], 0);
        if (fill) fill();
        ui_close();
    }
    ui_close();
    lst_top--;
}
static void fill_res(void) {
    ui_list("res");
    for (int k = 0; k < 4; k++) { ui_item(); ui_bool("v", 0); ui_int("h", 0); ui_text("c", "#000000"); ui_close(); }
    ui_close();
}
static void fill_quick(void) { ui_text("key", ""); ui_text("name", ""); }
static void empty_tabs(void) { L_begin("tabs"); L_end(6, NULL); }
enum { PAD_MEM = 3, PAD_ROWS = 32, PAD_ENEMY = 8 };

// ---- 목록 창 ----
// 목록은 문서에 전부 그리고 rows 상자(scroll="blist")가 넘긴다(끌기·휠·오른쪽 스틱, 키로 옮기면 그 줄이 보이게: 호스트). 줄 수가 바뀌어도
// 배열이 줄지 않게 PAD_ROWS까지 빈 항목으로 채운다. 터치로 고른 줄은 UI_NAV_DIRECT로 번호만 넘어오고(ui_pick), 아래에서 원본 커서에 직접 쓴다.
// 직접 쓰기 뒤 UI_NAV_ACT 목록은 누르면 바로 결정이라 OK를 보낸다(원본 커서가 그 줄이 된 다음 틱에: ui_key_tap 대기열이 차면 다시 해 본다).
static int b_ok;           // 아직 못 보낸 OK
static int combi_pick = -1, combi_sent; // 조합: 고르기 상태 전에 눌린 줄(OK로 고르기 상태를 먼저 연 뒤 쓴다)
// 대상 선택 중(원본이 머리 위 화살표를 그리는 동안)에는 문서에서 고를 것이 없다. 방향패드+OK+취소(UI_PAD_DIR)로 한다.
static int targeting_now;

// ---- 문서 채우기 ----
static void emit_members(uint32_t B) {
    uint32_t P = R32(B + B_P), game = R32(B + B_GAME);
    uint32_t atb = R32(P + P_ATB), cur = atb ? atb - M_ACT : 0;
    int turn = R32(game + G_OPT) & 1;
    int n = (int)R32(P + P_N);
    // 대상 표시: 0x1bcd4는 B+0x174가 켜져 있고 유닛 플래그(아군 M+0x58, 적 X+4)에 0x100000이 있으면 노란 화살표를 그린다
    int tgt_on = R32(B + B_TARGETING) != 0;
    L_begin("members");
    for (int i = 0; i < n && i < 8; i++) {
        uint32_t M = R32(P + P_MEM + 4 * i);
        if (!M) continue;
        uint32_t S = R32(M + M_STATS);
        int hp = (int)R32(S), hpmax = (int)R32(S + 4), mp = (int)R32(S + 8);
        int mpmax = (int)R32(M + M_MPMAX), atbmax = (int)R32(S + 0xc), atbv = (int)R32(S + 0x10);
        int mg = (int)R32(M + M_MG), mgmax = (int)R32(M + M_MGMAX);
        L_item();
        str_or_empty("name", CALLP(vfn(M, 0x1c), M));
        ui_int("hp", hp); ui_int("hpmax", hpmax); ui_int("hp_pct", pct(hp, hpmax));
        ui_int("mp", mp); ui_int("mpmax", mpmax); ui_int("mp_pct", pct(mp, mpmax));
        ui_int("atb_pct", pct(atbv, atbmax)); ui_bool("atb_full", atbv >= atbmax);
        ui_int("mg", mg); ui_int("mgmax", mgmax); ui_int("mg_pct", pct(mg, mgmax)); ui_bool("mg_full", mgmax > 0 && mg >= mgmax);
        ui_bool("ko", (R32(M + M_FLAGS) & MF_KO) != 0);
        ui_bool("act", M == cur);
        ui_bool("tgt", tgt_on && (R32(M + M_FLAGS) & UF_TARGET));
        ui_bool("low", hpmax > 0 && hp * 4 <= hpmax);
        ui_close();
    }
    L_end(PAD_MEM, NULL);
    ui_bool("turn", turn);
}

// 아이템 창(LW 모드 1)과 조합 목록(모드 2). 원본 0x1f3d0과 같은 조건으로 항목을 고른다.
// 터치: 아이템 모드는 칸을 좌우(원본 0x1f974)로, 분류 탭은 위아래(0x1f1f8)로, 조합 모드는 줄을 위아래(0x1fb38)로 넘긴다.
// nav 번호는 전체 목록 번호(창 안 번호가 아니다). 아이템 분류 1~5에서 LW+0x18은 줄 번호가 아니라 아이템 id라 줄 번호를 따로 찾는다.
static void emit_item_list(uint32_t B, uint32_t LW) {
    uint32_t P = R32(B + B_P), inv = P + P_INV, db = R32(inv + INV_DB);
    int mode = (int)R32(LW + LW_MODE);
    int ptab = ui_pick("tabs"), prow = ui_pick("rows");
    if (targeting_now) ptab = prow = -1;
    // 분류 탭(원본 0x1f1f8): +0x3c에 쓰고 0x1f8e8이 그 분류의 첫 칸을 고른다. 없는 분류(어둡게 보이는 탭)는 원본도 건너뛴다.
    if (mode == 1 && ptab >= 0 && ptab <= 5 && ptab != (int)R32(LW + LW_TAB) && (ptab == 0 ? R32(LW + LW_HAS) : R32(LW + 0x24 + 4 * ptab))) {
        W32(LW + LW_TAB, (uint32_t)ptab);
        CALL(0x11f8e8, LW);
    }
    int sel = (int)R32(LW + LW_SEL);
    int ids[64], n = 0, sel_row = -1, sel_id = -1;
    int narrow = ui_screen_w() < 400;
    if (mode == 1) {
        int tab = (int)R32(LW + LW_TAB);
        uint32_t cat = GOTP(GOT_CAT);
        int ok = tab != 0 || R32(LW + LW_HAS);
        int any = 0;
        for (int k = 0; k < 4; k++) any |= R32(LW + 0x28 + 4 * k) != 0;
        if (ok && any) {
            if (tab == 0) {
                int hn = (int)R32(inv + INV_HOTN);
                for (int i = 0; i < hn && n < 64; i++) ids[n++] = (int)R8(inv + INV_HOT + i);
                if (sel >= 0 && sel < n) { sel_row = sel; sel_id = ids[sel]; }
            } else {
                int lo = tab == 1 ? 0 : (int)R32(cat + 4 * (tab - 2)), hi = (int)R32(cat + 4 * (tab - 1));
                for (int id = lo; id < hi && n < 64; id++) {
                    if (!R8(inv + INV_COUNT + id)) continue;
                    if (id == sel) { sel_row = n; sel_id = id; }
                    ids[n++] = id;
                }
            }
        }
        // 누른 줄을 원본 커서(LW+0x18: 핫슬롯은 번호, 그 밖은 아이템 id)에 쓰고 결정(OK)까지 보낸다.
        if (prow >= 0 && prow < n) {
            sel = tab == 0 ? prow : ids[prow];
            W32(LW + LW_SEL, (uint32_t)sel);
            sel_row = prow; sel_id = ids[prow];
            b_ok = 1;
        }
        L_begin("rows");
        for (int r = 0; r < n; r++) {
            int id = ids[r];
            L_item();
            ui_int("i", r);
            str_or_empty("name", CALL(FN_ITEM_NAME, db, (uint32_t)id));
            ui_int("n", (int)R8(inv + INV_COUNT + id)); ui_int("cost", -1);
            item_icon(B, id);
            ui_bool("sel", r == sel_row); ui_bool("dis", 0);
            ui_close();
        }
        L_end(PAD_ROWS, NULL);
        // 분류 탭: 0 = 핫슬롯, 1~5 = id 구간. 원본은 아이콘 줄 오른쪽의 ▲▼만 보였고 분류 이름은 게임 데이터에 없다.
        // 이름은 각 구간에 든 아이템의 효과로 붙였다: 0~7 힐링·마나, 8~11 리미트·홀리워터·미라클포션,
        // 12~18 안티~·디커즈, 19~21 거인의사과·소울워터·퓨리포션, 22~27 폭탄·수면제·독침 등 적에게 쓰는 것.
        static const char *const tab_names[6] = {"핫슬롯", "HP·MP", "특수회복", "상태회복", "강화", "공격"};
        static const char *const tab_short[6] = {"핫슬롯", "회복", "특수", "상태", "강화", "공격"}; // 좁은 화면(탭 칸 34dp)용
        int tab_narrow = ui_screen_w() < 400;
        L_begin("tabs");
        for (int t = 0; t <= 5; t++) {
            L_item();
            ui_int("t", t);
            ui_text("name", (tab_narrow ? tab_short : tab_names)[t]);
            ui_bool("sel", t == tab);
            ui_bool("has", t == 0 ? R32(LW + LW_HAS) != 0 : R32(LW + 0x24 + 4 * t) != 0);
            ui_close();
        }
        L_end(6, NULL);
        ui_str("empty", !ok ? gstr(GOTP(GOT_NO_HOT)) : !any ? gstr(GOTP(GOT_NO_ITEM)) : "");
        ui_text("title", "아이템");
        if (sel_id >= 0) {
            str_or_empty("dname", CALL(FN_ITEM_NAME, db, (uint32_t)sel_id));
            str_or_empty("desc", CALL(FN_ITEM_DESC, db, (uint32_t)sel_id));
            ui_int("dcount", (int)R8(inv + INV_COUNT + sel_id));
        } else { ui_text("dname", ""); ui_text("desc", ""); ui_int("dcount", 0); }
        ui_int("dcost", -1);
        if (!targeting_now) {
            // 분류는 위아래, 칸은 좌우(원본 키). 분류는 고르기만 하고 결정은 없다.
            ui_nav("tabs", tab, 6, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_DIRECT);
            if (n) ui_nav("rows", sel_row, n, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_ACT | UI_NAV_DIRECT);
        }
    } else {
        // 조합: 배운 스킬 가운데 조합 플래그(0x400000)이고 지금 멤버와 동료(P+0xa4) 둘 다 쓸 수 있는 것
        uint32_t sdb = R32(B + B_SKILLDB);
        uint32_t atb = R32(P + P_ATB), cm = atb ? atb - M_ACT : 0, partner = R32(P + P_PARTNER);
        int first = (int)R32(LW + LW_FIRST);
        int show_sel = R32(B + B_LISTMODE) == 2;
        if (first >= 0 && sel >= 0) {
            for (int id = 0; id <= 0xff && n < 64; id++) {
                if (!((R8(inv + INV_LEARNED + (id >> 3)) >> (id & 7)) & 1)) continue;
                uint32_t rec = CALL(FN_SKILL_REC, sdb, (uint32_t)id);
                if (!rec) continue;
                uint32_t fl = R32(rec + 8);
                if (!(fl & 0x400000)) continue;
                if (!((fl >> (cm ? R8(cm + M_ID) : 0)) & 1)) continue;
                if (!partner || !((fl >> R8(partner + M_ID)) & 1)) continue;
                if (id == sel) { sel_row = n; sel_id = id; }
                ids[n++] = id;
            }
        }
        // 누른 줄: 고르기 상태(B+0x198 == 2)가 아니면 OK로 먼저 그 상태로 넘기고 그 뒤에 쓴다. 원본 0x1fb38: 고른 줄 LW+0x18과 창 맨 위 LW+0x1c는 skill id,
        // LW+0x20은 창 안 줄(0 또는 1: 아래로 한 칸 가면 줄 1에서 창이 밀린다). 키로 닿는 같은 규칙(맨 위 = 고른 줄 - 줄, 줄 = min(번호, 1))으로 쓴다.
        if (prow >= 0 && prow < n) { combi_pick = prow; combi_sent = 0; }
        if (combi_pick >= n) combi_pick = -1;
        if (combi_pick >= 0) {
            if (!show_sel) { if (!combi_sent) { b_ok = 1; combi_sent = 1; } }
            else {
                int row = combi_pick > 0 ? 1 : 0;
                W32(LW + LW_SEL, (uint32_t)ids[combi_pick]);
                W32(LW + LW_FIRST, (uint32_t)ids[combi_pick - row]);
                W32(LW + 0x20, (uint32_t)row);
                sel_row = combi_pick; sel_id = ids[combi_pick];
                combi_pick = -1; b_ok = 1;
            }
        }
        L_begin("rows");
        for (int r = 0; r < n; r++) {
            uint32_t rec = CALL(FN_SKILL_REC, sdb, (uint32_t)ids[r]);
            L_item();
            ui_int("i", r);
            str_or_empty("name", CALL(FN_SKILL_NAME, sdb, (uint32_t)ids[r]));
            ui_int("n", -1);
            ui_int("cost", rec ? (int)R8(rec + 3) : 0);
            ui_text("icon", "");
            ui_bool("sel", show_sel && r == sel_row); ui_bool("dis", 0);
            ui_close();
        }
        L_end(PAD_ROWS, NULL);
        empty_tabs();
        ui_str("empty", first < 0 ? gstr(GOTP(GOT_NO_COMBI)) : "");
        ui_text("title", "조합기");
        if (sel_id >= 0) {
            uint32_t rec = CALL(FN_SKILL_REC, sdb, (uint32_t)sel_id);
            str_or_empty("dname", CALL(FN_SKILL_NAME, sdb, (uint32_t)sel_id));
            str_or_empty("desc", CALL(FN_SKILL_DESC, sdb, (uint32_t)sel_id));
            ui_int("dcost", rec ? (int)R8(rec + 3) : -1);
        } else { ui_text("dname", ""); ui_text("desc", ""); ui_int("dcost", -1); }
        ui_int("dcount", 0);
        // 고르기 전(B+0x198 != 2)에는 위아래가 듣지 않고 OK가 고르기 상태로 넘어간다. 그래서 다른 줄을 누르면 먼저 OK를 한 번 보내
        // 고르기 상태로 넘기고, 그 뒤 위아래로 그 줄까지 옮긴다(방향 키를 보내면 영영 닿지 않는다). 위아래는 순환하지 않는다(0x1fb38).
        if (!targeting_now && n) ui_nav("rows", sel_row, n, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);
    }
    ui_int("nrows", n);
}

// 스킬(모드 1)·필살(모드 2) 목록. 원본 0x1fe38은 2~3줄만 보이고 SL+0x48부터 그린다. 커서 SL+0x44는 보이는 줄 기준.
// 터치: 줄을 위아래 키(0x1fc84, 끝에서 처음으로 순환)로 옮기고 다시 누르면 OK.
static void emit_skill_list(uint32_t B, uint32_t SL) {
    uint32_t sdb = R32(B + B_SKILLDB), M = R32(SL + SL_M);
    int mode = (int)R32(SL + SL_MODE);
    int n = M ? (int)R32(M + (mode == 1 ? M_SKILLN : M_SPECIALN)) : 0;
    uint32_t ids = M + (mode == 1 ? M_SKILLS : M_SPECIALS);
    int pr = ui_pick("rows");
    if (targeting_now) pr = -1;
    // 누른 줄을 원본 커서에 쓴다: 원본(0x1fc84)은 2줄(큰 화면 3줄) 창에서 SL+0x48 창 맨 위, SL+0x44 창 안 줄을 키마다 한 칸씩 옮긴다.
    // 창 안이면 줄만, 벗어나면 맨 아래(위) 줄에 두는 키로 닿는 자리로 쓴다.
    if (pr >= 0 && pr < n) {
        int vis = R32(R32(B + B_GAME) + G_BIG) ? 3 : 2;
        if (vis > n) vis = n;
        int top = (int)R32(SL + SL_SCROLL);
        if (pr < top) top = pr;
        else if (pr >= top + vis) top = pr - vis + 1;
        W32(SL + SL_SCROLL, (uint32_t)top);
        W32(SL + SL_CUR, (uint32_t)(pr - top));
        b_ok = 1;
    }
    int cur = (int)R32(SL + SL_SCROLL) + (int)R32(SL + SL_CUR);
    int mp = M ? (int)R32(R32(M + M_STATS) + 8) : 0;
    int sel_id = -1;
    if (n > 32) n = 32;
    L_begin("rows");
    for (int i = 0; i < n; i++) {
        int id = (int)R8(ids + i);
        if (id == 0xff) continue;
        uint32_t rec = CALL(FN_SKILL_REC, sdb, (uint32_t)id);
        int cost = rec ? (int)R8(rec + 3) : 0;
        L_item();
        ui_int("i", i);
        str_or_empty("name", CALL(FN_SKILL_NAME, sdb, (uint32_t)id));
        ui_int("n", -1);
        ui_int("cost", mode == 1 ? cost : -1);
        ui_text("icon", "");
        ui_bool("sel", i == cur);
        ui_bool("dis", mode == 1 && mp < cost);
        ui_close();
    }
    L_end(PAD_ROWS, NULL);
    if (cur >= 0 && cur < n) sel_id = (int)R8(ids + cur);
    empty_tabs();
    ui_int("nrows", n);
    ui_str("empty", n ? "" : gstr(GOTP(mode == 1 ? GOT_NO_SKILL : GOT_NO_SPECIAL)));
    ui_text("title", mode == 1 ? "스킬" : "필살기");
    if (sel_id >= 0 && sel_id != 0xff) {
        uint32_t rec = CALL(FN_SKILL_REC, sdb, (uint32_t)sel_id);
        str_or_empty("dname", CALL(FN_SKILL_NAME, sdb, (uint32_t)sel_id));
        str_or_empty("desc", CALL(FN_SKILL_DESC, sdb, (uint32_t)sel_id));
        ui_int("dcost", mode == 1 && rec ? (int)R8(rec + 3) : -1);
    } else { ui_text("dname", ""); ui_text("desc", ""); ui_int("dcost", -1); }
    ui_int("dcount", 0);
    if (!targeting_now && n) ui_nav("rows", cur, n, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);
}

static void emit_enemies(uint32_t B, int with_res) {
    uint32_t E = R32(B + B_E);
    int n = (int)R32(E + P_N);
    uint32_t cols = GOTP(GOT_EP_COL);
    L_begin("enemies");
    for (int i = 0; i < n && i < 16; i++) {
        uint32_t X = R32(E + E_ENEMY + 4 * i);
        if (!X || (R32(X + X_FLAGS) & XF_HIDDEN)) continue;
        uint32_t S = R32(X + X_STATS);
        L_item();
        ui_fmt("letter", "%c", 'A' + i);   // 원본은 0x61+i 프레임(알파벳 기호)
        str_or_empty("name", CALLP(vfn(X, 0x14), X));
        ui_int("hp", (int)R32(S));
        {
            L_begin("res");
            uint32_t R = R32(X + X_RES);
            for (int k = 0; k < 4; k++) {
                int v = (R8(R + 0x21 + k / 2) >> (k & 1 ? 4 : 0)) & 0xf;
                uint32_t col = R32(cols + 4 * k);
                L_item();
                ui_int("h", (13 - v) * 100 / 13);   // 원본 막대 높이 (13-n)/13
                ui_fmt("c", "#%02x%02x%02x", (col >> 8) & 0xff, (col >> 16) & 0xff, (col >> 24) & 0xff);
                ui_close();
            }
            L_end(4, NULL);
        }
        ui_close();
    }
    L_end(PAD_ENEMY, fill_res);
    (void)with_res;
}

// 대상 선택 중인가. B+0x174는 명령을 받는 동안 내내 켜져 있어(명령 메뉴가 떠 있어도) 대상 선택의 표지가 못 된다.
// 대상 선택은 명령 메뉴가 사라지고(fr.cm = 0) 대상 쪽 유닛에 0x100000 플래그가 선 때다(적 X+4, 아군 M+0x58: 상태 패널의 "대상" 표시와 같은 조건).
// 적 대상(B+0x28 비트 0x8/0x10)뿐 아니라 아이템을 아군에게 쓸 때도 그렇다(그때 B+0x28은 0이다).
static int is_targeting(uint32_t B) {
    if (!R32(B + B_TARGETING) || fr.cm) return 0;
    uint32_t P = R32(B + B_P), E = R32(B + B_E);
    for (int i = 0; i < (int)R32(P + P_N) && i < 8; i++) {
        uint32_t M = R32(P + P_MEM + 4 * i);
        if (M && (R32(M + M_FLAGS) & UF_TARGET)) return 1;
    }
    for (int i = 0; i < (int)R32(E + P_N) && i < 16; i++) {
        uint32_t X = R32(E + E_ENEMY + 4 * i);
        if (X && (R32(X + X_FLAGS) & (XF_HIDDEN | UF_TARGET)) == UF_TARGET) return 1;
    }
    return 0;
}

static void emit_hud(uint32_t B) {
    uint32_t game = R32(B + B_GAME), P = R32(B + B_P), CM = R32(B + B_CM);
    uint32_t atb = R32(P + P_ATB), M = atb ? atb - M_ACT : 0;
    int sw = ui_screen_w();
    icons_begin();
    ui_begin("battle");
    targeting_now = is_targeting(B);
    if (b_ok && (fr.list || fr.skill) && ui_key_tap(UI_KEY_OK)) b_ok = 0; // 직접 쓴 커서 위에서 결정(이전 틱에 쓴 것이면 원본 커서는 이미 그 줄)
    // 화면 패드: 대상 선택(원본이 머리 위 화살표를 그리고 방향키로 고른다) 동안만 방향패드+OK+취소. 그 밖에는 누를 것이 문서에 있거나
    // (명령·목록·결과) 입력을 받지 않는 연출이라 패드를 숨긴다. 연출 중에도 이 문서가 떠 있어 자동 판정이면 필드 패드가 나오므로 명시한다.
    // 사용자 결정: 전투 화면에서는 OK·취소를 늘 둔다(대상 고르기 때는 방향도).
    ui_pad(targeting_now ? UI_PAD_DIR : UI_PAD_KEYS);
    ui_bool("narrow", sw < 400);
    // 패배(상태 0x19): 원작은 쓰러진 뒤 화면 전체가 검게 사라진다. HUD도 같이 사라지게 한다.
    ui_bool("defeat", R32(B + B_STATE) == 0x19);
    ui_int("panel_h", (int)R32(B + B_PANEL_H));
    emit_members(B);

    // 명령 메뉴: 원본은 5개 가운데 3줄을 스크롤해 보였다. 여기서는 다 보인다.
    int cm = fr.cm && !fr.list && !fr.skill;
    ui_bool("cmd", cm);
    ui_int("cur", (int)R32(CM + CM_CUR));
    ui_bool("combi_ok", R32(B + B_COMBI) != 0);
    ui_bool("sub", cm && fr.sub);
    ui_int("subcur", (int)R32(CM + CM_SUBCUR));
    uint32_t bits = R32(CM + CM_SUBBITS);
    ui_bool("sub_skill_dis", (bits & 2) != 0);
    ui_bool("sub_sp_dis", (bits & 4) != 0);
    // 터치: 명령 5개는 위아래 키로(원본 0x1e594, 끝에서 순환), 공격 하위메뉴 3칸은 좌우 키로(0x1eb40, 순환 없음: 위아래는 ±2).
    if (cm && !targeting_now) {
        if (fr.sub) ui_nav("sub", (int)R32(CM + CM_SUBCUR), 3, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_ACT);
        else ui_nav("cmd", (int)R32(CM + CM_CUR), 5, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_WRAP);
    }

    // 목록
    int list = 0;
    if (fr.skill) {
        uint32_t SL = CM + CM_SL;
        list = R32(SL + SL_MODE) == 1 ? 3 : 4;
        emit_skill_list(B, SL);
    } else if (fr.list) {
        uint32_t LW = CM + CM_LW;
        list = R32(LW + LW_MODE) == 2 ? 2 : 1;
        emit_item_list(B, LW);
    } else {
        // 문서가 읽는 키는 늘 같은 모양으로 채운다(없는 배열을 읽으면 경고)
        L_begin("rows"); L_end(PAD_ROWS, NULL); empty_tabs();
        ui_int("nrows", 0); ui_str("empty", ""); ui_text("title", ""); ui_text("dname", ""); ui_text("desc", "");
        ui_int("dcost", -1); ui_int("dcount", 0);
    }
    ui_int("list", list);

    // 적 정보 패널: 원본 조건(능력 비트 29 && '1' 토글)일 때만. 원본은 목록 창이 같은 띠를 덮어 그려 목록이 떠 있으면 안 보였다.
    // 하위메뉴가 떠 있는 동안은 감춘다(원본도 이때 패널 내용을 비웠다. 512 폭에서도 하위메뉴 오른쪽과 겹친다).
    int ep = fr.ep && !fr.list && !fr.skill && !fr.sub;
    ui_bool("ep", ep);
    if (ep) emit_enemies(B, 1);
    else { L_begin("enemies"); L_end(PAD_ENEMY, fill_res); }

    // 숫자 키 안내(명령 메뉴가 떠 있을 때). 단축 스킬 3/6/9는 등록된 것만.
    ui_bool("ep_key", M && (R32(M + M_ABIL) & 0x20000000));
    L_begin("quick");
    if (M) {
        uint32_t sdb = R32(B + B_SKILLDB);
        for (int k = 0; k < 3; k++) {
            int id = (int)R8(M + M_QUICK + k);
            if (id == 0xff) continue;
            L_item();
            ui_fmt("key", "%d", 3 + 3 * k);   // 문서가 key 속성과 키 이름표로 쓰는 글
            str_or_empty("name", CALL(FN_SKILL_NAME, sdb, (uint32_t)id));
            ui_close();
        }
    }
    L_end(3, fill_quick);

    ui_bool("targeting", targeting_now);
    (void)game;
    ui_end();
    icons_end();
}

static char msg_text[512];
static int msg_on;

static void emit_msg(void) {
    ui_begin("battle_msg");
    ui_bool("narrow", ui_screen_w() < 400); // 좁은 화면은 위에 숫자 키 안내가 있어 그 아래로 내린다
    L_begin("lines");
    const char *s = msg_text;
    while (*s) {
        const char *e = strchr(s, '\n');
        int len = e ? (int)(e - s) : (int)strlen(s);
        while (len && (s[len - 1] == '\r')) len--;
        L_item();
        ui_strn("t", s, len);
        ui_close();
        if (!e) break;
        s = e + 1;
    }
    L_end(6, NULL);
    ui_end();
}

static void emit_result(uint32_t B) {
    uint32_t P = R32(B + B_P);
    int page = (int)R32(B + B_PAGE);
    icons_begin();
    ui_begin("battle_result");
    ui_pad(UI_PAD_KEYS);   // 화면 전체를 눌러도 넘어가지만(tapall) 전투 화면 내내 OK·취소를 둔다.
    ui_int("page", page);
    ui_int("gold", (int)R32(P + P_GOLD));
    int exp = (int)R32(P + P_EXP);
    int n = (int)R32(P + P_N);
    L_begin("members");
    for (int i = 0; i < n && i < 8; i++) {
        uint32_t M = R32(P + P_MEM + 4 * i);
        if (!M) continue;
        int ko = (R32(M + M_FLAGS) & MF_KO) != 0;
        L_item();
        str_or_empty("name", CALLP(vfn(M, 0x1c), M));
        ui_int("exp", ko ? 0 : exp);
        ui_bool("ko", ko);
        ui_bool("lvup", R32(M + M_LVUP) != 0);
        ui_close();
    }
    L_end(PAD_MEM, NULL);
    // 쪽 1: 스킬 획득(원본은 앞 3명만 본다)
    L_begin("skills");
    for (int i = 0; i < 3 && i < n; i++) {
        uint32_t M = R32(P + P_MEM + 4 * i);
        if (!M || !R32(M + M_SKILLGET)) continue;
        uint32_t name = CALLP(vfn(M, 0x1c), M);
        char b[96];
        snprintf(b, sizeof b, gstr(GOTP(GOT_SKILL_GET)), name ? gstr(name) : "");
        L_item(); ui_str("t", b); ui_close();
    }
    L_end(3, NULL);
    // 쪽 2: 드롭
    L_begin("drops");
    int dn = (int)R8(P + P_DROPN);
    uint32_t db = R32(B + B_ITEMDB);
    for (int i = 0; i < dn && i < 16; i++) {
        int id = (int)R8(P + P_DROP + i);
        if (!CALL(FN_ITEM_REC, db, (uint32_t)id)) continue;
        L_item();
        str_or_empty("name", CALL(FN_ITEM_NAME, db, (uint32_t)id));
        item_icon(B, id);
        ui_close();
    }
    L_end(8, NULL);
    ui_end();
    icons_end();
}

// ---- 감싼 함수 ----

// 0x1bcd4(B, gfx): 전투 그리기 전체
void orig_f_1bcd4(cpu_t *c);
void f_1bcd4(cpu_t *c) {
    if (!ui_modern()) { orig_f_1bcd4(c); return; }
    uint32_t B = c->r[0];
    memset(&fr, 0, sizeof fr);
    msg_on = 0;
    fr.in = 1;
    orig_f_1bcd4(c);
    fr.in = 0;
    uint32_t regs[16];
    save_regs(regs);
    uint32_t game = R32(B + B_GAME), gfx = R32(game + G_GFX);
    if (fr.sp || fr.cm || fr.ep) {
        // 원본 패널이 덮던 아래 띠를 칠한다(HUD 바탕과 같은 짙은 남색)
        int w = (int)R32(gfx + GFX_CLIP_W), h = (int)R32(gfx + GFX_CLIP_H), ph = (int)R32(B + B_PANEL_H);
        CALL(FN_FILL, 0, (uint32_t)(h - ph), (uint32_t)w, (uint32_t)ph, 0x2a120c00);
        emit_hud(B);
    }
    if (!fr.list && !fr.skill) { b_ok = 0; combi_pick = -1; } // 목록이 닫혔으면 못 보낸 OK와 눌린 줄은 버린다
    if (msg_on) emit_msg();
    if (fr.result) emit_result(B);
    ab_hud();   // AUTO·x2 버튼(restored/autobattle.c)
    load_regs(regs);
}

#define WRAP_SCRATCH(off, body)                              \
    void orig_f_##off(cpu_t *c);                             \
    void f_##off(cpu_t *c) {                                 \
        if (!ui_modern() || !fr.in) { orig_f_##off(c); return; } \
        body;                                                \
        SCRATCH(orig_f_##off(c));                            \
    }

// 상태 패널 0x1e0d8(SP, gfx, 이름 생략)
WRAP_SCRATCH(1e0d8, { fr.sp = 1; fr.sp_names = c->r[2] == 0; })
// 명령 메뉴 0x1e894(CM, gfx): 안에서 목록 창이나 하위메뉴를 부른다
WRAP_SCRATCH(1e894, { fr.cm = 1; })
// 공격 하위메뉴 0x1ec54(CM+0x28, gfx)
WRAP_SCRATCH(1ec54, { fr.sub = 1; })
// 아이템/조합 창 0x1f3d0(LW, gfx)
WRAP_SCRATCH(1f3d0, { fr.list = 1; })
// 스킬/필살 목록 0x1fe38(SL, gfx)
WRAP_SCRATCH(1fe38, { fr.skill = 1; })
// 적 정보 패널 0x1ee3c(EP, gfx, M)
WRAP_SCRATCH(1ee3c, { fr.ep = 1; fr.ep_m = c->r[2]; })
// '7' 적 이름표 0x1dfec는 감싸지 않는다(사용자 결정 2026-11-10: 원작과 똑같이). 원본이 맵 위에 상자와 이름을 그리고
// 그 자리를 갱신 격자에 표시한다. 새 UI의 이름표 문서와 대상 이름 띠, 터치의 이름 버튼은 없앴다.
// 결과 화면 0x1c1ac(B)
WRAP_SCRATCH(1c1ac, { fr.result = 1; })

// 0x23120(MSG, gfx): 대사 상자 클래스의 그리기 슬롯. 전투 밖(필드 대사)은 원본 그대로 둔다(그 상자는 영역 dialog의 0x23578).
// 전투 안에서는 0x23578을 부르지 않고 같은 결과만 낸다: 보이면(+0x60 bit0) 글을 받아 두고 자리를 갱신 격자에 표시한 뒤 0을 돌려준다.
// 0x23578은 글 커서(+0x1c)와 글자색을 되돌려 놓는 순수 그리기라 부르지 않아도 상태가 같다.
void orig_f_23120(cpu_t *c);
void f_23120(cpu_t *c) {
    if (!ui_modern() || !fr.in) { orig_f_23120(c); return; }
    uint32_t msg = c->r[0];
    uint32_t regs[16];
    save_regs(regs);
    uint32_t ret = 1;
    if (R32(msg + MSG_FLAGS) & 1) {
        uint32_t p = R32(msg + MSG_TEXT);
        const char *s = p ? gstr(p) : "";
        if (*s == 8) s++;
        snprintf(msg_text, sizeof msg_text, "%s", s);
        msg_on = msg_text[0] != 0;
        uint32_t B = R32(R32(msg + 4) + 0x1c8), grid = R32(B + SC_GRID);
        CALLP(vfn(grid, 0xc), grid, msg + MSG_RECT);
        ret = 0;
    }
    load_regs(regs);
    c->r[0] = ret;
}
