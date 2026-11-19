// 필드 메뉴의 캐릭터 화면 넷을 RmlUi로 다시 그린다(영역 fmchar).
//   화면 2 스킬 0x33d94, 화면 4 스테이터스 0x3312c, 화면 5 퀘스트 0x338b8, 화면 6 전투파티 0x34cb8.
// 입력과 상태 전이는 원본(각 화면의 키 함수)이 그대로 맡고, 여기서는 그리기 함수만 감싼다.
//
// 원본 그리기는 바꾼 값(스킬 화면의 선택 레코드 +0x60, 메시지 타이머 +0x34, 전투파티 화살표 위상 +0x30)을
// 그리면서 갱신하므로, 원본을 ui_scratch 안에서 그대로 부르고 화면만 버린다. 값은 그 뒤 구조체에서 읽는다.
// 원본 메뉴 창은 120×146px 한 칸에 모든 것을 우겨 넣었는데, 넓은 화면에서는 캐릭터 정보와 목록, 설명을 나란히 둔다.
// 주소는 파일 오프셋, 게스트 함수는 +0x100000.
#include "restored.h"
#include "enhance.h"
#include "fm_shell.h"

enum {
    GOT = 0x14e624,             // PIC 코드의 sl. 메모리 위의 GOT 값은 이미 게스트 주소다
    APP_VAR = 0x14ee34,         // 앱 객체 포인터를 가진 전역
    APP_GAME = 0x48,
    GAME_PARTY = 0x1c0, GAME_IDB = 0x1cc, GAME_SDB = 0x1d0, GAME_MENU = 0x1d4,
    MENU_SHEET = 0x14,          // 메뉴 UI 스프라이트(리소스 5)
    PARTY_CHARS = 0x84,         // 캐릭터 포인터 5칸
    PARTY_QUESTS = 0x3c,        // 퀘스트 8칸 × 8바이트, +0 u16 글 번호
    PARTY_COMBO_BITS = 0xb8,    // 익힌 조합 스킬 비트열(32바이트)
    SCR_MENU = 0x04, SCR_SLOT = 0x0c, SCR_STATE = 0x10,

    // 캐릭터
    CH_ID = 0x04, CH_BIT = 0x05, CH_SEAL = 0x58, CH_STATS = 0x64, CH_CLASS = 0xb4,
    CH_HP = 0xcc, CH_HPMAX = 0xd0, CH_MP = 0xd4, CH_MPMAX = 0xf4, CH_EXP = 0x104, CH_EXPNEXT = 0x108,
    CH_STR = 0x118, CH_DEX = 0x11a, CH_INT = 0x11c, CH_HITBONUS = 0x135,
    CH_HOTKEY = 0x13d,          // 단축키 3/6/9에 단 스킬 id(0xff 없음)
    CH_EQUIP = 0x140,           // 장착 아이템 레코드 포인터 5칸(무기, 방어구 3칸, 장신구)
    CH_SKILLS = 0x160, CH_NSKILLS = 0x174, CH_SPECIALS = 0x178, CH_NSPECIALS = 0x17c,
    CH_GP = 0x194,
    GOT_SLOT_NAMES = 0x4a0,     // 부위 이름 12바이트 × 5(무기류, 장갑/방패류, 갑옷류, 투구류, 악세사리류)
    ST_LV = 0x18,               // 능력치 블록(+0x64)의 레벨 바이트
    SEALED = 0x8000,            // 캐릭터 상태: 스킬 봉인

    // 게스트 함수
    FN_CHAR_NAME = 0x1252f8,    // (캐릭터) -> char*. 사용자 이름이 없으면 직업 기본 이름
    FN_FACE_FRAME = 0x112fe8,   // (game, 캐릭터 id, 0) -> 메뉴 스프라이트의 얼굴 프레임
    FN_DRAW_FRAME = 0x1056ec,
    FN_ENAME = 0x12fe7c,   // (시트, 프레임, x, y, dx, dy, 뒤집기) 합성 프레임
    FN_SKILL_REC = 0x123f50, FN_SKILL_NAME = 0x123f78, FN_SKILL_DESC = 0x123f98,
    FN_ITEM_ID = 0x124100, FN_ITEM_NAME = 0x124090,
    FN_QUEST_TEXT = 0x10d344,   // (game, 글 번호) -> char*
};

static uint32_t game_of(cpu_t *c, uint32_t scr) { return R32(R32(scr + SCR_MENU)); }
static uint32_t party_of(cpu_t *c, uint32_t game) { return R32(game + GAME_PARTY); }
static uint32_t member(cpu_t *c, uint32_t game, int i) {
    return (i < 0 || i > 4) ? 0 : R32(party_of(c, game) + PARTY_CHARS + 4 * i);
}
static const char *got_str(cpu_t *c, int off) { return gstr(R32(GOT + off)); }
static int pct(int v, int max) { return max <= 0 ? 0 : v <= 0 ? 0 : v >= max ? 100 : v * 100 / max; }

// 캐릭터 얼굴. 원본은 24×23 칸에 잘라 보이지만(0x300a0), 여기서는 합성 프레임 전체를 떼어 크게 쓴다.
// 반드시 ui_scratch 안에서 부른다.
static void face_sprite(cpu_t *c, const char *key, uint32_t game, uint32_t ch) {
    uint32_t sheet = R32(R32(game + GAME_MENU) + MENU_SHEET);
    ui_scratch_clear();
    if (ch && sheet) {
        uint32_t frame = CALL(FN_FACE_FRAME, game, R8(ch + CH_ID), 0);
        CALL(FN_DRAW_FRAME, sheet, frame, 120, 120, 0, 0, 0);
    }
    ui_sprite_trim(key, 40, 40, 160, 160);
}

// 영문 이름 그림("ARC" 등). 원본 스킬 머리(0x34a08)와 전투파티 선택 줄(0x308a4)이 0x2fe7c로 그린다.
// 0x2fe7c(menu, 캐릭터, x, y, 오른쪽맞춤)는 게임 상태(+0x36)에 따라 이름을 안 그리거나 다른 프레임을 쓰므로 그대로 부른다.
static void ename_sprite(cpu_t *c, const char *key, uint32_t game, uint32_t ch) {
    ui_scratch_clear();
    if (ch) CALL(FN_ENAME, R32(game + GAME_MENU), ch, 40, 40, 0);
    ui_sprite_trim(key, 20, 20, 120, 60);
}

// 캐릭터 한 명의 공통 값(이름, 레벨, HP/MP). 메인 메뉴 카드(0x30a38)와 스킬 머리(0x34a08)가 보이는 것과 같다.
// 최대 MP는 능력치 블록이 아니라 캐릭터 +0xf4에 있다(원본도 그렇게 읽는다).
static void char_vals(cpu_t *c, uint32_t ch) {
    if (!ch) {
        ui_str("name", ""); ui_int("lv", 0);
        ui_int("hp", 0); ui_int("hpmax", 0); ui_int("mp", 0); ui_int("mpmax", 0);
        ui_int("hp_pct", 0); ui_int("mp_pct", 0); ui_bool("sealed", false);
        return;
    }
    uint32_t st = R32(ch + CH_STATS);
    int hp = (int)R32(st + 0), hpmax = (int)R32(st + 4), mp = (int)R32(st + 8), mpmax = (int)R32(ch + CH_MPMAX);
    ui_str("name", gstr(CALL(FN_CHAR_NAME, ch)));
    ui_int("lv", (int)R8(st + ST_LV));
    ui_int("hp", hp); ui_int("hpmax", hpmax); ui_int("mp", mp); ui_int("mpmax", mpmax);
    ui_int("hp_pct", pct(hp, hpmax)); ui_int("mp_pct", pct(mp, mpmax));
    ui_bool("sealed", (R32(ch + CH_SEAL) & SEALED) != 0);
}

static int party_count(cpu_t *c, uint32_t game) {
    int n = 0;
    while (n < 5 && member(c, game, n)) n++;
    return n;
}

// ---------------------------------------------------------------- 화면 4: 스테이터스 0x3312c
// 객체 [menu+0x34]: +0xc 캐릭터 칸, +0x10 GP 분배 커서(0 STR, 1 DEX, 2 INT, -1 없음),
// +0x1c 레벨업 알림 글(전투 뒤 0x1cf38이 넣는다), +0x20 알림 상자 모양 값.
enum { ST_NOTE = 0x1c, ST_NOTE_STYLE = 0x20 };

static int cap999(int v) { return v > 999 ? 999 : v; }

void orig_f_3312c(cpu_t *c);
void f_3312c(cpu_t *c) {
    if (!ui_modern()) { // 원작 그리기: 그리는 동안만 장착 레코드에 칸 강화를 얹는다(restored/enhance.c)
        uint32_t s0 = c->r[0];
        enh_patch_begin(member(c, game_of(c, s0), (int)R32(s0 + SCR_SLOT)), 0, -1);
        orig_f_3312c(c);
        enh_patch_end();
        return;
    }
    uint32_t scr = c->r[0], gfx = c->r[1];
    uint32_t game = game_of(c, scr);
    int slot = (int)R32(scr + SCR_SLOT);
    uint32_t ch = member(c, game, slot);
    // 원본은 부수 효과가 없지만, 같은 틀(원본을 부르고 화면을 버린다)로 맞춘다. 레벨업 알림은 아래에서 따로 띄우므로 잠시 비운다.
    uint32_t note = R32(scr + ST_NOTE);
    W32(scr + ST_NOTE, 0);
    ui_scratch_begin();
    orig_f_3312c(c);
    ui_scratch_end();
    W32(scr + ST_NOTE, note);
    if (!ch) return;
    // 터치로 고른 능력치 줄(DIRECT): 원본 키(0x32ff8)가 위아래로 바꾸는 필드는 커서 SCR_STATE(0~2, GP가 없으면 -1) 하나뿐이다.
    // 레벨업 알림이 떠 있으면 알림이 키를 가로채므로 줄 선택은 버린다. 옮긴 뒤 같은 틱 안에서 OK(1점 분배)를 보낸다.
    // 필드 메뉴 틀 안에서는 GP 분배를 되돌릴 수 없으므로(초기화는 유료 아이템) 줄을 눌러서는 고르기만 하고, 줄의 ▲(pick "gpup")나
    // 버튼 띠의 「GP 분배」로만 올린다(사용자 결정 2026-11-11: 한 번 누름으로 실수하지 않게).
    int bpk = ui_pick("base"), upk = ui_pick("gpup"), shell = fm_shell_on();
    bool can_gp = R32(ch + CH_GP) > 0 && (int)R32(scr + SCR_STATE) >= 0 && !(note && R8(note));
    if (shell && upk >= 0 && upk < 3 && can_gp) {
        W32(scr + SCR_STATE, (uint32_t)upk);
        cpu_t sv = *c;
        fm_key_raw(R32(scr + SCR_MENU), 0x1b); // OK: 1점 분배
        *c = sv;
    } else if (bpk >= 0 && bpk < 3 && can_gp) {
        W32(scr + SCR_STATE, (uint32_t)bpk);
        if (!shell) ui_key_tap(UI_KEY_OK);
    }

    ui_begin("fmchar_status");
    fm_doc_frame(R32(scr + SCR_MENU));
    ui_int("narrow", ui_screen_w() < 400);
    ui_scratch_begin();
    face_sprite(c, "face", game, ch);
    ui_scratch_end();
    char_vals(c, ch);
    int n = party_count(c, game);
    ui_int("idx", slot + 1); ui_int("count", n);
    int gp = (int)R32(ch + CH_GP), cur = (int)R32(scr + SCR_STATE);
    ui_int("gp", gp); ui_int("cursor", cur);
    int exp = (int)R32(ch + CH_EXP), next = (int)R32(ch + CH_EXPNEXT);
    ui_int("exp", exp); ui_int("expnext", next); ui_int("exp_pct", pct(exp, next));

    // 기본 능력치(표시값). GP 1점은 기본값 x10(+0x10c..)에 10을 더하고 이것을 다시 계산한다(키 함수 0x32ff8).
    static const char *base_names[3] = {"STR", "DEX", "INT"};
    int s_str = (int)R16(ch + CH_STR), s_dex = (int)R16(ch + CH_DEX), s_int = (int)R16(ch + CH_INT);
    int base[3] = {s_str, s_dex, s_int};
    ui_list("base");
    for (int i = 0; i < 3; i++) { ui_item(); ui_text("label", base_names[i]); ui_int("val", base[i]); ui_int("i", i); ui_close(); }
    ui_close();
    // 터치(키 0x32ff8): GP가 남았으면 위아래로 능력치를 고르고 OK로 1점 분배(고른 줄을 다시 누르면 OK).
    // 레벨업 알림이 떠 있는 동안은 알림이 키를 가로채므로(위아래 무시) 알리지 않는다. 알림은 OK/CLR로 닫힌다.
    if (gp > 0 && cur >= 0 && !(note && R8(note))) ui_nav("base", cur, 3, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);

    // 파생 능력치: 원본 0x3312c가 그리면서 계산하는 식을 그대로 옮겼다(값은 999에서 자른다. HIT/AVO는 자르지 않는다).
    uint32_t cls = R32(ch + CH_CLASS), wpn = R32(ch + CH_EQUIP);
    int t = s_str * (int)R8(cls + 0xc) + s_dex * (int)R8(cls + 0xd) + s_int * (int)R8(cls + 0xe);
    int ebp = 0, ebm = 0; // 칸 강화 보너스(restored/enhance.c)
    if (wpn) enh_slot_bonus(ch, 0, wpn, &ebp, &ebm);
    int patt = (t + 0x9f6) * (int)R8(cls + 0x16) / 2560 + (wpn ? (int)R16(wpn + 0x10) + ebp : 0);
    int matt = (t + 0x9f6) * (int)R8(cls + 0x17) / 2560 + (wpn ? (int)R8(wpn + 0x12) + ebm : 0);
    int pdef = t * 12 / 1000 + (int)R8(cls + 0x18), mdef = t * 12 / 1000 + (int)R8(cls + 0x19);
    for (int k = 1; k <= 3; k++) {
        uint32_t a = R32(ch + CH_EQUIP + 4 * k);
        if (a) { enh_slot_bonus(ch, k, a, &ebp, &ebm); pdef += (int)R16(a + 0x10) + ebp; mdef += (int)R8(a + 0x12) + ebm; }
    }
    int hit = s_dex * ((int)R8(ch + CH_HITBONUS) + 100) * (int)R8(cls + 0x15) / 100 / 100;
    int avo = s_dex * (int)R8(cls + 0x14) / 10;
    static const char *der_names[6] = {"P.ATT", "M.ATT", "P.DEF", "M.DEF", "HIT", "AVO"};
    int der[6] = {cap999(patt), cap999(matt), cap999(pdef), cap999(mdef), hit, avo};
    ui_list("derived");
    for (int i = 0; i < 6; i++) { ui_item(); ui_text("label", der_names[i]); ui_int("val", der[i]); ui_close(); }
    ui_close();
    gp = (int)R32(ch + CH_GP); cur = (int)R32(scr + SCR_STATE);
    fm_bar_begin();
    fm_bar_hint("←→ 캐릭터");
    fm_bar_ok("GP 분배", gp > 0 && cur >= 0 && !(note && R8(note)));
    fm_bar_emit(R32(scr + SCR_MENU));
    ui_end();

    // 레벨업 알림: 원본과 같은 순서로 반투명 막 + 메시지 상자(vt 0x4da48, 그리기 0x23578)를 띄운다.
    // 상자 그리기는 영역 dialog가 감싸므로 그 문서가 이 문서 위에 온다.
    if (note && R8(note)) {
        static uint32_t box;
        if (!box) box = heap_alloc(0x80);
        CALL(0x1046c8, 0, 0, 0);
        CALL(0x123150, box);
        CALL(0x12320c, box, game, 0);
        CALL(0x12323c, box, 0, 0, 0x20, 1, R32(scr + ST_NOTE_STYLE));
        CALL(0x123298, box, note, 1);
        CALL(R32(R32(box) + 4), box);
        extern int ui_dialog_levelup_call; // ui_dialog.c: 이 호출을 레벨업 알림으로 알아보게 한다
        ui_dialog_levelup_call = 1;
        CALL(R32(R32(box) + 0xc), box, gfx);
        ui_dialog_levelup_call = 0;
        CALL(0x1231d0, box);
    }
}

// ---------------------------------------------------------------- 화면 5: 퀘스트 0x338b8
// 객체 [menu+0x38]: +0x1c 퀘스트 수(진입 때 0x337f0이 빈칸 아닌 칸을 센다), +0x20 보는 칸, +0x24 그 칸의 글.
// 키(0x33834)는 좌우로 칸을 넘긴다(양끝 순환). 칸 번호는 앞에서부터 차 있다고 보고 그대로 쓴다(원본과 같다).
enum { QU_COUNT = 0x1c, QU_CUR = 0x20, QU_TEXT = 0x24, GOT_QUEST = 0x4b4, GOT_NO_QUEST = 0x4b8 };

void orig_f_338b8(cpu_t *c);
void f_338b8(cpu_t *c) {
    if (!ui_modern()) { orig_f_338b8(c); return; }
    uint32_t scr = c->r[0], game = game_of(c, scr);
    // 터치로 고른 줄(DIRECT): 원본 키(0x33834)가 좌우로 바꾸는 필드는 보는 칸 +0x20과 그 칸의 글 +0x24(0xd344)뿐이다.
    // 줄 번호는 칸 번호 그대로다(수는 비지 않은 칸 수지만 인덱스는 앞에서부터 찼다고 본다. 원본과 같다).
    // CALL이 레지스터(r0 = 화면, r1 = gfx)를 덮으므로 원본 그리기를 부르기 전에 되돌린다.
    cpu_t saved = *c;
    {
        int pk = ui_pick("quests"), cnt = (int)R32(scr + QU_COUNT);
        if (pk >= 0 && pk < cnt && pk < 8) {
            uint32_t qid = R16(party_of(c, game) + PARTY_QUESTS + 8 * pk), qt = CALL(FN_QUEST_TEXT, game, qid);
            W32(scr + QU_CUR, (uint32_t)pk);
            W32(scr + QU_TEXT, qt);
        }
    }
    *c = saved;
    ui_scratch_begin();
    orig_f_338b8(c);
    ui_scratch_end();

    int n = (int)R32(scr + QU_COUNT), cur = (int)R32(scr + QU_CUR);
    uint32_t text = R32(scr + QU_TEXT), pd = party_of(c, game);
    ui_begin("fmchar_quest");
    fm_doc_frame(R32(scr + SCR_MENU));
    ui_int("narrow", ui_screen_w() < 400);
    ui_str("title", got_str(c, GOT_QUEST));
    ui_str("none", got_str(c, GOT_NO_QUEST));
    ui_int("count", n); ui_int("cursor", cur);
    ui_int("page", n > 0 ? cur + 1 : 0); // 원본은 퀘스트가 없어도 0/0을 보인다
    ui_str("text", n > 0 && text ? gstr(text) : "");
    ui_list("items");
    for (int i = 0; i < n && i < 8; i++) {
        int id = (int)R16(pd + PARTY_QUESTS + 8 * i);
        ui_item();
        ui_int("no", i + 1); ui_int("i", i);
        ui_str("text", id ? gstr(CALL(FN_QUEST_TEXT, game, id)) : "");
        ui_close();
    }
    ui_close();
    // 터치(키 0x33834): 좌우로 퀘스트를 넘긴다(양끝 순환). 줄을 누르면 그 퀘스트로. 고르기만 하므로 ACT는 없다(OK는 나가기).
    if (n > 0) ui_nav("quests", cur, n < 8 ? n : 8, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_WRAP | UI_NAV_DIRECT);
    // 퀘스트는 보기만 한다(원본 OK도 고르는 일이 없다). 「결정」은 흐린 채 자리만 지킨다.
    fm_bar_begin();
    if (n > 1) fm_bar_hint("↑↓ 퀘스트");
    fm_bar_ok("결정", 0);
    fm_bar_emit(R32(scr + SCR_MENU));
    ui_end();
}

// ---------------------------------------------------------------- 화면 6: 전투파티(전투동료설정) 0x34cb8
// 객체 [menu+0x3c]: +0x20 아래 줄 커서(파티 칸), +0x28 파티 인원(진입 때 셈), +0x2c 다 골랐음,
// +0x30 화살표 흔들림(그리기가 매 프레임 뒤집는다), +0x34/+0x38/+0x3c 고른 동료 3칸(0 = EMPTY).
// OK는 커서의 동료를 첫 빈 칸에 넣고, 다 고른 뒤 OK면 그 순서로 파티를 다시 짠다(0x34e24). CLR은 마지막 칸을 비운다.
// 원본은 들어올 때(0x34e84) 세 칸을 늘 비우므로 지금 편성이 보이지 않는다(사용자 보고: 칸이 늘 EMPTY). 0x34e24가 고른 셋을
// 파티 앞 세 칸에 두므로 지금 전투 동료는 파티 칸 0~2다. 아직 하나도 고르지 않았으면 빈 칸 대신 그 자리의 지금 동료를 흐리게
// 보이고(cur), 첫 OK부터는 원본대로 고른 것만 보인다.
enum { PT_CURSOR = 0x20, PT_COUNT = 0x28, PT_DONE = 0x2c, PT_PICKS = 0x34, GOT_PARTY_TITLE = 0x4f0 };

void orig_f_34cb8(cpu_t *c);
void f_34cb8(cpu_t *c) {
    if (!ui_modern()) { orig_f_34cb8(c); return; }
    uint32_t scr = c->r[0], game = game_of(c, scr);
    // 터치로 고른 동료(DIRECT): 원본 키(0x34bbc → 0x34ebc)가 좌우로 바꾸는 필드는 커서 +0x20(파티 칸) 하나뿐이다.
    // 다 고른 뒤(+0x2c)에는 원본이 좌우를 받지 않으므로 버린다. 옮긴 뒤 같은 틱 안에서 OK(첫 빈 칸에 넣기)를 보낸다.
    {
        int pk = ui_pick("roster"), cnt = (int)R32(scr + PT_COUNT);
        if (pk >= 0 && pk < cnt && pk < 5 && !R32(scr + PT_DONE)) {
            W32(scr + PT_CURSOR, (uint32_t)pk);
            ui_key_tap(UI_KEY_OK);
        }
    }
    ui_scratch_begin();
    orig_f_34cb8(c);
    ui_scratch_end();

    int n = (int)R32(scr + PT_COUNT), cur = (int)R32(scr + PT_CURSOR), done = R32(scr + PT_DONE) != 0;
    uint32_t picks[3];
    int npick = 0;
    for (int k = 0; k < 3; k++) { picks[k] = R32(scr + PT_PICKS + 4 * k); if (picks[k]) npick++; }
    bool show_cur = npick == 0 && !done;
    ui_begin("fmchar_party");
    fm_doc_frame(R32(scr + SCR_MENU));
    ui_bool("show_cur", show_cur);
    ui_int("narrow", ui_screen_w() < 400);
    ui_str("title", got_str(c, GOT_PARTY_TITLE));
    ui_int("count", n); ui_int("cursor", cur); ui_bool("done", done);
    ui_int("need", n < 3 ? n : 3); ui_int("picked", npick);
    ui_scratch_begin();
    ui_list("slots");
    for (int k = 0; k < 3; k++) {
        uint32_t ch = picks[k], cur_ch = show_cur && k < n ? member(c, game, k) : 0;
        if (!ch) ch = cur_ch;
        ui_item();
        ui_int("no", k + 1);
        ui_bool("empty", ch == 0);
        ui_bool("cur", cur_ch != 0);
        char_vals(c, ch);
        face_sprite(c, "face", game, ch);
        ename_sprite(c, "ename", game, ch);
        ui_close();
    }
    ui_close();
    ui_list("roster");
    for (int i = 0; i < n && i < 5; i++) {
        uint32_t ch = member(c, game, i);
        int order = 0;
        for (int k = 0; k < 3; k++) if (ch && picks[k] == ch) order = k + 1;
        if (show_cur && i < 3) order = i + 1; // 지금 편성의 순번(흐린 표)
        ui_item();
        ui_int("order", order); ui_int("i", i);
        char_vals(c, ch);
        face_sprite(c, "face", game, ch);
        ename_sprite(c, "ename", game, ch);
        ui_close();
    }
    ui_close();
    ui_scratch_end();
    // 터치(키 0x34bbc): 다 고르기 전에는 좌우로 동료를 넘기고(순환) 다시 누르면 OK(첫 빈 칸에 넣기).
    // 다 고른 뒤의 OK/CLR(확정/되돌리기)은 안내줄 버튼이 맡는다.
    if (!done && n > 0) ui_nav("roster", cur, n < 5 ? n : 5, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_WRAP | UI_NAV_ACT | UI_NAV_DIRECT);
    fm_bar_begin();
    if (!done && n > 1) fm_bar_hint("←→ 동료");
    fm_bar_ok(done ? "확인" : "고르기", n > 0);
    fm_bar_emit(R32(scr + SCR_MENU));
    ui_end();
}

// ---------------------------------------------------------------- 화면 2: 스킬 0x33d94
// 객체 [menu+0x2c](0x339e8): +0xc 캐릭터 칸, +0x10 상태(0 분류, 1 일반 동작 고르기, 2 목록, 3 보기 전용 목록,
// 4 사용 대상, 5 단축키 장착 대기, 6 스킬 정보), +0x1c 동작 버튼(0 사용, 1 장착, 2 정보), +0x20 목록 맨 위
// (일반·필살은 목록 칸, 조합은 스킬 id, -1 빈 목록), +0x24 선택(같은 방식), +0x28 커서 줄, +0x2c 분류(0 일반, 1 조합, 2 필살기),
// +0x30 메시지(10 사용불가/MP부족, 11 단축키 안내), +0x34 메시지 타이머, +0x38/+0x3c/+0x40 대상 고르기(맨 위 칸, 고른 칸, 줄),
// +0x60 선택 스킬 레코드(목록을 그리면서 원본이 갱신한다).
enum {
    SK_ACT = 0x1c, SK_TOP = 0x20, SK_SEL = 0x24, SK_ROW = 0x28, SK_CAT = 0x2c, SK_MSG = 0x30,
    SK_TTOP = 0x38, SK_TSEL = 0x3c, SK_TROW = 0x40, SK_REC = 0x60,
    REC_MP = 3, REC_FLAGS = 8,
    F_ALL = 0x800, F_COMBO = 0x400000,      // 전체 대상, 조합 스킬(하위 바이트 = 조합 멤버 비트)
    GOT_CAT_NORMAL = 0x4d0, GOT_CAT_COMBO = 0x4cc, GOT_CAT_SPECIAL = 0x4c8,
    GOT_ACTIONS = 0x4d4,                    // "사용" "장착" "정보" 5바이트 간격
    GOT_HK1 = 0x4d8, GOT_HK2 = 0x4dc,       // "3,6,9 단축키" "사용가능"
    GOT_CANT = 0x460, GOT_NOMP = 0x4e8,     // "사용불가" "MP부족"
    MSG_CANT = 10, MSG_HOTKEY = 11,
};
static const int hotkey_digit[3] = {3, 6, 9};

// 분류 이름. 원본 글은 뒤에 " x"가 붙어 개수가 이어진다("일반 스킬 x1"). 개수는 따로 넘기므로 떼어 낸다.
static void cat_name(cpu_t *c, const char *key, int off) {
    const char *s = got_str(c, off);
    int n = (int)strlen(s);
    if (n >= 2 && s[n - 1] == 'x' && s[n - 2] == ' ') n -= 2;
    ui_strn(key, s, n);
}

static bool combo_known(cpu_t *c, uint32_t game, uint32_t sdb, uint32_t ch, int id) {
    uint32_t bits = party_of(c, game) + PARTY_COMBO_BITS;
    if (!(R8(bits + (id >> 3)) >> (id & 7) & 1)) return false;
    uint32_t rec = CALL(FN_SKILL_REC, sdb, id);
    if (!rec) return false;
    uint32_t f = R32(rec + REC_FLAGS);
    return (f & F_COMBO) && (f >> R8(ch + CH_BIT) & 1);
}

// 분류의 스킬 id 목록(원본 0x3403c와 같은 차례). 반환은 개수.
static int skill_ids(cpu_t *c, uint32_t game, uint32_t sdb, uint32_t ch, int cat, int *ids) {
    int n = 0;
    if (cat == 1) {
        for (int id = 0; id < 256; id++) if (combo_known(c, game, sdb, ch, id)) ids[n++] = id;
    } else {
        uint32_t list = ch + (cat == 0 ? CH_SKILLS : CH_SPECIALS);
        int cnt = (int)R32(ch + (cat == 0 ? CH_NSKILLS : CH_NSPECIALS)), max = cat == 0 ? 20 : 4;
        for (int i = 0; i < cnt && i < max; i++) {
            int id = (int)R8(list + i);
            if (id == 0xff) break;
            ids[n++] = id;
        }
    }
    return n;
}

static void skill_info(cpu_t *c, uint32_t sdb, int id) {
    uint32_t rec = id >= 0 ? CALL(FN_SKILL_REC, sdb, id) : 0;
    ui_obj("info");
    ui_bool("ok", rec != 0);
    if (rec) {
        uint32_t f = R32(rec + REC_FLAGS);
        ui_str("name", gstr(CALL(FN_SKILL_NAME, sdb, id)));
        ui_str("desc", gstr(CALL(FN_SKILL_DESC, sdb, id)));
        ui_int("mp", (int)R8(rec + REC_MP));
        ui_bool("field", (f >> 28) == 1);   // 필드에서 쓸 수 있는 스킬(회복 등)
        ui_bool("all", (f & F_ALL) != 0);
        ui_bool("combo", (f & F_COMBO) != 0);
    } else {
        ui_str("name", ""); ui_str("desc", ""); ui_int("mp", 0);
        ui_bool("field", false); ui_bool("all", false); ui_bool("combo", false);
    }
    ui_close();
}

// 터치로 고른 줄(DIRECT)을 원본 커서에 직접 쓴다. 원본 키(0x33a30)가 상태마다 바꾸는 필드를 같은 규칙으로 맞춘다.
//   상태 0 분류: 위아래는 +0x2c(0~2)만 바꾼다(OK가 목록 시작 0x3478c를 부른다).
//   상태 1 동작: 좌우는 +0x1c(0~2)만 바꾼다.
//   상태 2 목록(0x34874): 선택 +0x24, 맨 위 +0x20, 커서 줄 +0x28을 함께 바꾼다. 불변식은 줄 = 선택 - 맨 위, 0 <= 줄 < 창(일반 4, 필살 5, 조합 5)이고
//     선택이 창 밖으로 나갈 때만 맨 위가 최소한으로 움직인다. 일반·필살은 칸 번호, 조합은 스킬 id로 가리킨다(위 skill_ids의 차례 그대로).
//   상태 4 대상(0x30dc0, 도우미 +0x38 맨 위 / +0x3c 선택 / +0x40 줄): 창 4, 같은 규칙.
// 옮긴 뒤 OK는 호스트가 아니라 여기서 보낸다(ACT 목록).
static void skill_picks(cpu_t *c, uint32_t scr, uint32_t game) {
    int state = (int)R32(scr + SCR_STATE);
    int pc = ui_pick("cats"), pa = ui_pick("acts"), pi = ui_pick("items"), pt = ui_pick("targets"), ptab = ui_pick("skcat");
    uint32_t ch = member(c, game, (int)R32(scr + SCR_SLOT)), sdb = R32(game + GAME_SDB);
    if (!ch) return;
    // 틀: 분류 탭은 늘 눌린다(분류 고르기 상태로 갔다가 목록으로 돌아온다)
    if (ptab >= 0 && ptab < 3 && fm_shell_on()) {
        if ((int)R32(scr + SK_CAT) != ptab || state != 2) fm_skill_set_cat(R32(scr + SCR_MENU), ptab);
        return;
    }
    if (state == 0 && pc >= 0 && pc < 3) {
        W32(scr + SK_CAT, (uint32_t)pc);
        ui_key_tap(UI_KEY_OK);
    } else if (state == 1 && R32(scr + SK_CAT) == 0 && pa >= 0 && pa < 3) {
        W32(scr + SK_ACT, (uint32_t)pa);
        ui_key_tap(UI_KEY_OK);
    } else if (state == 2 && pi >= 0) {
        int cat = (int)R32(scr + SK_CAT), ids[256];
        int n = skill_ids(c, game, sdb, ch, cat, ids);
        if (pi >= n || (int)R32(scr + SK_TOP) < 0) return;
        int top = (int)R32(scr + SK_TOP), ti = 0, win = cat == 0 ? 4 : 5;
        for (int i = 0; i < n; i++) if ((cat == 1 ? ids[i] : i) == top) ti = i;
        if (pi < ti) ti = pi;
        else if (pi > ti + win - 1) ti = pi - win + 1;
        W32(scr + SK_TOP, (uint32_t)(cat == 1 ? ids[ti] : ti));
        W32(scr + SK_SEL, (uint32_t)(cat == 1 ? ids[pi] : pi));
        W32(scr + SK_ROW, (uint32_t)(pi - ti));
        ui_key_tap(UI_KEY_OK);
    } else if (state == 4 && pt >= 0 && pt < party_count(c, game)) {
        int top = (int)R32(scr + SK_TTOP);
        if (pt < top) top = pt;
        else if (pt > top + 3) top = pt - 3;
        W32(scr + SK_TTOP, (uint32_t)top);
        W32(scr + SK_TSEL, (uint32_t)pt);
        W32(scr + SK_TROW, (uint32_t)(pt - top));
        ui_key_tap(UI_KEY_OK);
    }
}

void orig_f_33d94(cpu_t *c);
void f_33d94(cpu_t *c) {
    if (!ui_modern()) { orig_f_33d94(c); return; }
    uint32_t scr = c->r[0], game = game_of(c, scr);
    // 메시지는 이번 그리기가 보이는 값(그리기가 타이머를 올리고 다 되면 지운다)
    int msg = (int)R32(scr + SK_MSG);
    // 터치로 고른 줄(DIRECT)은 원본이 그리기 전에 커서에 쓴다(그리기가 선택 스킬 +0x60을 갱신하므로).
    // 조합 목록을 읽을 때 CALL이 레지스터(r0 = 화면, r1 = gfx)를 덮으므로 되돌린다.
    // 필드 메뉴 틀: 분류·동작 고르기 단계를 화면에서 없앤다. 그리기 앞에서 목록 상태(동작 = 사용)로 몰아 둔다(restored/fm_shell.c).
    { cpu_t saved = *c; fm_skill_normalize(R32(scr + SCR_MENU)); skill_picks(c, scr, game); *c = saved; }
    ui_scratch_begin();
    orig_f_33d94(c);
    ui_scratch_end();

    int slot = (int)R32(scr + SCR_SLOT), state = (int)R32(scr + SCR_STATE);
    int act = (int)R32(scr + SK_ACT), cat = (int)R32(scr + SK_CAT);
    int top = (int)R32(scr + SK_TOP), sel = (int)R32(scr + SK_SEL);
    uint32_t ch = member(c, game, slot), sdb = R32(game + GAME_SDB);
    if (!ch) return;
    bool narrow = ui_screen_w() < 400;

    ui_begin("fmchar_skill");
    fm_doc_frame(R32(scr + SCR_MENU));
    ui_int("narrow", narrow);
    ui_int("state", state); ui_int("act", act); ui_int("cat", cat);
    ui_int("count", party_count(c, game));
    ui_scratch_begin();
    face_sprite(c, "face", game, ch);
    ename_sprite(c, "ename", game, ch);
    ui_scratch_end();
    char_vals(c, ch);

    // 분류 세 줄(원본 상태 0 0x33ef0: "일반 스킬 x1" 꼴)
    int counts[3] = {(int)R32(ch + CH_NSKILLS), 0, (int)R32(ch + CH_NSPECIALS)};
    int ids[256], tmp[256];
    counts[1] = skill_ids(c, game, sdb, ch, 1, tmp);
    static const int cat_got[3] = {GOT_CAT_NORMAL, GOT_CAT_COMBO, GOT_CAT_SPECIAL};
    ui_list("cats");
    for (int k = 0; k < 3; k++) { ui_item(); cat_name(c, "name", cat_got[k]); ui_int("n", counts[k]); ui_close(); }
    ui_close();
    cat_name(c, "catname", cat_got[cat < 0 || cat > 2 ? 0 : cat]);

    // 목록. 원본은 4~5줄 창만 그리지만 여기서는 전체를 scroll="slist" 상자에 그린다(끌기·휠·오른쪽 스틱은 호스트, 키로 옮긴 줄은 호스트가 보이게 넘긴다).
    int n = skill_ids(c, game, sdb, ch, cat, ids);
    int selpos = -1;
    for (int i = 0; i < n; i++) {
        int key = cat == 1 ? ids[i] : i; // 조합은 id로, 나머지는 칸 번호로 가리킨다
        if (key == sel) selpos = i;
    }
    bool listing = state >= 1 && state != 4;   // 상태 0은 분류만 고르는 중이라 선택 줄이 없다
    ui_int("n", n);
    ui_list("items");
    for (int i = 0; i < n; i++) {
        bool ok = true;
        int id = ids[i];
        uint32_t rec = ok ? CALL(FN_SKILL_REC, sdb, id) : 0;
        uint32_t f = rec ? R32(rec + REC_FLAGS) : 0;
        int hk = 0;
        for (int k = 0; ok && k < 3; k++) if ((int)R8(ch + CH_HOTKEY + k) == id) hk = hotkey_digit[k];
        ui_item();
        ui_bool("ok", ok);
        ui_int("i", i);   // 터치: 목록 전체 기준 번호
        ui_str("name", ok ? gstr(CALL(FN_SKILL_NAME, sdb, id)) : "");
        ui_int("mp", rec ? (int)R8(rec + REC_MP) : 0);
        ui_int("hk", hk);
        // 원본: 일반 분류에서 '사용' 동작일 때 필드에서 못 쓰는 스킬은 붉은 글(0x9263ff00)
        ui_bool("unusable", ok && cat == 0 && act == 0 && (f >> 28) != 1);
        ui_bool("sel", ok && listing && i == selpos);
        // 상태 2·5에서만 원본이 선택 줄을 강조한다. 1·3·6에서는 포커스가 다른 곳이라 옅게 보인다.
        ui_bool("focus", state == 2 || state == 5);
        ui_close();
    }
    ui_close();

    // 동작 버튼(일반 분류만): 사용/장착/정보. 봉인이면 사용이 회색. 상태 3이면 원본이 강조하지 않는다.
    ui_list("acts");
    for (int k = 0; k < 3; k++) {
        ui_item();
        ui_str("name", got_str(c, GOT_ACTIONS) + 5 * k);
        ui_bool("sel", k == act && state != 3);
        ui_bool("dis", k == 0 && (R32(ch + CH_SEAL) & SEALED));
        ui_close();
    }
    ui_close();

    // 단축키 3/6/9(원본은 목록 줄 앞의 동그라미 숫자로만 보인다)
    ui_list("hotkeys");
    for (int k = 0; k < 3; k++) {
        int id = (int)R8(ch + CH_HOTKEY + k);
        ui_item();
        ui_int("key", hotkey_digit[k]);
        ui_str("name", id != 0xff ? gstr(CALL(FN_SKILL_NAME, sdb, id)) : "");
        ui_close();
    }
    ui_close();

    // 정보 창: 상태 6은 원본 0x33dfc와 같은 +0x60 레코드, 목록 상태에서는 선택한 스킬
    int info_id = -1;
    if (state == 6 || state == 4) {
        uint32_t rec = R32(scr + SK_REC);
        if (rec) info_id = (int)R8(rec);
    } else if (listing && selpos >= 0) info_id = ids[selpos];
    skill_info(c, sdb, info_id);

    // 메시지
    ui_int("msg", msg == MSG_CANT || msg == MSG_HOTKEY ? msg : 0);
    ui_str("msg_text", msg == MSG_CANT ? got_str(c, state == 4 ? GOT_NOMP : GOT_CANT) : "");
    ui_str("msg_hk1", got_str(c, GOT_HK1)); ui_str("msg_hk2", got_str(c, GOT_HK2));

    // 상태 4: 대상 고르기(원본 0x34528). 원본은 3줄씩 넘기지만 파티 5명을 모두 보인다.
    int npt = party_count(c, game), tsel = (int)R32(scr + SK_TSEL);
    uint32_t rec = R32(scr + SK_REC);
    bool all = rec && (R32(rec + REC_FLAGS) & F_ALL);
    ui_bool("t_all", all);
    ui_int("tsel", tsel);
    // 대상 줄도 다섯 칸 고정(위 목록과 같은 까닭). 상태 4가 아니면 모두 빈 칸.
    ui_list("targets");
    ui_scratch_begin();
    for (int i = 0; i < 5; i++) {
        uint32_t m = state == 4 && i < npt ? member(c, game, i) : 0;
        ui_item();
        ui_bool("ok", m != 0);
        ui_int("i", m ? (all ? tsel : i) : -1);   // 터치: 전체 대상이면 어느 줄을 눌러도 이미 고른 줄이라 곧바로 OK
        char_vals(c, m);
        face_sprite(c, "face", game, m);
        ename_sprite(c, "ename", game, m);
        ui_bool("sel", m && (all || i == tsel));
        ui_close();
    }
    ui_scratch_end();
    ui_close();

    // 터치: 상태마다 원본 키(0x33a30)가 받는 목록 하나를 알린다. 상태 1은 동작 좌우(일반 분류만 거친다),
    // 3은 보기 전용(좌우 캐릭터뿐), 5는 3/6/9 외에는 받지 않고, 6은 아무 키나 닫는다(문서가 OK로 받는다).
    if (state == 0) ui_nav("cats", cat, 3, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_WRAP | UI_NAV_ACT | UI_NAV_DIRECT);
    else if (state == 1 && cat == 0) ui_nav("acts", act, 3, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_ACT | UI_NAV_DIRECT);
    else if (state == 2 && n > 0 && selpos >= 0) ui_nav("items", selpos, n, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_VERT | UI_NAV_DIRECT);
    else if (state == 4 && npt > 0) ui_nav("targets", tsel, npt, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_DIRECT);
    // 틀: 고정 버튼 띠. 보조 [단축키](목록 → 3/6/9 대기까지 한 번에), 결정은 「사용」(대상 고르기에서도), 단축키 대기에서는 「완료」.
    bool sealed = (R32(ch + CH_SEAL) & SEALED) != 0;
    fm_bar_begin();
    fm_bar_ex(0, "단축키", state == 2 && cat == 0 && n > 0 && !msg, '*', fm_skill_hotkey, "star"); // 키는 틀이 받는다(ext_shell_key)
    fm_bar_hint(state == 2 && !msg ? "←→ 분류" : "");
    if (state == 5) fm_bar_ok("완료", 1);
    else fm_bar_ok("사용", (state == 2 && n > 0 && !sealed && !msg) || state == 4);
    fm_bar_emit(R32(scr + SCR_MENU));
    ui_end();
    // 목록이 다른 것으로 바뀌거나(캐릭터·분류) 상태 2로 들어올 때는 커서가 안 바뀌어 호스트가 자동으로 넘기지 않는다.
    // 그때는 선택 줄이 보이게 직접 넘긴다(문서에 줄이 생긴 다음 틱까지 다시 해 본다).
    static int last_key = -1, want_show;
    int key = (slot << 8) | (cat << 4) | (state == 2);
    if (key != last_key) { last_key = key; want_show = 1; }
    if (want_show && state == 2 && selpos >= 0) want_show = !ui_scroll_show("slist", selpos);
    else if (state != 2) want_show = 1;
}
