// 스크립트 대사창, 선택창, 알림 띠, 지역 미니맵, 비행정 안내를 RmlUi로 다시 그린다(영역 dialog).
//
// 원작의 대사창과 선택창은 화면 폭 전체로 늘어나는 아래쪽 띠였다(폭 568에서 한 줄이 560px). 넓은 화면에서는 읽기 좋은
// 줄 길이(약 400dp)의 아래쪽 판에 큰 초상화와 이름표를 붙이고, 선택지는 대사창 자리에 목록으로 띄운다. 알림(지역 이름,
// 아이템 획득, 파티 GP, 레벨업, 마릿수)은 위쪽 토스트로 바꾼다. 입력과 상태(타자 효과, 쪽 넘김, 커서, 닫힘 시각)는 원본이
// 그대로 처리하고, 여기서는 그리기만 바꾼다.
//
// 그리는 방법:
//   1. 원본 그리기를 ui_scratch 안에서 그대로 부른다(다음 쪽 화살표 깜빡임 상태 +0x94/+0x95와 시각 +0x90, 미니맵 점 깜빡임
//      카운터 같은 부수 효과를 원작대로 남긴다). 화면에 보일 글은 원본이 DrawString으로 그린 글을 받는다(ui_cap_text).
//      대사는 타자 효과로 드러난 만큼만 원본이 그리므로, 받은 글이 곧 "드러난 길이"다.
//   2. 대사 줄바꿈: 원본은 창 사각형 폭(W+0x54) - 8로 글을 줄과 쪽(2줄)으로 나눈다(갱신 0x37630과 그리기 0x37388 모두).
//      새 창의 글 칸 폭에 맞춰 두 함수를 부르는 동안만 이 폭을 바꿨다가 되돌린다. 그러면 원본이 새 칸에 맞는 줄로 나누고
//      쪽을 넘기므로, 받은 줄을 그대로(줄바꿈 없이) 놓으면 된다. 글꼴은 게임 글자와 같은 Neo둥근모 12px이다.
//   3. 초상화와 미니맵 그림은 scratch 화면에 원본 스프라이트/지도 그리기를 따로 불러 떼어 낸다.
//
// 주소는 파일 오프셋이다(게스트 주소는 +0x100000). 조사 내용은 work/ui/survey/field.md, 이 영역의 조사 기록은 work/ui/modern/dialog/.
#include "restored.h"
#include "items_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int font_string_width(const uint8_t *s, int len);

#define G32(a) R32_(&cpu, (a))
#define G16(a) R16_(&cpu, (a))
#define G8(a) R8_(&cpu, (a))
#define FN(x) (0x100000u + (x))

enum {
    APP_PTR = 0x14ee34,         // 앱 객체 포인터(BSS)
    APP_GAME = 0x48,
    // 게임(월드) 객체 G
    GAME_GFX = 0x08, GAME_SCENE = 0x0c, GAME_FLAGS = 0x14, GAME_FSTATE = 0x18, GAME_FLAGS2 = 0x20,
    GAME_TYPING = 0x14c,        // 0x10이면 타자 효과 없이 쪽째로 보인다(환경설정)
    GAME_LINEGAP = 0x160,       // 대사 줄 간격 보정(0이 아니면 +2px)
    GAME_FIELD = 0x184, GAME_MENU = 0x1d4, GAME_DIALOG = 0x34c, GAME_NOTICE = 0x3e4,
    GAME_HAND = 0x4c0,          // 손 커서 스프라이트 슬롯(선택창 0x60dc 프레임 0x21)
    GAME_PLACE = 0x4fc,         // 지역 이름(지역 이름 띠 0xd600)
    GAME_BLINK = 0x49c,         // 미니맵 주인공 점 깜빡임 카운터
    MENU_SHEET = 0x14,          // UI 스프라이트 시트(초상화 프레임도 여기 있다)
    FIELD_CAM_X = 0x18,
    GFX_CLIP = 0xdc,            // {x, y, w, h}
    GFX_SURF = 0x40,            // 그리기 표면: +0 화소, +4 w, +8 h, +0xc 줄 바이트

    // 대사창 W(G + 0x34c, vtable 0x4dbcc)
    DLG_TEXT = 0x14,            // 대사 전체(CP949)
    DLG_PAGE = 0x20,            // 이 쪽이 시작하는 바이트
    DLG_SHOWN = 0x24,           // 드러난 끝 바이트(절대)
    DLG_PAGE_DONE = 0x28,       // 이 쪽을 다 드러냈다
    DLG_RECT = 0x4c,            // {x, y, w, h}: 화면 폭으로 늘어난 아래쪽 띠
    DLG_RECT_W = 0x54,
    DLG_ACTOR = 0x70,           // 말하는 액터(+4 id, +8 x)
    DLG_NAME = 0x74,            // 화자 이름
    DLG_NOFACE = 0x78,          // 1이면 얼굴을 그리지 않는다
    DLG_MODE = 0x7c,            // 0 보통(흰 글), 1 얼굴·이름 없음(하늘색 글, 명령 0x8d), 2 강조(노란 글, 명령 0x89)
    DLG_COL1 = 0x80, DLG_COL2 = 0x84,
    DLG_FACE = 0x88,            // 초상화 프레임(UI 시트), 음수면 없음
    DLG_FLIP = 0x8c,            // 초상화 기본 뒤집기
    DLG_ARROW = 0x94,           // 다음 쪽 표시 상태: 2면 표시(다 드러남)

    // 선택창 C(VM + 0x4c, vtable 0x4dbdc)
    CH_RECT = 0x4c, CH_COL_TITLE = 0x70, CH_COL_ITEM = 0x74, CH_CURSOR = 0x7c, CH_COUNT = 0x80,
    CH_TITLE = 0x88,            // String(+0x84)의 글 포인터. 0이면 제목 없음
    CH_ITEMS = 0xa8,            // 항목 글 포인터 배열

    // 팝업(글 상자) P(클래스 0x4da48 계열, 그리기 0x23578)
    POP_TEXT = 0x14, POP_FLAGS = 0x60, POP_COLOR = 0x6c,
    POPF_SHOW = 0x1, POPF_DIM = 0x200, POPF_MULTI = 0x400,

    ARGBUF = 64,
    GFX_ORIGIN_Y = 0x54,        // 그리기 틀 원점 y(연출 뒷배경 장면에서 87)
    GFX_FRAME_BYTES = 0x140,    // 0x389c가 바꾸는 필드(+8 ~ +0x138)를 덮는 범위
};

// 대사 글 칸의 폭(dp = 게임 px). 원본 줄바꿈 폭도 이것이다.
enum { TEXT_MAX = 400, NARROW_W = 400, FACE_COL = 128, FACE_COL_MAX = 216 };

static uint32_t game_obj(void) { return G32(G32(APP_PTR) + APP_GAME); }

// 레지스터를 통째로 보관/복원한다(guest_call은 SP와 LR만 되돌린다).
typedef struct { uint32_t r[16]; } regs_t;
static void regs_save(cpu_t *c, regs_t *s) { memcpy(s->r, c->r, sizeof s->r); }
static void regs_load(cpu_t *c, const regs_t *s) { memcpy(c->r, s->r, sizeof s->r); }

// 32비트 원본 색(0xBBGGRRxx)을 RmlUi 색 글로
static void ui_color(const char *key, uint32_t col) {
    ui_fmt(key, "#%02x%02x%02x", (col >> 8) & 0xff, (col >> 16) & 0xff, (col >> 24) & 0xff);
}

// ---- 받은 글(ui_cap_text) 정리 ----
typedef struct { int x, y; unsigned rgb; const char *s; } capline_t;
static int cap_cmp(const void *a, const void *b) {
    const capline_t *p = a, *q = b;
    return p->y != q->y ? p->y - q->y : p->x - q->x;
}
// 받은 글을 위→아래, 왼→오른 순으로 늘어놓는다.
static int caps_sorted(capline_t *out, int max) {
    int n = ui_cap_count();
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
        int x, y; unsigned rgb;
        out[i].s = ui_cap_text(i, &x, &y, &rgb);
        out[i].x = x; out[i].y = y; out[i].rgb = rgb;
    }
    qsort(out, (size_t)n, sizeof *out, cap_cmp);
    return n;
}
static void ui_rgb565(const char *key, unsigned p) {
    ui_fmt(key, "#%02x%02x%02x", (p >> 11 & 31) * 255 / 31, (p >> 5 & 63) * 255 / 63, (p & 31) * 255 / 31);
}

// ==================================================================================================
// 대사창: 갱신 0x37630(W, G, 키 허용), 그리기 0x37388(W, G, gfx). 둘 다 0x11530(대사 대기 상태 7)이 부른다.
// ==================================================================================================

// 초상화가 있으면 판 한쪽에 칸을 둔다. 좁은 화면은 판 위에 원작처럼 얹는다.
static bool dlg_has_face(uint32_t W) {
    return G32(W + DLG_MODE) != 1 && (int32_t)G32(W + DLG_FACE) >= 0 && G32(W + DLG_NOFACE) != 1;
}
// 원작에 초상이 없는 인물: 얼굴 없이 말하는 대사의 화자 이름이 이 표에 있으면 리마스터 일러스트(host ui_illust_image)를 띄운다.
// 이름은 NPC 이름 표(게임 메모리 0x44d…)의 CP949 그대로. 일러스트를 끄면(도트) 원작처럼 얼굴이 없다.
static const char *const NPC_NAMES[] = {
    "\xb0\xd5\xb3\xaa\xb5\xe5", // 겐나드
    "\xb8\xf0\xbe\xc6\xb5\xe5", // 모아드
    "\xbe\xc6\xc4\xc9\xc0\xce", // 아케인
    "\xc5\xda\xb7\xaf",           // 텔러
    "\xc7\xcf\xb8\xe1\xb8\xb0", // 하멜린
};
static int dlg_npc(uint32_t W) {
    if (!ui_illust() || G32(W + DLG_MODE) == 1 || G32(W + DLG_NOFACE) == 1 || (int32_t)G32(W + DLG_FACE) >= 0 || !G32(W + DLG_NAME)) return -1;
    const char *name = gstr(G32(W + DLG_NAME));
    for (int i = 0; i < (int)(sizeof NPC_NAMES / sizeof *NPC_NAMES); i++) {
        int id = UI_ILLUST_NPC_FIRST + i, w, h;
        if (!strcmp(name, NPC_NAMES[i]) && ui_illust_size(id, &w, &h)) return id;
    }
    return -1;
}

// 합성 프레임의 폭(0x54f0과 같은 계산: 조각 (dx + 그림 폭)의 최댓값 - dx의 최솟값(0부터)).
static int frame_w(uint32_t sheet, int frame) {
    uint32_t frames = G32(sheet + 4);
    if (!frames || frame < 0) return 0;
    uint32_t f = G32(frames + 4u * (uint32_t)frame);
    if (!f) return 0;
    int n = (int)G32(f), lo = 0, hi = 0;
    for (int i = 0; i < n && i < 64; i++) {
        uint32_t part = f + 4 + 4u * (uint32_t)i;
        int dx = (int8_t)G8(part + 2), w = G8(G32(sheet + 0x10) + 12u * G8(part + 1));
        if (dx < lo) lo = dx;
        if (dx + w > hi) hi = dx + w;
    }
    return hi - lo;
}
// 넓은 화면의 얼굴 칸: 2배 초상화가 들어가게(128~216dp). 칸이 넓어지면 글 칸이 줄고 원본이 그 폭으로 줄을 나눈다.
static int dlg_face_col(uint32_t W, uint32_t G) {
    int npc = dlg_npc(W), nw, nh;
    if (npc >= 0 && ui_illust_size(npc, &nw, &nh)) { int col = nw * 2 + 8; return col < FACE_COL ? FACE_COL : col > FACE_COL_MAX ? FACE_COL_MAX : col; }
    if (!dlg_has_face(W)) return 0;
    uint32_t sheet = G32(G32(G + GAME_MENU) + MENU_SHEET);
    int col = sheet ? frame_w(sheet, (int)G32(W + DLG_FACE)) * 2 + 8 : FACE_COL;
    return col < FACE_COL ? FACE_COL : col > FACE_COL_MAX ? FACE_COL_MAX : col;
}
static int dlg_text_w(uint32_t W, uint32_t G) {
    int sw = ui_screen_w();
    if (sw < NARROW_W) return sw - 8 - 20;                 // 판 = 화면 - 8, 안쪽 여백 10씩
    int pw = sw - 16 < 560 ? sw - 16 : 560;
    int tw = pw - 24 - dlg_face_col(W, G);
    return tw < TEXT_MAX ? tw : TEXT_MAX;
}

// 원본이 줄을 나누는 폭(창 폭 - 8)을 새 글 칸에 맞춘다. 타자 효과를 끈 설정에서 0x37630은 화면 폭(gfx 클립 폭) - 8을 쓰므로 그것도.
typedef struct { uint32_t W, gfx, rect_w, clip_w; } dlgwidth_t;
static void dlg_width_push(dlgwidth_t *s, uint32_t W, uint32_t G, uint32_t gfx) {
    s->W = W; s->gfx = gfx;
    s->rect_w = G32(W + DLG_RECT_W);
    s->clip_w = G32(gfx + GFX_CLIP + 8);
    uint32_t w = (uint32_t)dlg_text_w(W, G) + 8;
    W32_(&cpu, W + DLG_RECT_W, w);
    W32_(&cpu, gfx + GFX_CLIP + 8, w);
}
static void dlg_width_pop(dlgwidth_t *s) {
    W32_(&cpu, s->W + DLG_RECT_W, s->rect_w);
    W32_(&cpu, s->gfx + GFX_CLIP + 8, s->clip_w);
}

// ---- 대사 기록(LOG, 사용자 요청 2026-11-12) ----
// 지나간 대사를 다시 본다. 대사가 이어지는 동안만 들고 있는 휘발성 기록이다: 쪽이 다 드러날 때 화자와 글을 적고,
// 필드에서 자유롭게 움직이게 되면(f_1054c) 지운다. 저장하지 않는다. 대사창 오른쪽 위 LOG 버튼이나 원본 0번 키
// (대사 동안 원본은 OK만 받는다. 패드 LB, 키보드 X)로 열고, 열려 있는 동안 대사는 멈춘다(원본 갱신 0x37630을 부르지 않는다).
enum { LOG_MAX = 80, LOG_NAME = 40, LOG_TEXT = 384 };
static struct { char name[LOG_NAME], text[LOG_TEXT]; } dlog[LOG_MAX];
static int dlog_n, dlog_open, dlog_bottom;
static uint32_t dlog_key_text, dlog_key_page;
static unsigned dlog_gen;
extern unsigned game_generation;
static void dlog_clear(void) { dlog_n = 0; dlog_open = 0; dlog_key_text = 0; }
static void dlog_add(uint32_t text_ptr, uint32_t page, const char *name, const char *text) {
    if (dlog_gen != game_generation) { dlog_clear(); dlog_gen = game_generation; }
    if ((text_ptr == dlog_key_text && page == dlog_key_page) || !text[0]) return;
    dlog_key_text = text_ptr; dlog_key_page = page;
    if (dlog_n == LOG_MAX) { memmove(dlog, dlog + 1, sizeof dlog[0] * (LOG_MAX - 1)); dlog_n--; }
    snprintf(dlog[dlog_n].name, LOG_NAME, "%s", name);
    snprintf(dlog[dlog_n].text, LOG_TEXT, "%s", text);
    dlog_n++;
}
// 누름(LOG 버튼·0번 키 = pick "dlog", 넘기기 = pick "dlogs" 0 위 1 아래). 대사 갱신 앞에서 본다.
static void dlog_input(void) {
    int t = ui_pick("dlog"), s = ui_pick("dlogs");
    if (t >= 0) { dlog_open = !dlog_open && dlog_n > 0; dlog_bottom = 3; }
    if (dlog_open && s >= 0) ui_scroll("dlog", s ? 64.f : -64.f);
}
static void dlog_emit(void) {
    ui_begin("dialog_log");
    ui_int("narrow", ui_screen_w() < NARROW_W);
    ui_list("items");
    for (int i = 0; i < dlog_n; i++) { ui_item(); ui_str("name", dlog[i].name); ui_str("text", dlog[i].text); ui_close(); }
    ui_close();
    ui_end();
    if (dlog_bottom > 0) { dlog_bottom--; ui_scroll("dlog", 100000.f); } // 열면 가장 최근 대사(맨 아래)부터. 배치가 잡힌 뒤라야 넘어가서 몇 틱 되풀이한다
}

void orig_f_37630(cpu_t *c);
void f_37630(cpu_t *c) {
    if (!ui_modern()) { orig_f_37630(c); return; }
    dlog_input();
    if (dlog_open) { c->r[0] = 0; return; } // 기록을 보는 동안 대사는 멈춘다(키도 타자 효과도 가지 않는다)
    // 이 함수는 그리지 않는다(키와 쪽 넘김). 클립 폭을 잠시 바꿔도 화면에는 영향이 없다.
    uint32_t W = c->r[0], G = c->r[1], gfx = G32(G + GAME_GFX);
    dlgwidth_t s;
    dlg_width_push(&s, W, G, gfx);
    orig_f_37630(c);
    dlg_width_pop(&s);
}

// 초상화를 떼어 낸다. 원본(0x3716c)과 같이 UI 시트의 합성 프레임 W+0x88을 0x56ec로 그린다.
// 얼굴 자리: 화자가 화면 왼쪽 절반에 있으면 얼굴은 왼쪽(이름표는 오른쪽), 아니면 오른쪽이다. 오른쪽 얼굴은 뒤집어 안쪽을 본다.
static bool dlg_face(uint32_t W, uint32_t G, int *left) {
    uint32_t actor = G32(W + DLG_ACTOR), field = G32(G + GAME_FIELD);
    *left = 0;
    if (field && actor) {
        int dx = (int)G32(actor + 8) - (int)G32(field + FIELD_CAM_X);
        *left = dx * 2 < (int)G32(W + DLG_RECT_W);
    }
    int npc = dlg_npc(W);
    if (npc >= 0) return ui_illust_image("face", npc, !*left); // 그림은 오른쪽을 본다. 오른쪽 칸이면 뒤집어 안쪽을 보게
    if (!dlg_has_face(W)) return false;
    uint32_t sheet = G32(G32(G + GAME_MENU) + MENU_SHEET);
    if (!sheet) return false;
    int flip = (int)(G32(W + DLG_FLIP) ^ (*left ? 0u : 1u)) & 1;
    int sw = ui_screen_w();
    // 연출 뒷배경 장면(0xcc94)은 그리기 틀을 가운데 146줄 띠로 줄여 둔다(0x389c: 원점 gfx+0x50/+0x54, 크기 +0x44/+0x48, 클립).
    // 그대로 그리면 초상이 띠에 잘리고 클립 좌표가 화면 좌표와 어긋나 리마스터 그림이 띠 안쪽 일부에만 씌워진다.
    // 떼는 동안만 틀을 화면 전체로 돌려놓는다.
    uint32_t gfx = G32(G + GAME_GFX);
    uint8_t frame[GFX_FRAME_BYTES];
    memcpy(frame, cpu.m + gfx, sizeof frame);
    CALL(FN(0x389c), gfx, 0, 0, (uint32_t)sw, 320);
    ui_scratch_clear();
    CALL(FN(0x56ec), sheet, G32(W + DLG_FACE), (uint32_t)(sw / 2), 40, 0, 0, (uint32_t)flip);
    ui_sprite_trim("face", 0, 0, sw, 320);
    memcpy(cpu.m + gfx, frame, sizeof frame);
    return true;
}

// AUTO·SKIP(호스트 ui_dialog_auto/skip). 입력은 원본 그대로 OK 키를 넣어 넘긴다(0x37630은 OK만 받는다).
// SKIP: 틱마다 OK(타자 효과 건너뛰기 → 쪽 넘김). 선택지가 뜨거나 대사가 끝나면 호스트가 끈다.
// AUTO: 쪽이 다 드러난 뒤 글 길이에 맞춰 기다렸다 OK. 1.2초 + 글자당 0.06초(CP949 2바이트 = 한 글자), 5초까지.
static void dlg_autoplay(uint32_t W, int done, int page_bytes) {
    static uint32_t key_text, key_page;
    static uint64_t since;
    if (dlog_open) { since = 0; return; }
    if (ui_dialog_skip()) { ui_key_tap(UI_KEY_OK); since = 0; return; }
    if (!ui_dialog_auto() || !done) { since = 0; return; }
    uint32_t t = G32(W + DLG_TEXT), pg = G32(W + DLG_PAGE);
    uint64_t now = now_ms();
    if (!since || t != key_text || pg != key_page) { since = now; key_text = t; key_page = pg; return; }
    uint64_t wait = 1200 + (uint64_t)(page_bytes / 2) * 60;
    if (wait > 5000) wait = 5000;
    if (now - since >= wait && ui_key_tap(UI_KEY_OK)) since = now; // 아직 같은 쪽이면(키가 씹힘) 다시 기다린다
}

void orig_f_37388(cpu_t *c);
void f_37388(cpu_t *c) {
    if (!ui_modern()) { orig_f_37388(c); return; }
    regs_t saved;
    regs_save(c, &saved);
    uint32_t W = c->r[0], G = c->r[1], gfx = c->r[2];
    int sw = ui_screen_w(), narrow = sw < NARROW_W;
    int mode = (int)G32(W + DLG_MODE);

    ui_begin("dialog");
    ui_int("narrow", narrow);
    ui_scratch_begin();
    int face_left = 0;
    bool face = dlg_face(W, G, &face_left);
    if (!face) { ui_text("face", ""); ui_int("face_w", 0); ui_int("face_h", 0); }
    ui_int("has_face", face);
    ui_int("face_left", face_left);

    // 원본 그리기(새 글 칸 폭으로): 타자 효과로 드러난 줄을 받는다. 이름표 글(창 위)과 본문(창 안)을 y로 나눈다.
    ui_scratch_clear();
    regs_load(c, &saved);
    dlgwidth_t s;
    dlg_width_push(&s, W, G, gfx);
    int rect_y = (int)G32(W + DLG_RECT + 4) + (int)G32(gfx + GFX_ORIGIN_Y); // 받은 글 좌표는 화면 기준(틀 원점이 더해져 있다)
    orig_f_37388(c);
    dlg_width_pop(&s);
    uint32_t ret = c->r[0];
    capline_t caps[16];
    int n = caps_sorted(caps, 16);
    ui_scratch_end();

    const char *name = G32(W + DLG_NAME) ? gstr(G32(W + DLG_NAME)) : "";
    ui_str("name", mode == 1 ? "" : name);
    ui_int("mode", mode);
    ui_color("color", mode == 1 ? G32(W + DLG_COL1) : mode == 2 ? G32(W + DLG_COL2) : 0xffffff00u);
    // 받은 줄은 대사 글의 부분 글이다. 쪽 시작(W+0x20)부터 줄을 차례로 찾아 넘기면 이 쪽이 끝나는 곳을 안다.
    const char *text = gstr(G32(W + DLG_TEXT));
    int total = (int)strlen(text), page = (int)G32(W + DLG_PAGE);
    const char *p = text + (page >= 0 && page <= total ? page : 0);
    char page_text[LOG_TEXT];
    int pt = 0;
    page_text[0] = 0;
    ui_list("lines");
    for (int i = 0; i < n; i++) {
        if (caps[i].y < rect_y) continue;      // 이름표
        ui_item(); ui_str("t", caps[i].s); ui_close();
        pt += snprintf(page_text + pt, pt < LOG_TEXT ? sizeof page_text - (size_t)pt : 0, "%s%s", pt ? "\n" : "", caps[i].s);
        if (pt >= LOG_TEXT) pt = LOG_TEXT - 1;
        const char *f = caps[i].s[0] ? strstr(p, caps[i].s) : NULL;
        if (f) p = f + strlen(caps[i].s);
    }
    ui_close();
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    // 다음 쪽/끝 표시: 원본은 상태 2(쪽을 다 드러냄)일 때만 그린다. 남은 글이 있으면 프레임 0x20(다음), 없으면 0x18(끝).
    int done = G8(W + DLG_ARROW) == 2;
    ui_int("next", done && *p);
    ui_int("end", done && !*p);
    if (done) dlog_add(G32(W + DLG_TEXT), G32(W + DLG_PAGE), mode == 1 ? "" : name, page_text); // 다 드러난 쪽을 기록(대사 기록)
    ui_int("log_n", dlog_n);
    dlg_autoplay(W, done, (int)(p - (text + (page >= 0 && page <= total ? page : 0))));
    ui_bool("auto", ui_dialog_auto());
    ui_bool("skip", ui_dialog_skip());
    // 판 폭: 글 칸 + 안쪽 여백(10씩) + 얼굴 칸. 좁은 화면은 화면 - 8.
    int tw = dlg_text_w(W, G), fcol = face && !narrow ? dlg_face_col(W, G) : 0;
    ui_int("tw", tw);
    ui_int("fcol", fcol);
    ui_int("pw", narrow ? sw - 8 : tw + 20 + fcol);
    ui_end();
    if (dlog_open) dlog_emit();

    regs_load(c, &saved);
    c->r[0] = ret;
}

// ==================================================================================================
// 선택창: 그리기 0x37894(C, G, gfx). 스크립트 대기 상태 13(0x39614)과 미니게임 시작(0x397d0)이 부른다.
// 여관(0x39840), 저장 지점(0x398d4), 꽃(0x39954)·헌화(0x39914), 도움말 선택(0x3cf10), 카지노(0x3ccfc/0x3cdc0/0x3d018)가 모두 이것이다.
// 키(0x37840): 위/2, 아래/8(끝에서 멈춤), OK/5 결정. 취소 키는 없다.
// ==================================================================================================
void orig_f_37894(cpu_t *c);
void f_37894(cpu_t *c) {
    if (!ui_modern()) { orig_f_37894(c); return; }
    regs_t saved;
    regs_save(c, &saved);
    uint32_t C = c->r[0];
    ui_scratch_begin();
    orig_f_37894(c);
    uint32_t ret = c->r[0];
    ui_scratch_end();

    int n = (int)G32(C + CH_COUNT), cur = (int)G32(C + CH_CURSOR);
    if (n > 8) n = 8;
    uint32_t title = G32(C + CH_TITLE);
    ui_begin("dialog_choice");
    ui_int("narrow", ui_screen_w() < NARROW_W);
    ui_str("title", title ? gstr(title) : "");
    ui_int("has_title", title && G8(title));
    ui_color("tcolor", G32(C + CH_COL_TITLE));
    ui_color("icolor", G32(C + CH_COL_ITEM));
    ui_int("cursor", cur);
    ui_list("items");
    for (int i = 0; i < n; i++) {
        uint32_t s = G32(C + CH_ITEMS + 4u * (uint32_t)i);
        ui_item(); ui_str("t", s ? gstr(s) : ""); ui_close();
    }
    ui_close();
    // 터치: 항목을 누르면 커서를 옮기고 다시 누르면 결정. 원본 키(0x37840)는 위/아래(끝에서 멈춤, 순환 없음)와 OK뿐이다.
    ui_nav("choice", cur, n, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_VERT);
    ui_int("first", cur <= 0);
    ui_int("last", cur >= n - 1);
    ui_end();
    regs_load(c, &saved);
    c->r[0] = ret;
}

// ==================================================================================================
// 글 상자 팝업 그리기 0x23578(P, gfx): 가상 함수(vtable +0xc)로 불린다. 여러 화면이 같은 클래스를 쓰므로 부른 곳(LR)으로 고른다.
//   0xd600  지역 이름 띠(맵 이동 뒤, G+0x4fc가 있는 동안)        → 문서 dialog_banner
//   0x11594 획득 알림(아이템/골드/파티 GP, 스크립트 대기 상태 9)  → 문서 dialog_notice
//   0x3312c 전투 뒤 레벨업 알림(스테이터스 화면 위)               → 문서 dialog_notice(금색)
//   0x12b98 마릿수/초 세기 띠(0x12c08, G+0x14 & 0x1800)          → 문서 dialog_counter
// 그 밖(전투 메시지 상자 0x23120 등 다른 영역 몫)은 원본 그대로 둔다.
// 원본은 scratch에서 그대로 부르고(그린 글을 받는다) 보일 때(P+0x60 & 1)만 문서를 띄운다.
// ==================================================================================================
static bool lr_in(uint32_t lr, uint32_t lo, uint32_t hi) {
    lr &= ~1u;
    return lr >= FN(lo) && lr < FN(hi);
}

// 스테이터스 화면(ui_fmchar.c)은 레벨업 알림 상자를 호스트 쪽에서 직접 부른다. 그때는 LR이 0x3312c 안이 아니므로 이 값으로 알린다.
int ui_dialog_levelup_call;
void orig_f_23578(cpu_t *c);
void f_23578(cpu_t *c) {
    uint32_t lr = c->LR, P = c->r[0];
    const char *doc = NULL;
    int levelup = 0;
    if (lr_in(lr, 0xd600, 0xd680)) doc = "dialog_banner";
    else if (lr_in(lr, 0x11594, 0x115dc)) doc = "dialog_notice";
    else if (ui_dialog_levelup_call || lr_in(lr, 0x3312c, 0x33730)) doc = "dialog_notice", levelup = 1;
    else if (lr_in(lr, 0x12b98, 0x12c08)) doc = "dialog_counter";
    if (!doc || !ui_modern() || !(G32(P + POP_FLAGS) & POPF_SHOW)) { orig_f_23578(c); return; }

    regs_t saved;
    regs_save(c, &saved);
    ui_scratch_begin();
    orig_f_23578(c);
    uint32_t ret = c->r[0];
    capline_t caps[16];
    int n = caps_sorted(caps, 16);
    ui_scratch_end();

    // 세기 띠는 한 틱에 둘(남은 수, 마릿수)까지 그려질 수 있다. 같은 틱에 받은 글을 모아 마지막 ui_end에 함께 넣는다.
    if (!strcmp(doc, "dialog_counter")) {
        static char cnt[4][64];
        static int ncnt;
        static uint32_t cnt_frame;
        uint32_t frame = G32(G32(APP_PTR) + 0x58);
        if (frame != cnt_frame) { cnt_frame = frame; ncnt = 0; }
        for (int i = 0; i < n && ncnt < 4; i++) snprintf(cnt[ncnt++], sizeof cnt[0], "%s", caps[i].s);
        ui_begin(doc);
        ui_list("lines");
        for (int i = 0; i < ncnt; i++) { ui_item(); ui_str("t", cnt[i]); ui_close(); }
        ui_close();
        ui_end();
        regs_load(c, &saved);
        c->r[0] = ret;
        return;
    }
    // 같은 높이의 글(탭으로 나눈 두 칸)은 한 줄로 합친다.
    ui_begin(doc);
    ui_int("levelup", levelup);
    ui_int("dim", (G32(P + POP_FLAGS) & POPF_DIM) != 0);
    ui_color("color", G32(P + POP_COLOR));
    // 상자 폭은 여기서 잰다. RmlUi는 위쪽 가운데 무대(flex)에서 여러 줄 토스트의 폭을 글에 맞추지 못하고 0 가까이로 잡아,
    // 긴 알림(펀터도우미 설명 등, 원본이 화면 폭 - 128로 나눈 여러 줄)이 한 낱말씩 세로로 상자 밖에 넘쳤다(폰에서 발견).
    // 글꼴이 게임 글자와 같은 Neo둥근모라 원본 글 폭 함수로 dp까지 맞게 잰다. 두 칸 줄은 사이 16dp를 더한다.
    int maxw = 0;
    ui_list("lines");
    for (int i = 0; i < n;) {
        int j = i + 1;
        while (j < n && caps[j].y == caps[i].y) j++;
        const char *t2 = j > i + 1 ? caps[i + 1].s : "";
        int w = font_string_width((const uint8_t *)caps[i].s, (int)strlen(caps[i].s));
        if (t2[0]) w += 16 + font_string_width((const uint8_t *)t2, (int)strlen(t2));
        if (w > maxw) maxw = w;
        ui_item();
        ui_str("t", caps[i].s);
        ui_str("t2", t2);
        ui_close();
        i = j;
    }
    ui_close();
    int bw = maxw + 2 * 14 + 2 + 2; // 좌우 안쪽 여백(.toast 14dp), 테두리, 여유
    int lim = ui_screen_w() * 9 / 10;
    if (bw < 160) bw = 160;
    if (lim > 0 && bw > lim) bw = lim;
    ui_int("w", bw);
    ui_end();
    regs_load(c, &saved);
    c->r[0] = ret;
}

// ==================================================================================================
// 지역 미니맵: 장면 함수 0x10b8c(G, 입력). 필드를 그리고(0xc958) 그 위에 제목 창·지도 창·키 안내를 그린 뒤 CLR/#이면 필드로 돌아간다.
// 지도 그림(타일 색 사각형 0x2f06c, 테두리, 표시점, 깜빡이는 주인공 점)은 원작 그대로 떼어 내고 틀과 글만 새로 그린다.
//   1차(scratch): 필드 그리기를 잠시 끄고(G+0x18 = 1, HUD 비트 끔) 원본을 불러 지도만 그리게 해 떼어 낸다.
//                 키 처리(장면 전환)는 같은 값을 두 번 쓰는 것이라 무해하고, 깜빡임 카운터는 되돌린다.
//   2차(실제 화면): 그리기 클립을 빈 사각형으로 두고 원본을 부른다. 0xc958은 필드를 자기 클립(G+0x568)으로 그리고 빈 클립을
//                 되돌려 놓으므로, 그 뒤의 창·지도·키 안내는 하나도 찍히지 않는다. 키 처리와 깜빡임은 여기서 원작대로 한 번 일어난다.
// 0x2f06c(지도 그리기)는 미니맵에서만 불리므로 감싸서 지도 자리(x, y, 배율)를 받는다.
// ==================================================================================================
static int mm_rec, mm_ok, mm_x, mm_y, mm_w, mm_h, mm_sx, mm_sy;

void orig_f_2f06c(cpu_t *c);
void f_2f06c(cpu_t *c) {
    if (mm_rec) {
        uint32_t F = c->r[0];
        int sx = (int)arg(c, 4), sy = (int)arg(c, 5);
        mm_x = (int)c->r[2]; mm_y = (int)c->r[3];
        mm_sx = sx; mm_sy = sy;
        mm_w = (int)G32(F + 0xc) * sx >> 12;
        mm_h = (int)G32(F + 0x10) * sy >> 12;
        mm_ok = 1;
    }
    orig_f_2f06c(c);
}

void orig_f_10b8c(cpu_t *c);
void f_10b8c(cpu_t *c) {
    if (!ui_modern()) { orig_f_10b8c(c); return; }
    regs_t saved;
    regs_save(c, &saved);
    uint32_t G = c->r[0], gfx = G32(G + GAME_GFX);
    int sw = ui_screen_w();

    // 1차: 지도만
    uint32_t fstate = G32(G + GAME_FSTATE), flags = G32(G + GAME_FLAGS), blink = G32(G + GAME_BLINK);
    W32_(&cpu, G + GAME_FSTATE, 1);
    W32_(&cpu, G + GAME_FLAGS, flags & ~0x1808u);
    ui_begin("dialog_minimap");
    ui_scratch_begin();
    mm_rec = 1; mm_ok = 0;
    orig_f_10b8c(c);
    mm_rec = 0;
    if (mm_ok) ui_sprite("map", mm_x - 1, mm_y - 1, mm_w + 2, mm_h + 2);
    else { ui_text("map", ""); ui_int("map_w", 0); ui_int("map_h", 0); }
    ui_scratch_end();
    W32_(&cpu, G + GAME_FSTATE, fstate);
    W32_(&cpu, G + GAME_FLAGS, flags);
    W32_(&cpu, G + GAME_BLINK, blink);

    int mw = mm_ok ? mm_w + 2 : 0, mh = mm_ok ? mm_h + 2 : 0;
    int scale = sw - 40 >= mw * 2 && 320 - 90 >= mh * 2 ? 2 : 1;
    ui_int("scale", scale);
    ui_int("mw", mw * scale);
    ui_int("mh", mh * scale);
    ui_str("place", gstr(G + GAME_PLACE));
    ui_end();

    // 2차: 필드만(창은 빈 클립에 막힌다)
    uint32_t clip[4];
    for (int i = 0; i < 4; i++) clip[i] = G32(gfx + GFX_CLIP + 4u * (uint32_t)i);
    for (int i = 0; i < 4; i++) W32_(&cpu, gfx + GFX_CLIP + 4u * (uint32_t)i, 0);
    regs_load(c, &saved);
    orig_f_10b8c(c);
    uint32_t ret = c->r[0];
    for (int i = 0; i < 4; i++) W32_(&cpu, gfx + GFX_CLIP + 4u * (uint32_t)i, clip[i]);
    regs_load(c, &saved);
    c->r[0] = ret;
}

// ==================================================================================================
// 비행정 안내 0x12b18(G): 화면 맨 위 띠(높이 글줄+8)를 반투명으로 어둡게 하고 「【이착륙:0번키】」를 가운데 쓴다.
// 조건은 0xc958이 본다(G+0x14 & 8, G+0x20 & 0x20000). 부수 효과가 없는 그리기라 원본은 부르지 않고 글만 받는다.
// ==================================================================================================
void orig_f_12b18(cpu_t *c);
void f_12b18(cpu_t *c) {
    if (!ui_modern()) { orig_f_12b18(c); return; }
    regs_t saved;
    regs_save(c, &saved);
    ui_scratch_begin();
    orig_f_12b18(c);
    capline_t caps[4];
    int n = caps_sorted(caps, 4);
    ui_scratch_end();
    ui_begin("dialog_hud");
    // 원본 글 「【이착륙:0번키】」에서 이름표(이착륙)만 글로 넘기고 키 안내는 문서가 '0'|key 이름표로 그린다
    // (장치마다 키 이름이 다르다. 화면 패드에는 이착륙 버튼이 따로 있다). 형식이 다르면 원본 글 그대로.
    char hud[96] = "";
    if (n) snprintf(hud, sizeof hud, "%s", caps[0].s);
    char *colon = strchr(hud, ':');
    const char *head = hud;
    if (colon) { *colon = 0; if (!strncmp(head, "【", strlen("【"))) head += strlen("【"); }
    ui_str("t", head);
    ui_int("has_key", colon != NULL);
    ui_end();
    regs_load(c, &saved);
}

// ==================================================================================================
// 말풍선 창(vtable 0x4dbec, 생성 0x36c90 ← 0x11740 ← 스크립트 명령 0x8a/0x8b): 그리기 0x36d74(P, gfx).
// 액터 머리 위(+0x100 플래그면 화면 위쪽 띠)에 한 줄 글. 시간이 지나면 스스로 보이기 플래그를 끈다(0x36e24) — 원본을 불러 지킨다.
// 같은 틱에 여러 개가 그려질 수 있어(G+0x348 목록) 그 틱에 모은 것을 매번 다시 넣는다(마지막 ui_end가 남는다).
// ==================================================================================================
typedef struct { int x, y, top; char s[96]; } balloon_t;
static balloon_t balloons[8];
static int nballoon;
static uint32_t balloon_frame;

void orig_f_36d74(cpu_t *c);
void f_36d74(cpu_t *c) {
    if (!ui_modern()) { orig_f_36d74(c); return; }
    regs_t saved;
    regs_save(c, &saved);
    uint32_t P = c->r[0];
    int top = (G32(P + POP_FLAGS) & 0x100) != 0;
    ui_scratch_begin();
    orig_f_36d74(c);
    uint32_t ret = c->r[0];
    capline_t caps[4];
    int n = caps_sorted(caps, 4);
    ui_scratch_end();

    uint32_t frame = G32(G32(APP_PTR) + 0x58);
    if (frame != balloon_frame) { balloon_frame = frame; nballoon = 0; }
    if (n && nballoon < 8) {
        balloon_t *b = &balloons[nballoon++];
        b->x = caps[0].x; b->y = caps[0].y; b->top = top;
        snprintf(b->s, sizeof b->s, "%s", caps[0].s);
    }
    if (!nballoon) { regs_load(c, &saved); c->r[0] = ret; return; } // 보이는 말풍선이 없다: 문서를 띄우지 않는다
    ui_begin("dialog_balloon");
    ui_list("items");
    for (int i = 0; i < nballoon; i++) {
        ui_item();
        ui_str("t", balloons[i].s);
        ui_int("x", balloons[i].x);
        ui_int("y", balloons[i].y);
        ui_int("top", balloons[i].top);
        ui_close();
    }
    ui_close();
    ui_end();
    regs_load(c, &saved);
    c->r[0] = ret;
}

// ==================================================================================================
// 필드 HUD(새 UI에만 있다, 사용자 요청): 오른쪽 위에 늘 보이는 지역 미니맵과 그 왼쪽 퀵세이브 버튼.
// 필드 장면 틱 0x1054c(G, 입력)를 감싸 원본 틱 뒤에 그린다. 필드(장면 7)이고 필드 상태(G+0x18)가 평소(0)일 때만 보인다.
//   지도: 미니맵 장면 0x10b8c를 scratch에서 부른다(위 f_10b8c의 1차와 같은 방법). 키를 읽지 않게 입력 객체의 "눌림"(+0xc)을
//         잠시 0으로 둔다(0x61d4가 0을 돌려준다). 주인공 점은 원본이 깜빡여 그리므로 그리지 않는 쪽(카운터 % 7 > 3)으로 두고
//         문서가 같은 자리(0x10d5e의 식: 주인공 [G+0x1b4]의 +8/+0xc를 14로 나눠 배율)에 점을 얹는다. 그래서 지도 그림은
//         맵마다 거의 같아 텍스처가 늘지 않는다. 누르면 원본 지도 화면(#)을 연다.
//   퀵세이브: 지금 쓰는 슬롯(이번 실행에서 마지막으로 불러오거나 저장한 슬롯, 사용자 결정)에 묻지 않고 0x110d4(G, 슬롯)로 저장한다.
//         그런 슬롯이 없으면 첫 빈 슬롯, 빈 슬롯도 없으면 저장 슬롯 화면(0xcea8(G, 7, 3, 1, 1), 이벤트 저장과 같은 모드 3)을 연다.
//         저장 가능 조건은 옵션의 저장 줄과 같다(0x12afc: G+0x20의 0x20000이 없고 0x1000이 있다). F5도 같다.
// ==================================================================================================
enum {
    G_HERO = 0x1b4, G_SLOTS = 0x334,
    FN_NOW = 0x103234,
};
static int cur_slot = -1;           // 지금 쓰는 슬롯(0..2)
extern unsigned game_generation;    // host/main.c: 게임을 다시 시작하면(세이브 불러오기 등) 는다. 그 전의 슬롯은 잊는다
static unsigned slot_gen;
static void slot_now(int slot) { cur_slot = slot; slot_gen = game_generation; }
static char hud_msg[96];            // 퀵세이브 결과 토스트(UTF-8)
static uint32_t hud_msg_until;

void orig_f_110d4(cpu_t *c);
void f_110d4(cpu_t *c) {
    int slot = (int)c->r[1];
    orig_f_110d4(c);
    if (c->r[0] == 0 && slot >= 0 && slot < 3) { slot_now(slot); ext_slot_save(slot); }
}
void orig_f_111b8(cpu_t *c);
void f_111b8(cpu_t *c) {
    int slot = (int)c->r[1];
    orig_f_111b8(c);
    if (slot >= 0 && slot < 3) { slot_now(slot); if (c->r[0] == 0) ext_slot_load(slot); }
}

static bool slot_used(uint32_t G, int i) {
    uint32_t S = G32(G + G_SLOTS);
    return S && G32(S + 0x4cu * (uint32_t)i + 0x5c) != 0;
}

static void hud_say(const char *msg) {
    snprintf(hud_msg, sizeof hud_msg, "%s", msg);
    hud_msg_until = CALL(FN_NOW) + 2000;
}

// 퀵세이브. 쓸 슬롯이 없으면 저장 슬롯 화면을 연다(장면이 14로 바뀐다).
static void quicksave(uint32_t G) {
    uint32_t gf = G32(G + GAME_FLAGS2);
    if ((gf & 0x20000) || !(gf & 0x1000)) { hud_say("여기서는 저장할 수 없습니다"); return; }
    if (slot_gen != game_generation) cur_slot = -1;
    int slot = cur_slot;
    if (slot < 0)
        for (int i = 0; i < 3; i++) if (!slot_used(G, i)) { slot = i; break; }
    if (slot < 0) { CALL(FN(0xcea8), G, 7, 3, 1, 1); return; }
    uint32_t r = CALL(FN(0x110d4), G, (uint32_t)slot);
    char b[96];
    if (r == 0) snprintf(b, sizeof b, "슬롯 %d에 저장했습니다", slot + 1);
    else snprintf(b, sizeof b, "저장하지 못했습니다(저장공간 부족)");
    hud_say(b);
}

void orig_f_1054c(cpu_t *c);
__attribute__((weak)) void ext_field_tick(uint32_t G);
__attribute__((weak)) int ext_incense_hud(int *left);
// 진단(ECPE_FACEDUMP=<폴더>): UI 시트의 프레임을 모두 그려 PPM으로 남긴다. 필드에서 한 번.
static void face_dump(uint32_t G) {
    static bool dumped;
    const char *dd = getenv("ECPE_FACEDUMP");
    if (!dd || dumped) return;
    uint32_t sheet = G32(G32(G + GAME_MENU) + MENU_SHEET);
    if (!sheet) return;
    dumped = true;
    int sw = ui_screen_w();
    ui_scratch_begin();
    uint32_t frames = G32(sheet + 4);
    for (int k = 0; k < 400 && frames; k++) {
        uint32_t f = G32(frames + 4u * (uint32_t)k);
        if (f < 0x100000 || f >= 0x4000000 || G32(f) == 0 || G32(f) > 64) continue;
        ui_scratch_clear();
        CALL(FN(0x56ec), sheet, (uint32_t)k, (uint32_t)(sw / 2), 120, 0, 0, 0);
        char path[512]; snprintf(path, sizeof path, "%s/f%03d.ppm", dd, k);
        ui_scratch_dump(path);
    }
    ui_scratch_end();
}

void f_1054c(cpu_t *c) {
    uint32_t G = c->r[0], input = c->r[1];
    if (ext_field_tick) { regs_t sv; regs_save(c, &sv); ext_field_tick(G); regs_load(c, &sv); } // 확장(귀환·향): restored/warp.c (게스트 호출이 레지스터를 덮는다)
    orig_f_1054c(c);
    if (!ui_modern()) { ui_pick("qsave"); ui_pick("hudmap"); return; }
    regs_t saved;
    regs_save(c, &saved);
    bool field = G32(G + GAME_SCENE) == 7 && G32(G + GAME_FSTATE) == 0;
    if (field) { dlog_clear(); ui_pick("dlog"); ui_pick("dlogs"); } // 대사가 끝나 필드로 돌아왔다: 대사 기록을 지운다(휘발성)
    int qs = ui_pick("qsave"), mp = ui_pick("hudmap");
    if (field && mp >= 0) ui_key_tap('#');
    if (field && qs >= 0) quicksave(G);
    if (!field || G32(G + GAME_SCENE) != 7) { regs_load(c, &saved); return; }
    face_dump(G);

    // 지도만 떼어 낸다(f_10b8c 1차와 같다)
    uint32_t flags = G32(G + GAME_FLAGS), blink = G32(G + GAME_BLINK), held = G32(input + 0xc);
    W32_(&cpu, G + GAME_FSTATE, 1);
    W32_(&cpu, G + GAME_FLAGS, flags & ~0x1808u);
    W32_(&cpu, G + GAME_BLINK, 4);  // 4 % 7 > 3: 주인공 점을 그리지 않는다
    W32_(&cpu, input + 0xc, 0);
    ui_begin("hud");
    ui_scratch_begin();
    mm_rec = 1; mm_ok = 0;
    c->r[0] = G; c->r[1] = input;
    orig_f_10b8c(c);
    mm_rec = 0;
    if (mm_ok) ui_sprite("map", mm_x - 1, mm_y - 1, mm_w + 2, mm_h + 2);
    else { ui_text("map", ""); ui_int("map_w", 0); ui_int("map_h", 0); }
    ui_scratch_end();
    W32_(&cpu, G + GAME_FSTATE, 0);
    W32_(&cpu, G + GAME_FLAGS, flags);
    W32_(&cpu, G + GAME_BLINK, blink);
    W32_(&cpu, input + 0xc, held);

    // 주인공 점: 지도 그림(테두리 1px 포함) 기준 가운데 자리
    uint32_t hero = G32(G + G_HERO);
    int hx = -1, hy = -1;
    if (mm_ok && hero) {
        hx = 1 + ((int)G32(hero + 8) / 14 * mm_sx >> 12);
        hy = 1 + ((int)G32(hero + 0xc) / 14 * mm_sy >> 12);
    }
    ui_bool("has_map", mm_ok);
    ui_int("mw", mm_ok ? mm_w + 2 : 0);
    ui_int("mh", mm_ok ? mm_h + 2 : 0);
    ui_int("hx", hx);
    ui_int("hy", hy);
    ui_str("place", gstr(G + GAME_PLACE));
    int inc_left = 0, inc_mode = ext_incense_hud ? ext_incense_hud(&inc_left) : 0; // 조우율 향(restored/incense.c)
    ui_int("inc_mode", inc_mode);
    ui_int("inc_left", inc_left);
    uint32_t gf = G32(G + GAME_FLAGS2);
    ui_bool("can_save", !(gf & 0x20000) && (gf & 0x1000));
    bool msg = hud_msg[0] && (int32_t)(hud_msg_until - CALL(FN_NOW)) > 0;
    ui_text("msg", msg ? hud_msg : "");
    ui_end();
    regs_load(c, &saved);
}
