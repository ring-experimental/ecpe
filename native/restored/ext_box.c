// 랜덤 상자(248) 처리기. 열면 원작 아이템 중 하나를 준다(장비 위주, 장비는 가격이 비쌀수록 드물게).
// 사용자 결정(2026-11-10): 새 프리미엄 아이템(246~)은 상자에서 나오지 않는다. 원작 아이템·장비만, 소모품보다 장비가 잘 나온다.
// 근거: 가챠류 상자는 "무엇이 나오는지 확률을 보여 주고, 등급을 색으로 구분하며, 결과를 카드로 한 번 보여 준다"(이 파일은 확률과 등급,
// 결과 카드용 기록을 맡고, 카드는 ui/premium.rml이 그린다). 확률표는 아래 상수만 고치면 된다.
#include "restored.h"
#include "items_ext.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// ---- 확률표(1000분율) ----
enum {
    BOX_PCT_CONSUMABLE = 300, // 원작 소모품이 나올 확률. 나머지(700)는 장비·장신구
    BOX_CONS_MAX_N = 3,       // 소모품 개수는 1~이 값. 값비싼 소모품(가격 CONS_PRICE_ONE 이상)은 늘 1개
    CONS_PRICE_ONE = 3000,
    EQUIP_PRICE_BASE = 100,   // 장비 무게 = 10000 / (이 값 + 가격): 가격이 비쌀수록 드물다
    // 등급(가격 기준)
    RARITY_GOOD = 300, RARITY_RARE = 2000, RARITY_LEGEND = 8000,
};

enum { APP_PTR = 0x14ee34, G_ITEMDB = 0x1cc, REC_KIND = 0, REC_PRICE = 8, REC_FLAGS = 0xc, FLAG_NO_SELL = 1 << 18 };
#define G32(a) R32_(&cpu, (a))

static uint32_t rng_s;
static uint32_t rnd(uint32_t n) { // xorshift32, 0..n-1
    if (!rng_s) rng_s = (uint32_t)time(NULL) * 2654435761u | 1;
    rng_s ^= rng_s << 13; rng_s ^= rng_s >> 17; rng_s ^= rng_s << 5;
    return rng_s % n;
}

static ext_reward_t last;
ext_reward_t ext_box_last(void) { return last; }
const char *ext_rarity_name(int r) {
    static const char *const n[] = {"일반", "고급", "희귀", "전설"};
    return r >= 0 && r < 4 ? n[r] : "";
}
static int rarity_of_price(uint32_t price) {
    return price >= RARITY_LEGEND ? 3 : price >= RARITY_RARE ? 2 : price >= RARITY_GOOD ? 1 : 0;
}

// 원작 아이템 중 종류 조건에 맞는 것을 하나 고른다(무게 합으로 뽑는다). equip면 장비(종류 1~5), 아니면 소모품(종류 0).
static int pick_orig(uint32_t db, int equip) {
    uint32_t rec = G32(db + 8);
    uint64_t total = 0;
    for (int pass = 0; pass < 2; pass++) {
        uint64_t at = pass ? rnd((uint32_t)(total ? total : 1)) : 0, acc = 0;
        for (int id = 0; id < IT_EXT_FIRST; id++) {
            uint32_t r = rec + 32u * (uint32_t)id;
            int kind = R8_(&cpu, r + REC_KIND);
            uint32_t price = G32(r + REC_PRICE), flags = G32(r + REC_FLAGS);
            if (kind == 6 || (flags & FLAG_NO_SELL) || ext_count(id) >= IT_MAX_COUNT) continue;
            if (equip ? (kind < 1 || kind > 5 || price == 0) : kind != 0) continue;
            uint32_t w = equip ? 10000u / (EQUIP_PRICE_BASE + price) : 1;
            if (!w) w = 1;
            if (!pass) total += w;
            else { acc += w; if (at < acc) return id; }
        }
    }
    return -1;
}

static int box_use(int id, int where) {
    (void)id; (void)where;
    uint32_t G = ext_game(), db = G ? G32(G + G_ITEMDB) : 0;
    if (!db) return 0;
    int got = -1, n = 1, rar = 0;
    for (int tries = 0; tries < 8 && got < 0; tries++) {
        uint32_t roll = rnd(1000);
        {
            int equip = roll >= BOX_PCT_CONSUMABLE;
            int id = pick_orig(db, equip);
            if (id < 0) continue;
            uint32_t price = G32(G32(db + 8) + 32u * (uint32_t)id + REC_PRICE);
            got = id; rar = rarity_of_price(price);
            n = !equip && price < CONS_PRICE_ONE ? 1 + (int)rnd(BOX_CONS_MAX_N) : 1;
        }
    }
    if (got < 0) { ext_toast("상자가 열리지 않았습니다(보관함이 가득 찼습니다)"); return 0; }
    n = ext_give(got, n);
    last = (ext_reward_t){got, n, rar};
    char b[160];
    snprintf(b, sizeof b, "랜덤 상자: %s ×%d 획득!%s", ext_item_name(got), n, rar >= 2 ? (rar == 3 ? " (전설)" : " (희귀)") : "");
    ext_toast(b);
    return 1;
}
__attribute__((constructor)) static void box_register(void) { ext_register_use(IT_RANDOM_BOX, box_use); }
