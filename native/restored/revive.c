// 부활석(items_ext.h의 IT_REVIVE, 원작에 없다. 사용자 요청 2026-11-10).
//
// 원작 전투의 패배: 승패 검사 0x1d6d0(전투 B)이 아군 진영 P(=[B+4])의 전원이 쓰러졌는지(0x1991c) 보고, 전멸이면
// B+0x34=0, B+0x30=0x19(패배: 페이드 → 타이틀, 게임오버 창 없음)로 넘긴다. 이 함수는 상태 2(0x1c7ec), 상태 4(0x1c8a8),
// 승패 상태 15·16(0x1d330)이 부르고 0을 받으면 "여기서 끝"으로 돌아간다. 그래서 전멸이 결정되는 곳은 이 한 군데다.
//
// 부활석이 있으면 0x1d6d0을 감싸 전멸 때 원작 처리를 미루고 묻는다.
//   - 전투 상태를 0x18(갱신 없음)로 돌려놓고 0을 돌려준다(부른 쪽은 원작과 똑같이 "끝"으로 보고 돌아간다).
//   - 이후 틱마다 갱신 0x1c4cc(B, 입력)를 감싸 원본 갱신을 건너뛰고(적 행동·ATB·메시지가 멈춘다) 키·터치를 받는다.
//   - 예: 부활석 1개를 쓰고 파티 전원을 되살린 뒤 상태 15(승패 검사)로 돌려 보낸다. 전원이 살아 있으니 원작이 평소 행동 뒤처럼
//     다음 행동자로 넘어간다. 적은 그대로다.
//   - 아니오: 원본 0x1d6d0을 그대로 불러 패배 처리를 한다(감싼 함수를 거치지 않고 orig를 바로 부른다).
// 되살리기: 쓰러짐(+0x58 비트15)과 상태이상 비트를 지우고, 상태이상 효과(효과 목록 종류 4~15, 9·10 제외)를 0x14e40으로 떼고,
// ATB를 처음 값(M+0x198)으로 돌리고, HP·MP를 최대로 채운다. 자세는 0x14ec0(HP 비율에 따라 쓰러짐/위험/보통 자세 메시지)이 다시 정한다.
// (전투 끝 정리 0x17d38이 같은 일을 하지만 전투 끝에 부르라고 만든 함수라 쓰지 않는다.)
//
// 묻는 창은 문서 ui/revive.rml. 원작 그리기(F2)에서도 보이게 ui_force()를 쓴다(새 UI가 꺼져도 이 문서는 그린다).
#include "restored.h"
#include "items_ext.h"
#include <stdio.h>
#include <stdlib.h>

enum {
    B_P = 0x04, B_STATE = 0x30, B_PAGE = 0x34,
    P_N = 0x0c, P_ACTOR = 0x10,
    M_ACT = 0x54, M_FLAGS = 0x58, M_STATS = 0x64, M_MPMAX = 0xf4,
    MF_KO = 0x8000, MF_DEAD_LOOK = 0x4000, MF_AILMENT = 0x1f00, MF_GONE = 0x10000,
    ST_IDLE = 0x18, ST_CHECK = 15,
    KEY_UP = 0x1f, KEY_DOWN = 0x1e, KEY_LEFT = 0x1d, KEY_RIGHT = 0x1c, KEY_OK = 0x1b, KEY_CLR = 0x10,
    KEY_2 = 0x52, KEY_4 = 0x54, KEY_5 = 0x55, KEY_6 = 0x56, KEY_8 = 0x58,
    LOCK_TICKS = 8,     // 창이 뜬 직후 눌린 키는 버린다(전투 중 OK를 두드리다 부활석이 새는 것을 막는다)
};

static uint32_t ask_B;  // 묻는 중인 전투(0이면 안 묻는다)
static int ask_sel;     // 0 예, 1 아니오
static int ask_tick;
bool ext_revive_asking(void) { return ask_B != 0; }

static uint32_t gnd(uint32_t a) { return R32_(&cpu, a); }
#define DBG(...) do { if (getenv("REVIVE_DBG")) fprintf(stderr, __VA_ARGS__); } while (0)
static int all_dead(uint32_t P) { return (int)CALL(0x100000 + 0x1991c, P); }

// 쓰러진 멤버를 되살린다. 반환: 되살린 사람 수
static int revive_party(uint32_t B) {
    uint32_t P = gnd(B + B_P);
    int n = 0;
    int cnt = (int)gnd(P + P_N); // 0x1991c(전멸 판정)과 같은 목록: 진영 +0xc 개수, +0x10부터 행동 하위 객체(= 멤버 + 0x54)
    if (cnt > 5) cnt = 5;
    for (int i = 0; i < cnt; i++) {
        uint32_t act = gnd(P + P_ACTOR + 4 * i), M = act - M_ACT, st;
        if (!act || !(st = gnd(M + M_STATS))) continue;
        DBG("[revive] M=%x st=%x flags=%x hp=%d/%d mp=%d/%d\n", M, st, gnd(M + M_FLAGS), (int)gnd(st), (int)gnd(st + 4), (int)gnd(st + 8), (int)gnd(M + M_MPMAX));
        for (int k = 4; k <= 15; k++)                                   // 상태이상 효과 해제(전투 끝 정리 0x17d38과 같은 목록: 효과 4~15, 9·10 제외)
            if (k != 9 && k != 10) CALL(0x100000 + 0x14e40, M + M_ACT, (uint32_t)k);
        W32_(&cpu, st + 0x14, gnd(M + 0x198));                          // ATB를 처음 값으로
        W32_(&cpu, M + M_FLAGS, gnd(M + M_FLAGS) & ~(uint32_t)(MF_KO | MF_DEAD_LOOK | MF_AILMENT | MF_GONE));
        W32_(&cpu, st + 0, gnd(st + 4));                                // HP = 최대 HP
        W32_(&cpu, st + 8, gnd(M + M_MPMAX));                           // MP = 최대 MP
        CALL(0x100000 + 0x14ec0, M + M_ACT);                            // 자세를 다시 정한다
        n++;
    }
    return n;
}

void orig_f_1d6d0(cpu_t *c);
void f_1d6d0(cpu_t *c) {
    uint32_t B = c->r[0];
    if (ext_count(IT_REVIVE) > 0 && all_dead(gnd(B + B_P))) {
        DBG("[revive] ask B=%x P=%x state=%x\n", B, gnd(B + B_P), gnd(B + B_STATE));
        ask_B = B; ask_sel = 0; ask_tick = 0;
        W32_(&cpu, B + B_STATE, ST_IDLE);
        c->r[0] = 0; // 원작이 "끝"을 알릴 때와 같다
        return;
    }
    c->r[0] = B; // CALL이 r0~r3을 덮었다
    orig_f_1d6d0(c);
}

static void key_pick(int k, int *sel, int *done) {
    if (k == KEY_LEFT || k == KEY_RIGHT || k == KEY_4 || k == KEY_6 || k == KEY_UP || k == KEY_DOWN || k == KEY_2 || k == KEY_8) *sel ^= 1;
    else if (k == KEY_OK || k == KEY_5) *done = 1;
    else if (k == KEY_CLR) { *sel = 1; *done = 1; }
}

void orig_f_1c4cc(cpu_t *c);
void f_1c4cc(cpu_t *c) {
    uint32_t B = c->r[0], in = c->r[1];
    if (!ask_B || B != ask_B) {
        if (ask_B && B != ask_B) ask_B = 0;
        orig_f_1c4cc(c);
        return;
    }

    int done = 0, pk = ui_pick("revive");
    int k = R32_(&cpu, in + 0xc) && R16_(&cpu, in + 2) == 1 ? (int)R16_(&cpu, in) : 0;
    if (ask_tick++ >= LOCK_TICKS) {
        if (pk >= 0) { ask_sel = pk ? 1 : 0; done = 1; }
        else key_pick(k, &ask_sel, &done);
    }
    int have = ext_count(IT_REVIVE);
    if (done && ask_sel == 0 && have > 0) {
        revive_party(B);
        ext_take(IT_REVIVE, 1);
        W32_(&cpu, B + B_STATE, ST_CHECK);
        W32_(&cpu, B + B_PAGE, 0);
        ask_B = 0;
        char t[64];
        snprintf(t, sizeof t, "부활석을 사용했다 (남은 %d개)", have - 1);
        ext_toast(t);
        return;
    }
    if (done) { // 아니오: 원작 패배 처리
        ask_B = 0;
        c->r[0] = B;
        orig_f_1d6d0(c);
        return;
    }
    ui_begin("revive");
    ui_force();
    ui_int("have", have);
    ui_int("sel", ask_sel);
    ui_int("lock", ask_tick < LOCK_TICKS);
    ui_bool("narrow", ui_screen_w() < 400);
    ui_end();
}
