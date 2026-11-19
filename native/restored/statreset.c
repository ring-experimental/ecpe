// 망각의 물약(items_ext.h의 IT_STAT_RESET 246, 원작에 없다. 사용자 요청 2026-11-10): 한 캐릭터의 GP 분배를 처음으로 되돌린다.
//
// ---- 원작의 GP 구조(조사 결과, 되돌리는 길이 원작에 없다는 근거) ----
//  * 캐릭터 객체 M의 기본 능력치 STR/DEX/INT는 +0x10c/+0x10e/+0x110(u16, 10배 단위: 표시값 = 값/10), 장비 보너스 +0x112/+0x114/+0x116,
//    표시 합계 +0x118/+0x11a/+0x11c(0x15e60이 기본/10 + 보너스로 계산), GP(남은 분배 점수)는 +0x194(u32). 직업 데이터는 M+0xb4가 가리키는
//    레코드(바이트: +1/+3/+5 시작값, +2/+4/+6 레벨당 증가). 능력치 블록 [M+0x64]+0x18이 레벨.
//  * 레벨 L의 '분배 안 한' 기본값은 시작값 + 증가*(L-1)이다: 0x15dc0(M, 레벨)이 이 식으로 처음 값을 만들고(0x15c60 캐릭터 초기화), 레벨업
//    0x1cdb0이 0x15e2c로 증가분만 더한다(그리고 GP +1). 저장 파일의 주인공 8명 모두 이 식과 정확히 맞는 것을 확인했다(분배 전).
//  * 분배는 스테이터스 화면 키 처리 0x32ff8 한 곳뿐이다: GP(+0x194)가 0보다 클 때 선택한 능력치에 +10, GP −1, 그 뒤 0x15e60·0x15ec4·
//    0x15f40·0x15f78·0x15f98로 파생값을 다시 계산한다. 값을 줄이거나 GP를 돌려주는 코드는 없다(+0x10c 계열을 쓰는 함수는 0x15dc0 초기화,
//    0x15e2c 레벨업, 0x16118 소모품 효과뿐인데, 소모품 효과는 능력치 종류 0x1a~0x1c를 쓰는 아이템이 아이템 표에 없다. 거인의사과 0x13은 +0x11e(최대 HP 기본),
//    소울워터 0x14는 +0x120(최대 MP 기본)을 올린다).
//  * GP를 직접 늘리는 곳은 레벨업과 스크립트 명령(0x3a4a4의 GP 가산)이라 "레벨 − 가입 레벨"로 총 GP를 셀 수는 없다. 그래서 기록 대신
//    분배량 = (지금 기본값 − 레벨 기본값)/10 으로 계산한다: 기존 저장에도 맞고 따로 저장할 것이 없다. 식에서 어긋나는 값(음수, 10으로
//    나눠 떨어지지 않는 나머지)은 건드리지 않는다.
//
// ---- 사용 ----
//  아이템 창에서 '사용'하면 ext_use → sr_use: 파티 전원이 돌려받을 GP가 0이면 토스트만 하고 소모하지 않는다. 아니면 창을 연다.
//  창은 필드 메뉴 키 분배(0x2fa60)와 메뉴 장면 틱(0x109b8)에 훅(ext_statreset_key / ext_statreset_draw)으로 얹는다(enhance.c가 부른다).
//  파티원을 고르고(왼쪽 목록, 오른쪽에 지금 → 되돌린 뒤 능력치와 돌려받을 GP 미리보기) 확인(예/아니오) → 되돌리고 파생값 재계산 → 물약 1개 소모 → 토스트.
//  (다른 게임의 스탯 초기화권도 같은 순서: 대상 고르기 → 결과 미리보기 → 확인.) 원작 그리기(F2)에서도 뜬다(ui_force).
//
// ---- 성장의 물약(IT_LEVEL_UP 253, 사용자 요청 2026-11-11): 같은 창을 모드만 바꿔 쓴다(S.item) ----
//  한 캐릭터의 레벨을 1 올린다. 원작 레벨업 0x1cdb0(레벨업 담당 객체 [G+0x1c8], M, GP 줄지)을 스크립트 명령 0x3a4a4(레벨 올리기)와
//  똑같이 부른다: 레벨 +1(99 넘으면 0을 돌려주고 아무것도 안 한다), 0x15e2c로 능력치 증가분(직업 레코드 +2/+4/+6), 파생값 재계산,
//  최대 HP 비율대로 지금 HP 맞춤, GP +1, 그 레벨에 배우는 기술(0x1ce4c). EXP(레벨 안의 진행분)는 건드리지 않는다.
//  미리보기는 LV → LV+1, 능력치 지금 → 증가분을 더한 값, GP +1. 쓰고 나서 물약이 남아 있으면 창을 닫지 않는다(여러 번 연달아 쓰기).
#include "restored.h"
#include "items_ext.h"
#include "fm_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t cp949_to_utf8(const unsigned char *s, int len, char *out);

enum {
    G_SCENE = 0x0c, G_PARTY = 0x1c0, G_MENU = 0x1d4, MENU_SHEET = 0x14, PARTY_CHARS = 0x84,
    M_ID = 0x04, M_STATS = 0x64, M_JOB = 0xb4, M_BASE = 0x10c, M_BONUS = 0x112, M_TOTAL = 0x118, M_MPMAX = 0xf4, M_GP = 0x194,
    ST_LV = 0x18,
    FN_CHAR_NAME = 0x1252f8, FN_FACE_FRAME = 0x112fe8, FN_DRAW_FRAME = 0x1056ec,
    KEY_UP = 0x1f, KEY_DOWN = 0x1e, KEY_LEFT = 0x1d, KEY_RIGHT = 0x1c, KEY_OK = 0x1b, KEY_CLR = 0x10,
    KEY_2 = 0x52, KEY_4 = 0x54, KEY_5 = 0x55, KEY_6 = 0x56, KEY_8 = 0x58,
    PICK_YES = 10, PICK_NO = 11, PICK_GO = 12, PICK_CLOSE = 13,
    LOCK_TICKS = 3,
    G_LVUP = 0x1c8, FN_LEVEL_UP = 0x11cdb0, LV_MAX = 99,
};
#define G32(a) R32_(&cpu, (a))
#define G16(a) R16_(&cpu, (a))
#define G8(a) R8_(&cpu, (a))
#define DBG(...) do { if (getenv("SR_DBG")) fprintf(stderr, __VA_ARGS__); } while (0)

static struct { int open, stage, cursor, yes_sel, lock, item; uint64_t seen; } S; // stage 0 파티원 고르기, 1 확인. item: 쓰는 물약
#define LVMODE (S.item == IT_LEVEL_UP)

static uint32_t member(int i) {
    uint32_t G = ext_game(), pd = G ? G32(G + G_PARTY) : 0;
    return pd && i >= 0 && i < 5 ? G32(pd + PARTY_CHARS + 4 * (uint32_t)i) : 0;
}
static int member_count(void) { int n = 0; while (n < 5 && member(n)) n++; return n; }

// 한 캐릭터의 분배 현황: sp[3] = 능력치별 분배량(GP 점수), def[3] = 레벨 기본값(10배 단위)
typedef struct { int lv, gp, sp[3], base[3], def[3], bonus[3], tot[3], refund, grow[3]; } info_t;
static void info_of(uint32_t M, info_t *o) {
    memset(o, 0, sizeof *o);
    uint32_t st = G32(M + M_STATS), job = G32(M + M_JOB);
    o->lv = (int)G8(st + ST_LV);
    o->gp = (int)G32(M + M_GP);
    for (int s = 0; s < 3; s++) {
        o->base[s] = (int)G16(M + M_BASE + 2 * (uint32_t)s);
        o->bonus[s] = (int)G16(M + M_BONUS + 2 * (uint32_t)s);
        o->tot[s] = (int)G16(M + M_TOTAL + 2 * (uint32_t)s);
        o->grow[s] = (int)G8(job + 2 + 2 * (uint32_t)s);
        o->def[s] = (int)G8(job + 1 + 2 * (uint32_t)s) + o->grow[s] * (o->lv - 1);
        o->sp[s] = o->base[s] > o->def[s] ? (o->base[s] - o->def[s]) / 10 : 0;
        o->refund += o->sp[s];
    }
}

static void utf8_name(uint32_t M, char *out, size_t n) {
    uint32_t a = CALL(FN_CHAR_NAME, M);
    const unsigned char *s = (const unsigned char *)gstr(a);
    size_t len = strlen((const char *)s);
    char tmp[128];
    if (len > 30) len = 30;
    size_t w = cp949_to_utf8(s, (int)len, tmp);
    if (w >= sizeof tmp) w = sizeof tmp - 1;
    tmp[w] = 0;
    snprintf(out, n, "%s", tmp);
}

// 되돌리기: 기본 능력치를 레벨 기본값 쪽으로 분배량만큼 내리고 GP를 돌려준 뒤, 0x32ff8과 같은 순서로 파생값을 다시 계산한다.
static int apply_reset(uint32_t M) {
    info_t in;
    info_of(M, &in);
    if (in.refund <= 0) return 0;
    for (int s = 0; s < 3; s++) W16_(&cpu, M + M_BASE + 2 * (uint32_t)s, (uint32_t)(in.base[s] - 10 * in.sp[s]));
    W32_(&cpu, M + M_GP, (uint32_t)(in.gp + in.refund));
    static const uint32_t FN[5] = {0x15e60, 0x15ec4, 0x15f40, 0x15f78, 0x15f98};
    for (int i = 0; i < 5; i++) CALL(0x100000 + FN[i], M);
    uint32_t st = G32(M + M_STATS); // 최대 HP·MP가 줄었으면 지금 값도 맞춘다
    if ((int)G32(st) > (int)G32(st + 4)) W32_(&cpu, st, G32(st + 4));
    if ((int)G32(st + 8) > (int)G32(M + M_MPMAX)) W32_(&cpu, st + 8, G32(M + M_MPMAX));
    return in.refund;
}

static int sr_use(int id, int where) {
    (void)id; (void)where;
    int n = member_count(), any = -1;
    for (int i = 0; i < n; i++) { info_t in; info_of(member(i), &in); if (in.refund > 0) { any = i; break; } }
    if (any < 0) { ext_toast("되돌릴 GP 분배가 없다"); ext_toast("망각의 물약은 소모되지 않는다"); return 0; }
    S.open = 1; S.stage = 0; S.cursor = any; S.yes_sel = 0; S.lock = LOCK_TICKS;
    return 0; // 소모는 확인 뒤에 우리가 한다
}

// 고른 파티원에게 지금 쓸 수 있나(망각: 돌려받을 GP가 있다, 성장: 레벨이 99 아래)
static int can_on(uint32_t M) {
    info_t in;
    if (!M) return 0;
    info_of(M, &in);
    return LVMODE ? in.lv < LV_MAX : in.refund > 0;
}

static int lv_use(int id, int where) {
    (void)id; (void)where;
    S.item = IT_LEVEL_UP;
    int n = member_count(), any = -1;
    for (int i = 0; i < n; i++) if (can_on(member(i))) { any = i; break; }
    if (any < 0) { ext_toast("더 올릴 수 있는 레벨이 없다"); return 0; }
    S.open = 1; S.stage = 0; S.cursor = any; S.yes_sel = 0; S.lock = LOCK_TICKS;
    return 0; // 소모는 확인 뒤에 우리가 한다
}
static int sr_use_reset(int id, int where) { S.item = IT_STAT_RESET; return sr_use(id, where); }
__attribute__((constructor)) static void sr_register(void) {
    ext_register_use(IT_STAT_RESET, sr_use_reset);
    ext_register_use(IT_LEVEL_UP, lv_use);
}

static void confirm_open(void) {
    if (!member(S.cursor)) return;
    if (!can_on(member(S.cursor))) { ext_toast(LVMODE ? "이미 최고 레벨이다" : "되돌릴 GP 분배가 없다"); return; }
    S.stage = 1; S.yes_sel = 0; S.lock = LOCK_TICKS;
}

static void do_levelup(uint32_t M) {
    uint32_t G = ext_game(), lvo = G ? G32(G + G_LVUP) : 0;
    char nm[96], t[200];
    if (!lvo || !can_on(M)) { S.stage = 0; return; }
    utf8_name(M, nm, sizeof nm);
    if (!CALL(FN_LEVEL_UP, lvo, M, 1)) { S.stage = 0; return; }
    ext_take(IT_LEVEL_UP, 1);
    info_t in;
    info_of(M, &in);
    snprintf(t, sizeof t, "%s LV%d 달성 (GP +1)", nm, in.lv);
    ext_toast(t);
    S.stage = 0; S.lock = LOCK_TICKS;
    if (ext_count(IT_LEVEL_UP) <= 0) S.open = 0; // 남아 있으면 창을 두고 계속 쓴다
}

static void do_confirm(void) {
    uint32_t M = member(S.cursor);
    if (!M || ext_count(S.item) <= 0) { S.open = 0; return; }
    if (LVMODE) { do_levelup(M); return; }
    char nm[96], t[200];
    utf8_name(M, nm, sizeof nm);
    int r = apply_reset(M);
    if (r <= 0) { S.stage = 0; return; }
    ext_take(IT_STAT_RESET, 1);
    snprintf(t, sizeof t, "%s의 GP +%d 반환", nm, r);
    ext_toast(t);
    S.open = 0;
}

// 필드 메뉴 키(0x2fa60). 열려 있으면 모두 가져간다(1이면 원본 키 처리를 하지 않는다).
int ext_statreset_key(uint32_t menu, int k) {
    (void)menu;
    if (!S.open) return 0;
    if (S.lock > 0 || !k) return 1;
    int n = member_count();
    if (S.stage == 0) {
        if (k == KEY_UP || k == KEY_2 || k == KEY_LEFT || k == KEY_4) { if (n) S.cursor = (S.cursor + n - 1) % n; }
        else if (k == KEY_DOWN || k == KEY_8 || k == KEY_RIGHT || k == KEY_6) { if (n) S.cursor = (S.cursor + 1) % n; }
        else if (k == KEY_OK || k == KEY_5) confirm_open();
        else if (k == KEY_CLR) S.open = 0;
    } else {
        if (k == KEY_LEFT || k == KEY_RIGHT || k == KEY_4 || k == KEY_6 || k == KEY_UP || k == KEY_DOWN || k == KEY_2 || k == KEY_8) S.yes_sel ^= 1;
        else if (k == KEY_OK || k == KEY_5) { if (S.yes_sel == 0) do_confirm(); else S.stage = 0; }
        else if (k == KEY_CLR) S.stage = 0;
    }
    return 1;
}

// 얼굴 그림: 메뉴 시트의 얼굴 프레임(ui_fmchar.c face_sprite와 같은 방식). 반드시 ui_scratch 안에서.
static void face(uint32_t G, uint32_t M, const char *key) {
    uint32_t menu = G32(G + G_MENU), sheet = menu ? G32(menu + MENU_SHEET) : 0;
    ui_scratch_clear();
    if (M && sheet) {
        uint32_t frame = CALL(FN_FACE_FRAME, G, G8(M + M_ID), 0);
        CALL(FN_DRAW_FRAME, sheet, frame, 120, 120, 0, 0, 0);
    }
    ui_sprite_trim(key, 40, 40, 160, 160);
}

static void emit_member(uint32_t G, uint32_t M, int idx) {
    info_t in;
    info_of(M, &in);
    char nm[96];
    utf8_name(M, nm, sizeof nm);
    static const char *const SN[3] = {"STR", "DEX", "INT"};
    ui_int("idx", idx);
    ui_text("name", nm);
    ui_int("lv", in.lv);
    ui_int("gp", in.gp);
    int lv = LVMODE;
    ui_int("refund", in.refund);
    ui_int("can", lv ? in.lv < LV_MAX : in.refund > 0);
    ui_int("lv_after", in.lv < LV_MAX ? in.lv + 1 : in.lv);
    ui_int("gp_after", lv ? in.gp + 1 : in.gp + in.refund);
    face(G, M, "face");
    ui_list("stats");
    for (int s = 0; s < 3; s++) {
        int after = lv ? (in.lv < LV_MAX ? in.tot[s] + ((in.base[s] + in.grow[s]) / 10 - in.base[s] / 10) : in.tot[s])
                       : in.tot[s] - (in.base[s] / 10 - (in.base[s] - 10 * in.sp[s]) / 10);
        ui_item();
        ui_text("n", SN[s]);
        ui_int("now", in.tot[s]);
        ui_int("after", after);
        ui_int("sp", lv ? 0 : in.sp[s]);
        ui_close();
    }
    ui_close();
}

// 메뉴 장면 틱 뒤(0x109b8): 창을 그린다. 누른 줄(pick)을 처리한다.
void ext_statreset_draw(uint32_t G) {
    if (!S.open) { ui_pick("sr"); return; }
    if (G32(G + G_SCENE) != 14 || ext_count(S.item) <= 0) { S.open = 0; return; }
    if (S.lock > 0) S.lock--;
    int n = member_count();
    int shell_on = ui_modern() && G32(G + G_MENU) && R32_(&cpu, G32(G + G_MENU) + 0x1c) == 1; // 필드 메뉴 틀 안(모드 1)
    int pk = ui_pick("sr");
    if (pk >= 0 && S.lock == 0) {
        if (S.stage == 0) {
            // 틀 안에서는 파티원을 누르면 바로 그 사람의 확인 단계로 간다(되돌릴 수 없는 동작이라 예/아니오 확인은 그대로 둔다).
            if (pk >= 0 && pk < n) { if (pk == S.cursor || shell_on) { S.cursor = pk; confirm_open(); } else S.cursor = pk; }
            else if (pk == PICK_GO) confirm_open();
            else if (pk == PICK_CLOSE) S.open = 0;
        } else {
            if (pk == PICK_YES) do_confirm();
            else if (pk == PICK_NO) S.stage = 0;
        }
    }
    if (!S.open) return;
    if (S.cursor >= n) S.cursor = n ? n - 1 : 0;
    ui_begin("statreset");
    ui_force();
    int shell = fm_overlay_frame(G32(G + G_MENU)); // 필드 메뉴 틀 안이면 내용 자리에, 되돌리기·닫기는 고정 버튼 띠
    ui_int("have", ext_count(S.item));
    ui_int("lvmode", LVMODE);
    ui_int("stage", S.stage);
    ui_int("cursor", S.cursor);
    ui_int("sel", S.yes_sel);
    ui_int("lock", S.lock > 0);
    ui_int("can", can_on(member(S.cursor)));
    ui_int("narrow", ui_screen_w() < 400);
    ui_scratch_begin();
    ui_list("members");
    for (int i = 0; i < n; i++) {
        ui_item();
        emit_member(G, member(i), i);
        ui_int("cur", i == S.cursor);
        ui_close();
    }
    ui_close();
    ui_obj("cur");
    emit_member(G, member(S.cursor), S.cursor);
    ui_close();
    ui_scratch_end();
    (void)shell; { // 결정: 고르기에서는 「사용」(확인 창으로), 확인 창에서는 「확인」(고른 예/아니오). 이전: 확인 창 → 고르기 → 닫기
        fm_bar_begin();
        if (S.stage == 0) fm_bar_ok("사용", can_on(member(S.cursor)) && S.lock == 0);
        else fm_bar_ok("확인", S.lock == 0);
        fm_bar_emit(G32(G + G_MENU));
    }
    ui_end();
}

// 창이 떠 있다 / 닫는다(필드 메뉴 틀 fm_shell.c: 레일을 누르면 CLR과 같은 길로 닫는다. 물약은 확인한 뒤에만 쓰므로 그대로 남는다)
int ext_statreset_open(void) { return S.open; }
void ext_statreset_close(void) { S.open = 0; S.stage = 0; }
