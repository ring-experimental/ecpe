// 조우율 향: 정적의 향(IT_INCENSE_LOW 250), 유혹의 향(IT_INCENSE_HIGH 251). 원작에 없다(사용자 요청 2026-11-10).
//
// ---- 원작의 필드 무작위 조우(조사 결과) ----
//  조우 객체 E = [G+0x1c4]. 주인공이 한 칸(14px) 움직일 때마다 0x259e4/0x25a30이 0x1afac(E, 1)을 불러 걸음 카운터 E+0x34를 1 올린다
//  (G+0x14 비트 4가 켜져 있으면 안 센다). 필드 장면 틱 0x1054c가 매 틱 0x1aee8(E, 파티)을 불러 조우를 정한다:
//    E+4, E+8(그 자리에 조우 표가 있다), E+0xe, E+0xf가 0이면 조우 없다. E+0x3c(전투 직후 유예)가 남아 있으면 줄이고 조우 없다.
//    문턱 = E+0x18(최소 걸음) + 난수(E+0x19-E+0x18)(한 번 뽑아 E+0x38에 표시), 파티 효과(조우 반감 비트 [P+4]&4 등)로 보정.
//    카운터(E+0x34) >= 문턱이면 카운터를 지우고 조우(1)를 돌려 필드 틱이 전투 장면으로 넘긴다.
//  그래서 조우까지의 걸음 수는 문턱/걸음당 증가량이고, 증가량을 곱하면 조우율을 곱하는 것과 같다. 문턱 계산(0x1aee8)과 난수(0x67a8, rates.c)는
//  건드리지 않고 0x1afac의 증가량 n만 바꾼다. 정수라 소수는 나머지를 모아 둔다(acc).
//    정적의 향 ×0.25: 4걸음에 1을 센다. 첫 필드(아이언우드)는 문턱이 55~70걸음이니 평소 평균 약 62걸음마다 만나던 것이 약 250걸음마다.
//      0(전혀 안 만남)이 아닌 이유: 300걸음 동안 사냥할 이유가 사라지고 보상이 너무 크며, 다른 RPG의 소모품 대부분도 완전 차단이 아닌
//      확률 감소다(포켓몬 순백의 향·퓨어 인센스 ×2/3, FF7 '적 회피' 반감, DQ 성수는 약한 적만). ×0.25는 '거의 안 만나지만 아주 안전하진 않음'.
//    유혹의 향 ×3: 걸음마다 3을 센다(평균 약 21걸음마다). 레벨 올리기용이라 ×3이면 체감이 분명하고 연속 조우로 부담스럽진 않다
//      (E+0x3c 유예와 최소 걸음이 있어 연속 전투는 없다).
//  지속은 걸음(칸)으로 300. 마을 안 걸음도 센다(포켓몬 방충제와 같다. 마을에서 쓰고 필드로 나가는 것이 자연스럽다).
//  기록: ext_set("incense.mode") 0 없음 1 정적 2 유혹, "incense.left" 남은 걸음. 같은 향이면 더한다(상한 CAP), 다른 향이면 바뀐다(300).
//  표시: 필드 HUD(ui/hud.rml)에 작은 표지, 끝나면 토스트.
#include "restored.h"
#include "items_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { STEPS_PER_USE = 300, CAP = 900, PCT_QUIET = 25, PCT_LURE = 300, MODE_NONE = 0, MODE_QUIET = 1, MODE_LURE = 2 };

static int acc; // 나머지(백분율 소수 모음)
static int raw_steps, last_enc_step; // 시험용 걸음 수

static const char *mode_name(int m) { return m == MODE_QUIET ? "정적의 향" : "유혹의 향"; }

static int incense_use(int id, int where) {
    (void)where;
    int want = id == IT_INCENSE_LOW ? MODE_QUIET : MODE_LURE;
    int mode = ext_get("incense.mode", 0), left = ext_get("incense.left", 0);
    char b[160];
    if (left <= 0) mode = MODE_NONE;
    if (mode == want) {
        if (left >= CAP) { snprintf(b, sizeof b, "%s은 더 피울 수 없습니다(최대 %d걸음)", mode_name(want), CAP); ext_toast(b); return 0; }
        left += STEPS_PER_USE;
        if (left > CAP) left = CAP;
        snprintf(b, sizeof b, "%s을 더했습니다(남은 %d걸음)", mode_name(want), left);
    } else {
        left = STEPS_PER_USE;
        if (mode == MODE_NONE) snprintf(b, sizeof b, "%s을 피웠습니다(%d걸음)", mode_name(want), left);
        else snprintf(b, sizeof b, "%s이 %s으로 바뀌었습니다(%d걸음)", mode_name(mode), mode_name(want), left);
        acc = 0;
    }
    ext_set("incense.mode", want);
    ext_set("incense.left", left);
    ext_toast(b);
    return 1;
}
__attribute__((constructor)) static void incense_register(void) {
    ext_register_use(IT_INCENSE_LOW, incense_use);
    ext_register_use(IT_INCENSE_HIGH, incense_use);
}

// 필드 HUD용: 지금 향의 종류(0 없음)와 남은 걸음
int ext_incense_hud(int *left) {
    int m = ext_get("incense.mode", 0), l = ext_get("incense.left", 0);
    if (l <= 0) m = 0;
    if (left) *left = l;
    return m;
}

// 걸음 카운터 증가 0x1afac(E, n). 향이 있으면 n을 바꾸고 남은 걸음을 줄인다.
void orig_f_1afac(cpu_t *c);
void f_1afac(cpu_t *c) {
    int n = (int)c->r[1];
    raw_steps += n > 0 ? n : 0;
    int mode = ext_get("incense.mode", 0), left = ext_get("incense.left", 0);
    if (n > 0 && mode != MODE_NONE && left > 0) {
        int pct = mode == MODE_QUIET ? PCT_QUIET : PCT_LURE;
        acc += n * pct;
        c->r[1] = (uint32_t)(acc / 100);
        acc %= 100;
        left -= n;
        if (left <= 0) {
            ext_set("incense.mode", 0);
            ext_set("incense.left", 0);
            acc = 0;
            char b[96];
            snprintf(b, sizeof b, "%s이 다 타서 사라졌습니다", mode == MODE_QUIET ? "정적의 향" : "유혹의 향");
            ext_toast(b);
        } else ext_set("incense.left", left);
    }
    orig_f_1afac(c);
}

// ---- 시험용: INCENSE_SIM=걸음수 이면 필드에서 한 번, 실제 0x1afac/0x1aee8을 그만큼 되풀이해 향 없음/정적/유혹의 조우 간격을 잰다 ----
// (전투로 넘어가지 않는다. 조우 객체의 카운터는 끝나고 되돌려 놓는다.)
void ext_incense_tick(uint32_t G) {
    static int done;
    const char *e = getenv("INCENSE_SIM");
    if (done || !e) return;
    done = 1;
    int total = atoi(e);
    uint32_t E = R32_(&cpu, G + 0x1c4), P = R32_(&cpu, G + 0x1c0);
    if (!E) return;
    { uint32_t ro = CALL(0x100000 + 0x144b0); fprintf(stderr, "[sim] rng obj=%x r:", ro); for (int i = 0; i < 8; i++) fprintf(stderr, " %u", CALL(0x100000 + 0x67a8, ro, 15)); fprintf(stderr, " E+0x18=%d,%d E+0x38=%d\n", R8_(&cpu, E + 0x18), R8_(&cpu, E + 0x19), R32_(&cpu, E + 0x38)); }
    uint32_t sv[0x40 / 4];
    for (int i = 0; i < 0x40 / 4; i++) sv[i] = R32_(&cpu, E + 4u * (uint32_t)i);
    int m0 = ext_get("incense.mode", 0), l0 = ext_get("incense.left", 0);
    for (int mode = 0; mode <= 2; mode++) {
        ext_set("incense.mode", mode);
        ext_set("incense.left", mode ? 1000000 : 0);
        acc = 0;
        int enc = 0, last = 0, mn = 1 << 30, mx = 0, steps = 0;
        for (steps = 1; steps <= total; steps++) {
            CALL(0x100000 + 0x1afac, E, 1);   // 0x1afac(E, 1): 걸음 하나
            uint32_t r = CALL(0x100000 + 0x1aee8, E, P);
            if (r == 1) { int iv = steps - last; last = steps; enc++; if (iv < mn) mn = iv; if (iv > mx) mx = iv; }
        }
        fprintf(stderr, "[sim] mode=%d steps=%d encounters=%d avg=%.1f min=%d max=%d\n", mode, total, enc, enc ? (double)total / enc : 0.0, enc ? mn : 0, mx);
    }
    for (int i = 0; i < 0x40 / 4; i++) W32_(&cpu, E + 4u * (uint32_t)i, sv[i]);
    ext_set("incense.mode", m0);
    ext_set("incense.left", l0);
    acc = 0;
}

// ---- 시험용: INCENSE_LOG=1이면 실제 걸음 수(0x1afac 부름 수)와 조우를 적는다. =2면 조우를 막고(전투로 안 넘어간다) 적기만 한다 ----
void orig_f_1aee8(cpu_t *c);
void f_1aee8(cpu_t *c) {
    orig_f_1aee8(c);
    static const char *e; static int init;
    if (!init) { init = 1; e = getenv("INCENSE_LOG"); }
    if (e && c->r[0] == 1) {
        fprintf(stderr, "[enc] steps=%d interval=%d mode=%d left=%d\n", raw_steps, raw_steps - last_enc_step, ext_get("incense.mode", 0), ext_get("incense.left", 0));
        last_enc_step = raw_steps;
        if (atoi(e) == 2) c->r[0] = 0;
    }
}
