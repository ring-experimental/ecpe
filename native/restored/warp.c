// 귀환 주문서(IT_WARP 249, 원작에 없다. 사용자 요청 2026-11-10).
//
// ---- 원작의 맵 전환 구조(조사 결과) ----
//  * 이동 대상은 필드 게임 객체 G의 +0x2d(맵 번호, 바이트), +0x2e(입구 번호 a), +0x2f(b)다. 스크립트 명령 0x3e/0xc1이 쓴다
//    (0x3cfe0: G+0x20 객체의 +0xd/+0xe/+0xf). 번호는 map.pmd의 맵 레코드 색인(0~97)이다.
//  * 스크립트 명령 0x49가 실제로 읽는다: 0x3c294(VM, &명령) → 0xd044(G)가 맵을 읽고(0x217e0 → 0x218c0 → 0x2e5c4가 입구 a의 칸에서
//    주인공 위치와 지역 이름 G+0x4dc/+0x4fc(CP949 0x20바이트, 기록에는 앞 20바이트)를 정한다) → 0xd704가 연출(G+0x14 비트)을 시작하고
//    명령 인자 [+6](u16)가 0이 아니면 0xd684가 지역 이름 띠를 띄운다. 명령은 {op 0x49, fl 0x22, 바이트4=1, 바이트5=1, u16=0x09c4}가 흔하다.
//    문으로 들어갈 때도 스크립트가 이 두 명령을 쓴다. 불러오기(0x111b8 → 0x211b8)도 0xd044를 부른다.
//  * 그래서 귀환은 (맵, a, b)를 G에 써 놓고 같은 명령 0x49를 만들어 0x3c294를 부른다(원작이 마을로 들어갈 때와 같은 길, 같은 페이드·띠).
//  * 문(스크립트)으로 들어갈 때의 명령열은 [0xc1 또는 0x3e: 맵·a·b(+이름)] → [0x54: 주인공을 위치(px)에 놓는다, 0xd984] → [0x49: 맵 읽기]다.
//    곧 도착 자리는 (맵, a, b)와 주인공 위치(px)이고, 0xd044가 끝난 시점의 G+0x2d~0x2f와 주인공 +8/+0xc(px)가 그것이다(불러오기도 같다).
//    귀환은 주인공 위치를 미리 써 놓고(0x54와 같다) 0x49를 부른다. (입구 번호만 쓰면 주인공이 옛 자리에 남아 엉뚱한 곳에 선다.)
//  * 마을 판정: 허용 목록(아래 TOWNS 표)에 든 지역 이름만 마을이다. 근거는 TOWNS 표 위 주석. 맵을 읽은 6틱 뒤에 (1)지역 이름 G+0x4dc가 표의 이름이고
//    (안쪽 방은 4fc가 달라도 4dc가 마을 이름으로 남는다), (2)조우 객체 E=[G+0x1c4]의 +8이 0, (3)저장 가능한 자유 상태(free_to_move)이면 '가 봄'으로 기록한다.
//    지금 있는 마을은 불러오기 직후에도 같은 길로 들어간다. 도착 자리는 표의 (맵, a, b, 위치)다(원작이 그 마을로 들어올 때의 자리).
// 기록(ext 저장은 슬롯 전체 512칸이라 아낀다): "warp.<표 번호>" = 1(가 봄), "warp.<표 번호>.t" 최근 방문 차례, "warp.seq" 차례 카운터.
//
// ---- 사용 ----
//  아이템 창에서 '사용'하면 ext_use → warp_use: 쓸 수 있는 자리인지 보고(이벤트·던전 특수 상태·비행정이면 토스트, 소모 안 함) 목록 창을 연다.
//  목록 창은 필드 메뉴 키 분배(0x2fa60, enhance.c가 감쌈)와 메뉴 장면 틱(0x109b8, enhance.c가 감쌈)에 훅(ext_warp_key / ext_warp_draw)으로 얹는다.
//  고르면 주문서 1개를 쓰고 메뉴를 닫은 뒤(메뉴 객체 +0x60 = 1, 원작의 닫기와 같다) 필드 틱(ext_field_tick)에서 이동한다.
#include "restored.h"
#include "items_ext.h"
#include "fm_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    G_SCENE = 0x0c, G_FLAGS = 0x14, G_FSTATE = 0x18, G_FLAGS2 = 0x20, G_DEST = 0x2d, G_VM = 0x170, G_HERO = 0x1b4, G_E = 0x1c4, G_MENU = 0x1d4,
    G_REGION = 0x4dc, G_PLACE = 0x4fc, M_CLOSE = 0x60, M_MODE = 0x1c,
    FN_LOADMAP = 0x13c294, FN_LOAD = 0x10d044,
    KEY_UP = 0x1f, KEY_DOWN = 0x1e, KEY_OK = 0x1b, KEY_CLR = 0x10, KEY_2 = 0x52, KEY_8 = 0x58, KEY_5 = 0x55,
    LOCK_TICKS = 10,
};
#define G32(a) R32_(&cpu, (a))
static int dbg_on(void) { static int v = -1; if (v < 0) v = getenv("WARP_DBG") != NULL; return v; }
#define DBG(...) do { if (dbg_on()) fprintf(stderr, __VA_ARGS__); } while (0)

static uint32_t cur_game(void) { return ext_game(); }

// ---------------------------------------------------------------- 마을 허용 목록
// 마을 판단 근거: (1) 필드 스크립트 37의 '여관' 서비스 문자열 목록(프리빌여인숙, 호스스피릿여관, 아멜고여관, 트리튼여관, 매그파이여관, 라바티여관,
// 쥬얼리여관, 포스웰여관)이 가진 8곳, (2) 시작 허브 제라노스 성, (3) 사용자가 추가한 5곳(가디언마을, 뉴러키우드, 페어리랜드, 엘프랜드, 호스우드).
// (3)은 여관 같은 서비스 근거가 없고 뉴러키우드·페어리랜드·호스우드는 숲·야영지 지역이라 '마을'이 아니지만 사용자 결정으로 넣었다.
// 모두 아래 입구로 실제 이동해 길 위에 선 주인공을 확인했다(드래고넬은 '드래고넬평원' 필드라 뺐다).
// 입구 (맵, a, b, px, py): 스크립트(script.bin)가 그 곳으로 들어가는 0x3e/0xc1 + 0x54(주인공 위치 px) + 0x49 묶음에서 뽑았고, 위치가 없는 프리빌은
// 맵 24의 마을 한가운데 길을 직접 골랐다(같은 맵 24 아래쪽은 '프리빌남쪽 숲'). 포스웰·가디언마을은 위쪽에서 걸어 들어오는 자리다.
// 엘프랜드·호스우드는 맵 자체의 지역 이름이 비어 있어(스크립트의 0xc1이 이름을 준다) 이동할 때 이름(cp)을 미리 써 둔다.
typedef struct { const char *name, *cp; int m, a, b, px, py; } town_def_t; // cp: 같은 이름의 CP949(이동할 때 지역 이름으로 미리 써 둔다)
static const town_def_t TOWNS[] = {
    {"제라노스 성", "\xc1\xa6\xb6\xf3\xb3\xeb\xbd\xba\x20\xbc\xba", 39, 0, 0, 168, 686},
    {"수도 아멜고", "\xbc\xf6\xb5\xb5\x20\xbe\xc6\xb8\xe1\xb0\xed", 38, 0, 3, 518, 161},
    {"프리빌", "\xc7\xc1\xb8\xae\xba\xf4", 24, 0, 0, 350, 231},
    {"호스스피릿", "\xc8\xa3\xbd\xba\xbd\xba\xc7\xc7\xb8\xb4", 51, 0, 0, 119, 399},
    {"트리튼", "\xc6\xae\xb8\xae\xc6\xb0", 52, 0, 0, 245, 511},
    {"매그파이마운트", "\xb8\xc5\xb1\xd7\xc6\xc4\xc0\xcc\xb8\xb6\xbf\xee\xc6\xae", 55, 0, 1, 329, 175},
    {"라바티마을", "\xb6\xf3\xb9\xd9\xc6\xbc\xb8\xb6\xc0\xbb", 65, 0, 0, 168, 534},
    {"쥬얼힐즈", "\xc1\xea\xbe\xf3\xc8\xfa\xc1\xee", 65, 2, 4, 334, 322},
    {"포스웰", "\xc6\xf7\xbd\xba\xc0\xa3", 65, 3, 3, 175, 25},
    {"가디언마을", "\xb0\xa1\xb5\xf0\xbe\xf0\xb8\xb6\xc0\xbb", 15, 0, 0, 266, 28},
    {"뉴러키우드", "\xb4\xba\xb7\xaf\xc5\xb0\xbf\xec\xb5\xe5", 80, 0, 0, 105, 112},
    {"페어리랜드", "\xc6\xe4\xbe\xee\xb8\xae\xb7\xa3\xb5\xe5", 82, 0, 1, 375, 70},
    {"엘프랜드", "\xbf\xa4\xc7\xc1\xb7\xa3\xb5\xe5", 28, 0, 0, 602, 161},
    {"호스우드", "\xc8\xa3\xbd\xba\xbf\xec\xb5\xe5", 72, 3, 0, 318, 364},
};
#define NTOWN ((int)(sizeof TOWNS / sizeof TOWNS[0]))

size_t cp949_to_utf8(const unsigned char *s, int len, char *out);
// 게임의 지역 이름(CP949)이 표의 몇 번째 마을인지(없으면 -1)
static int town_of(const char *cp949) {
    char u[128];
    int n = 0;
    while (cp949[n] && n < 40) n++;
    size_t l = cp949_to_utf8((const unsigned char *)cp949, n, u);
    u[l] = 0;
    for (int i = 0; i < NTOWN; i++) if (!strcmp(u, TOWNS[i].name)) return i;
    return -1;
}

// ---------------------------------------------------------------- 기록
static int visited(int i) { char k[32]; snprintf(k, sizeof k, "warp.%d", i); return ext_get(k, 0); }
static int visit_seq(int i) { char k[32]; snprintf(k, sizeof k, "warp.%d.t", i); return ext_get(k, 0); }
static void set_visit(int i) {
    char k[32];
    int s = ext_get("warp.seq", 0) + 1;
    ext_set("warp.seq", s);
    snprintf(k, sizeof k, "warp.%d", i); ext_set(k, 1);
    snprintf(k, sizeof k, "warp.%d.t", i); ext_set(k, s);
}

typedef struct { int idx, m, a, b, seq, cur, px, py; const char *name; } town_t;
static int cmp_town(const void *x, const void *y) { return ((const town_t *)y)->seq - ((const town_t *)x)->seq; }
// 가 본 마을(최근 방문 순). 지금 있는 마을은 cur=1.
static int towns(town_t *t, const char *here_cp) {
    int n = 0, here = town_of(here_cp);
    for (int i = 0; i < NTOWN; i++) {
        if (!visited(i)) continue;
        town_t *x = &t[n++];
        x->idx = i; x->m = TOWNS[i].m; x->a = TOWNS[i].a; x->b = TOWNS[i].b; x->px = TOWNS[i].px; x->py = TOWNS[i].py;
        x->seq = visit_seq(i); x->name = TOWNS[i].name; x->cur = i == here;
    }
    qsort(t, (size_t)n, sizeof *t, cmp_town);
    return n;
}

// 맵을 읽은 직후(0xd044 성공): 6틱 뒤(조우 구역이 정해진 뒤) 마을인지 본다. 불러오기도 이 길이라 지금 있는 마을이 바로 기록된다.
static int rec_left = -1;
void orig_f_d044(cpu_t *c);
void f_d044(cpu_t *c) {
    uint32_t G = c->r[0];
    orig_f_d044(c);
    if (c->r[0] == 0 && G) rec_left = 6;
}

static int free_to_move(uint32_t G, const char **why);

// ---------------------------------------------------------------- 미리 채우기(기존 저장)
// 불러온 뒤 처음 한 번만(ext "warp.seeded") 이미 지나온 마을을 이벤트 플래그로 추정해 채운다.
// 근거 1(트리거 플래그): 필드의 이벤트 트리거 액터(vtable 0x14da10, 한 칸 크기)는 밟아서 실행되면 자기 플래그 [+0x40]을 배열 A(G+0x38, 512비트)에
//   켠다(스크립트 명령 0x73 → 0xdeb4(G, 번호, 1, 1), 트리거 +5가 0일 때). 배열 A를 켜는 다른 곳은 스크립트 0xab 한 곳(번호 320)뿐이고 끄는 곳은
//   없어서, 플래그가 켜져 있으면 '그 트리거가 있는 맵 구역에 서 본 것'이다(꺼지지 않는다). 번호는 맵·구역(a)마다 실제로 이동해(WARP_ASCAN)
//   그 맵의 트리거 액터를 읽어 뽑았다. 프리빌은 맵 24가 마을과 남쪽 숲을 함께 담으나 트리거 두 곳(265,363)·(377,307)이 마을 안임을 확인했다.
// 근거 2(진행 순서): 저장 사본 셋의 플레이 시각이 아이언우드 1:11 < 수도 아멜고 7:54 < 제라노스 성 12:13이고 배열 A가 그 차례로 포개져 늘었다
//   ({84,130} ⊂ {84,85,86,130,150,153} ⊂ {…,124,125,128,151,152,155,156}). 124·125·126·127·128·156은 제라노스 성 안 트리거(맵 40)인데 아멜고 저장
//   때는 꺼져 있었으므로, 이것이 켜져 있으면 아멜고 시점을 지난 것이다(수도 아멜고에 들렀다).
// 쓰지 못한 것: 84·86·130·150·151·155는 어느 트리거·스크립트와도 잇지 못했다. 85는 아이언우드(맵 71) 트리거라 마을과 무관.
// 표: {플래그 비트, 그 플래그가 켜져 있으면 지나온 마을}
typedef struct { int flag; const char *town; } seed_rule_t;
static const struct { int flags[10]; const char *town; } SEEDS[] = {
    {{152, 153, 154, 124, 125, 126, 127, 128, 156, -1}, "제라노스 성"},   // 맵 39(a0) 성 앞 트리거 + 맵 40 성 안 트리거
    {{122, 149, -1}, "수도 아멜고"},                                       // 맵 38(a0)
    {{124, 125, 126, 127, 128, 156, -1}, "수도 아멜고"},                   // 근거 2: 성 안 사건은 아멜고 다음
    {{116, 117, -1}, "프리빌"},                                            // 맵 24(a0)
    {{66, 67, -1}, "호스스피릿"},                                          // 맵 51(a0)
    {{69, 70, 167, -1}, "트리튼"},                                         // 맵 52(a0)
    {{181, 182, -1}, "매그파이마운트"},                                    // 맵 55(a0)
    {{83, 157, 158, -1}, "라바티마을"},                                    // 맵 65(a0)
    {{319, -1}, "쥬얼힐즈"},                                               // 맵 65(a2)
    {{188, -1}, "포스웰"},                                                 // 맵 65(a3)
    {{107, -1}, "가디언마을"},                                             // 맵 15(a0)
    {{160, -1}, "뉴러키우드"},                                             // 맵 80(a0)
    {{186, 187, -1}, "페어리랜드"},                                        // 맵 82(a0)
    {{353, 355, -1}, "엘프랜드"},                                          // 맵 28(a0)
    {{91, 92, 93, -1}, "호스우드"},                                        // 맵 72(a3)
};
static int flag_on(uint32_t G, int bit) {
    static const char *test; static int init;
    if (!init) { init = 1; test = getenv("WARP_TESTFLAGS"); } // 시험용: "66,69"처럼 적으면 그 비트도 켜진 것으로 본다
    if (test) for (const char *q = test; *q;) { if (atoi(q) == bit) return 1; q = strchr(q, ','); if (!q) break; q++; }
    return (int)(G32(G + 0x38 + 4u * (uint32_t)(bit >> 5)) >> (bit & 31) & 1);
}
static void seed_visits(uint32_t G) {
    if (ext_get("warp.seeded", 0)) return;
    ext_set("warp.seeded", 1);
    for (size_t k = 0; k < sizeof SEEDS / sizeof SEEDS[0]; k++) {
        int hit = -1;
        for (int j = 0; SEEDS[k].flags[j] >= 0; j++) if (flag_on(G, SEEDS[k].flags[j])) { hit = SEEDS[k].flags[j]; break; }
        if (hit < 0) continue;
        for (int i = 0; i < NTOWN; i++)
            if (!strcmp(TOWNS[i].name, SEEDS[k].town) && !visited(i)) { set_visit(i); DBG("[warp] seed %s (flag %d)\n", TOWNS[i].name, hit); }
    }
}

static void record_tick(uint32_t G) {
    if (rec_left < 0) return;
    if (--rec_left > 0) return;
    rec_left = -1;
    const char *why;
    seed_visits(G); // 현재 마을보다 먼저 올려 목록에서 현재 마을이 가장 최근이 된다
    int i = town_of(gstr(G + G_REGION));
    if (i < 0 || !free_to_move(G, &why)) { DBG("[warp] skip town=%d\n", i); return; }
    if (!visited(i)) DBG("[warp] new town %d %s\n", i, TOWNS[i].name);
    set_visit(i);
}

// ---------------------------------------------------------------- 사용
static struct {
    int open, cursor, lock, show, armed; // show: 다음 문서에서 커서 줄이 보이게 넘긴다(키 이동·열 때)
    // armed: 사용자가 줄을 직접 눌러 골랐다(한 번 더 누르면 이동)
    int n; town_t t[16];
    int pend, pm, pa, pb, ppx, ppy;
    const char *pcp; // 도착지 이름(CP949), 없으면 NULL  // 고른 뒤 필드에서 이동하기를 기다린다
} W;

static int free_to_move(uint32_t G, const char **why) {
    uint32_t fl = G32(G + G_FLAGS), gf = G32(G + G_FLAGS2);
    if (G32(G + G_SCENE) != 14 && G32(G + G_SCENE) != 7) { *why = "지금은 쓸 수 없습니다"; return 0; }
    if (G32(G + G_FSTATE) != 0) { *why = "이벤트 중에는 쓸 수 없습니다"; return 0; }
    if ((fl & 8) || (fl & 0x1800)) { *why = "여기서는 쓸 수 없습니다"; return 0; }       // 비행정(8)·미니게임(0x1800)
    if ((gf & 0x20000) || !(gf & 0x1000)) { *why = "이곳에서는 귀환 주문서를 쓸 수 없습니다"; return 0; } // 저장 불가 지역(던전 특수 상태·이벤트)
    return 1;
}

static int warp_use(int id, int where) {
    (void)id;
    uint32_t G = cur_game();
    const char *why = NULL;
    if (!G || where != EXT_FIELD) { ext_toast("여기서는 쓸 수 없습니다"); return 0; }
    if (!free_to_move(G, &why)) { ext_toast(why); return 0; }
    W.n = towns(W.t, gstr(G + G_REGION));
    int any = 0;
    for (int i = 0; i < W.n; i++) if (!W.t[i].cur) any = 1;
    if (!any) { ext_toast(W.n ? "이곳 말고는 가 본 마을이 없습니다" : "아직 가 본 마을이 없습니다"); return 0; }
    W.open = 1; W.lock = LOCK_TICKS; W.show = 1; W.armed = 0;
    W.cursor = 0;
    while (W.cursor < W.n - 1 && W.t[W.cursor].cur) W.cursor++; // 첫 줄이 지금 있는 곳이면 다음 줄
    return 0; // 소모는 마을을 고른 뒤에 한다
}
__attribute__((constructor)) static void warp_register(void) { ext_register_use(IT_WARP, warp_use); }

static void confirm(uint32_t G) {
    town_t *t = &W.t[W.cursor];
    if (t->cur) { ext_toast("지금 있는 곳입니다"); return; }
    const char *why = NULL;
    if (!free_to_move(G, &why)) { ext_toast(why); W.open = 0; return; }
    if (!ext_take(IT_WARP, 1)) { ext_toast("귀환 주문서가 없습니다"); W.open = 0; return; }
    W.pend = 1; W.pm = t->m; W.pa = t->a; W.pb = t->b; W.ppx = t->px; W.ppy = t->py; W.pcp = TOWNS[t->idx].cp;
    
    W.open = 0;
    uint32_t M = G32(G + G_MENU);
    if (M) W32_(&cpu, M + M_CLOSE, 1); // 원작의 메뉴 닫기(2fb1c가 필드로 돌려 보낸다)
    DBG("[warp] confirm m=%d a=%d b=%d %s\n", t->m, t->a, t->b, t->name);
}

// 필드 메뉴 키(0x2fa60). 열려 있으면 모두 가져간다(1을 돌려주면 원본 키 처리를 하지 않는다).
int ext_warp_key(uint32_t menu, int k) {
    if (!W.open) return 0;
    uint32_t G = G32(menu);
    if (W.lock > 0) return 1;
    if (!k) return 1;
    if (k == KEY_UP || k == KEY_2) { if (W.n) W.cursor = (W.cursor + W.n - 1) % W.n; W.armed = 0; W.show = 1; }
    else if (k == KEY_DOWN || k == KEY_8) { if (W.n) W.cursor = (W.cursor + 1) % W.n; W.armed = 0; W.show = 1; }
    else if (k == KEY_OK || k == KEY_5) confirm(G);
    else if (k == KEY_CLR) W.open = 0;
    return 1;
}

// 메뉴 장면 틱 뒤(0x109b8): 목록 창을 그린다. 누른 줄(pick)을 처리한다.
void ext_warp_draw(uint32_t G) {
    if (!W.open) { ui_pick("warp"); return; }
    if (G32(G + G_SCENE) != 14) { W.open = 0; return; }
    if (W.lock > 0) W.lock--;
    int pk = ui_pick("warp");
    if (pk >= 0 && pk < W.n && W.lock == 0) {
        if (pk == W.cursor && W.armed) confirm(G); // 눌러서 고른 줄을 한 번 더 누르면 이동(처음부터 가리키던 줄을 바로 누르면 이동하지 않는다)
        else { W.cursor = pk; W.armed = 1; }
    }
    if (!W.open) return;
    ui_begin("warp");
    ui_force();
    int shell = fm_overlay_frame(G32(G + G_MENU)); // 필드 메뉴 틀 안이면 내용 자리에, 이동·취소는 고정 버튼 띠
    ui_int("have", ext_count(IT_WARP));
    ui_int("n", W.n);
    ui_int("cursor", W.cursor);
    ui_int("narrow", ui_screen_w() < 400);
    ui_int("can", W.n && !W.t[W.cursor].cur);
    ui_list("items");
    for (int i = 0; i < W.n; i++) {
        ui_item();
        ui_text("name", W.t[i].name);
        ui_int("idx", i);
        ui_int("cur", W.t[i].cur);
        ui_int("newest", i == 0 || (W.n > 1 && i == 1 && W.t[0].cur));
        ui_close();
    }
    ui_close();
    (void)shell; { fm_bar_begin(); fm_bar_ok("이동", W.n && !W.t[W.cursor].cur && W.lock == 0); fm_bar_emit(G32(G + G_MENU)); }
    ui_end();
    // 목록은 scroll="wlist" 상자(끌기·휠·오른쪽 스틱은 호스트). 키로 옮긴 커서 줄만 보이게 넘긴다(줄이 생긴 다음 틱까지 다시 해 본다).
    if (W.show) W.show = !ui_scroll_show("wlist", W.cursor);
}

// ---------------------------------------------------------------- 이동(필드 틱)
static uint32_t cmd_g; // 게스트에 만든 명령 0x49: [0] 포인터 칸, [4..] 명령

static uint32_t heap_alloc_str(const char *cp) {
    static uint32_t buf; // 게스트에 놓은 이름 칸 하나를 되쓴다
    if (!buf) buf = heap_alloc(48);
    strncpy((char *)cpu.m + buf, cp, 47);
    cpu.m[buf + 47] = 0;
    return buf;
}

static void do_warp(uint32_t G) {
    if (!cmd_g) cmd_g = heap_alloc(32);
    uint32_t pc = cmd_g, cmd = cmd_g + 8;
    W32_(&cpu, pc, cmd);
    W16_(&cpu, cmd, 0x49); W16_(&cpu, cmd + 2, 0x22); W8_(&cpu, cmd + 4, 1); W8_(&cpu, cmd + 5, 1); W16_(&cpu, cmd + 6, 0x09c4);
    W8_(&cpu, G + G_DEST, (uint32_t)W.pm); W8_(&cpu, G + G_DEST + 1, (uint32_t)W.pa); W8_(&cpu, G + G_DEST + 2, (uint32_t)W.pb);
    if (W.pcp) CALL(0x100000 + 0xd6a8, G, heap_alloc_str(W.pcp), heap_alloc_str(W.pcp)); // 스크립트 0xc1처럼 이름을 먼저 준다(맵이 이름을 주면 그것이 이긴다)
    uint32_t hero = G32(G + G_HERO);
    if (hero && W.ppx) { W32_(&cpu, hero + 8, (uint32_t)W.ppx); W32_(&cpu, hero + 0xc, (uint32_t)W.ppy); } // 스크립트 0x54와 같다
    DBG("[warp] go m=%d a=%d b=%d pos=%d,%d\n", W.pm, W.pa, W.pb, W.ppx, W.ppy);
    CALL(FN_LOADMAP, G32(G + G_VM), pc);
}

// 시험용: 환경 변수 WARP_CMD=<파일>에 "맵 a b [px py [표번호]]"를 적어 두면 필드 틱에서 한 번 이동하고 지운다.
static void debug_cmd(uint32_t G) {
    static const char *p; static int init;
    if (!init) { init = 1; p = getenv("WARP_CMD"); }
    if (!p) return;
    FILE *f = fopen(p, "r");
    if (!f) return;
    int m, a, b, x = 0, y = 0, ti = -1;
    if (fscanf(f, "%d %d %d %d %d %d", &m, &a, &b, &x, &y, &ti) >= 3) { W.pm = m; W.pa = a; W.pb = b; W.ppx = x; W.ppy = y; W.pcp = ti >= 0 && ti < NTOWN ? TOWNS[ti].cp : NULL; fclose(f); remove(p); do_warp(G); return; }
    fclose(f);
}

__attribute__((weak)) void ext_incense_tick(uint32_t G) { (void)G; }

// 필드 틱 0x1054c 앞(ui_dialog.c가 부른다)
void ext_ascan(uint32_t G);
void ext_field_tick(uint32_t G) {
    ext_ascan(G);
    if (G32(G + G_SCENE) != 7) return;
    if (W.pend && G32(G + G_FSTATE) == 0) {
        W.pend = 0;
        do_warp(G); // 도착하면 지역 이름 띠가 뜬다
        return;
    }
    record_tick(G);
    debug_cmd(G);
    ext_incense_tick(G);
}

// ---- 조사용(WARP_ASCAN=<"맵,a,b" 목록 파일>): 맵마다 실제로 이동해 액터의 플래그 번호를 적는다 ----
static void dump_actors(uint32_t G, int m, int a, int b) {
    fprintf(stderr, "AMAP %d,%d,%d n4dc=%s n4fc=%s\n", m, a, b, gstr(G + 0x4dc), gstr(G + 0x4fc));
    uint32_t nb = G32(G + 0x340), arr = G32(G + 0x344);
    for (uint32_t i = 0; i < nb && i < 30; i++) {
        uint32_t base = arr + i * 0x44, cnt = G32(base);
        for (uint32_t j = 0; j < cnt && j < 16; j++) {
            uint32_t o = G32(base + 4 + 4 * j);
            if (!o) continue;
            fprintf(stderr, "ACT %d,%d,%d vt=%x flag=%d b4=%d b5=%d w6=%x x=%d y=%d\n", m, a, b, G32(o), (int)G32(o + 0x40), R8_(&cpu, o + 4), R8_(&cpu, o + 5), R16_(&cpu, o + 6), (int)G32(o + 0x14), (int)G32(o + 0x18));
            if (G32(o) == 0x14da10) fprintf(stderr, "TRIG %d,%d,%d flag=%d b5=%d r=%d,%d,%d,%d\n", m, a, b, (int)G32(o + 0x40), R8_(&cpu, o + 5), (int)G32(o + 0x2c), (int)G32(o + 0x30), (int)G32(o + 0x34), (int)G32(o + 0x38));
            if (G32(o) == 0x14dafc) fprintf(stderr, "CHEST %d,%d,%d w3c=%d w44=%d w48=%d w4c=%d w50=%d w54=%d b5=%d\n", m, a, b, (int)G32(o + 0x3c), (int)G32(o + 0x44), (int)G32(o + 0x48), (int)G32(o + 0x4c), (int)G32(o + 0x50), (int)G32(o + 0x54), R8_(&cpu, o + 5));
        }
    }
    fflush(stderr);
}
void ext_ascan(uint32_t G) {
    static char buf[16384];
    static char *tok; static int init, wait, cur[3], have;
    const char *dp = getenv("WARP_ASCAN");
    if (!dp) return;
    if (!init) {
        init = 1;
        FILE *f = fopen(dp, "r");
        size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0; buf[n] = 0; if (f) fclose(f);
        tok = strtok(buf, " \n");
        wait = 30;
    }
    if (G32(G + G_FSTATE) != 0) return;
    if (wait-- > 0) return;
    if (have) { dump_actors(G, cur[0], cur[1], cur[2]); have = 0; }
    if (!tok) { static int fin; if (!fin) { fin = 1; fprintf(stderr, "ASCANDONE\n"); } return; }
    sscanf(tok, "%d,%d,%d", &cur[0], &cur[1], &cur[2]);
    tok = strtok(NULL, " \n");
    if (cur[0] == 0) return;
    W.pm = cur[0]; W.pa = cur[1]; W.pb = cur[2]; W.ppx = 0; W.ppy = 0; W.pcp = NULL;
    do_warp(G);
    have = 1; wait = 25;
}

// 창이 떠 있다 / 닫는다(필드 메뉴 틀 fm_shell.c: 레일을 누르면 CLR과 같은 길로 닫고 그 화면으로 간다. 주문서는 고른 뒤에만 쓰므로 그대로 남는다)
int ext_warp_open(void) { return W.open; }
void ext_warp_close(void) { W.open = 0; }
