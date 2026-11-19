// 자동 전투와 전투 2배속(원작에 없다, 사용자 요청 2026-11-10). 켜고 끄기는 호스트(host/ui.cpp: R·F 키, 문서 버튼, 게임패드 키 설정, ui.ini).
//
// 자동 전투: 전투 틱 0x109f0을 감싸 틱이 끝날 때마다 상태를 보고 원본 키를 흉내 낸다(ui_key_tap, 대사 AUTO와 같은 길).
//   명령 차례(아군 ATB가 찬 멤버가 있고 행동 중이 아닐 때)가 오면 파티 상태를 보고 행동을 하나 정한다(make_plan, 차례마다 한 번):
//     1) 전투불능 아군이 있고 부활 아이템(효과 0x10: 홀리워터, 전체 대상 미라클포션)이 있으면 부활. 둘 이상이 쓰러졌으면 전체 대상을 먼저.
//     2) 아군 HP가 낮으면(LOW_HP_PCT) 회복 아이템. 여럿이 낮으면 전원 대상(설명이 "전체"로 시작하는 것, 지금 데이터에는 회복 쪽에 없다)을 먼저, 아니면 필요한 만큼을 채우는 가장 작은 것.
//     3) MG가 가득 차고 MP가 충분한 필살기가 있으면 그 가운데 무작위 하나(MP 모자란 것은 후보에서 뺀다). 적이 하나 남고 거의 죽었으면 아낀다.
//     4) 그 밖에는 기본 공격 → 가장 HP가 낮은 살아 있는 적(다른 모바일 RPG의 자동 전투도 기본은 낮은 HP 우선 조준이다).
//   아이템은 한 차례에 하나, 같은 대상에 연달아 쓰지 않는다(HP가 CRIT_HP_PCT 이하이면 예외). 마나 아이템은 쓰지 않는다:
//   MP를 쓰는 스킬을 아예 고르지 않으므로 MP가 모자랄 일이 없고, 모자라 보이는 상황은 기본 공격으로 때운다.
//   아이템은 전투 아이템 창의 분류 탭(`*` 키로 열고 ↑↓ 분류, ←→ 칸)으로 고른다. 핫슬롯 탭이 아닌 분류 탭에도 가진 것 전부가 있다(ui_battle.c emit_item_list).
//   아이템 효과 읽기(레코드): +0x14 효과1(0x10 부활, 0x15 HP 고정량, 0x16 HP 퍼센트, 0x17·0x18 MP), +0x1a 효과1 값. 전원 대상은 레코드가 아니라 설명 글로 알아본다(미라클포션).
//   사람 우선: 그 차례가 시작된 뒤(또는 자동 전투를 켠 뒤) 사람이 키를 넣으면 그 차례는 건드리지 않는다. 다음 차례부터 다시 맡는다.
//   결과 화면(상태 0x17)은 잠깐 보여 준 뒤 OK로 넘긴다. 패배(0x19)는 건드리지 않는다.
// 전투 2배속: 게임 틱은 타이머로 돈다(host/runtime.c). 전투 장면에서만 타이머 간격을 반으로 줄인다. ab_speed_pct()가 그 배율이다.
//   틱 하나는 키를 한 번만 읽고(입력 객체를 틱 끝에 지운다) 같은 틱을 두 번 돌리는 게 아니라 틱이 두 배 자주 오는 것이라
//   키가 두 번 먹지 않는다. 소리는 게임이 부르는 대로(효과음은 연출과 함께 빨라지고 음악은 원래 속도).
// 위 상태 번호·구조는 work/ui/survey/battle.md.
#include "restored.h"
#include "items_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// R32 등은 지역 변수 c를 쓴다. 래퍼 밖의 도우미는 전역 cpu를 본다(래퍼의 c도 같은 &cpu다).
static cpu_t *const c = &cpu;

enum {
    G_SCENE = 0x0c, G_BATTLE = 0x1c8, SCENE_BATTLE = 13,
    B_P = 0x04, B_E = 0x08, B_STATE = 0x30, B_CM = 0x5c,
    ST_RESULT = 0x17, ST_DEFEAT = 0x19,
    P_N = 0x0c, P_ATB = 0x1c, P_COMMANDING = 0x7c, P_MEM = 0x98, E_ENEMY = 0x3d0,
    M_ACT = 0x54, M_FLAGS = 0x58, M_ACTION = 0x60, ACT_READY = 4, ACT_TARGET = 9, // M+0x60: 4 명령 메뉴, 9 대상 선택(5~8은 목록 창들로 짐작, 시험 전)
    MF_KO = 0x8000, MF_ACTING = 0x900, UF_TARGET = 0x100000,
    X_FLAGS = 0x04, X_STATS = 0x10, X_Y = 0x2c, XF_HIDDEN = 0x10000,
    FN_ITEM_REC = 0x1240dc, FN_ITEM_DESC = 0x1240b0, FN_SKILL_REC = 0x123f50, B_SKILLDB = 0x50, M_SPECIALS = 0x178, B_ITEMDB = 0x54, P_INV = 0xa8, INV_COUNT = 0x50,
    M_STATS = 0x64, M_MG = 0xf8, M_MGMAX = 0xfc, M_SPECIALN = 0x17c, CM_LW = 0x94, CM_SL = 0x48,
    SL_CUR = 0x44, SL_SCROLL = 0x48, LW_MODE = 0x14, LW_SEL = 0x18, LW_TAB = 0x3c, SL_MODE = 0x14, SBIT_SPECIAL_OFF = 4,
    EFF_REVIVE = 0x10, EFF_HEAL_FIX = 0x15, EFF_HEAL_PCT = 0x16, ITEM_LAST = 28,
    CM_CUR = 0x14, CM_SCROLL = 0x1c, CM_SUB = 0x20, CM_LIST = 0x24, CM_SUBCUR = 0x3c, CM_SUBBITS = 0x44,
    SBIT_SKILL_OPEN = 1,    // CM+0x44 bit0: 스킬 목록이 열려 있다
};
// 판단 기준값
enum {
    LOW_HP_PCT = 35,            // 이 퍼센트 이하이면 회복이 필요하다
    CRIT_HP_PCT = 15,           // 이 이하이면 같은 대상에 연달아 회복해도 된다
    ALL_HEAL_MIN_LOW = 2,       // 이만큼 이상이 낮으면 전원 대상 회복을 먼저 쓴다
    SAVE_SPECIAL_ENEMY_PCT = 25, // 남은 적이 하나이고 HP가 이 이하이면 필살기를 아낀다
    ITEM_STAGE_TRIES = 16,      // 아이템 쓰기가 이만큼 키를 넣고도 안 되면 포기하고 기본 공격으로
};
enum { DELAY_MS = 350, GAP_MS = 160, RESULT_FIRST_MS = 900, RESULT_GAP_MS = 700, MAX_TRIES = 60, DIR_MAX = 6 };

// 전투 2배속 배율(%). runtime.c가 타이머를 걸 때 부른다. 결과·패배 화면은 원래 속도로 둔다(읽어야 하고, 부활 등 다른 기능이 얹힌다).
int ab_speed_pct(void) {
    if (!ui_battle_fast()) return 100;
    uint32_t G = ext_game();
    if (!G || R32(G + G_SCENE) != SCENE_BATTLE) return 100;
    uint32_t B = R32(G + G_BATTLE);
    if (!B) return 100;
    uint32_t st = R32(B + B_STATE);
    return st == ST_RESULT || st == ST_DEFEAT ? 100 : 200;
}

// ---- 자동 전투 ----
static int enemy_count(uint32_t E) { int n = (int)R32(E + P_N); return n > 16 ? 16 : n; }
static uint32_t enemy_at(uint32_t E, int i) { return R32(E + E_ENEMY + 4 * i); }
static int enemy_alive(uint32_t X) { return X && !(R32(X + X_FLAGS) & XF_HIDDEN) && (int)R32(R32(X + X_STATS)) > 0; }

// 지금 대상으로 고른(노란 화살표) 살아 있는 적의 번호. 없으면 -1. 대상 선택 중인지 알아보는 표지이기도 하다(restored/ui_battle.c is_targeting과 같은 조건).
static int enemy_cursor(uint32_t E) {
    for (int i = 0; i < enemy_count(E); i++) {
        uint32_t X = enemy_at(E, i);
        if (X && (R32(X + X_FLAGS) & (XF_HIDDEN | UF_TARGET)) == UF_TARGET) return i;
    }
    return -1;
}
static int ally_cursor(uint32_t P) {
    for (int i = 0; i < (int)R32(P + P_N) && i < 8; i++) {
        uint32_t M = R32(P + P_MEM + 4 * i);
        if (M && (R32(M + M_FLAGS) & UF_TARGET)) return i;
    }
    return -1;
}

// 조준: 살아 있는 적 가운데 HP가 가장 낮은 것(동률이면 앞 번호). 마무리를 먼저 해 적이 빨리 줄고 받는 공격이 준다.
static int choose_target(uint32_t E) {
    int best = -1, hp = 0;
    for (int i = 0; i < enemy_count(E); i++) {
        uint32_t X = enemy_at(E, i);
        if (!enemy_alive(X)) continue;
        int h = (int)R32(R32(X + X_STATS));
        if (best < 0 || h < hp) best = i, hp = h;
    }
    return best;
}

// ---- 파티 상태로 행동 고르기 ----
enum { PL_ATTACK, PL_ITEM, PL_SPECIAL };
static struct { int kind, item, tab, all, sp; uint32_t tgt; } plan;   // sp: 고른 필살기의 목록 번호
   // tgt: 아이템 대상 아군(전원 대상이면 0)

static struct {
    int was_on;                 // 지난 틱에 켜져 있었나(켠 순간을 알아본다)
    int in_cmd;                 // 지난 틱에 명령 차례였나
    uint32_t actor;             // 명령을 받는 멤버
    uint64_t claim_ms;          // 이 시각 뒤에 사람 키가 있으면 사람 차례. 차례 시작이나 자동 전투를 켠 때
    uint64_t next_ms;           // 다음 키를 넣을 수 있는 때
    int tries;                  // 이 차례에 넣은 키 수(멈춤 방지)
    int dir, flip, last_cur;    // 대상 고르기에 쓴 방향 키 수, 방향 뒤집기, 직전 커서
    int attacking;              // 이 차례에 '기본'(또는 필살)을 골랐다(대상 선택으로 가는 중)
    int stage_tries;            // 이 차례의 아이템 쓰기에 쓴 키 수
    uint32_t last_item_tgt;     // 직전 아이템 사용 대상(연달아 방지)
    int acted_since;            // 그 뒤에 아이템이 아닌 행동을 했다
    uint32_t last_st;
} ab;

// 방향 키 고르기: 이미 알려진 방향이 없으니(분류 ↑↓, 칸 ←→, 대상 ↑↓ 모두 순환 여부를 모른다) 커서가 안 움직이면 방향을 뒤집는다.
static struct { int stage, last, flip, n; } sd;
static int steer(int stage, int cur, int want, int inc_key, int dec_key) {
    if (sd.stage != stage) { sd.stage = stage; sd.last = -999; sd.flip = 0; sd.n = 0; }
    if (sd.n && cur == sd.last) sd.flip ^= 1;
    sd.last = cur; sd.n++;
    return (want > cur) ^ sd.flip ? inc_key : dec_key;
}

static int hp_of(uint32_t M) { return (int)R32(R32(M + M_STATS)); }
static int hpmax_of(uint32_t M) { return (int)R32(R32(M + M_STATS) + 4); }
static int ko_of(uint32_t M) { return (R32(M + M_FLAGS) & MF_KO) != 0 || hp_of(M) <= 0; }
// 전원 대상 아이템인가: 설명이 "전체"로 시작한다(레코드의 어느 비트인지는 모르겠다: +0xe 비트 1은 퍼센트 회복에도 붙어 있다).
static int item_is_all(uint32_t db, int id) {
    uint32_t d = CALL(FN_ITEM_DESC, db, (uint32_t)id);
    return d && !strncmp(gstr(d), "\xc0\xfc\xc3\xbc", 4);
}
static int item_cat_tab(int id) { return id < 8 ? 1 : id < 12 ? 2 : id < 19 ? 3 : id < 22 ? 4 : 5; } // 분류 탭(ui_battle.c GOT_CAT 8,12,19,22,28)

// 회복량(HP). 고정량 효과는 값 그대로, 퍼센트 효과는 최대 HP 기준.
static int heal_amount(uint32_t rec, int hpmax) {
    int e = (int)R8(rec + 0x14), v = (int)R8(rec + 0x1a);
    return e == EFF_HEAL_FIX ? v : e == EFF_HEAL_PCT ? hpmax * v / 100 : 0;
}

// 이 차례의 행동을 정한다. 아이템 규칙은 파일 위 설명 참고.
static void make_plan(uint32_t B, uint32_t M) {
    uint32_t P = R32(B + B_P), E = R32(B + B_E), inv = P + P_INV, db = R32(B + B_ITEMDB);
    int n = (int)R32(P + P_N); if (n > 8) n = 8;
    memset(&plan, 0, sizeof plan);
    plan.kind = PL_ATTACK;

    // 1) 부활: 전투불능이 둘 이상이고 전원 대상 부활(미라클포션)이 있으면 그것, 아니면 가장 번호가 낮은(홀리워터) 것
    int ko_n = 0; uint32_t ko_first = 0;
    for (int i = 0; i < n; i++) {
        uint32_t T = R32(P + P_MEM + 4 * i);
        if (T && ko_of(T)) { if (!ko_n) ko_first = T; ko_n++; }
    }
    if (ko_n) {
        int pick = -1, pick_all = 0;
        for (int id = 0; id < ITEM_LAST; id++) {
            if (!R8(inv + INV_COUNT + id)) continue;
            uint32_t rec = CALL(FN_ITEM_REC, db, (uint32_t)id);
            if (!rec || R8(rec + 0x14) != EFF_REVIVE) continue;
            int all = item_is_all(db, id);
            // 둘 이상이면 전원 대상 우선, 하나면 단일 대상 우선
            if (pick < 0 || (all == (ko_n >= 2) && pick_all != (ko_n >= 2))) pick = id, pick_all = all;
        }
        if (pick >= 0) {
            plan.kind = PL_ITEM; plan.item = pick; plan.tab = item_cat_tab(pick); plan.all = pick_all; plan.tgt = pick_all ? 0 : ko_first;
            ab.last_item_tgt = ko_first; ab.acted_since = 0;
            return;
        }
    }

    // 2) 회복: 가장 위급한 아군과 낮은 아군 수
    uint32_t low = 0; int low_n = 0, low_pct = 101;
    for (int i = 0; i < n; i++) {
        uint32_t T = R32(P + P_MEM + 4 * i);
        if (!T || ko_of(T) || hpmax_of(T) <= 0) continue;
        int pct = hp_of(T) * 100 / hpmax_of(T);
        if (pct > LOW_HP_PCT) continue;
        low_n++;
        if (pct < low_pct) low = T, low_pct = pct;
    }
    if (low && !(low == ab.last_item_tgt && !ab.acted_since && low_pct > CRIT_HP_PCT)) {
        int missing = hpmax_of(low) - hp_of(low), best = -1, best_amt = 0, best_all = 0;
        for (int pass = 0; pass < 2 && best < 0; pass++) {   // pass 0: 전원 대상 우선(여럿이 낮을 때만), pass 1: 전부
            if (pass == 0 && low_n < ALL_HEAL_MIN_LOW) continue;
            for (int id = 0; id < ITEM_LAST; id++) {
                if (!R8(inv + INV_COUNT + id)) continue;
                uint32_t rec = CALL(FN_ITEM_REC, db, (uint32_t)id);
                if (!rec) continue;
                int e = R8(rec + 0x14), all = item_is_all(db, id);
                if (e != EFF_HEAL_FIX && e != EFF_HEAL_PCT) continue;
                if (R8(rec + 0x15) != 0xff) continue;        // 부가 효과가 붙은 것(리미트 등)은 회복용이 아니다
                if (pass == 0 && !all) continue;
                int amt = heal_amount(rec, hpmax_of(low));
                // 필요한 만큼을 채우는 가장 작은 것, 없으면 가장 큰 것
                // 등급은 번호 순(라이트<하이<메가<기가). 채우는 것 가운데 단일 대상을 먼저, 그다음 낮은 번호. 못 채우면 가장 많이 채우는 것.
                int fit = amt >= missing;
                int best_fit = best >= 0 && best_amt >= missing;
                int better = best < 0 || (fit && !best_fit) || (fit && best_fit && best_all && !all) || (!fit && !best_fit && amt > best_amt);
                if (better) best = id, best_amt = amt, best_all = all;
            }
        }
        if (best >= 0) {
            plan.kind = PL_ITEM; plan.item = best; plan.tab = item_cat_tab(best); plan.all = best_all; plan.tgt = best_all ? 0 : low;
            ab.last_item_tgt = low; ab.acted_since = 0;
            return;
        }
    }

    // 3) 필살기: MG 가득, 필살기 보유, 하위메뉴가 쓸 수 있다고 한 때(CM+0x44 비트 2가 사용 불가). 마지막 적이 거의 죽었으면 아낀다.
    uint32_t CM = R32(B + B_CM);
    int mg_full = R32(M + M_MGMAX) > 0 && R32(M + M_MG) >= R32(M + M_MGMAX);
    // 필살기도 MP를 쓰는 것이 있다(레코드 +3). MP가 모자란 것은 후보에서 빼고, 남은 것 가운데 무작위로 하나(사용자 결정). 후보가 없으면 기본 공격.
    int cand[32], nc = 0, sp_ok = 0;
    if (mg_full) {
        int n = (int)R32(M + M_SPECIALN); if (n > 32) n = 32;
        int mp = (int)R32(R32(M + M_STATS) + 8);
        for (int i = 0; i < n; i++) {
            uint32_t rec = CALL(FN_SKILL_REC, R32(B + B_SKILLDB), (uint32_t)R8(M + M_SPECIALS + i));
            if (rec && mp >= (int)R8(rec + 3)) cand[nc++] = i;
        }
        if (nc) { static uint32_t rs; if (!rs) rs = (uint32_t)now_ms() | 1; rs = rs * 1664525u + 1013904223u; plan.sp = cand[(rs >> 16) % (uint32_t)nc]; sp_ok = 1; }
    }
    if (sp_ok && !(R32(CM + CM_SUBBITS) & SBIT_SPECIAL_OFF)) {
        int alive = 0, last = -1;
        for (int i = 0; i < enemy_count(E); i++) if (enemy_alive(enemy_at(E, i))) alive++, last = i;
        int save = 0;
        if (alive == 1) {
            uint32_t X = enemy_at(E, last);
            int mx = (int)R32(R32(X + X_STATS) + 4), h = (int)R32(R32(X + X_STATS));
            save = mx > 0 && h * 100 <= SAVE_SPECIAL_ENEMY_PCT * mx;
        }
        if (!save) { plan.kind = PL_SPECIAL; ab.acted_since = 1; return; }
    }
    ab.acted_since = 1;
}

static void tap(int key) {
    if (ui_key_tap(key)) { ab.next_ms = now_ms() + GAP_MS; ab.tries++; }
}

// 아이템·필살기를 포기하고 기본 공격으로
static void abort_item(void) { plan.kind = PL_ATTACK; ab.acted_since = 1; }

// 명령 차례 한 걸음. 지금 화면 상태를 보고 plan대로 한 단계씩 간다.
static void choose_command(uint32_t B, uint32_t M) {
    uint32_t P = R32(B + B_P), E = R32(B + B_E), CM = R32(B + B_CM);
    int cur = enemy_cursor(E), acur = ally_cursor(P);
    if (plan.kind != PL_ATTACK && ++ab.stage_tries > ITEM_STAGE_TRIES) abort_item();

    if (cur >= 0) {                                   // 적 대상 선택 중
        if (plan.kind == PL_ITEM) { tap(UI_KEY_CLEAR); return; }   // 아이템이 적을 고르게 됐다: 나간다
        // 위아래는 적 사이를, 좌우는 적과 아군 사이를 옮긴다(시험으로 확인, 끝에서 순환하지 않는다).
        // 화면 y가 위인 쪽으로 가려면 위, 아래면 아래. 키가 커서를 못 옮겼으면 반대 방향을 시도하고, 끝내 안 되면 아무거나 친다.
        int want = choose_target(E);
        if (want < 0 || want == cur || ab.dir >= 2 * DIR_MAX) { tap(UI_KEY_OK); return; }
        if (ab.dir && cur == ab.last_cur) ab.flip ^= 1;
        ab.last_cur = cur;
        int up = (int)R32(enemy_at(E, want) + X_Y) < (int)R32(enemy_at(E, cur) + X_Y);
        tap(up ^ ab.flip ? UI_KEY_UP : UI_KEY_DOWN);
        ab.dir++;
        return;
    }
    if (acur >= 0) {                                  // 아군 대상 선택 중
        if (plan.kind == PL_ITEM) {
            if (plan.all || !plan.tgt) { tap(UI_KEY_OK); return; }
            int want = -1;
            for (int i = 0; i < (int)R32(P + P_N) && i < 8; i++) if (R32(P + P_MEM + 4 * i) == plan.tgt) want = i;
            if (want < 0 || want == acur) { tap(UI_KEY_OK); return; }
            tap(steer(3, acur, want, UI_KEY_DOWN, UI_KEY_UP));
            if (sd.n > 8) abort_item();
            return;
        }
        if (ab.attacking) { tap(UI_KEY_LEFT); return; }   // 대상이 아군 쪽으로 넘어갔다: 적 쪽으로 되돌린다
        tap(UI_KEY_CLEAR); return;
    }
    ab.dir = 0; ab.flip = 0; ab.last_cur = -1;
    uint32_t bits = R32(CM + CM_SUBBITS), list = R32(CM + CM_LIST);
    if (list) {                                       // 아이템·조합 창
        uint32_t LW = CM + CM_LW;
        if (plan.kind != PL_ITEM || R32(LW + LW_MODE) != 1) { tap(UI_KEY_CLEAR); return; }
        int tab = (int)R32(LW + LW_TAB), sel = (int)R32(LW + LW_SEL);
        if (tab != plan.tab) tap(steer(1, tab, plan.tab, UI_KEY_DOWN, UI_KEY_UP));     // 분류: ↑↓
        else if (sel != plan.item) tap(steer(2, sel, plan.item, UI_KEY_RIGHT, UI_KEY_LEFT)); // 칸: ←→
        else tap(UI_KEY_OK);
        if (sd.n > 12) abort_item();
        return;
    }
    if (bits & SBIT_SKILL_OPEN) {                     // 스킬·필살 목록
        if (plan.kind == PL_SPECIAL && R32(CM + CM_SL + SL_MODE) == 2) {   // 고른 필살기까지 ↑↓(순환)로 옮긴 뒤 OK
            int at = (int)R32(CM + CM_SL + SL_SCROLL) + (int)R32(CM + CM_SL + SL_CUR);
            if (at != plan.sp) { tap(steer(4, at, plan.sp, UI_KEY_DOWN, UI_KEY_UP)); if (sd.n > 12) abort_item(); }
            else { tap(UI_KEY_OK); ab.attacking = 1; }
        }
        else tap(UI_KEY_CLEAR);
        return;
    }
    if (R32(CM + CM_SUB)) {                           // 공격 하위메뉴
        if (plan.kind == PL_ITEM) { tap(UI_KEY_CLEAR); return; }
        W32(CM + CM_SUBCUR, plan.kind == PL_SPECIAL ? 2 : 0);   // 0 기본, 2 필살
        ab.attacking = 1;
        tap(UI_KEY_OK);
        return;
    }
    if (plan.kind == PL_ITEM) { tap('*'); return; }   // 명령 메뉴에서 아이템 창 바로 열기(원본 키 '*')
    W32(CM + CM_CUR, 0); W32(CM + CM_SCROLL, 0);      // 명령 메뉴: Attack
    tap(UI_KEY_OK);
}

// 0x109f0(장면, 입력): 전투 장면 틱(갱신 0x1c4cc → 그리기 0x1bcd4). 원본을 돌린 뒤 이번 상태를 보고 다음 키를 넣는다.
void orig_f_109f0(cpu_t *c);
static void ab_tick(uint32_t scene);
void f_109f0(cpu_t *cc) {
    uint32_t scene = cc->r[0];
    orig_f_109f0(cc);
    uint32_t regs[16];                                // 아래에서 게스트 함수를 불러도 원본이 돌려준 레지스터를 지킨다
    memcpy(regs, cc->r, sizeof regs);
    ab_tick(scene);
    memcpy(cc->r, regs, sizeof regs);
}

static void ab_tick(uint32_t scene) {

    if (!ui_battle_auto()) { ab.was_on = 0; ab.in_cmd = 0; return; }
    // 0x109f0은 전투 밖 장면도 이 모양으로 불린다(scene+0x1c8이 쓰레기). 게임의 현재 장면이 전투일 때만.
    uint32_t G = ext_game();
    if (!G || R32(G + G_SCENE) != SCENE_BATTLE) return;
    uint32_t B = R32(scene + G_BATTLE);
    if (!B || B != R32(G + G_BATTLE)) return;
    uint64_t now = now_ms();
    if (!ab.was_on) { ab.was_on = 1; ab.claim_ms = now; ab.in_cmd = 0; }
    // 부활석 창(restored/revive.c)이 묻는 동안은 키를 넣지 않는다: OK가 부활석을 쓰게 되므로 사람이 고른다.
    if (ext_revive_asking()) return;
    uint32_t st = R32(B + B_STATE);

    if (st == ST_RESULT) {                            // 결과 화면: 잠깐 보여 준 뒤 쪽마다 OK
        if (ab.last_st != ST_RESULT) ab.next_ms = now + RESULT_FIRST_MS;
        ab.last_st = st;
        if (now >= ab.next_ms && ui_key_tap(UI_KEY_OK)) ab.next_ms = now + RESULT_GAP_MS;
        return;
    }
    ab.last_st = st;

    uint32_t P = R32(B + B_P);
    uint32_t atb = R32(P + P_ATB), M = atb ? atb - M_ACT : 0;
    // 명령을 받는 중: 원본 키 처리(0x1c49c)는 ATB 찬 멤버의 행동 상태(M+0x60)가 4일 때 키를 받는다. 관찰: 4 = 명령 메뉴(공격 하위메뉴 포함), 9 = 대상 선택.
    // 액티브 모드는 적이 움직이는 동안에도(전투 상태 4·6 등) 메뉴가 열려 있어 전투 상태로는 못 가린다.
    int cmd = M && R32(P + P_COMMANDING) != 3 && (R32(M + M_ACTION) >= ACT_READY && R32(M + M_ACTION) <= ACT_TARGET) && !(R32(M + M_FLAGS) & (MF_KO | MF_ACTING));
    if (!cmd) { ab.in_cmd = 0; return; }
    if (!ab.in_cmd || M != ab.actor) {                // 새 명령 차례
        ab.in_cmd = 1; ab.actor = M; ab.tries = 0; ab.dir = 0; ab.flip = 0; ab.last_cur = -1; ab.attacking = 0;
        ab.claim_ms = now;
        ab.next_ms = now + DELAY_MS;
        ab.stage_tries = 0; sd.stage = 0;
        make_plan(B, M);
    }
    if (ui_human_ms() > ab.claim_ms) return;          // 사람이 만졌다: 이 차례는 사람 몫
    if (now < ab.next_ms) return;
    if (ab.tries > MAX_TRIES) {                       // 키가 계속 안 먹으면 멈추고 알린다
        ui_toggle_battle_auto();
        ext_toast("자동 전투 멈춤: 명령을 넣지 못했습니다");
        return;
    }
    choose_command(B, M);
}

// ---- 화면: AUTO·×2 버튼(ui/battle_auto.rml). restored/ui_battle.c의 전투 그리기 끝에서 부른다 ----
void ab_hud(void) {
    ui_begin("battle_auto");
    ui_bool("narrow", ui_screen_w() < 400);
    ui_bool("auto", ui_battle_auto());
    ui_bool("fast", ui_battle_fast());
    ui_end();
}
