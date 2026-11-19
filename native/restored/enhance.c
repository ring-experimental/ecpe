// 장비 강화 +1~+10(원작에 없다. 사용자 요청 2026-11-10). 재료는 강화석(items_ext.h의 IT_ENHANCE).
//
// 모델: 강화는 '캐릭터의 장비칸'에 붙는다. 원작은 캐릭터 +0x140+4k(k=0~4)가 아이템 레코드를 가리키는 포인터이고 같은 아이템은
// 한 레코드를 나눠 쓰므로 개체마다 저장할 곳이 없다. 그래서 ext "enh.<캐릭터 id>.<칸>"에 단계를 두고, 장비를 바꿔도 칸의 단계가 남는다.
//
// 효과: 칸 아이템의 기본 능력치(레코드 +0x10 u16, +0x12 u8)에 단계 × STEP_PCT %를 올림해서 더한다. 무기칸은 물공·마공,
// 방어구칸은 물방·마방(원본 0x167a0이 레코드 플래그 비트16으로 가르는 것과 같다). 기본값에 비례하므로 초반 장비는 +1~+10에 몇 점,
// 후반 장비는 수십 점이고, +10이면 그 장비의 기본값만큼(×2) 오른다. 장신구는 기본 능력치가 0이라 강화할 수 없다.
//
// 반영 위치(모두 같은 식이라 어긋나지 않는다)
//   1. 전투: 캐릭터 +0x124(물공=직업 계수+무기 u16) +0x128(마공) +0x12c(물방) +0x130(마방). 스킬 데미지 0x16d14가 이 필드를 곱한다.
//      필드는 장비 때 원본 0x167a0이 ±로 맞추므로, 강화는 '직업 기본값 + 장비 + 강화'로 처음부터 다시 맞춘다(enh_sync).
//      상태가 없어서 불러오기·새 게임·장비 교체 어느 경로로 바뀌어도 다음 맞춤에서 바로잡힌다. 맞춤 시점: 장비 장착·해제(0x16534, 0x16598) 직후,
//      필드 메뉴 매 틱(0x109b8), 전투 시작(0x1b1d0), 강화 직후.
//   2. 장비·스테이터스 화면: 원본이 레코드에서 직접 계산하므로(필드 아님) 새 UI의 식(ui_fmcore.c, ui_fmchar.c)에 같은 보너스를 더한다.
//      원작 그리기(F2)는 그리는 동안만 레코드 능력치를 보정한다(enh_patch_begin/end).
//
// 강화 화면(ui/enhance.rml): 장비 화면의 * 키(새 UI는 버튼 띠의 「강화」)로만 연다(사용자 결정 2026-11-11: 상점은 강화석 받기만). 입력은 필드 메뉴 키 분배 0x2fa60을
// 감싸 열려 있는 동안 키를 모두 가져가고, 그리기는 메뉴 장면 틱 0x109b8의 끝에 얹는다.
#include "restored.h"
#include "items_ext.h"
#include "enhance.h"
#include "fm_shell.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------- 조정 상수
// 단계 n(1~10)으로 올리는 시도의 성공률(%). 단계가 오를수록 낮다. 실패해도 단계는 그대로고 장비는 부서지지 않는다(테스트판).
static const int RATE_PCT[ENH_MAX + 1] = {0, 100, 95, 85, 75, 65, 55, 45, 35, 25, 15};
// 시도 한 번에 쓰는 강화석. 1,1,2,2,3,3,4,4,5,5. (성공까지 기대 소모 = 비용/성공률, +1부터 +10까지 약 91개)
static int cost_of(int target) { return 1 + (target - 1) / 2; }
#define STEP_PCT 10 // 단계마다 기본값의 10%(올림)

enum {
    GOT = 0x14e624, GOT_SLOT_NAMES = 0x4a0,
    APP_PTR = 0x14ee34, APP_GAME = 0x48,
    G_STATE = 0x0c, G_GFX = 0x08, G_PARTY = 0x1c0, G_ITEMDB = 0x1cc, G_MENU = 0x1d4, G_CLASS_NAMES = 0x314, SCENE_MENU = 14,
    M_SPR = 0x04, M_SHEET = 0x14, M_SCREEN = 0x18, M_SCREENS = 0x24,
    PARTY_CHARS = 0x84, PARTY_MAX = 5,
    CH_ID = 0x04, CH_STATS = 0x64, CH_CLASS = 0xb4, CH_EQUIP = 0x140, CH_NAME = 0x19c, CH_STR = 0x118, CH_DEX = 0x11a, CH_INT = 0x11c,
    F_PATK = 0x124, F_MATK = 0x128, F_PDEF = 0x12c, F_MDEF = 0x130,
    ST_LV = 0x18,
    CLS_W_STR = 0x0c, CLS_W_DEX = 0x0d, CLS_W_INT = 0x0e, CLS_PATK = 0x16, CLS_MATK = 0x17, CLS_PDEF = 0x18, CLS_MDEF = 0x19, CLS_NAME = 0x1e,
    IDB_RECS = 0x08, IDB_NAMES = 0x14,
    REC_ICON = 0x02, REC_NAME = 0x04, REC_FLAGS = 0x0c, REC_P = 0x10, REC_M = 0x12, REC_DEF_BIT = 16,
    EQ_SCREEN = 3, EQ_STATE = 0x10, EQ_CHAR = 0x0c, EQ_SLOT = 0x24,
    KEY_UP = 0x1f, KEY_DOWN = 0x1e, KEY_LEFT = 0x1d, KEY_RIGHT = 0x1c, KEY_OK = 0x1b, KEY_LSK = 0x1a, KEY_CLR = 0x10,
    KEY_2 = 0x52, KEY_4 = 0x54, KEY_5 = 0x55, KEY_6 = 0x56, KEY_8 = 0x58, KEY_STAR = 0x4a, KEY_HASH = 0x43,
    PICK_SLOT0 = 0, PICK_MEMBER0 = 10,
    OPEN_LOCK_MS = 250, RESULT_MS = 900, ENH_LOCK_MS = 650,
};

#define G32(a) R32_(&cpu, (a))
#define G16(a) R16_(&cpu, (a))
#define G8(a) R8_(&cpu, (a))
#define FN(x) (0x100000u + (x))

static uint32_t game_obj(void) { return ext_game(); }
static uint32_t party_char(uint32_t game, int i) { return (i < 0 || i >= PARTY_MAX) ? 0 : G32(G32(game + G_PARTY) + PARTY_CHARS + 4u * (uint32_t)i); }
static uint32_t item_db(uint32_t game) { return G32(game + G_ITEMDB); }
static int rec_id(uint32_t game, uint32_t rec) { return (int)((rec - G32(item_db(game) + IDB_RECS)) / 32u); }

// ---------------------------------------------------------------- 단계와 보너스
int enh_step(uint32_t ch, int slot) {
    char k[40];
    snprintf(k, sizeof k, "enh.%d.%d", (int)G8(ch + CH_ID), slot);
    int v = ext_get(k, 0);
    return v < 0 ? 0 : v > ENH_MAX ? ENH_MAX : v;
}
static void step_set(uint32_t ch, int slot, int v) {
    char k[40];
    snprintf(k, sizeof k, "enh.%d.%d", (int)G8(ch + CH_ID), slot);
    ext_set(k, v);
}
int enh_rec_is_def(uint32_t rec) { return (int)((G32(rec + REC_FLAGS) >> REC_DEF_BIT) & 1); }
static int ceil_pct(int base, int step) { return (base * step * STEP_PCT + 99) / 100; }
void enh_bonus(uint32_t rec, int step, int *p, int *m) {
    *p = rec && step > 0 ? ceil_pct((int)G16(rec + REC_P), step) : 0;
    *m = rec && step > 0 ? ceil_pct((int)G8(rec + REC_M), step) : 0;
}
void enh_slot_bonus(uint32_t ch, int slot, uint32_t rec, int *p, int *m) { enh_bonus(rec, enh_step(ch, slot), p, m); }

// ---------------------------------------------------------------- 전투용 필드 맞춤
// 원본 0x167a0(장비·해제)이 필드를 만지는 방식을 그대로 옮긴 기대값: 직업 기본값(+0x16~0x19) + 칸마다 (레코드 u16, u8) + 강화.
// 레코드 플래그 비트16이 있으면 방어(+0x12c, +0x130), 없으면 공격(+0x124, +0x128)에 더한다. 두 캐릭터(LV4, LV2)의 실제 값과 일치함을 확인했다.
void enh_sync(uint32_t ch) {
    if (!ch) return;
    uint32_t cls = G32(ch + CH_CLASS);
    if (!cls) return;
    uint32_t want[4] = {G8(cls + CLS_PATK), G8(cls + CLS_MATK), G8(cls + CLS_PDEF), G8(cls + CLS_MDEF)};
    for (int k = 0; k < ENH_SLOTS; k++) {
        uint32_t rec = G32(ch + CH_EQUIP + 4u * (uint32_t)k);
        if (!rec) continue;
        int bp, bm;
        enh_slot_bonus(ch, k, rec, &bp, &bm);
        int o = enh_rec_is_def(rec) ? 2 : 0;
        want[o] += G16(rec + REC_P) + (uint32_t)bp;
        want[o + 1] += G8(rec + REC_M) + (uint32_t)bm;
    }
    static const int fld[4] = {F_PATK, F_MATK, F_PDEF, F_MDEF};
    for (int i = 0; i < 4; i++)
        if (G32(ch + (uint32_t)fld[i]) != want[i]) W32_(&cpu, ch + (uint32_t)fld[i], want[i]);
}
void enh_sync_all(void) {
    uint32_t g = game_obj();
    if (!g || !G32(g + G_PARTY) || !G32(g + G_ITEMDB)) return;
    for (int i = 0; i < PARTY_MAX; i++) enh_sync(party_char(g, i));
}

void orig_f_16534(cpu_t *c);
void f_16534(cpu_t *c) { // (캐릭터, game, 칸, 아이템 id, 모드) 장착
    uint32_t ch = c->r[0];
    orig_f_16534(c);
    enh_sync(ch);
}
void orig_f_16598(cpu_t *c);
void f_16598(cpu_t *c) { // (캐릭터, game, 칸) 해제
    uint32_t ch = c->r[0];
    orig_f_16598(c);
    enh_sync(ch);
}
void orig_f_1b1d0(cpu_t *c);
void f_1b1d0(cpu_t *c) { // 전투 객체 생성: 시작 전에 모두 맞춘다(불러오기 뒤 첫 전투 등)
    enh_sync_all();
    orig_f_1b1d0(c);
}

// ---------------------------------------------------------------- 원작 그리기용 임시 보정
static struct { uint32_t rec; uint16_t p; uint8_t m; } patched[ENH_SLOTS + 1];
static int npatched;
static void patch_one(uint32_t rec, int bp, int bm) {
    if (!rec || (!bp && !bm)) return;
    for (int i = 0; i < npatched; i++) if (patched[i].rec == rec) return;
    patched[npatched].rec = rec; patched[npatched].p = G16(rec + REC_P); patched[npatched].m = (uint8_t)G8(rec + REC_M);
    npatched++;
    int np = (int)G16(rec + REC_P) + bp, nm = (int)G8(rec + REC_M) + bm;
    W16_(&cpu, rec + REC_P, (uint32_t)(np > 65535 ? 65535 : np));
    W8_(&cpu, rec + REC_M, (uint32_t)(nm > 255 ? 255 : nm));
}
void enh_patch_begin(uint32_t ch, uint32_t cand_rec, int cand_slot) {
    npatched = 0;
    if (!ch) return;
    for (int k = 0; k < ENH_SLOTS; k++) {
        uint32_t rec = G32(ch + CH_EQUIP + 4u * (uint32_t)k);
        int bp, bm;
        enh_slot_bonus(ch, k, rec, &bp, &bm);
        patch_one(rec, bp, bm);
    }
    if (cand_rec && cand_slot >= 0 && cand_slot < ENH_SLOTS) {
        int bp, bm;
        enh_slot_bonus(ch, cand_slot, cand_rec, &bp, &bm);
        patch_one(cand_rec, bp, bm);
    }
}
void enh_patch_end(void) {
    for (int i = 0; i < npatched; i++) { W16_(&cpu, patched[i].rec + REC_P, patched[i].p); W8_(&cpu, patched[i].rec + REC_M, patched[i].m); }
    npatched = 0;
}

// ---------------------------------------------------------------- 강화 화면 상태
static struct {
    int open, member, slot;
    uint64_t opened_ms, last_tick_ms, lock_until_ms, result_until_ms;
    int res, res_flip;       // 결과 연출: 0 없음, 1 성공, 2 실패(같은 결과가 이어져도 연출이 다시 돌도록 res_flip으로 클래스를 바꾼다)
    int res_step;
    char msg[96];
} S;

static uint64_t mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static uint32_t rng_state;
static int rnd100(void) { // 0..99
    if (!rng_state) rng_state = (uint32_t)mono_ms() | 1;
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5;
    return (int)(rng_state % 100u);
}

static const char *char_name(uint32_t game, uint32_t ch) {
    uint32_t user = G32(ch + CH_NAME);
    if (user) return gstr(user);
    return gstr(G32(game + G_CLASS_NAMES) + G16(G32(ch + CH_CLASS) + CLS_NAME));
}
static const char *slot_kind(int k) { return gstr(G32(GOT + GOT_SLOT_NAMES) + 12u * (uint32_t)k); }
static const char *rec_name(uint32_t game, uint32_t rec) { return gstr(G32(item_db(game) + IDB_NAMES) + G16(rec + REC_NAME)); }
static int members_n(uint32_t game) { int n = 0; for (int i = 0; i < PARTY_MAX; i++) if (party_char(game, i)) n++; return n; }

// 칸 강화로 오르는 것이 있는가(기본 능력치가 0이면 강화할 수 없다: 장신구)
static int rec_enhanceable(uint32_t rec) { return rec && (G16(rec + REC_P) || G8(rec + REC_M)); }

// 장비 화면(0x32778)과 같은 식의 능력치 표시(P.ATT, M.ATT, P.DEF, M.DEF). hs_slot의 단계를 hs_step으로 바꿔 계산할 수 있다(미리보기). 999 상한은 호출한 쪽.
static void char_stats(uint32_t ch, int hs_slot, int hs_step, int out[4]) {
    uint32_t cls = G32(ch + CH_CLASS);
    int b = (int)G16(ch + CH_STR) * (int)G8(cls + CLS_W_STR) + (int)G16(ch + CH_DEX) * (int)G8(cls + CLS_W_DEX) + (int)G16(ch + CH_INT) * (int)G8(cls + CLS_W_INT);
    out[0] = (b + 2550) * (int)G8(cls + CLS_PATK) / 2560;
    out[1] = (b + 2550) * (int)G8(cls + CLS_MATK) / 2560;
    out[2] = b * 12 / 1000 + (int)G8(cls + CLS_PDEF);
    out[3] = b * 12 / 1000 + (int)G8(cls + CLS_MDEF);
    for (int k = 0; k < 4; k++) { // 장신구(4)는 화면 식에 없다
        uint32_t rec = G32(ch + CH_EQUIP + 4u * (uint32_t)k);
        if (!rec) continue;
        int bp, bm, step = k == hs_slot ? hs_step : enh_step(ch, k);
        enh_bonus(rec, step, &bp, &bm);
        int o = k == 0 ? 0 : 2;
        out[o] += (int)G16(rec + REC_P) + bp;
        out[o + 1] += (int)G8(rec + REC_M) + bm;
    }
}

// ---------------------------------------------------------------- 그림 떼기(초상화, 장비 아이콘)
static int cell_x, cell_y, cell_h;
static void cells_reset(void) { ui_scratch_clear(); cell_x = cell_y = cell_h = 0; }
static void cell(int w, int h, int *x, int *y) {
    int sw = ui_screen_w();
    if (cell_x + w > sw) { cell_x = 0; cell_y += cell_h; cell_h = 0; }
    if (cell_y + h > 320) cells_reset();
    *x = cell_x; *y = cell_y;
    cell_x += w;
    if (h > cell_h) cell_h = h;
}
static void put_face(const char *key, uint32_t game, uint32_t menu, uint32_t ch) { // ui_fmcore.c put_face와 같다
    int id = (int)G8(ch + CH_ID);
    uint32_t frame = CALL(FN(0x12fe8), game, (uint32_t)id, 0);
    int k = frame == 0x43 ? 8 : id;
    int dx = (int8_t)G8(G32(GOT + 0x458) + (uint32_t)k), dy = (int8_t)G8(G32(GOT + 0x45c) + (uint32_t)k);
    uint32_t gfx = G32(game + G_GFX);
    int x, y;
    cell(40, 40, &x, &y);
    CALL(FN(0x3900), gfx, (uint32_t)x, (uint32_t)y, 40, 40);
    CALL(FN(0x56ec), G32(menu + M_SHEET), frame, (uint32_t)(x + 8), (uint32_t)(y + 2), (uint32_t)dx, (uint32_t)dy, 0);
    CALL(FN(0x3994), gfx);
    ui_sprite(key, x, y, 40, 40);
}
static void put_icon_no(const char *key, uint32_t menu, int icon) { // 장비 아이콘 번호(빈 칸 13)
    int x, y;
    cell(16, 16, &x, &y);
    CALL(FN(0x301c4), 0, menu + M_SPR, (uint32_t)(x + 2), (uint32_t)(y + 2), (uint32_t)icon);
    ui_sprite_trim(key, x, y, 16, 16);
}

// ---------------------------------------------------------------- 강화 화면 그리기
static void bonus_text(char *out, size_t cap, int role_def, int bp, int bm) {
    const char *a = role_def ? "물방" : "물공", *b = role_def ? "마방" : "마공";
    if (bp && bm) snprintf(out, cap, "%s +%d  %s +%d", a, bp, b, bm);
    else if (bp) snprintf(out, cap, "%s +%d", a, bp);
    else if (bm) snprintf(out, cap, "%s +%d", b, bm);
    else snprintf(out, cap, "-");
}

static void keys_hint(const char *k, const char *t, int nav) { ui_item(); ui_int("nav", nav); ui_text("k", k); ui_text("key", nav ? "" : k); ui_text("t", t); ui_close(); }

static void emit(uint32_t game) {
    uint32_t menu = G32(game + G_MENU);
    uint64_t now = mono_ms();
    if (!party_char(game, S.member)) { for (int i = 0; i < PARTY_MAX; i++) if (party_char(game, i)) { S.member = i; break; } }
    uint32_t ch = party_char(game, S.member);
    if (S.slot < 0) S.slot = 0;
    if (S.slot >= ENH_SLOTS) S.slot = ENH_SLOTS - 1;
    if (S.res && now >= S.result_until_ms) S.res = 0;

    ui_begin("enhance");
    ui_force(); // 원작 그리기(F2)에서도 보인다
    // 필드 메뉴 틀 안이면(새 UI, 모드 1) 레일 오른쪽 내용 자리에 놓고, 파티원은 틀의 캐릭터 띠가 보이고 바꾼다(fm_shell.c).
    // 강화·닫기는 고정 버튼 띠의 「강화」「이전」이다. 원작 그리기(F2)에서는 틀이 없으므로 지금처럼 전체 화면이다.
    int shell = fm_overlay_frame(menu);
    ui_int("narrow", ui_screen_w() < 400);
    if (!ch) { ui_end(); return; }

    // 파티원
    ui_scratch_begin();
    cells_reset();
    ui_list("members");
    for (int i = 0; i < PARTY_MAX; i++) {
        uint32_t m = party_char(game, i);
        if (!m) continue;
        ui_item();
        ui_str("name", char_name(game, m));
        ui_int("lv", (int)G8(G32(m + CH_STATS) + ST_LV));
        ui_int("sel", i == S.member);
        ui_int("i", PICK_MEMBER0 + i);
        put_face("face", game, menu, m);
        int sum = 0;
        for (int k = 0; k < ENH_SLOTS; k++) sum += enh_step(m, k);
        ui_int("sum", sum);
        ui_close();
    }
    ui_close();

    // 장비칸 5개
    ui_list("slots");
    for (int k = 0; k < ENH_SLOTS; k++) {
        uint32_t rec = G32(ch + CH_EQUIP + 4u * (uint32_t)k);
        int step = enh_step(ch, k), bp, bm;
        enh_bonus(rec, step, &bp, &bm);
        ui_item();
        ui_str("kind", slot_kind(k));
        ui_int("empty", rec == 0);
        ui_int("can", rec_enhanceable(rec));
        ui_int("step", step);
        ui_int("sel", k == S.slot);
        ui_int("i", PICK_SLOT0 + k);
        if (rec) {
            ui_str("name", rec_name(game, rec));
            put_icon_no("icon", menu, (int)G8(rec + REC_ICON));
            char t[48];
            bonus_text(t, sizeof t, enh_rec_is_def(rec), bp, bm);
            ui_text("bonus", step ? t : (rec_enhanceable(rec) ? "" : "강화 불가"));
        } else {
            ui_text("name", "");
            put_icon_no("icon", menu, 13);
            ui_text("bonus", "");
        }
        ui_close();
    }
    ui_close();

    // 고른 칸의 자세한 값
    uint32_t rec = G32(ch + CH_EQUIP + 4u * (uint32_t)S.slot);
    int step = enh_step(ch, S.slot), can = rec_enhanceable(rec), maxed = step >= ENH_MAX;
    int target = step + 1, rate = maxed ? 0 : RATE_PCT[target], cost = maxed ? 0 : cost_of(target);
    int have = ext_count(IT_ENHANCE);
    ui_int("has_item", rec != 0);
    ui_int("can", can);
    ui_int("maxed", maxed);
    ui_int("step", step);
    ui_int("target", target);
    ui_int("rate", rate);
    ui_int("cost", cost);
    ui_int("have", have);
    ui_int("enough", have >= cost);
    ui_str("kind", slot_kind(S.slot));
    ui_str("cname", char_name(game, ch));
    ui_str("iname", rec ? rec_name(game, rec) : "");
    ui_int("res", S.res ? (S.res_flip ? S.res + 2 : S.res) : 0);
    ui_int("res_step", S.res_step);
    ui_text("msg_t", S.msg);
    ui_int("go", rec != 0 && can && !maxed && now >= S.lock_until_ms);

    // 능력치(장비 화면과 같은 식): 지금 값, 한 단계 올린 값, 이 칸이 주는 보너스
    int now_s[4], nxt_s[4];
    char_stats(ch, -1, 0, now_s);
    char_stats(ch, S.slot, maxed ? step : target, nxt_s);
    int role_def = S.slot != 0, o = role_def ? 2 : 0;
    static const char *names[4] = {"P.ATT", "M.ATT", "P.DEF", "M.DEF"};
    int bp0, bm0, bp1, bm1;
    enh_bonus(rec, step, &bp0, &bm0);
    enh_bonus(rec, maxed ? step : target, &bp1, &bm1);
    int b0[2] = {bp0, bm0}, b1[2] = {bp1, bm1};
    ui_list("stats");
    if (S.slot < 4) for (int j = 0; j < 2; j++) {
        int v0 = now_s[o + j] > 999 ? 999 : now_s[o + j], v1 = nxt_s[o + j] > 999 ? 999 : nxt_s[o + j];
        ui_item();
        ui_text("name", names[o + j]);
        ui_int("v", v0);
        ui_int("nv", v1);
        ui_int("b", b0[j]);
        ui_int("nb", b1[j]);
        ui_int("up", v1 > v0);
        ui_close();
    }
    ui_close();

    // 단계 표(+1~+10): 성공률과 비용을 미리 보인다
    ui_list("ladder");
    for (int n = 1; n <= ENH_MAX; n++) {
        ui_item();
        ui_int("n", n);
        ui_int("rate", RATE_PCT[n]);
        ui_int("cost", cost_of(n));
        ui_int("done", n <= step);
        ui_int("next", n == target && !maxed);
        ui_close();
    }
    ui_close();

    // 터치·패드: 파티원과 칸은 눌러 고르고(pick), 강화(OK)·닫기(CLR)는 키 버튼
    ui_list("keys");
    keys_hint("◀▶", "파티원", 1);
    keys_hint("▲▼", "장비칸", 1);
    keys_hint("ok", "강화", 0);
    keys_hint("clear", "닫기", 0);
    ui_close();
    (void)shell; { // 문서의 <fmbar/> 값은 틀 밖(원작 그리기)에서도 채운다(비어 있으면 RmlUi가 경고를 낸다). 강화는 재화를 쓰는 동작이라 지금처럼 「강화」 한 번(결정)에 한 번 시도한다(확인 창 없음, 결과는 가운데 띠)
        fm_bar_begin();
        fm_bar_ok("강화", rec != 0 && can && !maxed && have >= cost && now >= S.lock_until_ms);
        fm_bar_emit(menu);
    }
    ui_end();
    ui_scratch_end();
}

// ---------------------------------------------------------------- 강화 시도
static void toast_fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void toast_fmt(const char *fmt, ...) {
    char b[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    ext_toast(b);
}

// 현재 파티원·칸에서 한 번 시도한다. 결과 문구는 토스트와 화면의 msg_t에 둔다.
static void try_enhance(uint32_t game) {
    uint32_t ch = party_char(game, S.member);
    if (!ch) return;
    uint32_t rec = G32(ch + CH_EQUIP + 4u * (uint32_t)S.slot);
    int step = enh_step(ch, S.slot);
    uint64_t now = mono_ms();
    if (now < S.lock_until_ms) return;
    if (!rec) { snprintf(S.msg, sizeof S.msg, "장비를 끼운 칸만 강화할 수 있습니다"); ext_toast(S.msg); return; }
    if (!rec_enhanceable(rec)) { snprintf(S.msg, sizeof S.msg, "능력치가 없는 장비는 강화할 수 없습니다"); ext_toast(S.msg); return; }
    if (step >= ENH_MAX) { snprintf(S.msg, sizeof S.msg, "이미 +%d 최대 단계입니다", ENH_MAX); ext_toast(S.msg); return; }
    int target = step + 1, cost = cost_of(target), have = ext_count(IT_ENHANCE);
    if (have < cost) {
        snprintf(S.msg, sizeof S.msg, "강화석이 부족합니다 (필요 %d, 보유 %d)", cost, have);
        ext_toast(S.msg);
        return;
    }
    ext_take(IT_ENHANCE, cost);
    const char *nm = ext_item_name(rec_id(game, rec)); // UTF-8
    S.lock_until_ms = now + ENH_LOCK_MS;
    S.result_until_ms = now + RESULT_MS;
    S.res_flip ^= 1;
    if (rnd100() < RATE_PCT[target]) {
        step_set(ch, S.slot, target);
        enh_sync(ch);
        S.res = 1; S.res_step = target;
        snprintf(S.msg, sizeof S.msg, "강화 성공! %s +%d", nm, target);
    } else {
        S.res = 2; S.res_step = step;
        snprintf(S.msg, sizeof S.msg, "강화 실패... %s +%d 그대로 (강화석 %d개 소모)", nm, step, cost);
    }
    // 결과는 가운데 띠 하나로만 알린다(사용자 결정 2026-11-10: 토스트와 겹쳐 두 번 보이지 않게)
}

// ---------------------------------------------------------------- 열기와 입력
static void open_for(uint32_t game, int member, int slot) {
    S.open = 1;
    S.member = member;
    S.slot = slot < 0 ? 0 : slot;
    S.res = 0;
    S.msg[0] = 0;
    S.opened_ms = mono_ms();
    S.last_tick_ms = S.opened_ms;
    (void)game;
}


static int step_member(uint32_t game, int dir) {
    int n = PARTY_MAX;
    for (int t = 1; t <= n; t++) {
        int i = ((S.member + dir * t) % n + n) % n;
        if (party_char(game, i)) { S.member = i; return 1; }
    }
    return 0;
}

static void on_key(uint32_t game, int k) {
    if (k == KEY_CLR || k == KEY_HASH || k == KEY_LSK) { S.open = 0; return; }
    if (k == KEY_UP || k == KEY_2) S.slot = (S.slot + ENH_SLOTS - 1) % ENH_SLOTS;
    else if (k == KEY_DOWN || k == KEY_8) S.slot = (S.slot + 1) % ENH_SLOTS;
    else if (k == KEY_LEFT || k == KEY_4) step_member(game, -1);
    else if (k == KEY_RIGHT || k == KEY_6) step_member(game, 1);
    else if (k == KEY_OK || k == KEY_5) try_enhance(game);
}

// 필드 메뉴 키 분배 0x2fa60(메뉴, 키 이벤트): 열려 있으면 키를 모두 가져간다. 닫혀 있고 장비 화면(상태 0)에서 *를 누르면 연다.
__attribute__((weak)) int ext_warp_key(uint32_t menu, int k);
__attribute__((weak)) void ext_warp_draw(uint32_t game);
__attribute__((weak)) int ext_statreset_key(uint32_t menu, int k);   // 망각의 물약 창(statreset.c)
__attribute__((weak)) void ext_statreset_draw(uint32_t game);
// 필드 메뉴 틀(fm_shell.c): 키 앞 훅(「이전」 규칙, 숨긴 단계 건너뛰기. 1이면 키를 가져갔다), 키 뒤 훅(메인의 대상 선택 마저 넘기기),
// 메뉴 장면 틱 앞 훅(레일·캐릭터 띠·버튼 띠 누름을 원본 키 처리 전에 동기로 처리).
__attribute__((weak)) int ext_shell_key(uint32_t menu, int k);
__attribute__((weak)) void ext_shell_after(uint32_t menu, int k);
__attribute__((weak)) void ext_shell_tick(uint32_t game);
int ext_enhance_open(void) { return S.open; }
// 필드 메뉴 틀(fm_shell.c)이 쓴다: 캐릭터 띠의 칩·[ ]·LT/RT가 강화의 파티원을 바꾸고, 레일을 누르면 강화를 닫는다(CLR과 같은 닫기).
int ext_enhance_member(void) { return S.member; }
void ext_enhance_set_member(int slot) { uint32_t g = game_obj(); if (g && party_char(g, slot)) S.member = slot; }
void ext_enhance_close(void) { S.open = 0; S.res = 0; }
void orig_f_2fa60(cpu_t *c);
void f_2fa60(cpu_t *c) {
    uint32_t menu = c->r[0], ev = c->r[1];
    int k = G32(ev + 0xc) && G16(ev + 2) == 1 ? (int)G16(ev) : 0;
    uint32_t game = G32(menu);
    if (ext_warp_key && ext_warp_key(menu, k)) { c->r[0] = 0; return; } // 귀환 주문서 목록(warp.c)
    if (ext_statreset_key && ext_statreset_key(menu, k)) { c->r[0] = 0; return; } // 망각의 물약 창(statreset.c)
    if (S.open) {
        if (k && mono_ms() - S.opened_ms >= OPEN_LOCK_MS) on_key(game, k);
        c->r[0] = 0;
        return;
    }
    if (k == KEY_STAR && G32(menu + M_SCREEN) == EQ_SCREEN) {
        uint32_t scr = G32(menu + M_SCREENS + 4 * EQ_SCREEN);
        if (G32(scr + EQ_STATE) == 0) {
            open_for(game, (int)G32(scr + EQ_CHAR), (int)G32(scr + EQ_SLOT));
            c->r[0] = 0;
            return;
        }
    }
    if (ext_shell_key && ext_shell_key(menu, k)) { c->r[0] = 0; return; }
    orig_f_2fa60(c);
    if (ext_shell_after) { cpu_t saved = *c; ext_shell_after(menu, k); *c = saved; }
}

// 메뉴 장면 틱 0x109b8(game, 입력): 맞춤은 원본 앞에서, 강화 화면과 터치는 원본(메뉴 그리기) 뒤에서.
void orig_f_109b8(cpu_t *c);
void f_109b8(cpu_t *c) {
    uint32_t game = c->r[0];
    uint64_t now = mono_ms();
    if (S.open && now - S.last_tick_ms > 1000) S.open = 0; // 메뉴를 떠났다가 돌아왔다
    enh_sync_all();
    if (ext_shell_tick) { cpu_t saved = *c; ext_shell_tick(game); *c = saved; }
    orig_f_109b8(c);
    if (ext_warp_draw) ext_warp_draw(game); // 귀환 주문서 목록(warp.c)
    if (ext_statreset_draw) ext_statreset_draw(game); // 망각의 물약 창(statreset.c)
    if (!S.open) { ui_pick("enh"); return; }
    S.last_tick_ms = now;
    int pk = ui_pick("enh");
    if (pk >= PICK_MEMBER0 && party_char(game, pk - PICK_MEMBER0)) S.member = pk - PICK_MEMBER0;
    else if (pk >= PICK_SLOT0 && pk < PICK_SLOT0 + ENH_SLOTS) S.slot = pk - PICK_SLOT0;
    emit(game);
}

// ---------------------------------------------------------------- 전투 데미지 확인용 기록(환경 변수 ECPE_ENH_LOG가 있을 때만)
// 스킬 데미지 계산 0x16d14(공격자 = 캐릭터+0x54, 기술 레코드, 결과 칸, 기술)의 결과 칸 처음 두 값(물리, 마법 기본 위력)을 남긴다.
void orig_f_16d14(cpu_t *c);
void f_16d14(cpu_t *c) {
    uint32_t who = c->r[0], out = c->r[2], rec = c->r[3];
    orig_f_16d14(c);
    static int on = -1;
    if (on < 0) on = getenv("ECPE_ENH_LOG") != NULL;
    if (on) fprintf(stderr, "[enh-dmg] 공격자 %#x 기술 %#x atk필드 %u/%u 결과 %d %d\n", who, rec, G32(who + 0xd0), G32(who + 0xd4), (int)G32(out), (int)G32(out + 4));
}
