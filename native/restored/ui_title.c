// 타이틀 영역(첫 실행, 타이틀, 메인 메뉴와 그 하위, 로딩, 오류, 스태프롤)을 RmlUi로 다시 그린다.
// 조사: work/ui/survey/title.md. 주소는 파일 오프셋(게스트 = +0x100000).
//
// 원본 장면 함수는 틱, 키 처리, 그리기를 한 함수에서 한다. 그래서 장면 함수는 감싸지 않고 그 안의 그리기 함수만 감싼다.
// 감싼 함수는 원본을 ui_scratch 안에서 불러(부수 효과: 깜빡임 카운터 W+0x49c, 한마디 자료 읽기, 랭킹 String 등을 지킨다)
// 그린 것은 버리고, 그 틱의 상태를 문서로 넘긴다. 타이틀 바탕(띠, 금테, 로고)과 타이틀 그림은 원본이 그대로 그린다.
//
//   0x10ef0 속도최적화/재시작 안내(장면 22)  → title_boot
//   0xe85c  타이틀 PRESS ANY KEY(장면 2)     → title_press(알림 글만 바꾸고 그림은 원본)
//   0xf580  메인 메뉴 목록(레벨 0,1,2,4,8)  ┐
//   0xf8c0  환경설정(레벨 3)                 │
//   0xfc9c  게임문의(5) 0x100a0 도움말·게임속으로(6,7)
//   0xfe68  정보이용료(10) 0xffbc OST 곡 정보(12) → title_menu 한 문서(부모 메뉴를 옆에 두고 하위 메뉴/쪽을 오른쪽에)
//   0x13060 에픽한마디(9) 0x134cc 에픽랭킹(11) ┘
//   0x10410 Loading → title_loading, 0xe588 프로그램오류 → title_error, 0xbfe8 스태프롤 → title_staff
//   0x12598 화면 영역 적용: 새 UI에서는 퀵모드(384×240 영역)를 무시한다(README "정해진 것").
#include "restored.h"
#include "cfgx.h"
#include "help.h"
#include <stdio.h>
#include <string.h>

int font_string_width(const uint8_t *s, int len);

#define W_OPT_EDIT VG(W_OPT_EDIT_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
#define W_QUOTES VG(W_QUOTES_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
#define W_STAFF VG(W_STAFF_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
enum {
    W_GFX = 0x08, W_SCENE = 0x0c, W_SUB = 0x10, W_STATE = 0x14,
    W_OPT = 0x14c,       // 저장된 옵션/해금 플래그(gopt.sav 첫 워드)
    W_CUR = 0x15c,       // 메뉴 커서(쪽 화면에서는 쪽 번호, 한마디는 글 번호, 첫 실행은 진행 카운터)
    W_LAYOUT = 0x160,    // 화면 높이로 정한 배치 방식(0: <=179, 1: <=249, 2: 그 위). 320 높이는 2
    W_LEVEL = 0x164,     // 메뉴 레벨 0~12
    W_MENU_Y = 0x168,    // 로고 아래 메뉴가 시작하는 y
    W_TSHEET = 0x45c,    // 타이틀 스프라이트 시트(리소스 0x67)
    W_TSTR = 0x474,      // String: 한마디/랭킹 본문 작업본(+4가 char*)
    W_BLINK = 0x49c,     // 깜빡임 카운터. 한마디에서는 줄 스크롤 위치
    W_SCORE = 0x4ac,     // 보스열전 점수
    W_OPT_EDIT_D5 = 0x540,  // 환경설정 작업본
    W_QUOTES_D5 = 0x6d0,    // 한마디 자료(+8부터 NUL로 나뉜 글들, 빈 글로 끝)
    W_STAFF_D5 = 0x6c8,     // 스태프롤 글 객체 목록

    OPT_TURN = 0x1, OPT_SOUND_OFF = 0x2, OPT_QUICK = 0x40, OPT_SPEED_SHIFT = 8,
    OPT_TRIPLE = 0x4, OPT_HERO = 0x8, OPT_OST0 = 0x10000, OPT_STAFF = 0x400000,

    FN_FILL = 0x10467c,  // (gfx|0, rect*|0, 색 0xBBGGRR00) 클립 전체 채우기
    FN_SPR = 0x1056ec,   // (시트, 프레임, x, y, 0, 0, 뒤집기)

    LINE_H = 14,
};

static inline uint32_t rd(uint32_t a) { return R32_(&cpu, a); }
static inline const char *got_str(uint32_t off) { return gstr(rd(GOT_ADDR(off))); }
static inline uint32_t got_ptr(uint32_t off) { return rd(GOT_ADDR(off)); }

// 원본 그리기를 화면 밖(임시 화면)에서 돌린다. 원본이 그린 글은 ui_cap_text로 남는다.
#define RUN_HIDDEN(fn, c) do { ui_scratch_begin(); fn(c); ui_scratch_end(); } while (0)

// ---- 글 감싸기 ----
// 문서 폭에 맞춰 C에서 줄을 나눈다(RmlUi 글꼴은 게임 글리프 표와 같은 Neo둥근모라 폭이 같다).
// 쪽 표시와 스크롤 위치를 정하려면 줄 수를 알아야 해서 RmlUi의 자동 줄바꿈 대신 여기서 나눈다.
// 공백에서 끊고, 한 낱말이 한 줄보다 길면 글자에서 끊는다. CP949 2바이트 글자는 쪼개지 않는다.
typedef struct { int start, len; } span_t;
enum { MAX_LINES = 256 };

static int ch_len(const uint8_t *s) { return (s[0] & 0x80) && s[1] ? 2 : 1; }
static int text_w(const char *s, int n) { return font_string_width((const uint8_t *)s, n); }

static int wrap_text(const char *s, int maxw, span_t *out, int max) {
    int n = 0, len = (int)strlen(s), i = 0;
    while (i <= len && n < max) {
        int e = i;
        while (e < len && s[e] != '\n') e++;        // [i, e) 한 단락
        int pe = e;
        if (pe > i && s[pe - 1] == '\r') pe--;
        int p = i;
        if (p == pe) out[n++] = (span_t){p, 0};
        while (p < pe && n < max) {
            int q = p, last_sp = -1, w = 0;
            while (q < pe) {
                int cl = ch_len((const uint8_t *)s + q);
                int cw = text_w(s + q, cl);
                if (w + cw > maxw && q > p) break;
                if (s[q] == ' ') last_sp = q;
                w += cw; q += cl;
            }
            if (q < pe && last_sp > p) q = last_sp;  // 낱말 사이에서 끊는다
            out[n++] = (span_t){p, q - p};
            p = q;
            while (p < pe && s[p] == ' ') p++;       // 다음 줄 앞 공백은 버린다
        }
        i = e + 1;
    }
    return n;
}

static void emit_lines(const char *key, const char *s, const span_t *sp, int from, int count) {
    ui_list(key);
    for (int i = from; i < from + count; i++) { ui_item(); ui_strn("t", s + sp[i].start, sp[i].len); ui_close(); }
    ui_close();
}

// 원본 키 글자(OK, CLR, *, #, 0~9) → 문서의 키 이름(ok, clear, star, hash, 0~9). 방향 안내(↑↓ 등)는 키가 아니라 NULL.
// 터치에서는 이 이름이 쌍 전체를 누르는 버튼으로 만든다(`key` 속성), 키보드·패드에서는 `| key` 이름표가 장치별 글자로 바꾼다.
static const char *key_id(const char *k) {
    if (!strcmp(k, "OK") || !strcmp(k, "ok")) return "ok";
    if (!strcmp(k, "CLR") || !strcmp(k, "clear")) return "clear";
    if (!strcmp(k, "*") || !strcmp(k, "star")) return "star";
    if (!strcmp(k, "#") || !strcmp(k, "hash")) return "hash";
    if (k[0] >= '0' && k[0] <= '9' && !k[1]) return k;
    return NULL;
}

// 키 안내 한 쌍: k = 키 이름 또는 방향 안내 글, key = 누르면 보낼 키(방향 안내는 빈 값), nav = 방향 안내(터치에서 감춘다)
static void key_hint(const char *k, const char *t) {
    const char *id = key_id(k);
    ui_item(); ui_text("k", id ? id : k); ui_text("key", id ? id : ""); ui_int("nav", id == NULL); ui_text("t", t); ui_close();
}

// "OK : 계속", "CLR : 나가기" 같은 원본 키 안내 글(CP949)을 키와 설명으로 나눈다. 나누지 못하면 글 그대로 둔다.
static void emit_key_text(const char *s) {
    char k[32] = "", t[96] = "";
    const char *col = strstr(s, ":");
    ui_item();
    if (col && col - s < 30) {
        int kl = (int)(col - s);
        while (kl > 0 && s[kl - 1] == ' ') kl--;
        memcpy(k, s, (size_t)kl); k[kl] = 0;
        const char *v = col + 1;
        while (*v == ' ') v++;
        snprintf(t, sizeof t, "%s", v);
        const char *id = key_id(k);
        ui_str("k", id ? id : k); ui_text("key", id ? id : ""); ui_int("nav", id == NULL); ui_str("t", t);
    } else { ui_text("k", ""); ui_text("key", ""); ui_int("nav", 0); ui_str("t", s); }
    ui_close();
}

// =====================================================================
// 메인 메뉴(장면 4)
// =====================================================================

// 터치 목록(ui_nav)은 "키를 보낸 틱의 그리기가 새 커서를 알린다"고 가정한다(호스트가 알린 커서로 다음 키를 정한다).
// 그런데 ebc4는 그 틱의 키를 읽고(61d4) 그린 뒤에야 키를 처리하므로 그리기 때 보이는 커서는 한 틱 늦다. 그대로 알리면
// 호스트가 이미 닿은 줄을 모르고 한 칸 더 간다. 그래서 장면 함수가 받은 입력 객체를 기억해 두었다가, 그 틱에 처리될 방향 키를
// 미리 적용한 커서를 알린다(처리가 바뀌는 곳은 아래 pend_dir 호출부 주석).
static uint32_t g_input;
typedef struct { int lv, sel; } step_t;
static int chain_of(int lv, int cur, step_t *ch);

// 보이는 메뉴 누르기(사용자 결정 2026-11-10: 보이는 메뉴는 어느 단계든 바로 고를 수 있어야 한다). 원작은 지금 목록만 고를 수 있다.
// 문서의 pick="tmenu"(idx = 레벨*16 + 칸)를 받으면 틱마다 원본 키를 하나씩 넣는다: 목표가 지금 단계의 아래면(메인에서 미리 보인
// 하위 목록, 게임정보에서 미리 보인 OST 곡) 사슬의 칸에 커서를 두고 OK로 내려가고, 아니면 CLR로 올라간다. 목표 단계에 닿으면 칸에 두고 OK.
static int tm_lv = -1, tm_idx, tm_steps;
static void tm_inject(uint32_t in, int code) {
    W16_(&cpu, in, (uint32_t)code); W16_(&cpu, in + 2, 1); W32_(&cpu, in + 12, 1);
}
static int g_quit_arm;
// 처음 들어온 메인(사용자 요청 2026-11-10): 타이틀에서 메인으로 막 넘어오면 어느 항목에도 커서를 보이지 않는다(원본 커서는 0).
// 첫 키(방향, OK)는 삼키고 커서만 보이게 한다(게임시작에 놓인다). 터치로 항목을 누르면 바로 그 항목으로 간다(pick tmenu).
// 타이틀(장면 2, f_e85c)이 틱마다 켠다.
static int g_nofocus;
static int key_now(uint32_t in) { // 이 틱에 처리될 키(없으면 0)
    if (!in || !R32_(&cpu, in + 12) || R16_(&cpu, in + 2) != 1) return 0;
    return R16_(&cpu, in);
}

// ---- 환경설정의 원작에 없는 줄(화면, 일러스트, 조작 설정, 기타 설정, 세이브 데이터: restored/cfgx.c) ----
// 원작 줄(전투, 사운드, 속도) 아래에 둔다. 원본 커서(W_CUR 0~2, 3은 숨긴 퀵모드)로는 닿지 않는 가상 줄이라 커서는 cfgx가 들고,
// 그 줄에 있는 동안의 위아래·좌우·OK(여는 줄)는 원본에 보내지 않는다. 순서는 0 → 1 → 2 → 가상 줄들 → 0으로 돈다(터치 nav "opts"도 같은 고리).
static void opt_extra_key(uint32_t W, int cur, int k) {
    bool up = k == 0x1f || k == 0x52, down = k == 0x1e || k == 0x58, lr = k == 0x1d || k == 0x1c || k == 0x54 || k == 0x56;
    bool ok = k == 0x1b || k == 0x55;
    int set;
    if (!cfgx_key(up, down, lr, ok, cur, 2, &set)) return; // 값 줄의 OK·CLR(나가기)는 원본이 받는다. 나가면 lv가 바뀌어 줄 선택도 풀린다
    if (set >= 0) W32_(&cpu, W + W_CUR, (uint32_t)set);
    W16_(&cpu, g_input, 0);
}

void orig_f_ebc4(cpu_t *c);
void f_ebc4(cpu_t *c) {
    uint32_t W = c->r[0];
    g_input = c->r[1];
    if (ui_modern()) {
        int lv = (int)rd(W + W_LEVEL), cur = (int)rd(W + W_CUR);
        int pk = ui_pick("tmenu");
        if (pk >= 0) { tm_lv = pk / 16; tm_idx = pk % 16; tm_steps = 8; g_quit_arm = 0; g_nofocus = 0; }
        if (lv != 0 || ui_input_source() == UI_SRC_PAD) g_nofocus = 0; // 게임패드는 커서가 있어야 어디를 고르는지 안다(처음부터 첫 줄)
        if (g_nofocus) {
            int k0 = key_now(g_input);
            if (k0 == 0x1b || k0 == 0x55 || k0 == 0x1f || k0 == 0x1e || k0 == 0x52 || k0 == 0x58) { W16_(&cpu, g_input, 0); g_nofocus = 0; }
            else if (k0) g_nofocus = 0;
        }
        if (tm_lv >= 0 && g_input && tm_steps-- > 0) {
            if (lv == tm_lv) { W32_(&cpu, W + W_CUR, (uint32_t)tm_idx); tm_inject(g_input, 0x1b); tm_lv = -1; } // OK
            else {
                step_t ch[4];
                int n = chain_of(tm_lv, tm_idx, ch), down = -1;
                for (int i = 0; i + 1 < n; i++) if (ch[i].lv == lv) down = ch[i].sel;
                if (down >= 0) { W32_(&cpu, W + W_CUR, (uint32_t)down); tm_inject(g_input, 0x1b); } // 한 단계 아래로
                else if (lv != 0) tm_inject(g_input, 0x10);                                      // CLR로 한 단계 위로
                else tm_lv = -1;
            }
        } else if (tm_steps <= 0) tm_lv = -1;
        // 위에서 커서를 옮기고 OK를 넣었을 수 있으니 레벨·커서를 다시 읽는다(하위 메뉴에서 레일의 게임종료를 누르면
        // CLR로 메인까지 올라온 틱에 커서를 4로 두고 OK를 넣는다. 옛 값으로 판정하면 확인 없이 끝났다: 폰에서 발견).
        lv = (int)rd(W + W_LEVEL); cur = (int)rd(W + W_CUR);
        int k = key_now(g_input);
        // 환경설정에서 나가면 늘 저장한다(원작은 OK 저장, CLR 버림). 옵션 메뉴의 즉시 적용과 같은 뜻이고, 바꾼 값을 잃지 않는다.
        if (lv == 3 && k == 0x10) W16_(&cpu, g_input, 0x1b);
        if (lv == 3) opt_extra_key(W, cur, k);
        else cfgx_reset();
        // 게임종료 확인: 원작은 OK 한 번에 바로 끝난다. 첫 OK는 막고 패널에 확인을 띄운다. 다시 OK면 끝내고 CLR이면 접는다.
        if (lv != 0 || cur != 4) g_quit_arm = 0;
        else if (k == 0x1b || k == 0x55) {
            if (!g_quit_arm) { g_quit_arm = 1; W16_(&cpu, g_input, 0); }
        } else if (k == 0x10 && g_quit_arm) { g_quit_arm = 0; W16_(&cpu, g_input, 0); }
        else if (k) g_quit_arm = 0;
    }
    orig_f_ebc4(c);
}

// 이 틱에 처리될 방향 키: 위 -1, 아래 +1(dy), 왼쪽 -1, 오른쪽 +1(dx). 숫자 2/8/4/6도 방향으로 받는다(61d4와 같은 읽기).
static void pend_dir(int *dy, int *dx) {
    *dy = *dx = 0;
    if (!g_input || !R32_(&cpu, g_input + 12) || R16_(&cpu, g_input + 2) != 1) return;
    switch (R16_(&cpu, g_input)) {
    case 0x1f: case 0x52: *dy = -1; break;
    case 0x1e: case 0x58: *dy = 1; break;
    case 0x1d: case 0x54: *dx = -1; break;
    case 0x1c: case 0x56: *dx = 1; break;
    }
}

// 레벨별 목록 항목. f580이 받는 것과 같은 규칙으로 만든다(ebc4 `edf8`~`f45e`).
static int level_items(uint32_t W, int lv, uint32_t *out) {
    uint32_t fl = rd(W + W_OPT);
    int n = 0;
    switch (lv) {
    case 0: for (n = 0; n < 5; n++) out[n] = got_ptr(0x198 + 4 * n); break;
    case 1:
        // 원본 그대로: 해금 수만큼 앞에서부터 센다(트리플슬롯 없이 만든사람들만 풀려도 셋째 칸은 트리플슬롯 글이다)
        n = 2 + ((fl & OPT_TRIPLE) ? 1 : 0) + ((fl & OPT_HERO) ? 1 : 0) + ((fl & OPT_STAFF) ? 1 : 0);
        for (int i = 0; i < n; i++) out[i] = got_ptr(0x1ac + 4 * i);
        break;
    case 2: for (n = 0; n < 5; n++) out[n] = got_ptr(0x1c0 + 4 * n); break;
    case 4: out[0] = got_ptr(0x1d4); out[1] = got_ptr(0x1d8); n = 2; break;
    case 8: {
        uint32_t names = got_ptr(0x1dc);
        for (n = 0; n < 5; n++) out[n] = (fl & (OPT_OST0 << n)) ? rd(names + 4 * n) : got_ptr(0x1e0);
        break;
    }
    }
    return n;
}

// 레벨 → 부모 사슬. 각 단계는 (그 목록의 레벨, 그 목록에서 고른 칸)이다. 마지막이 지금 화면이다.
// 원본은 하위 레벨로 들어가며 커서를 0으로 두므로 부모 칸은 레벨에서 정해진다(CLR 표 0x47350과 같다).
static int chain_of(int lv, int cur, step_t *ch) {
    switch (lv) {
    case 0: ch[0] = (step_t){0, cur}; return 1;
    case 1: case 2: case 4: ch[0] = (step_t){0, lv - 1}; ch[1] = (step_t){lv, cur}; return 2;
    case 3: ch[0] = (step_t){0, 2}; return 1;                                   // 환경설정: 메인 + 쪽
    case 5: case 6: case 7: case 9: {
        static const int idx[10] = {[9] = 0, [5] = 1, [6] = 2, [7] = 3};
        ch[0] = (step_t){0, 1}; ch[1] = (step_t){2, idx[lv]}; return 2;
    }
    case 8: ch[0] = (step_t){0, 1}; ch[1] = (step_t){2, 4}; ch[2] = (step_t){8, cur}; return 3;
    case 10: case 11: ch[0] = (step_t){0, 3}; ch[1] = (step_t){4, lv - 10}; return 2;
    case 12: ch[0] = (step_t){0, 1}; ch[1] = (step_t){2, 4}; ch[2] = (step_t){8, cur}; return 3;
    }
    ch[0] = (step_t){0, 0};
    return 1;
}

// ---- 배치 ----
// 사용자 요청(2026-11-10): 단계마다 창이 늘고 줄고 자리를 바꾸며 아래 키 안내도 매번 바뀌어 어수선했다. 다른 게임 메뉴를 참고해
// (Octopath Traveler·Xenoblade 설정 화면의 왼쪽 분류 + 오른쪽 내용, FF 픽셀 리마스터의 항목 설명 줄과 뮤직 플레이어,
//  콘솔 공통의 오른쪽 아래 고정 버튼 안내) 틀을 고정한다.
//   - 왼쪽 레일: 메인 5항목. 어느 단계에서나 같은 자리에 있고 지금 분류가 금색으로 남는다.
//   - 오른쪽 패널: 늘 같은 크기. 머리 띠에 지금 위치(게임정보 › 도움말, 누르면 그 단계로), 안에 [하위 목록] + [내용 칸].
//     메인에서 커서를 옮기면 그 분류의 하위 목록과 설명을 흐리게 미리 보인다. 하위 목록에서는 내용 칸이 고른 항목의 설명을,
//     쪽을 열면 그 쪽을 보인다. 에픽 OST는 곡 목록 + 곡 정보(듣는 곡 ♪)를 한 칸에 둔다.
//   - 키 안내: 오른쪽 아래 「결정」「이전」 두 칸 고정. 쓸 수 없는 단계에서는 흐리게 한다. 쪽 안 조작(◀▶, ▲▼, 1/2)은 쪽 안에 둔다.
enum { MAX_ITEMS = 5, STAFF_ROWS = 24 };
enum { SIDE = 16, GAP = 6, RAIL_W = 92, SUB_W = 100, OST_W = 96, PANEL_PAD = 18, BODY_PAD = 16, PANEL_MAX = 560, HIDX_MIN = 300, COVER_TOP = 16 };

static int scr_w(void) { int w = ui_screen_w(); return w > 0 ? w : 240; }

// 항목 설명(새 UI가 붙인 글). 원작에는 없지만 고른 항목이 무엇을 하는지 미리 알려 준다.
static const char *const DESC_MAIN[5] = {
    "모험을 시작하거나 이어서 합니다.", "게임에 관한 여러 정보를 봅니다.", "전투 방식, 사운드, 속도를 바꿉니다.",
    "랭킹과 정보이용료를 봅니다.", "게임을 끝냅니다.",
};
static const char *const DESC_START[5] = {
    "저장한 곳에서 모험을 이어서 합니다.", "처음부터 새로 시작합니다.", "미니게임 트리플슬롯을 합니다.",
    "미니게임 저들의영웅을 합니다.", "만든 사람들을 봅니다.",
};
static const char *const DESC_INFO[5] = {
    "에픽크로니클에 보내 온 한마디를 봅니다.", "문의할 곳을 봅니다.", "게임 방법을 항목별로 봅니다.",
    "에픽크로니클의 이야기와 세계를 봅니다.", "게임 음악을 듣습니다.",
};
static const char *const DESC_NET[2] = { "네트워크 이용 요금을 안내합니다.", "보스열전 점수로 매긴 랭킹을 봅니다." };


// 이 틱의 화면 상태(래퍼마다 같은 규칙으로 정한다)
typedef struct {
    int lv, in_menu, narrow;
    int cat, rail_act;           // 레일 커서(메인 항목), 레일이 고르는 중인가
    int sub_lv, sub_mode, sub_cur; // 하위 목록 레벨(1,2,4, 없으면 -1), 0 없음 1 미리보기 2 고르는 중 3 고른 칸 표시, 칸
    int ost_mode, ost_cur;       // 0 없음 1 미리보기 2 고르는 중(L8) 3 듣는 중(L12)
    int rail, panel_w, sub, detail_w; // 배치
    char dkind[12];
} view_t;

// cur_eff: 이 틱의 방향 키를 미리 적용한 커서(목록 단계만, 아래 f_f580)
static void view_of(uint32_t W, const char *page_kind, int cur_eff, view_t *v) {
    memset(v, 0, sizeof *v);
    v->lv = (int)rd(W + W_LEVEL);
    v->in_menu = rd(W + W_SCENE) == 4;
    v->narrow = scr_w() < 400;
    v->cat = -1; v->sub_lv = -1; v->sub_cur = -1; v->ost_cur = -1;
    snprintf(v->dkind, sizeof v->dkind, "%s", page_kind);
    if (v->in_menu) {
        step_t ch[4];
        int lv = v->lv, nch = chain_of(lv, cur_eff, ch);
        (void)nch;
        if (lv == 0 && g_nofocus) {
            v->rail_act = 1; // 고르는 곳은 레일이지만 커서는 없다. 패널은 안내만
            if (!page_kind[0]) snprintf(v->dkind, sizeof v->dkind, "desc");
        } else if (lv == 0) {
            v->cat = cur_eff; v->rail_act = 1;
            static const int sub_of[5] = {1, 2, -1, 4, -1};
            v->sub_lv = cur_eff >= 0 && cur_eff < 5 ? sub_of[cur_eff] : -1;
            v->sub_mode = v->sub_lv > 0;
            if (!page_kind[0]) snprintf(v->dkind, sizeof v->dkind, "%s", cur_eff == 2 ? "opt" : g_quit_arm ? "quit" : "desc");
        } else {
            v->cat = ch[0].sel;
            if (lv == 1 || lv == 2 || lv == 4) {
                v->sub_lv = lv; v->sub_mode = 2; v->sub_cur = cur_eff;
                if (lv == 2 && cur_eff == 4) { v->ost_mode = 1; snprintf(v->dkind, sizeof v->dkind, "ost"); }
                else if (!page_kind[0]) snprintf(v->dkind, sizeof v->dkind, "desc");
            } else if (lv != 3) {
                v->sub_lv = ch[1].lv; v->sub_mode = 3; v->sub_cur = ch[1].sel;
                if (lv == 8 || lv == 12) {
                    v->ost_mode = lv == 8 ? 2 : 3; v->ost_cur = lv == 8 ? cur_eff : ch[2].sel;
                    snprintf(v->dkind, sizeof v->dkind, "ost");
                }
            }
        }
    }
    // 배치: 넓으면 레일 + 패널(패널 폭 고정). 좁으면(폭 < 400, 원작 대조 시험) 메인에서는 레일만, 그 아래에서는 패널만.
    int avail = scr_w() - 2 * SIDE;
    if (!v->in_menu) { v->rail = 0; v->panel_w = avail < 440 ? avail : 440; }
    else if (v->narrow) {
        int list_only = v->lv == 0 && !g_quit_arm; // 게임종료 확인은 패널에 뜬다
        v->rail = list_only;
        v->panel_w = list_only ? 0 : avail;
    } else {
        v->rail = 1;
        v->panel_w = avail - RAIL_W - GAP;
        if (v->panel_w > PANEL_MAX) v->panel_w = PANEL_MAX;
    }
    v->sub = v->panel_w > 0 && v->sub_mode != 0 && (!v->narrow || v->sub_mode == 2);
    v->detail_w = v->panel_w - PANEL_PAD - (v->sub ? SUB_W + GAP : 0);
}

// 원작 그리기(F2)에서 퀵모드로 줄어든 화면 영역이 새 UI로 돌아온 뒤에도 남아 있으면 다시 적용한다(f_12598이 전체로 편다).
// 12598은 한 번 검게 지우고 내보내므로 화면이 한 프레임 깜빡인다. 바꾼 순간 한 번뿐이다.
void f_12598(cpu_t *c);
static void ensure_full_view(uint32_t W) {
    uint32_t gfx = rd(W + W_GFX);
    if ((int)rd(gfx + 0xdc + 8) == scr_w() && rd(gfx + 0xdc + 12) == 320) return;
    // 게스트 주소로 부르면 원본(orig_f_12598)으로 가므로 감싼 함수를 바로 부른다
    uint32_t r[4] = {cpu.r[0], cpu.r[1], cpu.r[2], cpu.r[3]};
    cpu.r[0] = W;
    f_12598(&cpu);
    for (int i = 0; i < 4; i++) cpu.r[i] = r[i];
}

static span_t g_sp[MAX_LINES];
static void emit_opts(uint32_t o, int cur);

// 문서 머리: 레일, 위치 띠, 하위 목록, 설명, OST. 쪽 내용은 부른 쪽에서 이어 채우고 keys_end로 닫는다.
// 배열은 늘 같은 길이로 채운다(길이가 줄면 RmlUi가 사라질 칸의 식을 한 번 더 읽어 경고를 낸다).
static void menu_begin(uint32_t W, const view_t *v, const char *title) {
    ensure_full_view(W);
    ui_begin("title_menu");
    // 메인 메뉴의 틀은 로고 자리까지 올려 늘 같은 큰 크기로 둔다(사용자 요청: 단계마다 세로가 줄었다 늘었다 하지 않게).
    // 로고는 반투명 바탕 너머로 흐리게 비친다.
    int cover = v->in_menu;
    ui_int("cover", cover);
    ui_bool("illust", ui_illust());
    ui_int("top", cover ? COVER_TOP : (int)rd(W + W_MENU_Y) - 6);
    ui_int("narrow", v->narrow);
    ui_int("lv", v->lv);
    ui_int("rail_show", v->rail);
    ui_int("rail_w", v->narrow ? 160 : RAIL_W);
    ui_int("panel_show", v->panel_w > 0);
    ui_int("panel_w", v->panel_w);
    ui_int("sub_show", v->sub);
    ui_int("sub_w", SUB_W);
    ui_int("hwide", v->detail_w >= HIDX_MIN);
    ui_text("dkind", v->dkind);

    // 레일(메인 5항목). 고르는 중이면 nav="menu", 아니면 누르면 그 분류로(pick tmenu, 레벨 0).
    uint32_t it[8];
    int n = level_items(W, 0, it);
    ui_int("rail_act", v->rail_act);
    ui_int("nofocus", v->in_menu && v->lv == 0 && g_nofocus);
    ui_list("rail");
    for (int k = 0; k < MAX_ITEMS; k++) {
        ui_item();
        ui_str("t", k < n ? gstr(it[k]) : "");
        ui_int("sel", v->rail_act && k == v->cat);
        ui_int("on", !v->rail_act && k == v->cat);
        ui_close();
    }
    ui_close();

    // 위치 띠: 메인에서는 커서의 분류 이름, 목록에서는 그 목록의 분류, 쪽에서는 분류 › 쪽. 마지막 칸 앞은 누르면 그 단계로 간다.
    ui_list("path");
    int np = 0;
    if (!v->in_menu) { ui_item(); ui_str("t", title ? title : ""); ui_int("i", -1); ui_int("use", 1); ui_int("sep", 0); ui_close(); np = 1; }
    else {
        step_t ch[4];
        int nch = chain_of(v->lv, v->lv == 0 ? v->cat : (int)rd(W + W_CUR), ch);
        int show = v->lv == 0 ? 1 : (v->lv == 1 || v->lv == 2 || v->lv == 4 || v->lv == 8) ? nch - 1 : nch;
        for (int i = 0; i < show && i < 3; i++) {
            uint32_t li[8];
            int ln = level_items(W, ch[i].lv, li);
            const char *t = ch[i].sel >= 0 && ch[i].sel < ln ? gstr(li[ch[i].sel]) : "";
            if (v->lv == 12 && i == 2) t = gstr(rd(got_ptr(0x1dc) + 4 * ch[i].sel)); // 듣는 곡 이름(잠김 글이 아니라)
            ui_item(); ui_str("t", t);
            ui_int("i", i < show - 1 ? ch[i].lv * 16 + ch[i].sel : -1);
            ui_int("use", 1);
            ui_int("sep", i > 0);
            ui_close();
            np++;
        }
    }
    for (; np < 3; np++) { ui_item(); ui_text("t", ""); ui_int("i", -1); ui_int("use", 0); ui_int("sep", 0); ui_close(); }
    ui_close();

    // 하위 목록
    n = v->sub_lv > 0 ? level_items(W, v->sub_lv, it) : 0;
    ui_int("sub_mode", v->sub_mode);
    ui_list("sub");
    for (int k = 0; k < MAX_ITEMS; k++) {
        ui_item();
        ui_int("use", k < n);
        ui_str("t", k < n ? gstr(it[k]) : "");
        ui_int("sel", v->sub_mode == 2 && k == v->sub_cur);
        ui_int("on", v->sub_mode == 3 && k == v->sub_cur);
        ui_int("i", k >= n ? -1 : v->sub_mode == 2 ? k : v->sub_lv * 16 + k);
        ui_close();
    }
    ui_close();

    // 설명
    const char *dname = "", *desc = "";
    char dname_buf[64] = "";
    if (v->in_menu) {
        if (v->lv == 0 && g_nofocus) desc = "메뉴를 고르세요.";
        else if (v->lv == 0 && v->cat >= 0 && v->cat < 5) {
            snprintf(dname_buf, sizeof dname_buf, "%s", gstr(got_ptr(0x198 + 4 * v->cat)));
            desc = DESC_MAIN[v->cat];
        } else if (v->sub_mode == 2 && v->sub_cur >= 0 && v->sub_cur < 5) {
            snprintf(dname_buf, sizeof dname_buf, "%s", gstr(got_ptr((v->lv == 1 ? 0x1ac : v->lv == 2 ? 0x1c0 : 0x1d4) + 4 * v->sub_cur)));
            desc = v->lv == 1 ? DESC_START[v->sub_cur] : v->lv == 2 ? DESC_INFO[v->sub_cur] : v->sub_cur < 2 ? DESC_NET[v->sub_cur] : "";
        }
    }
    (void)dname;
    ui_str("dname", dname_buf);
    ui_text("desc", desc);

    // OST: 곡 목록 + 곡 정보. 잠긴 곡은 「? ? ? ? ?」(0x1e0)이고 누를 수 없다.
    uint32_t fl = rd(W + W_OPT);
    ui_int("ost_mode", v->ost_mode);
    ui_list("ost");
    uint32_t names = got_ptr(0x1dc);
    for (int k = 0; k < 5; k++) {
        bool open = (fl & (OPT_OST0 << k)) != 0;
        ui_item();
        ui_str("t", open ? gstr(rd(names + 4 * k)) : got_str(0x1e0));
        ui_int("dis", !open);
        ui_int("sel", v->ost_mode == 2 && k == v->ost_cur);
        ui_int("on", v->ost_mode == 3 && k == v->ost_cur);
        ui_int("i", !open ? -1 : v->ost_mode == 2 ? k : 8 * 16 + k);
        ui_close();
    }
    ui_close();
    // 곡 정보는 게임 글(CP949)이라 C에서 감싸 줄로 넘기고, 새 UI가 붙인 안내(UTF-8)는 onote 한 덩이로 넘겨 RmlUi가 감싼다.
    int oc = v->ost_cur;
    const char *oname = "", *otext = NULL, *onote = "게임 음악을 골라 듣습니다. 곡은 모험을 하며 풀립니다.";
    if (v->ost_mode >= 2 && oc >= 0 && oc < 5) {
        bool open = (fl & (OPT_OST0 << oc)) != 0;
        oname = open ? gstr(rd(names + 4 * oc)) : got_str(0x1e0);
        if (open) { otext = gstr(rd(got_ptr(0x214) + 4 * oc)); onote = ""; }
        else onote = "아직 들을 수 없는 곡입니다.";
    }
    ui_str("oname", oname);
    ui_int("oplay", v->ost_mode == 3);
    ui_text("onote", onote);
    {
        int ow = v->detail_w - OST_W - GAP - BODY_PAD;
        int ln = otext ? wrap_text(otext, ow > 40 ? ow : 40, g_sp, MAX_LINES) : 0;
        if (ln > 10) ln = 10;
        ui_list("olines");
        for (int i = 0; i < 10; i++) {
            ui_item();
            if (i < ln) ui_strn("t", otext + g_sp[i].start, g_sp[i].len); else ui_text("t", "");
            ui_int("use", i < ln);
            ui_close();
        }
        ui_close();
    }
    // 환경설정 쪽(레벨 3)에서만 줄을 고른다. 메인에서 커서가 환경설정에 있으면 저장된 설정을 흐리게 미리 보인다.
    ui_int("opt_act", v->in_menu && v->lv == 3);
}

// 오른쪽 아래 고정 키 안내. 늘 같은 두 칸이고 쓸 수 없을 때 흐리게 한다(키를 떼어 눌러도 아무 일이 없다).
// 방향 키 안내(키보드·게임패드에서 「결정」「이전」 왼쪽에 보인다). 쪽 래퍼가 keys_end 앞에서 적는다(한 번 쓰면 지운다).
static const char *g_hint = "";
static void keys_end(int ok_on, int clr_on) {
    ui_text("hint", g_hint);
    g_hint = "";
    ui_int("ok_on", ok_on);
    ui_int("clr_on", clr_on);
    ui_end();
}

// 쪽마다 모든 키를 같은 모양으로 채운다(없는 키를 읽으면 빈 값이지만 모양이 바뀌면 다시 묶는다).
typedef struct {
    const char *sub;              // 머리 띠 오른쪽 작은 글(쪽 번호 등)
    const char *text; span_t *sp; int nsp, from, vis;  // 감싼 본문과 보이는 줄
    int up, down, left, right;    // 넘길 수 있는 방향
} pagebody_t;

static void emit_page_common(const pagebody_t *p) {
    if (p->sub) ui_str("psub", p->sub); else ui_text("psub", "");
    if (p->text) emit_lines("lines", p->text, p->sp, p->from, p->vis);
    else { ui_list("lines"); ui_close(); }
    ui_int("up", p->up); ui_int("down", p->down); ui_int("left", p->left); ui_int("right", p->right);
}

// 환경설정 줄. 다른 쪽에서도 같은 모양으로 채운다(배열이 비면 RmlUi가 사라질 칸을 읽어 경고를 낸다).
static void emit_opts(uint32_t o, int cur) {
    ui_list("opts");
    // 전투: 턴/액티브(bit0 1=턴)
    ui_item(); ui_str("name", got_str(0x204)); ui_int("sel", cur == 0); ui_text("act", "");
    ui_list("ch");
    ui_item(); ui_str("t", got_str(0x208)); ui_int("on", (o & OPT_TURN) != 0); ui_close();
    ui_item(); ui_str("t", got_str(0x20c)); ui_int("on", (o & OPT_TURN) == 0); ui_close();
    ui_close(); ui_close();
    // 사운드: On/Off(bit1 1=Off)
    ui_item(); ui_str("name", got_str(0x200)); ui_int("sel", cur == 1); ui_text("act", "");
    ui_list("ch");
    ui_item(); ui_str("t", got_str(0x1f0)); ui_int("on", (o & OPT_SOUND_OFF) == 0); ui_close();
    ui_item(); ui_str("t", got_str(0x1f4)); ui_int("on", (o & OPT_SOUND_OFF) != 0); ui_close();
    ui_close(); ui_close();
    // 속도: 1~5(bits 8..11 = 값-1, f2a6이 0~4를 돈다)
    int sp = (int)(o >> OPT_SPEED_SHIFT & 0xff);
    ui_item(); ui_str("name", got_str(0x1fc)); ui_int("sel", cur == 2); ui_text("act", "");
    ui_list("ch");
    for (int i = 0; i < 5; i++) { ui_item(); ui_fmt("t", "%d", i + 1); ui_int("on", sp == i); ui_close(); }
    ui_close(); ui_close();
    // 원작에 없는 줄(restored/cfgx.c): 값 줄은 칩 둘, 여는 줄은 act(호스트 action 이름)와 빈 칩
    for (int i = 0; i < cfgx_count(); i++) {
        const char *on_t, *off_t;
        int on = cfgx_value(i, &on_t, &off_t), act = cfgx_action(i)[0] != 0;
        ui_item(); ui_text("name", cfgx_name(i)); ui_int("sel", cur == 3 + i); ui_text("act", cfgx_action(i));
        ui_list("ch");
        if (!act) {
            ui_item(); ui_text("t", on_t); ui_int("on", on); ui_close();
            ui_item(); ui_text("t", off_t); ui_int("on", !on); ui_close();
        }
        ui_close(); ui_close();
    }
    ui_close();
}

// 랭킹 아래 줄 키(최대 2쌍)도 길이를 고정한다
static void fkey_pad(int used) { for (; used < 2; used++) { ui_item(); ui_text("k", ""); ui_text("key", ""); ui_int("nav", 0); ui_text("t", ""); ui_close(); } }

static int g_help_which = HELP_MAIN; // 쪽이 항목별 글이 아닐 때도 모양만 채운다
static void emit_empty_extras(uint32_t W, const view_t *v, int opt, int contact, int rank) {
    if (!opt) emit_opts(v->in_menu && v->lv == 0 ? rd(W + W_OPT) : 0, -1); // 메인 미리보기는 저장된 설정
    if (!contact) { ui_text("logo", ""); ui_text("url", ""); ui_text("tel", ""); ui_text("mail", ""); }
    if (!rank) { ui_text("foot", ""); ui_list("fkeys"); fkey_pad(0); ui_close(); }
    help_emit(g_help_which); // 항목별 글 값(hsections, hparas …)은 어느 쪽에서나 같은 모양으로 채운다(restored/help.c)
}

// 메뉴 목록(레벨 0,1,2,4,8): f580(W, x, y, w, [sp]h, gap, count, strings[])
void orig_f_f580(cpu_t *c);
void f_f580(cpu_t *c) {
    if (!ui_modern()) { orig_f_f580(c); return; }
    uint32_t W = c->r[0];
    int n = (int)arg(c, 6);
    if (n > 8) n = 8;
    RUN_HIDDEN(orig_f_f580, c); // 깜빡임 카운터(W+0x49c)를 원본대로 올린다
    // 터치: 목록 위아래(원본 위/아래 키). 다시 누르면 OK. 이 틱에 처리될 방향 키를 미리 적용한 커서로 그린다(위 g_input 주석).
    int dy, dx, cur = (int)rd(W + W_CUR);
    pend_dir(&dy, &dx);
    if (n > 0) cur = ((cur + dy) % n + n) % n; // 원본 위/아래는 끝에서 돈다(시험으로 확인)
    view_t v;
    view_of(W, "", cur, &v);
    menu_begin(W, &v, NULL);
    if (!(v.lv == 0 && g_nofocus)) ui_nav("menu", cur, n, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_WRAP);
    pagebody_t p = {0};
    emit_page_common(&p);
    emit_empty_extras(W, &v, 0, 0, 0);
    if (!g_quit_arm) g_hint = "↑↓ 선택";
    keys_end(1, v.lv != 0 || g_quit_arm);
}

// 글 쪽(한마디, 정보이용료, 랭킹): 본문을 내용 칸 폭에 맞춰 감싸 VIS 줄을 보인다.
enum { TEXT_VIS = 12 };

static void text_page(uint32_t W, const char *title, const char *sub, const char *text, int scroll_from,
                      int left, int right, const char *kind, void (*extra)(uint32_t, const view_t *)) {
    view_t v;
    view_of(W, kind, (int)rd(W + W_CUR), &v);
    menu_begin(W, &v, title);
    int n = wrap_text(text, v.detail_w - BODY_PAD, g_sp, MAX_LINES);
    while (n > 0 && g_sp[n - 1].len == 0) n--; // 끝의 빈 줄
    int from = scroll_from;
    if (from > n - TEXT_VIS) from = n - TEXT_VIS;
    if (from < 0) from = 0;
    int vis = n - from < TEXT_VIS ? n - from : TEXT_VIS;
    pagebody_t p = {sub, text, g_sp, n, from, vis, from > 0, from + vis < n, left, right};
    emit_page_common(&p);
    if (extra) extra(W, &v);
    else emit_empty_extras(W, &v, 0, 0, 0);
    bool lr = left || right, ud = p.up || p.down;
    g_hint = lr && ud ? "←→ 다른 글   ↑↓ 넘기기" : lr ? "←→ 다른 글" : ud ? "↑↓ 넘기기" : "";
    keys_end(0, 1);
}

// 정보이용료(레벨 10): fe68(W, y). 본문 GOT 0x210.
void orig_f_fe68(cpu_t *c);
void f_fe68(cpu_t *c) {
    if (!ui_modern()) { orig_f_fe68(c); return; }
    uint32_t W = c->r[0];
    RUN_HIDDEN(orig_f_fe68, c);
    text_page(W, got_str(0x1d4), NULL, got_str(0x210), 0, 0, 0, "text", NULL);
}

// OST 곡 정보(레벨 12): ffbc(W, y). 곡 목록 + 곡 정보(menu_begin의 ost)로 L8과 같은 모양에 듣는 곡을 표시한다.
void orig_f_ffbc(cpu_t *c);
void f_ffbc(cpu_t *c) {
    if (!ui_modern()) { orig_f_ffbc(c); return; }
    uint32_t W = c->r[0];
    RUN_HIDDEN(orig_f_ffbc, c);
    view_t v;
    view_of(W, "ost", (int)rd(W + W_CUR), &v);
    menu_begin(W, &v, NULL);
    pagebody_t p = {0};
    emit_page_common(&p);
    emit_empty_extras(W, &v, 0, 0, 0);
    keys_end(0, 1);
}

// 에픽한마디(레벨 9): 13060(W, key, y). 글 번호 = W+0x15c(좌우), 줄 스크롤 = W+0x49c(위아래).
// 넓은 쪽에 글 전체를 다시 감싸 보인다. 원본이 위로 넘긴 만큼(남은 글의 시작)을 찾아 그 줄부터 보인다.
void orig_f_13060(cpu_t *c);
void f_13060(cpu_t *c) {
    if (!ui_modern()) { orig_f_13060(c); return; }
    uint32_t W = c->r[0];
    RUN_HIDDEN(orig_f_13060, c); // 처음이면 자료를 읽고(W+0x6d0) 키를 처리한다
    uint32_t q = rd(W + W_QUOTES);
    int cur = (int)rd(W + W_CUR), total = 0;
    const char *msg = "";
    if (q) {
        const char *s = gstr(q + 8);
        for (int i = 0; s[0] && i < 999; i++) {
            if (i == cur) msg = s;
            total++;
            s += strlen(s) + 1;
        }
    }
    // 원본 줄 스크롤(W+0x49c)이 0이 아니면 String(W+0x474)에는 앞 줄을 떼어 낸 나머지가 남아 있다.
    int from = 0;
    uint32_t rest = rd(W + W_TSTR + 4);
    if (rd(W + W_BLINK) && rest) {
        view_t v;
        view_of(W, "text", cur, &v);
        int off = (int)strlen(msg) - (int)strlen(gstr(rest));
        int n = wrap_text(msg, v.detail_w - BODY_PAD, g_sp, MAX_LINES);
        for (int i = 0; i < n; i++) if (g_sp[i].start <= off) from = i;
    }
    char sub[32];
    snprintf(sub, sizeof sub, "%d / %d", cur + 1, total);
    text_page(W, got_str(0x1c0), sub, msg, from, cur > 0, cur + 1 < total, "text", NULL);
}

// 도움말(레벨 6, 인자 1)/게임속으로(레벨 7, 인자 0): 100a0(W, 종류, y, key).
// 본문(GOT 0x218/0x21c)은 원작 폭(108px)에 맞춰 손으로 줄을 바꾼 글이고 원작은 6줄씩 쪽을 넘겼다. 새 UI는 항목별로 묶는다.
void orig_f_100a0(cpu_t *c);
void f_100a0(cpu_t *c) {
    if (!ui_modern()) { orig_f_100a0(c); return; }
    uint32_t W = c->r[0];
    int help = c->r[1] != 0;
    // 도움말과 게임속으로는 항목별 글이다(restored/help.c: 묶음, 문단, 지금 항목, 글 상자 넘기기). 도움말은 옵션의 도움말과
    // 같은 객체를 본다. 원본의 쪽 번호(W+0x15c)는 쓰지 않으므로 0에 묶어 두고, 이 틱의 좌우(항목)·위아래(넘기기)를 pend_dir로 받는다.
    int which = help ? HELP_MAIN : HELP_INTRO;
    int dy, dx;
    pend_dir(&dy, &dx);
    help_input(which, dx, dy);
    RUN_HIDDEN(orig_f_100a0, c);
    W32_(&cpu, W + W_CUR, 0);
    view_t v;
    view_of(W, "help", 0, &v);
    menu_begin(W, &v, NULL);
    pagebody_t p = {0};
    emit_page_common(&p);
    g_help_which = which;
    emit_empty_extras(W, &v, 0, 0, 0); // 그 글의 값은 여기서 채운다
    g_help_which = HELP_MAIN;
    g_hint = "←→ 항목   ↑↓ 넘기기";
    keys_end(0, 1);
}

// 환경설정(레벨 3): f8c0(W, y). 값은 작업본 W+0x540, 줄 = W+0x15c.
// 퀵모드 줄(3)은 새 UI에 두지 않는다. 원본 키 처리는 4줄을 돌므로, 커서가 3에 오면 오던 방향으로 건너뛴다
// (2에서 내려오면 0, 0에서 올라가 감기면 2). 그리기는 키 처리보다 먼저 불리므로 키가 3에서 값을 바꿀 틈이 없다.
// 바꾼 값은 나갈 때 늘 저장한다(f_ebc4가 이 단계의 CLR을 OK로 바꾼다. 옵션 메뉴의 즉시 적용과 같은 뜻).
void orig_f_f8c0(cpu_t *c);
void f_f8c0(cpu_t *c) {
    if (!ui_modern()) { orig_f_f8c0(c); return; }
    static int prev;
    uint32_t W = c->r[0];
    int cur = (int)rd(W + W_CUR);
    if (cur == 3) { cur = prev == 2 ? 0 : 2; W32_(&cpu, W + W_CUR, (uint32_t)cur); }
    prev = cur;
    RUN_HIDDEN(orig_f_f8c0, c);
    uint32_t o = rd(W + W_OPT_EDIT);
    view_t v;
    view_of(W, "opt", cur, &v);
    menu_begin(W, &v, NULL);
    // 터치: 줄 고르기(OK는 나가기라 ACT 없이). 퀵모드 줄은 건너뛰므로 3줄이 돌아간다. 값은 문서의 ◀▶ 버튼(좌우 키)으로.
    // 위 menu와 같은 까닭으로 이 틱에 처리될 위/아래를 미리 적용한다. 원본은 4줄을 돌고 3줄(퀵모드)은 다음 그리기가 건너뛴다.
    // 원작에 없는 줄(가상 줄 3~)까지 한 고리로 돈다. 그 줄을 오가는 키는 opt_extra_key가 이미 받아 지웠으므로 여기서는 dy가 0이다.
    int dy, dx, pc = cur;
    pend_dir(&dy, &dx);
    pc = (pc + dy + 4) % 4;
    if (pc == 3) pc = dy > 0 ? 0 : 2;
    if (cfgx_sel() >= 0) pc = 3 + cfgx_sel();
    ui_nav("opts", pc, 3 + cfgx_count(), UI_KEY_UP, UI_KEY_DOWN, UI_NAV_WRAP);
    pagebody_t p = {0};
    emit_page_common(&p);
    emit_opts(o, pc);
    emit_empty_extras(W, &v, 1, 0, 0);
    // 여는 줄(조작 설정·기타 설정·세이브 데이터)에서는 「결정」이 그 창을 연다. 값 줄은 좌우로 바꾼다.
    int open_row = cfgx_sel() >= 0 && cfgx_action(cfgx_sel())[0];
    g_hint = open_row ? "↑↓ 항목" : "↑↓ 항목   ←→ 값 바꾸기";
    keys_end(open_row, 1);
}

// 게임문의(레벨 5): fc9c(W, y). 내용은 모두 타이틀 시트 그림(fr16 NEXON 로고, fr17 주소, fr18 전화, fr19 메일)이라 그림을 떼어 쓴다.
void orig_f_fc9c(cpu_t *c);
void f_fc9c(cpu_t *c) {
    if (!ui_modern()) { orig_f_fc9c(c); return; }
    uint32_t W = c->r[0];
    RUN_HIDDEN(orig_f_fc9c, c);
    uint32_t sheet = rd(W + W_TSHEET);
    view_t v;
    view_of(W, "contact", 0, &v);
    menu_begin(W, &v, NULL);
    pagebody_t p = {0};
    emit_page_common(&p);
    static const char *keys[4] = {"logo", "url", "tel", "mail"};
    ui_scratch_begin();
    for (int i = 0; i < 4; i++) {
        ui_scratch_clear();
        CALL(FN_SPR, sheet, 16 + i, 8, 8, 0, 0, 0);
        ui_sprite_trim(keys[i], 0, 0, 220, 60);
    }
    ui_scratch_end();
    emit_empty_extras(W, &v, 0, 1, 0);
    keys_end(0, 1);
}

// 에픽랭킹(레벨 11): 134cc(W, init, y). 네트워크 상태 기계(W+0x10 0~10)와 키 처리가 함께 있다.
// 본문과 아래 줄은 모두 13b40(W, rect, 본문, 아래줄)로 그리므로 그 인자를 받아 둔다.
static char rank_body[512], rank_foot[96];
static uint32_t rank_foot_ptr;
static bool rank_seen;
void orig_f_13b40(cpu_t *c);
void f_13b40(cpu_t *c) {
    if (ui_modern()) {
        snprintf(rank_body, sizeof rank_body, "%s", gstr(c->r[2]));
        snprintf(rank_foot, sizeof rank_foot, "%s", gstr(c->r[3]));
        rank_foot_ptr = c->r[3];
        rank_seen = true;
    }
    orig_f_13b40(c); // String(W+0x474)에 본문을 넣는 일도 한다
}

// 아래 줄: 예/아니오만 쪽 안 버튼으로 둔다. 「CLR : 나가기」「CLR : 취소」는 오른쪽 아래 「이전」과 같은 키라 따로 두지 않는다.
static void rank_extra(uint32_t W, const view_t *v) {
    emit_empty_extras(W, v, 0, 0, 1);
    ui_list("fkeys");
    int used = 0;
    if (rank_foot_ptr == got_ptr(0x248)) { key_hint("1", "Yes"); key_hint("2", "No"); used = 2; } // 「1.Yes      2.No」
    else if (rank_foot[0] && !strstr(rank_foot, "CLR")) { emit_key_text(rank_foot); used = 1; }
    fkey_pad(used);
    ui_close();
    ui_text("foot", used ? "1" : "");
}

void orig_f_134cc(cpu_t *c);
void f_134cc(cpu_t *c) {
    if (!ui_modern()) { orig_f_134cc(c); return; }
    uint32_t W = c->r[0];
    // 타이틀 밖(엔딩 스크립트 38e18, 3a4a4)에서는 처음 한 번 필드를 그린다(13aea: cbf8 뒤 W+0x14의 4를 끈다).
    // 그 그림은 남겨야 하니 원본과 같이 여기서 먼저 그리고, 나머지는 숨겨서 돌린다.
    if (rd(W + W_SCENE) != 4 && (rd(W + W_STATE) & 4)) {
        uint32_t a0 = c->r[0], a1 = c->r[1], a2 = c->r[2];
        CALL(0x10cbf8, W, 0);
        W32_(&cpu, W + W_STATE, rd(W + W_STATE) & ~4u);
        c->r[0] = a0, c->r[1] = a1, c->r[2] = a2;
    }
    rank_seen = false;
    RUN_HIDDEN(orig_f_134cc, c);
    uint32_t r0 = c->r[0];
    if (!rank_seen) { rank_body[0] = 0; rank_foot[0] = 0; rank_foot_ptr = 0; } // 상태 1 등 글이 없는 틱
    // 엔딩에서도 같은 쪽을 쓴다(메뉴 장면이 아니면 레일 없이 패널만, view_of)
    text_page(W, got_str(0x1d8), NULL, rank_body, 0, 0, 0, "rank", rank_extra);
    c->r[0] = r0;
}

// =====================================================================
// 첫 실행(장면 22): 10ef0(W, input). 하위 0 = 진행(카운터 W+0x15c 0..15), 하위 1 = 재시작 안내.
// 원본은 매 틱 화면을 검게 지우고 그린다. 숨겨 돌린 뒤 검게만 지운다.
// 안내 둘째 줄(「게임속도가 느릴경우, … 퀵모드를 On으로 …」)은 새 UI에 퀵모드가 없으므로 보이지 않는다.
// =====================================================================
void orig_f_10ef0(cpu_t *c);
void f_10ef0(cpu_t *c) {
    if (!ui_modern()) { orig_f_10ef0(c); return; }
    uint32_t W = c->r[0];
    int sub = (int)rd(W + W_SUB), n = (int)rd(W + W_CUR);
    RUN_HIDDEN(orig_f_10ef0, c);
    uint32_t r0 = c->r[0];
    CALL(FN_FILL, 0, 0, 0);
    if (n > 15) n = 15;
    if (n < 0) n = 0;
    ui_begin("title_boot");
    if (sub == 0) ui_pad(UI_PAD_NONE); // 진행 중에는 입력이 없다. 안내(sub 1)는 화면 전체를 눌러 넘긴다.
    ui_int("sub", sub);
    ui_str("label", got_str(0x224));
    ui_int("pct", n * 100 / 15);
    // 안내 글: 첫 줄만
    const char *msg = got_str(0x228), *nl = strchr(msg, '\n');
    ui_strn("msg", msg, nl ? (int)(nl - msg) : (int)strlen(msg));
    ui_list("keys"); emit_key_text(got_str(0x22c)); ui_close();
    ui_end();
    c->r[0] = r0;
}

// =====================================================================
// 로고(장면 1): e5e4(W, input). 그림은 원본 그대로 두고, 아무 키나 눌러 넘기는 것을 터치로 할 수 있게 화면 전체를 누를 곳으로만 얹는다.
// 장면 함수 전체를 감싸므로 원본은 숨기지 않고 그대로 부른다. 아래 하위 3(흰 화면 대기)에서도 키는 받지 않지만 넘어가는 장면이라 무해하다.
// =====================================================================
void orig_f_e5e4(cpu_t *c);
void f_e5e4(cpu_t *c) {
    uint32_t W = c->r[0];
    orig_f_e5e4(c);
    if (!ui_modern() || rd(W + W_SCENE) != 1) return; // 다음 장면으로 넘어간 틱에는 얹지 않는다
    uint32_t r0 = c->r[0];
    ui_begin("title_tap");
    ui_end();
    c->r[0] = r0;
}

// =====================================================================
// 타이틀(장면 2): e85c(W, input). 그림(일러스트, 로고, PERFECT EDITION, 버전, 통신사, 등급)은 원본 그대로 두고
// 깜빡이는 「PRESS ANY KEY」(타이틀 시트 fr1)만 새 알림으로 바꾼다. 원본이 그리는 동안 fr1의 조각 수를 0으로 둔다.
// =====================================================================
void orig_f_e85c(cpu_t *c);
void f_e85c(cpu_t *c) {
    if (!ui_modern()) { orig_f_e85c(c); return; }
    uint32_t W = c->r[0];
    ensure_full_view(W);
    uint32_t sheet = rd(W + W_TSHEET), fr1 = 0, n1 = 0;
    if (sheet) { fr1 = rd(rd(sheet + 4) + 4); n1 = rd(fr1); W32_(&cpu, fr1, 0); }
    orig_f_e85c(c);
    uint32_t r0 = c->r[0];
    if (fr1) W32_(&cpu, fr1, n1);
    g_nofocus = 1; // 메인으로 넘어가면 커서 없이 시작한다(f_ebc4)
    if (rd(W + W_SCENE) == 2) { // 키를 눌러 메뉴로 넘어간 틱에는 띄우지 않는다
        ui_begin("title_press");
        ui_text("t", "PRESS ANY KEY");
        ui_end();
    }
    c->r[0] = r0;
}

// =====================================================================
// Loading(장면 5 새게임, 18 불러오기, 111b8): 103f4가 화면을 지운 뒤 10410이 오른쪽 아래에 글을 쓰고, 같은 틱에 읽기를 한다.
// 한 번만 그리는 화면이라 그 틱에만 문서를 띄운다(다음 표시까지 남는다).
// =====================================================================
void orig_f_10410(cpu_t *c);
void f_10410(cpu_t *c) {
    if (!ui_modern()) { orig_f_10410(c); return; }
    ui_begin("title_loading");
    ui_pad(UI_PAD_NONE); // 입력을 받지 않는 읽기 화면
    ui_str("t", got_str(0x220));
    ui_end();
}

// 프로그램오류(장면 0): e588(W, input). 검은 화면에 GOT 0x194 글. OK/CLR/5면 메인 메뉴로.
void orig_f_e588(cpu_t *c);
void f_e588(cpu_t *c) {
    if (!ui_modern()) { orig_f_e588(c); return; }
    RUN_HIDDEN(orig_f_e588, c);
    uint32_t r0 = c->r[0];
    CALL(FN_FILL, 0, 0, 0);
    ui_begin("title_error");
    ui_str("t", got_str(0x194));
    ui_list("keys"); key_hint("OK", "메인 메뉴"); ui_close();
    ui_end();
    c->r[0] = r0;
}

// =====================================================================
// 스태프롤: bfe8(W, ?) — 스크립트 실행기(38e18)가 매 틱 부른다. 글 객체들이 스스로 올라가며 13e48로 그린다.
// 원본을 숨겨 돌려 그린 글(위치, 색)을 받아 같은 자리에 다시 놓는다. 제목줄은 금색 8방향 + 가운데 검정으로 쓰는 테두리 글이라
// 같은 글이 9번 잡힌다. 가운데(검정) 것만 제목으로, 흰 글은 이름으로 쓴다. 위아래 띠(높이/16*3)에서 원본처럼 흐려진다.
// 키 안내(*:Exit, #:Fast)는 글 객체가 아니라 bfe8이 직접 쓴다.
// =====================================================================
//
// 고친 것(사용자 보고: 엔딩 크레딧에서 Exit·Fast 버튼이 안 됨):
// - Fast: 원본은 #을 누르고 있는 동안만(6204 누름 유지, 0x10043) 글마다 13e40(y -= 2)을 한 번 더 부른다. 터치 버튼은 눌렀다 떼는
//   한 번이라 거의 움직이지 않았다. #을 누를 때마다 켜고 끄는 전환으로 바꾸고, 켜져 있으면 틱마다 글 객체를 FAST_STEP씩 더 올린다.
//   키보드·패드로 누르고 있으면 원본 가속도 함께 걸린다.
// - Exit: 원본은 W+0x20의 0x100000(타이틀 메뉴 「만든사람들」로 들어옴)일 때만 *를 받고 안내도 그린다. 엔딩 스크립트에서는 그 비트가
//   없어 *가 무시됐다. 새 UI는 늘 Exit를 보이므로 *를 누른 틱에만 비트를 켜고 원본을 부른다(글을 모두 지우고 0을 돌려 끝난 것과 같다).
enum { W_FLAGS = 0x20, FLAG_STAFF_EXIT = 0x100000, GKEY_STAR = 0x4a, GKEY_HASH = 0x43, FAST_STEP = 4 };
static bool staff_fast;
void orig_f_bfe8(cpu_t *c);
void f_bfe8(cpu_t *c) {
    if (!ui_modern()) { orig_f_bfe8(c); return; }
    uint32_t W = c->r[0];
    uint32_t list = rd(W + W_STAFF);
    if (!list) staff_fast = false; // 다음 스태프롤은 보통 빠르기로 시작한다
    uint32_t in = rd(rd(W) + 0x38);
    int pressed = (rd(in + 0xc) && R16_(&cpu, in + 2) == 1) ? R16_(&cpu, in) : 0; // 61d4와 같은 읽기: 이번 틱에 누른 키
    if (list && pressed == GKEY_HASH) staff_fast = !staff_fast;
    uint32_t fl = rd(W + W_FLAGS);
    bool force_exit = list && pressed == GKEY_STAR && !(fl & FLAG_STAFF_EXIT);
    if (force_exit) W32_(&cpu, W + W_FLAGS, fl | FLAG_STAFF_EXIT);
    RUN_HIDDEN(orig_f_bfe8, c);
    if (force_exit) W32_(&cpu, W + W_FLAGS, rd(W + W_FLAGS) & ~(uint32_t)FLAG_STAFF_EXIT);
    uint32_t r0 = c->r[0];
    list = rd(W + W_STAFF);
    if (staff_fast) for (uint32_t o = list; o; o = rd(o + 0x18)) W32_(&cpu, o + 4, rd(o + 4) - FAST_STEP); // 다음 틱 그림에 반영된다
    if (!list) { c->r[0] = r0; return; } // 원본도 아무것도 그리지 않았다
    CALL(FN_FILL, 0, 0, 0x40030000);     // 원본 바탕(남색)
    int h = 320, band = (h >> 4) * 3;
    const char *exit_s = got_str(0x134), *fast_s = got_str(0x130);
    ui_begin("title_staff");
    ui_int("band", band);
    ui_list("rows");
    int nc = ui_cap_count(), used = 0;
    for (int i = 0; i < nc && used < STAFF_ROWS; i++) {
        int x, y; unsigned col;
        const char *s = ui_cap_text(i, &x, &y, &col);
        if (!strcmp(s, exit_s) || !strcmp(s, fast_s)) continue;
        bool title = col == 0;                     // 테두리 글의 가운데(검정)
        bool white = (col & 0xffff) == 0xffff;
        if (!title && !white) continue;            // 테두리 금색 조각
        int top = y - 11;                          // 기준선 → 14dp 줄의 위(RmlUi 12dp 글의 기준선은 줄 위 +11)
        // 원본은 (band, h-band) 띠로 자르고(39ac), 띠 안쪽 위아래 band줄에 걸쳐 흐리게 한다(4934, c1e2 반복).
        int cy = top + 7;
        if (cy < band || cy > h - band) continue;
        int a1 = (cy - band) * 100 / band, a2 = (h - band - cy) * 100 / band;
        int op = a1 < a2 ? a1 : a2;
        if (op > 100) op = 100;
        ui_item();
        ui_int("use", 1);
        ui_str("t", s);
        ui_int("y", top);
        ui_int("title", title);
        ui_int("op", op);
        ui_close();
        used++;
    }
    for (; used < STAFF_ROWS; used++) { // 배열 길이를 고정한다(줄면 RmlUi가 사라질 칸을 읽어 경고)
        ui_item(); ui_int("use", 0); ui_text("t", ""); ui_int("y", 0); ui_int("title", 0); ui_int("op", 0); ui_close();
    }
    ui_close();
    ui_list("keys");
    emit_key_text(exit_s); emit_key_text(fast_s);
    ui_close();
    ui_bool("fast", staff_fast);
    ui_end();
    c->r[0] = r0;
}

// =====================================================================
// 화면 영역 적용: 12598(W). 퀵모드(W+0x14c의 0x40)면 그리기 영역을 384×240으로 줄이고 가운데에 둔다.
// 새 UI는 늘 전체 화면에 그리므로(README 정해진 것) 원본을 부르는 동안만 퀵모드 비트를 끈다.
// 비트 자체는 남겨 저장 파일은 그대로다. 원작 그리기(F2/--classic)에서는 원본대로 줄인다(바꿀 때 다음 적용부터).
// =====================================================================
void orig_f_12598(cpu_t *c);
void f_12598(cpu_t *c) {
    if (!ui_modern()) { orig_f_12598(c); return; }
    uint32_t W = c->r[0], o = rd(W + W_OPT);
    if (o & OPT_QUICK) W32_(&cpu, W + W_OPT, o & ~(uint32_t)OPT_QUICK);
    orig_f_12598(c);
    if (o & OPT_QUICK) W32_(&cpu, W + W_OPT, rd(W + W_OPT) | OPT_QUICK);
}
