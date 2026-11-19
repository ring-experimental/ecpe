// 항목별 글(도움말 GOT 0x218, 게임속으로 GOT 0x21c). 원본 글은 좁은 창 폭으로 손으로 끊겨 있고 원작은 6줄씩 쪽을 넘겼다.
// 새 UI는 항목별로 묶고 끊긴 줄을 문단으로 다시 잇는다(사용자 결정 2026-11-10). 도움말은 옵션(restored/ui_system.c)과
// 타이틀 메인 메뉴(restored/ui_title.c)가 같은 객체를 본다(지금 항목도 하나). 게임속으로는 타이틀에서만 보인다.
#include "restored.h"
#include "help.h"
#include <stdio.h>
#include <string.h>


// 도움말 글(GOT 0x218): '\n'으로 줄을 나누고 '\t'에서 끝난다. 원본 페이저 0x10294와 같이 쪽마다 6줄.
static int help_lines(const char *text, const char **starts, int *lens, int max) {
    const char *p = text, *ls = p;
    int n = 0;
    for (;; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch & 0x80) { if (!p[1]) break; p++; continue; } // 두 바이트 글자(0x10294와 같이 둘째 바이트는 보지 않는다)
        if (ch == '\n' || ch == '\t' || ch == 0) {
            if (n < max) { starts[n] = ls; lens[n] = (int)(p - ls); }
            n++;
            ls = p + 1;
            if (ch != '\n') break;
        }
    }
    return n;
}

// ---- 도움말: 항목별로 묶고 문단을 다시 잇는다(사용자 결정 2026-11-10) ----
// 원본 글은 좁은 화면(240) 창 폭으로 줄마다 끊겨 있고(한글 9자 안팎) 쪽은 6줄씩 기계적으로 나뉜다. 새 UI는 【항목】마다 한 화면에
// 다 보이고(길면 글 상자를 넘긴다), 끊긴 줄을 이어 문단으로 흐르게 한다.
//   - 【…】 항목 시작, ◈ 소제목, ☞ 항목(들여 쓴 다음 줄과 이어진다), 빈 줄은 문단 나눔.
//   - 들여 쓰지 않은 줄은 앞 문단에 잇는다. 앞 줄이 창 폭을 거의 채웠으면 낱말 중간에서 끊긴 것이라 그냥 붙이고,
//     아니면 낱말 사이에서 끊긴 것이라 띄어 쓴다(원본 글꼴 폭으로 잰다. 창 폭 = 가장 긴 줄).
//   - 일반 문단 뒤에 들여 쓴 줄은 하위 줄로 따로 둔다(◈ 핫슬롯이용의 "등록: … / 삭제: …").
enum { HP_PLAIN, HP_HEAD, HP_BULLET, HP_SUB, HP_MAX = 160, HS_MAX = 24, HP_LEN = 600, HL_MAX = 400 };
typedef struct { int type; char t[HP_LEN]; } hpara_t;

// 글 하나(도움말, 게임속으로)의 설정과 만든 결과. 지금 보는 항목(sec)도 여기 있어 이 글을 보는 화면끼리 같다.
typedef struct {
    uint32_t got;                  // 글 포인터가 든 GOT 칸
    const char *sec_mark;          // 항목 머리 표시(CP949 두 바이트): 【(도움말), ◈(게임속으로)
    uint32_t hash; int nlines;     // 이 글(FNV-1a, 줄 수)이면 아래 표를 쓴다
    const unsigned char *tight; size_t ntight; // 이어 붙일 때 띄우지 않는 줄
    const unsigned char *comma; size_t ncomma; // 쉼표로 잇는 줄(줄마다 하나씩 놓인 이름 목록)
    // 만든 결과
    uint32_t built; bool known;
    hpara_t para[HP_MAX];
    int np, ns, first[HS_MAX + 1];
    char name[HS_MAX][64];
    int sec;
} hdoc_t;

// 줄 잇기의 띄어쓰기. 원본 글은 글자 수(한글 9자 안팎)로 손으로 끊어 폭만으로는 낱말 중간인지 사이인지 가를 수 없다
// ("술집|2층에가면"과 "펀터도우|미가"는 둘 다 꽉 찬 줄). 이 게임의 글은 이어 붙일 때 띄우지 않는 줄 번호를 사람이 읽고
// 정해 둔다. 글이 다르면(해시가 다르면) join_rule로 정한다.
static const unsigned char tight_help[] = {3, 4, 6, 7, 8, 9, 27, 31, 33, 38, 40, 41, 43, 47, 48, 49, 56, 63, 75, 76, 77, 82, 86, 91, 96,
                                           101, 102, 103, 108, 110, 119, 122, 123, 127, 128, 132};
static const unsigned char tight_intro[] = {2, 3, 4, 5, 7, 9, 15, 16, 18, 25, 27, 29, 30, 37};
static const unsigned char comma_intro[] = {39, 40, 41}; // 사운드 저작권자: fee le monde, HIDEpi, GAIA Head Office, TAM

static hdoc_t docs[HELP_COUNT] = {
    [HELP_MAIN] = {0x14e624 + 0x218, "\xa1\xbc", 0xf80c1b62u, 134, tight_help, sizeof tight_help, NULL, 0},
    [HELP_INTRO] = {0x14e624 + 0x21c, "\xa2\xc2", 0x99427accu, 43, tight_intro, sizeof tight_intro, comma_intro, sizeof comma_intro},
};

static bool in_list(const unsigned char *l, size_t n, int v) { for (size_t i = 0; i < n; i++) if (l[i] == v) return true; return false; }

static bool join_rule(const char *prev, int prev_len, int prev_w, int limit, const char *next) {
    unsigned char a = prev_len ? (unsigned char)prev[prev_len - 1] : ' ', b = (unsigned char)next[0];
    if (b == ',' || b == '.' || b == ')' || b == '!' || b == '?') return false;
    if (a == ',' || a == '.' || a == ':') return true;
    if ((a & 0x80) != (b & 0x80)) return true;       // 한글과 영문·숫자 사이
    int first = (b & 0x80) ? 2 : 1;
    return prev_w + font_string_width((const uint8_t *)next, first) <= limit; // 줄이 덜 찼으면 낱말 사이
}

static bool starts(const char *s, int n, const char *mark) { return n >= 2 && (unsigned char)s[0] == (unsigned char)mark[0] && (unsigned char)s[1] == (unsigned char)mark[1]; }

// 【…】(또는 그 글의 항목 표시)는 항목 시작, ◈ 소제목(항목 표시가 ◈인 글에서는 항목), ☞ 항목, 빈 줄은 문단 나눔.
// 들여 쓰지 않은 줄은 앞 문단에 잇고, 일반 문단 뒤에 들여 쓴 줄은 하위 줄로 따로 둔다(◈ 핫슬롯이용의 "등록: … / 삭제: …").
static void build(hdoc_t *D) {
    static const char *st[HL_MAX]; static int ln[HL_MAX];
    uint32_t addr = R32_(&cpu, D->got);
    if (!addr || addr == D->built) return;
    D->built = addr;
    const char *text = gstr(addr);
    int n = help_lines(text, st, ln, HL_MAX);
    if (n > HL_MAX) n = HL_MAX;
    int limit = 0;
    for (int i = 0; i < n; i++) { int w = font_string_width((const uint8_t *)st[i], ln[i]); if (w > limit) limit = w; }
    uint32_t h = 0x811c9dc5u;
    for (const unsigned char *q = (const unsigned char *)text; *q && *q != '\t'; q++) h = (h ^ *q) * 16777619u;
    D->known = h == D->hash && n == D->nlines;
    D->np = D->ns = 0;
    int cur = -1, prev_w = 0, prev_i = -1;
    for (int i = 0; i < n; i++) {
        const char *l = st[i]; int len = ln[i];
        int ind = 0;
        while (ind < len && l[ind] == ' ') ind++;
        if (ind == len) { cur = -1; continue; }                     // 빈 줄
        if (starts(l + ind, len - ind, D->sec_mark)) {              // 항목
            if (D->ns < HS_MAX) { snprintf(D->name[D->ns], sizeof D->name[0], "%.*s", len - ind, l + ind); D->first[D->ns++] = D->np; }
            cur = -1; continue;
        }
        if (!D->ns) { D->name[0][0] = 0; D->first[D->ns++] = D->np; }
        bool head = starts(l + ind, len - ind, "\xa2\xc2"), bullet = starts(l + ind, len - ind, "\xa2\xd1");
        int type = head ? HP_HEAD : bullet ? HP_BULLET : -1;
        bool listed = D->known && cur >= 0 && in_list(D->comma, D->ncomma, i); // 이름 목록(가운데 맞춤으로 들여 쓴 줄도 있다)
        if (type < 0 && !listed && ind && cur >= 0 && D->para[cur].type == HP_PLAIN) type = HP_SUB;
        if (type < 0 && !listed && (cur < 0 || D->para[cur].type == HP_HEAD || D->para[cur].type == HP_SUB)) type = cur >= 0 && D->para[cur].type == HP_SUB && ind ? HP_SUB : HP_PLAIN;
        if (type >= 0) {
            if (D->np >= HP_MAX) break;
            cur = D->np++;
            D->para[cur].type = type;
            snprintf(D->para[cur].t, HP_LEN, "%.*s", len - ind, l + ind);
            prev_w = font_string_width((const uint8_t *)l, len);
            prev_i = i;
            if (head) cur = -1; // 소제목은 한 줄
            continue;
        }
        size_t have = strlen(D->para[cur].t);
        const char *sep;
        if (D->known) sep = in_list(D->comma, D->ncomma, i) ? ", " : in_list(D->tight, D->ntight, i) ? "" : " ";
        else sep = join_rule(prev_i >= 0 ? st[prev_i] : "", prev_i >= 0 ? ln[prev_i] : 0, prev_w, limit, l + ind) ? " " : "";
        snprintf(D->para[cur].t + have, HP_LEN - have, "%s%.*s", sep, len - ind, l + ind);
        prev_w = font_string_width((const uint8_t *)l, len);
        prev_i = i;
    }
    D->first[D->ns] = D->np;
    if (D->sec >= D->ns) D->sec = 0;
}

void help_emit(int which) {
    hdoc_t *D = &docs[which];
    build(D);
    if (D->sec < 0) D->sec = 0;
    if (D->sec >= D->ns) D->sec = D->ns ? D->ns - 1 : 0;
    ui_int("hpage", D->sec + 1);
    ui_int("hpages", D->ns);
    ui_bool("hprev", D->sec > 0);
    ui_bool("hnext", D->sec + 1 < D->ns);
    ui_int("hprev_i", D->sec - 1);
    ui_int("hnext_i", D->sec + 1);
    ui_str("hsection", D->ns ? D->name[D->sec] : "");
    ui_list("hsections");
    for (int i = 0; i < D->ns; i++) { ui_item(); ui_str("t", D->name[i]); ui_bool("cur", i == D->sec); ui_int("i", i); ui_close(); }
    ui_close();
    ui_list("hparas");
    for (int i = D->ns ? D->first[D->sec] : 0; D->ns && i < D->first[D->sec + 1]; i++) {
        ui_item(); ui_int("ty", D->para[i].type); ui_str("t", D->para[i].t); ui_close();
    }
    ui_close();
}

void help_input(int which, int dx, int dy) {
    hdoc_t *D = &docs[which];
    build(D);
    int was = D->sec, pick = ui_pick("help");
    if (pick >= 0 && pick < D->ns) D->sec = pick;
    if (dx < 0 && D->sec > 0) D->sec--;
    if (dx > 0 && D->sec + 1 < D->ns) D->sec++;
    if (D->sec != was) ui_scroll("help", -100000); // 새 항목은 맨 위부터
    if (dy) ui_scroll("help", dy * 36.f);
}
