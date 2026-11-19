// 전투 보상 배율(기타 설정, 원작에 없다. 사용자 요청). 값은 호스트의 기타 설정 창(ui/etc.rml)이 ui.ini에 두고 ui_rate로 준다.
//
// 승리 계산은 0x1cb88(전투, 키) 한 함수가 다 한다(전투 상태 0x16, 상태 안 카운터 +0x34가 7을 넘는 틱에 한 번).
//   EXP: 적마다 더한 값을 아군 진영 P+0x30에 적고, 골드를 P+0x34에 적은 뒤 0x25ee8(P+0xa8 지갑, 골드)로 돈을 더한다.
//        그 뒤 멤버마다 P+0x30을 EXP(+0x104)에 더하고 레벨업을 본다. 결과 화면도 P+0x30/+0x34를 보인다.
//        그래서 0x25ee8을 감싸 승리 계산 중에 불리면 그 자리에서 P+0x30, P+0x34와 더할 골드를 함께 곱한다.
//   드롭: 적의 드롭 칸 3개마다 (0x67a8(난수, 1000) - 행운 보정) / 10(1 이상)을 칸의 확률 바이트(E+0x3dc+i)와 견줘,
//        바이트가 크거나 같으면 떨어진다. 승리 계산 중의 0x67a8(…, 1000)을 감싸 난수를 배율로 나누면 확률이 배율만큼 는다.
//        확률 바이트가 0인 칸(떨어뜨리지 않는 적)은 그대로 안 떨어진다.
// 배율이 모두 100%면 아무것도 바꾸지 않는다.
#include "restored.h"

enum { P_EXP = 0x30, P_GOLD = 0x34, P_PURSE = 0xa8, EXP_MAX = 9999999, GOLD_MAX = 9999999 };

static int in_victory;

static uint32_t scaled(uint32_t v, int pct, uint32_t max) {
    uint64_t r = (uint64_t)v * (uint64_t)pct / 100u;
    if (pct > 0 && v > 0 && r == 0) r = 1; // ×0.5에서 1이 0이 되지 않게
    return r > max ? max : (uint32_t)r;
}

void orig_f_1cb88(cpu_t *c);
void f_1cb88(cpu_t *c) {
    in_victory = 1;
    orig_f_1cb88(c);
    in_victory = 0;
}

void orig_f_25ee8(cpu_t *c);
void f_25ee8(cpu_t *c) {
    if (in_victory) {
        uint32_t P = c->r[0] - P_PURSE;
        int pe = ui_rate(0), pg = ui_rate(1);
        if (R32(P + P_GOLD) == c->r[1]) { // 승리 계산이 방금 적은 골드를 더하는 자리인지 확인
            if (pe != 100) W32(P + P_EXP, scaled(R32(P + P_EXP), pe, EXP_MAX));
            if (pg != 100) {
                c->r[1] = scaled(c->r[1], pg, GOLD_MAX);
                W32(P + P_GOLD, c->r[1]);
            }
        }
    }
    orig_f_25ee8(c);
}

void orig_f_67a8(cpu_t *c);
void f_67a8(cpu_t *c) {
    uint32_t n = c->r[1];
    orig_f_67a8(c);
    if (in_victory && n == 1000) {
        int pd = ui_rate(2);
        if (pd != 100) {
            uint64_t r = (uint64_t)c->r[0] * 100u / (uint64_t)pd;
            c->r[0] = (uint32_t)(r >= n ? n - 1 : r);
        }
    }
}
