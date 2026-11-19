// 확장 기능 공용 기반(items_ext.h). 원작에 없는 새 아이템(246~)과 그것을 받치는 장치들.
//
//  1. 아이템 표 확장: 아이템 DB는 새 게임·시작 때마다 0xc404가 0x22360 → 0x24088(db, 이름풀, 개수, 레코드)로 채운다.
//     0x24088을 감싸 원본이 채운 뒤 레코드 배열(32바이트×개수)을 더 큰 게스트 힙으로 옮겨 새 레코드를 붙이고 개수를 늘린다.
//     이름은 이름 풀(db+0x14)을 새 힙에 복사해 오프셋 0x6000 이후에 붙인다(풀은 0x5700 바이트 안쪽, 오프셋은 u16).
//     설명은 풀([[db+0x10]+0x318])의 크기를 몰라 따로 두고 0x240b0(설명)을 감싸 새 번호면 이 파일의 글을 돌려준다
//     (ui_fmcore.c 같이 풀을 직접 읽는 쪽은 ext_item_desc_g를 쓴다).
//  2. 사용 가로채기: 필드 아이템 창의 OK 처리기 0x318e0을 감싼다. 새 아이템이 소모품 탭에서 눌리면 원본의 대상 고르기 대신
//     ext_use(id, EXT_FIELD)(또는 사용할 수 없는 것은 설명 토스트). 핫슬롯 등록 0x31aa0도 감싸 새 아이템은 등록하지 않는다
//     (전투 아이템 창은 핫슬롯 목록만 보여 주므로 새 아이템은 전투에 나오지 않는다).
//  3. 슬롯별 확장 저장: 저장 폴더의 ext<슬롯>.ini. 저장(0x110d4)·불러오기(0x111b8)는 ui_dialog.c의 래퍼가 ext_slot_save/load를 부른다.
//     새 게임·시작(0xc404를 지난 0x24088)이면 비운다.
//  4. 토스트: 화면 가운데 위. ui/toast.rml. 새 UI가 꺼져도 보인다(host/ui.cpp가 이 문서만 예외로 둔다).
#include "restored.h"
#include "items_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define G32(a) R32_(&cpu, (a))
#define G8(a) R8_(&cpu, (a))
enum { APP_GAME = 0x48, G_PARTY = 0x1c0, G_ITEMDB = 0x1cc, PARTY_INV = 0xa8, INV_COUNT = 0x50,
       NAME_POOL_COPY = 0x5700, NAME_BASE = 0x6000, ORIG_ITEMS = 246, DB_MAX = 256 };

extern char save_dir[1024];
extern const uint16_t cp949_tab[126 * 190];
size_t cp949_to_utf8(const unsigned char *s, int len, char *out);

// ---------------------------------------------------------------- 새 아이템 정의
typedef struct {
    const char *name, *desc;
    int icon;      // 소모품 아이콘 번호(rec[2], 목록에서 메뉴 스프라이트 프레임 0x2c+번호)
    int field_use; // 1이면 필드 아이템 창의 OK가 ext_use를 부른다. 0이면 설명만(부활석·강화석)
} ext_def_t;
static const ext_def_t defs[] = {
    {"망각의 물약", "한 캐릭터의 GP 분배를 처음으로 되돌린다", 2, 1},          // 246
    {"부활석", "지니고 있으면 전멸했을 때 전원 완전 회복(소모)", 5, 0},        // 247
    {"랜덤 상자", "열면 원작 장비나 아이템 하나가 나온다", 6, 1},                         // 248
    {"귀환 주문서", "가 본 마을로 돌아간다", 4, 1},                             // 249
    {"정적의 향", "한동안 적과 덜 만난다", 1, 1},                               // 250
    {"유혹의 향", "한동안 적과 더 자주 만난다", 3, 1},                          // 251
    {"강화석", "장비 강화 재료(장비 화면의 강화에서 사용)", 0, 0},                // 252
    {"성장의 물약", "한 캐릭터의 레벨을 1 올린다", 2, 1},                       // 253
};
#define NDEF ((int)(sizeof defs / sizeof defs[0]))
#define EXT_COUNT (ORIG_ITEMS + NDEF)

static const ext_def_t *def_of(int id) { return id >= IT_EXT_FIRST && id < ORIG_ITEMS + NDEF ? &defs[id - IT_EXT_FIRST] : NULL; }

// ---------------------------------------------------------------- UTF-8 <-> CP949
// CP949 -> 유니코드 표(cp949_tab)를 거꾸로 훑어 만든 표. 처음 쓸 때 한 번 만든다.
static uint16_t *rev;
static void rev_build(void) {
    if (rev) return;
    rev = calloc(65536, 2);
    for (int a = 0; a < 126; a++)
        for (int b = 0; b < 190; b++) {
            uint16_t u = cp949_tab[a * 190 + b];
            if (u && !rev[u]) rev[u] = (uint16_t)(((a + 0x81) << 8) | (b + 0x41));
        }
}
// UTF-8 -> CP949. 못 바꾸는 글자는 '?'. 쓴 바이트 수(NUL 제외).
static int to_cp949(const char *s, uint8_t *out) {
    rev_build();
    uint8_t *o = out;
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        unsigned u;
        if (*p < 0x80) { *o++ = *p++; continue; }
        if ((*p & 0xe0) == 0xc0 && p[1]) { u = (unsigned)(*p & 31) << 6 | (p[1] & 63); p += 2; }
        else if ((*p & 0xf0) == 0xe0 && p[1] && p[2]) { u = (unsigned)(*p & 15) << 12 | (unsigned)(p[1] & 63) << 6 | (p[2] & 63); p += 3; }
        else { *o++ = '?'; p++; continue; }
        unsigned c = u < 65536 ? rev[u] : 0;
        if (c) { *o++ = (uint8_t)(c >> 8); *o++ = (uint8_t)c; } else *o++ = '?';
    }
    *o = 0;
    return (int)(o - out);
}
static uint32_t gstring(const char *utf8) { // 게스트 힙에 CP949로 놓는다
    uint8_t b[256];
    int n = to_cp949(utf8, b);
    uint32_t g = heap_alloc((uint32_t)n + 1);
    if (g) memcpy(cpu.m + g, b, (size_t)n + 1);
    return g;
}

// ---------------------------------------------------------------- 인벤토리
uint32_t ext_game(void) {
    uint32_t app = G32(APP_ADDR);
    return app ? G32(app + APP_GAME) : 0;
}
uint32_t ext_inv(void) {
    uint32_t G = ext_game(), pd = G ? G32(G + G_PARTY) : 0;
    return pd ? pd + PARTY_INV : 0;
}
int ext_count(int id) {
    uint32_t P = ext_inv();
    return P && id >= 0 && id < 256 ? (int)G8(P + INV_COUNT + (uint32_t)id) : 0;
}
int ext_give(int id, int n) {
    uint32_t P = ext_inv();
    if (!P || id < 0 || id >= 256 || n <= 0) return 0;
    int c = G8(P + INV_COUNT + (uint32_t)id), add = c + n > IT_MAX_COUNT ? IT_MAX_COUNT - c : n;
    W8_(&cpu, P + INV_COUNT + (uint32_t)id, (uint32_t)(c + add));
    return add;
}
int ext_take(int id, int n) {
    uint32_t P = ext_inv();
    if (!P || id < 0 || id >= 256 || n <= 0) return 0;
    int c = G8(P + INV_COUNT + (uint32_t)id);
    if (c < n) return 0;
    W8_(&cpu, P + INV_COUNT + (uint32_t)id, (uint32_t)(c - n));
    return 1;
}

// ---------------------------------------------------------------- 이름·설명
static char *ring_utf8(const unsigned char *cp, size_t *len_out) { // 짧은 수명의 UTF-8 칸 8개를 돌려 쓴다
    static char ring[8][256];
    static int at;
    char *o = ring[at++ & 7];
    int n = 0;
    while (cp[n] && n < 80) n++;
    size_t l = cp949_to_utf8(cp, n, o);
    o[l] = 0;
    if (len_out) *len_out = l;
    return o;
}
const char *ext_item_name(int id) {
    const ext_def_t *d = def_of(id);
    if (d) return d->name;
    uint32_t db = ext_game() ? G32(ext_game() + G_ITEMDB) : 0;
    if (!db || id < 0 || id >= ORIG_ITEMS || !G32(db + 0x14)) return "";
    uint32_t p = G32(db + 0x14) + R16_(&cpu, G32(db + 8) + 32u * (uint32_t)id + 4);
    return ring_utf8(cpu.m + p, NULL);
}
const char *ext_item_desc(int id) {
    const ext_def_t *d = def_of(id);
    if (d) return d->desc;
    uint32_t G = ext_game(), db = G ? G32(G + G_ITEMDB) : 0, pool = G ? G32(G + 0x318) : 0;
    if (!db || !pool || id < 0 || id >= ORIG_ITEMS) return "";
    return ring_utf8(cpu.m + pool + R16_(&cpu, G32(db + 8) + 32u * (uint32_t)id + 6), NULL);
}

static uint32_t desc_g[DB_MAX]; // 새 아이템 설명(게스트 CP949), 0x24088마다 다시 만든다
const char *ext_item_desc_g(int id) {
    return id >= IT_EXT_FIRST && id < EXT_COUNT && desc_g[id] ? (const char *)(cpu.m + desc_g[id]) : NULL;
}
uint32_t ext_item_count(void) { return EXT_COUNT; }

// ---------------------------------------------------------------- 아이템 DB 확장(0x24088)
static void ext_reset(void);
void orig_f_24088(cpu_t *c);
void f_24088(cpu_t *c) {
    uint32_t db = c->r[0];
    orig_f_24088(c);
    uint32_t n = R32(db), rec = R32(db + 8), pool = R32(db + 0x14);
    if (n != ORIG_ITEMS || !rec) return; // 예상 밖의 표면 건드리지 않는다
    uint32_t nrec = heap_alloc(DB_MAX * 32), npool = heap_alloc(0x10000);
    if (!nrec || !npool) return;
    memcpy(cpu.m + nrec, cpu.m + rec, ORIG_ITEMS * 32u);
    memcpy(cpu.m + npool, cpu.m + pool, NAME_POOL_COPY);
    uint32_t noff = NAME_BASE;
    for (int i = 0; i < NDEF; i++) {
        uint8_t b[256];
        int l = to_cp949(defs[i].name, b);
        memcpy(cpu.m + npool + noff, b, (size_t)l + 1);
        desc_g[ORIG_ITEMS + i] = gstring(defs[i].desc);
        uint8_t *r = cpu.m + nrec + 32u * (ORIG_ITEMS + i);
        memset(r, 0xff, 32);
        r[0] = 0;                       // 종류: 소모품(소비물품 탭)
        r[2] = (uint8_t)defs[i].icon;   // 아이콘 번호
        r[3] = 0x80;                    // 직업 마스크(원본 소모품과 같게)
        r[4] = (uint8_t)noff; r[5] = (uint8_t)(noff >> 8);
        r[6] = r[7] = 0;                // 설명 오프셋 0(설명은 0x240b0 래퍼가 돌려준다)
        memset(r + 8, 0, 4);            // 가격 0(판매·구입 대상이 아니다)
        r[0xc] = 0xff; r[0xd] = 0x40; r[0xe] = 0x04; r[0xf] = 0; // 필드 사용 가능 비트 영역, 판매불가(비트 18)
        memset(r + 0x10, 0, 4);
        r[0x14] = 0x12;                 // 효과 종류 0x12 = 원본 사용 처리에서는 사용불가(가로채지 못했을 때의 안전망)
        r[0x1a] = 0;
        noff += (uint32_t)l + 1;
    }
    W32(db, EXT_COUNT);
    W32(db + 8, nrec);
    W32(db + 0x14, npool);
    ext_reset(); // 새 게임·시작: 슬롯별 확장 저장 값은 비운다(불러오기면 곧 ext_slot_load가 채운다)
}

void orig_f_240b0(cpu_t *c);
void f_240b0(cpu_t *c) {
    int id = (int)c->r[1];
    if (id >= IT_EXT_FIRST && id < EXT_COUNT && desc_g[id]) { c->r[0] = desc_g[id]; return; }
    orig_f_240b0(c);
}

// ---------------------------------------------------------------- 사용
static ext_use_fn use_fn[256];
void ext_register_use(int id, ext_use_fn fn) { if (id >= 0 && id < 256) use_fn[id] = fn; }
int ext_use(int id, int where) {
    if (id < 0 || id >= 256 || !use_fn[id] || ext_count(id) <= 0) return 0;
    if (!use_fn[id](id, where)) return 0;
    ext_take(id, 1);
    return 1;
}
int ext_can_use(int id) { return id >= 0 && id < 256 && use_fn[id] != NULL; }

// 필드 아이템 창(화면 객체 scr)의 OK 0x318e0(scr, P). 상태 +0x10(0 목록), 선택 id +0x54, 판매 모드 +0x30, 분류 탭 +0x38/+0x3c.
void orig_f_318e0(cpu_t *c);
void f_318e0(cpu_t *c) {
    uint32_t scr = c->r[0];
    int id = (int)R32(scr + 0x54);
    if (R32(scr + 0x10) == 0 && R32(scr + 0x30) == 0 && R32(scr + 0x38) == 0 && R32(scr + 0x3c) == 0 &&
        id >= IT_EXT_FIRST && id < EXT_COUNT && R32(ext_game() + G_PARTY) && ext_count(id) > 0) {
        const ext_def_t *d = def_of(id);
        if (d->field_use && use_fn[id]) ext_use(id, EXT_FIELD);
        else {
            char b[160];
            snprintf(b, sizeof b, "%s: %s", d->name, d->desc);
            ext_toast(b);
        }
        c->r[0] = 0;
        return;
    }
    orig_f_318e0(c);
}
// 핫슬롯 등록 0x31aa0(scr, P, id): 새 아이템은 등록하지 않는다(전투 아이템 창이 핫슬롯만 보여 주므로 새 아이템은 전투에 없다).
void orig_f_31aa0(cpu_t *c);
void f_31aa0(cpu_t *c) {
    if ((int)c->r[2] >= IT_EXT_FIRST && (int)c->r[2] < EXT_COUNT) {
        ext_toast("이 아이템은 핫슬롯에 넣을 수 없습니다");
        c->r[0] = 1;
        return;
    }
    orig_f_31aa0(c);
}

// ---------------------------------------------------------------- 슬롯별 확장 저장
#define EXT_KV_MAX 4096 // 마을 하나가 7칸, 강화가 캐릭터×칸 40칸 남짓을 쓴다
static struct { char k[48]; int v; } kv[EXT_KV_MAX];
static int kv_n;
static void ext_reset(void) { kv_n = 0; }
int ext_get(const char *key, int def) {
    for (int i = 0; i < kv_n; i++) if (!strcmp(kv[i].k, key)) return kv[i].v;
    return def;
}
void ext_set(const char *key, int v) {
    for (int i = 0; i < kv_n; i++) if (!strcmp(kv[i].k, key)) { kv[i].v = v; return; }
    if (kv_n < EXT_KV_MAX) { snprintf(kv[kv_n].k, sizeof kv[kv_n].k, "%s", key); kv[kv_n++].v = v; }
}
static void slot_path(int slot, char *out, size_t cap) { snprintf(out, cap, "%s/ext%d.ini", save_dir, slot); }
void ext_slot_save(int slot) {
    if (slot < 0 || slot > 9) return;
    char p[1100];
    slot_path(slot, p, sizeof p);
    FILE *f = fopen(p, "w");
    if (!f) return;
    for (int i = 0; i < kv_n; i++) fprintf(f, "%s=%d\n", kv[i].k, kv[i].v);
    fclose(f);
}
void ext_slot_load(int slot) {
    ext_reset();
    if (slot < 0 || slot > 9) return;
    char p[1100], line[128];
    slot_path(slot, p, sizeof p);
    FILE *f = fopen(p, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        ext_set(line, atoi(eq + 1));
    }
    fclose(f);
    fprintf(stderr, "[ext] 슬롯 %d 확장 저장 %d개를 읽음\n", slot, kv_n);
}


// ---------------------------------------------------------------- 토스트
// 화면 가운데 위에 최대 3줄. 줄마다 2.4초. 문서 ui/toast.rml은 ext_toast_frame이 매 프레임(host/main.c video_present) 채운다.
#define TOAST_MAX 3
#define TOAST_MS 2400
static struct { char s[160]; uint64_t until; } toasts[TOAST_MAX];
static uint64_t mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
void ext_toast(const char *utf8) {
    if (!utf8 || !*utf8) return;
    fprintf(stderr, "[toast] %s\n", utf8);
    uint64_t now = mono_ms();
    for (int i = 0; i < TOAST_MAX; i++) // 같은 글이 떠 있으면 시간만 늘린다
        if (toasts[i].until > now && !strcmp(toasts[i].s, utf8)) { toasts[i].until = now + TOAST_MS; return; }
    int slot = -1;
    for (int i = 0; i < TOAST_MAX; i++) if (toasts[i].until <= now) { slot = i; break; }
    if (slot < 0) { // 가득 차면 가장 오래된 줄을 밀어낸다
        for (int i = 1; i < TOAST_MAX; i++) toasts[i - 1] = toasts[i];
        slot = TOAST_MAX - 1;
    }
    snprintf(toasts[slot].s, sizeof toasts[slot].s, "%s", utf8);
    toasts[slot].until = now + TOAST_MS;
}
// 시험용: 환경 변수 ECPE_EXT_SET="키=값,..."(슬롯 저장 시험)와 ECPE_EXT_GIVE="246:5,247:1"이면 게임이 서 있을 때 한 번 그만큼 준다(제대로 불러온 뒤, 인벤토리가 생기면).
static void debug_give(void) {
    static int done;
    const char *e = getenv("ECPE_EXT_GIVE");
    if (done || !e || !ext_inv() || !ext_game()) return;
    uint32_t G = ext_game();
    if (G32(G + 0x0c) != 7 || G32(G + 0x18) != 0) return; // 필드 장면에서만(불러온 뒤)
    done = 1;
    for (const char *p = e; *p;) {
        int id = atoi(p), n = 1;
        const char *c = strchr(p, ':'), *comma = strchr(p, ',');
        if (c && (!comma || c < comma)) n = atoi(c + 1);
        ext_give(id, n);
        if (!comma) break;
        p = comma + 1;
    }
    if ((e = getenv("ECPE_EXT_SET")) != NULL) { // "키=값,키=값"
        char b[256];
        snprintf(b, sizeof b, "%s", e);
        for (char *t = strtok(b, ","); t; t = strtok(NULL, ",")) { char *eq = strchr(t, '='); if (eq) { *eq = 0; ext_set(t, atoi(eq + 1)); } }
    }
}
void ext_toast_frame(void) {
    debug_give();
    uint64_t now = mono_ms();
    int n = 0;
    for (int i = 0; i < TOAST_MAX; i++) if (toasts[i].until > now) n++;
    if (!n) return;
    ui_begin("toast");
    ui_list("lines");
    for (int i = 0; i < TOAST_MAX; i++)
        if (toasts[i].until > now) { ui_item(); ui_text("t", toasts[i].s); ui_close(); }
    ui_close();
    ui_end();
}
