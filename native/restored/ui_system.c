// 필드 메뉴 화면 7(옵션: 저장/불러오기, 환경설정, 도움말, 게임종료)과 그 타이틀 모드(이어하기 슬롯), 예/아니오 상자를 RmlUi로 그린다.
// 문서 ui/system.rml(화면 7 전체), ui/sys_yesno.rml(예/아니오 상자가 화면 7 밖에서 불릴 때).
//
// 원본 그리기는 상태를 바꾼다(저장 대화 +0x24를 2→3→5/7로 넘기고, 3에서 실제 저장 0x110d4를 하고, 메시지 프레임 +0x2c를 센다).
// 그래서 원본을 그대로 부르되 ui_scratch로 화면만 버리고, 값은 구조체에서 읽는다(필드 뜻은 work/ui/survey/fieldmenu.md 「화면 7」).
//   화면 객체(scr = menu+0x40): +4 메뉴, +0xc 하위 화면(0 목록, 1 저장/불러오기, 2 환경설정, 3 도움말, 4 종료 확인),
//   +0x10 저장·불러오기 단계(0 고르기, 1 저장 슬롯, 2 불러오기 슬롯), +0x1c 목록 커서 기억, +0x20 커서, +0x24 슬롯 대화 단계,
//   +0x28 예(0)/아니오(1), +0x30 환경설정 편집 사본.
//   메뉴 객체: +0 게임, +0x1c 여는 방식(1 필드, 2 타이틀 이어하기, 3 이벤트 저장), +0x64/+0x68 창 x/y, +0x78 창 폭.
#include "restored.h"
#include "cfgx.h"
#include "help.h"
#include "items_ext.h"
#include "fm_shell.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    GOT = 0x14e624, APP_VAR = 0x14ee34,
    // GOT 칸(실행 중에는 게스트 주소로 고쳐져 있다)
    G_HELP = 0x218, G_HELP_T = 0x4f8, G_LIST = 0x4fc, G_QUIT_Q = 0x500, G_SL_T = 0x504, G_LOAD = 0x508, G_TOTAL = 0x50c,
    G_SAVE_Q = 0x514, G_LOAD_Q = 0x518, G_SAVING = 0x51c, G_SAVED = 0x520, G_INIT_FAIL = 0x524, G_NO_SPACE = 0x528,
    G_BAD_VER = 0x52c, G_EMPTY = 0x530, G_CFG_T = 0x534, G_SPEED = 0x53c, G_CFG_NAMES = 0x540, G_CFG_VALS = 0x544,
    G_OPT_T = 0x548, G_YESNO = 0x5b0, G_SAVE = 0x448,
    // 화면 객체
    S_MENU = 0x04, S_SUB = 0x0c, S_STAGE = 0x10, S_LISTCUR = 0x1c, S_CUR = 0x20, S_DLG = 0x24, S_YN = 0x28, S_CFG = 0x30,
    // 메뉴 객체
    M_GAME = 0x00, M_MODE = 0x1c, M_CLOSE = 0x60, M_X = 0x64, M_Y = 0x68, M_W = 0x78,
    // 게임 객체
    G_FLAGS = 0x20, G_START = 0x24, G_PLAYED = 0x150, G_SLOTS = 0x334, G_HELP_PAGE = 0x53c,
    FN_NOW = 0x103234, // 현재 시각(ms)
    HELP_LINES = 6,    // 쪽마다 줄 수(0x34f48이 0x10258에 넘기는 값)
};

#define G(off) R32_(&cpu, GOT + (off))
static const char *gs(uint32_t off) { return gstr(G(off)); }

static void hms(const char *key, uint32_t ms) {
    uint32_t s = ms / 1000;
    ui_fmt(key, "%02u:%02u:%02u", s / 3600 % 100, s / 60 % 60, s % 60);
}

// 슬롯 i: 머리 [game+0x334] + 0x4c*i + 0x14, 그 +4 플레이 시간(ms), +8 장소 이름. 있는지는 +0x5c ≠ 0(0x12ec4).
static void slots(uint32_t game) {
    uint32_t S = R32_(&cpu, game + G_SLOTS);
    ui_list("slots");
    for (int i = 0; i < 3; i++) {
        uint32_t s = S + 0x4c * i;
        bool used = S && R32_(&cpu, s + 0x5c) != 0;
        ui_item();
        ui_int("no", i + 1);
        ui_bool("used", used);
        ui_str("name", used ? gstr(s + 0x14 + 8) : "");
        if (used) hms("time", R32_(&cpu, s + 0x14 + 4)); else ui_text("time", "");
        ui_close();
    }
    ui_close();
    ui_str("empty", gs(G_EMPTY));
}

// 속도 단계: 비트 8..15를 0..4로 가둔다(0x359f8)
static int speed(uint32_t flags) { int s = (int)(flags >> 8 & 0xff); return s > 4 ? 4 : s; }

// 환경설정 네 줄(0x359f8): 사운드(비트 0x2면 Off), 타이핑(0x10이면 Off), 효과(0x800000이면 보통, 아니면 최상), 속도(비트 8..15, 0..4, 표시 +1).
static void settings(uint32_t flags, int cur) {
    uint32_t names = G(G_CFG_NAMES), vals = G(G_CFG_VALS);
    ui_list("cfg");
    for (int i = 0; i < 4; i++) {
        ui_item();
        ui_str("name", i < 3 ? gstr(R32_(&cpu, names + 4 * i)) : gs(G_SPEED));
        int v;
        if (i == 0) v = flags & 0x2 ? 0 : 1;
        else if (i == 1) v = flags & 0x10 ? 0 : 1;
        else if (i == 2) v = flags & 0x800000 ? 2 : 3;
        else v = -1;
        if (v >= 0) ui_str("val", gstr(R32_(&cpu, vals + 4 * v)));
        else {
            ui_fmt("val", "%d", speed(flags) + 1);
        }
        // 값 단계(게이지 점): 켜짐/끔과 효과는 두 칸, 속도는 다섯 칸
        ui_int("lv", i == 3 ? speed(flags) : i == 2 ? v == 3 : v);
        ui_int("lvmax", i == 3 ? 4 : 1);
        ui_text("act", "");
        ui_bool("sel", i == cur);
        ui_close();
    }
    // 원작에 없는 줄(restored/cfgx.c: 화면, 일러스트 = 값 줄, 조작 설정·기타 설정·세이브 데이터 = 여는 줄): 원작 네 줄 아래의 가상 줄 4~.
    // 키는 f_34f48이 cfgx_key로 받는다.
    for (int i = 0; i < cfgx_count(); i++) {
        const char *on_t, *off_t;
        int on = cfgx_value(i, &on_t, &off_t), act = cfgx_action(i)[0] != 0;
        ui_item();
        ui_text("name", cfgx_name(i));
        ui_text("val", act ? "열기" : on ? on_t : off_t);
        ui_int("lv", on);
        ui_int("lvmax", act ? 0 : 1);
        ui_text("act", cfgx_action(i));
        ui_bool("sel", cur == 4 + i);
        ui_close();
    }
    ui_close();
}

// 도움말은 타이틀 메뉴의 도움말과 같은 것을 본다(restored/help.c).
static void help(uint32_t game) { (void)game; help_emit(HELP_MAIN); }

static void common(uint32_t scr, uint32_t menu, uint32_t game) {
    (void)scr;
    ui_bool("narrow", ui_screen_w() < 400);
    // 빠듯한 폭(system.rml body.tight): 오른쪽 패널이 약 260dp보다 좁다. 틀 안(모드 1)은 레일·옵션 목록이 약 276dp, 밖은 목록 150dp를 차지한다.
    int sw = ui_screen_w();
    ui_bool("tight", sw >= 400 && sw < (R32_(&cpu, menu + M_MODE) == 1 ? 540 : 440));
    ui_int("mode", (int)R32_(&cpu, menu + M_MODE));
    ui_str("t_opt", gs(G_OPT_T));
    ui_str("t_sl", gs(G_SL_T));
    ui_str("t_cfg", gs(G_CFG_T));
    ui_str("t_help", gs(G_HELP_T));
    ui_str("t_save", gs(G_SAVE));
    ui_str("t_load", gs(G_LOAD));
    ui_str("t_total", gs(G_TOTAL));
    // 총 플레이 시간 = game+0x150 + (지금 - game+0x24) (0x356e8과 같은 식)
    uint32_t now = CALL(FN_NOW);
    hms("total", R32_(&cpu, game + G_PLAYED) + (now - R32_(&cpu, game + G_START)));
}

static uint64_t mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static uint64_t prem_seen;
static int prem_sel; // 옵션 목록의 맨 아래 「프리미엄 상점」 줄(원본 커서 4 아래, 원본에는 없는 가상 줄)에 커서가 있다
static int split; // 옵션 목록의 원본 칸 1이 저장(0)·불러오기(1) 중 어느 줄인지(아래 f_34f48)

// system 문서 한 장을 채운다. 하위 화면이 무엇이든 키 모양은 같게 채운다.
static void doc(uint32_t scr, uint32_t menu, uint32_t game, int sub, int stage, int dlg, int cur, int yn) {
    ui_begin("system");
    fm_doc_frame(menu); // 모드 1이면 필드 메뉴 틀 안(머리 띠 없이 목록 + 패널, 아래 고정 버튼 띠). 타이틀 쪽(모드 2·3)은 그대로
    common(scr, menu, game);
    ui_int("sub", sub);
    ui_bool("illust", ui_illust());
    ui_int("stage", stage);
    bool save = stage == 1;
    ui_bool("save", save);
    // 옵션 목록(원본 0x35470): 고정 16바이트 글 4개. 하위 화면에서는 들어온 항목(+0x1c)을 흐린 선택으로 둔다.
    // 종료 확인(4)은 목록 위에 뜨고 +0x20은 4로 남는다.
    int lcur = sub == 0 || sub == 4 ? cur : (int)R32_(&cpu, scr + S_LISTCUR);
    bool prem = R32_(&cpu, menu + M_MODE) == 1; // 필드 메뉴의 옵션에만 프리미엄 상점 줄을 둔다(타이틀 이어하기·이벤트 저장에는 없다)
    // 목록은 다섯 줄: 원본 칸 1(저장/불러오기)을 저장·불러오기 두 줄로 나눈다(아래 f_34f48). v = 새 목록의 줄.
    // 하위 화면에서는 들어온 쪽(저장 슬롯이면 저장 줄)을 흐린 선택으로 둔다.
    int part = sub == 1 && stage != 0 ? stage == 2 : split;
    int v = lcur < 1 ? -1 : lcur == 1 ? part : lcur;
    if (prem && prem_sel && (sub == 0 || sub == 4)) v = 5;
    ui_int("lcur", v);
    const char *names[5] = {gs(G_SAVE), gs(G_LOAD), gstr(G(G_LIST) + 16), gstr(G(G_LIST) + 32), gstr(G(G_LIST) + 48)};
    if (v == 5) ui_text("lname", "프리미엄 상점");
    else ui_str("lname", v >= 0 && v < 5 ? names[v] : "");
    ui_list("items");
    for (int i = 0; i < 5; i++) { ui_item(); ui_str("name", names[i]); ui_bool("dis", i == 0 && !(!(R32_(&cpu, game + G_FLAGS) & 0x20000) && (R32_(&cpu, game + G_FLAGS) & 0x1000))); ui_close(); }
    if (prem) { ui_item(); ui_text("name", "프리미엄 상점"); ui_bool("dis", false); ui_close(); }
    ui_close();
    ui_int("cur", cur);
    // 저장 가능 여부 0x12afc: game+0x20의 0x20000이면 불가, 아니면 0x1000
    uint32_t gf = R32_(&cpu, game + G_FLAGS);
    ui_bool("can_save", !(gf & 0x20000) && (gf & 0x1000));
    slots(game);
    // 환경설정 화면은 편집 사본(+0x30), 목록의 미리보기는 지금 적용된 값(game+0x14c)
    if (sub != 2) cfgx_reset();
    int ccur = sub == 2 ? (cfgx_sel() >= 0 ? 4 + cfgx_sel() : cur) : -1;
    settings(sub == 2 ? R32_(&cpu, scr + S_CFG) : R32_(&cpu, game + 0x14c), ccur);
    help(game);

    // 대화: 0 없음, 1 예/아니오, 2 메시지(0x356e8 표 G-0x2d38, 종료 확인은 0x35470 끝)
    int pop = 0, warn = 0;
    const char *msg = "";
    if (sub == 4) { pop = 1; msg = gs(G_QUIT_Q); }
    else if (sub == 1 && stage != 0) {
        switch (dlg) {
        case 1: pop = 1; msg = gs(save ? G_SAVE_Q : G_LOAD_Q); break;
        case 2: case 3: pop = 2; msg = gs(G_SAVING); break; // 3은 저장을 실행하는 프레임(원본은 빈 상자)
        case 5: if (save) { pop = 2; msg = gs(G_SAVED); } break;
        case 6: pop = 2; warn = 1; msg = gs(G_INIT_FAIL); break;
        case 7: pop = 2; warn = 1; msg = gs(G_NO_SPACE); break;
        case 9: pop = 2; warn = 1; msg = gs(G_BAD_VER); break;
        }
    }
    // 저장/불러오기 고르기와 슬롯은 새 배치에서 가로로 놓인다(좁은 화면의 슬롯만 세로). 원본은 위아래로 고르므로
    // 좌우 키도 위아래로 받는다. 예/아니오 대화(pop 1)는 원본이 좌우로 고르므로 바꾸지 않는다.
    if (pop == 0 && sub == 1) ui_key_alias_h2v();
    // 터치(nav의 prev/next는 별칭 이전의 원본 키): 옵션 목록(0x350f0)은 위아래(순환), 저장/불러오기 고르기(0x352c8)는 위아래 0↔1,
    // 슬롯(0x35338)은 위아래 0..2(끝에서 멈춤)이고 대화 중에는 좌우 예/아니오. 환경설정(0x35188)은 위아래 행(순환)에 좌우 값 조절,
    // OK는 적용하고 목록으로 나가므로 행을 다시 눌러도 OK를 보내지 않는다(ACT 없음: 값은 ◀ ▶ 버튼으로 바꾸고 확인은 키 안내 버튼으로).
    if (pop == 1) ui_nav("yn", yn, 2, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_ACT);
    else if (pop == 0) {
        if (sub == 0) ui_nav("items", v, prem ? 6 : 5, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_WRAP | UI_NAV_ACT | UI_NAV_VERT | UI_NAV_DIRECT);
        else if (sub == 1 && stage == 0) ui_nav("sl", cur, 2, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | UI_NAV_HORZ);
        else if (sub == 1 && dlg == 0) ui_nav("slot", cur, 3, UI_KEY_UP, UI_KEY_DOWN, UI_NAV_ACT | (ui_screen_w() < 400 ? UI_NAV_VERT : UI_NAV_HORZ));
        else if (sub == 2) ui_nav("cfg", ccur, 4 + cfgx_count(), UI_KEY_UP, UI_KEY_DOWN, UI_NAV_WRAP | UI_NAV_VERT);
    }
    ui_int("pop", pop);
    ui_str("msg", msg);
    ui_str("yes", gstr(R32_(&cpu, G(G_YESNO))));
    ui_str("no", gstr(R32_(&cpu, G(G_YESNO) + 4)));
    ui_int("yn", yn);
    ui_bool("warn", warn);
    // 틀의 고정 버튼 띠: 「결정」은 지금 고르는 것(목록 항목, 슬롯 저장·불러오기, 예/아니오)을 정한다
    fm_bar_begin();
    fm_bar_hint(pop == 1 ? "←→ 선택" : pop ? "" : sub == 0 ? "↑↓ 선택" : sub == 1 && stage != 0 ? "←→ 슬롯" : sub == 2 ? "↑↓ 항목   ←→ 값" : sub == 3 ? "←→ 항목   ↑↓ 넘기기" : "");
    if (pop == 1) fm_bar_ok("확인", 1);
    else if (pop == 2) fm_bar_ok("확인", 0);
    else if (sub == 1 && stage != 0) fm_bar_ok(save ? "저장" : "불러오기", 1);
    else if (sub == 2) fm_bar_ok(cfgx_sel() >= 0 && cfgx_action(cfgx_sel())[0] ? "열기" : "확인", 1);
    else if (sub == 3) fm_bar_ok("결정", 0);
    else fm_bar_ok(v == 5 ? "열기" : "선택", 1);
    fm_bar_emit(menu);
    ui_end();
}

// 원본 0x2faa4는 타이틀 모드(2)에서 바탕을 그리지 않고, 장면 14는 앞 프레임(타이틀 메뉴)을 그대로 둔다.
// 원본은 슬롯 상자 3개가 타이틀 메뉴의 「이어하기/새게임」 줄을 덮었는데, 슬롯 상자를 그리지 않으면 그 줄이 비쳐 남는다.
// 타이틀 바탕은 세로로만 바뀌는(줄마다 한 색) 그라데이션이라, 창 자리 둘레의 줄을 같은 줄 왼쪽 끝(x=8, 장식 없는 곳)의 색으로 다시 칠해 지운다.
static void title_clean(uint32_t menu) {
    uint32_t hnd = gfx_screen_handle();
    if (!hnd) return;
    uint32_t f = handle_data(hnd);
    int w = (int)R32_(&cpu, f), h = (int)R32_(&cpu, f + 4), st = (int)R32_(&cpu, f + 8) / 2;
    uint16_t *px = (uint16_t *)(cpu.m + handle_data(R32_(&cpu, f + 16)));
    int wx = (int)R32_(&cpu, menu + M_X), wy = (int)R32_(&cpu, menu + M_Y), ww = (int)R32_(&cpu, menu + M_W);
    int x0 = wx - 24, x1 = wx + ww + 24, y0 = wy - 8, y1 = wy + 3 * 39 + 8;
    if (x0 < 12) x0 = 12;
    if (x1 > w - 12) x1 = w - 12;
    if (y0 < 0) y0 = 0;
    if (y1 > h - 30) y1 = h - 30;
    for (int y = y0; y < y1; y++) {
        uint16_t c = px[y * st + 8];
        for (int x = x0; x < x1; x++) px[y * st + x] = c;
    }
}

// 예/아니오 상자 0x37c08(font, x, y, h, [sp]질문, [sp+4]종류, [sp+8]선택)는 화면 7(0x35470, 0x356e8)만 부른다.
// 화면 7 안에서는 system 문서가 상태(+0x28)로 그리므로 원본은 덧그림 화면에서 버린다. 다른 곳에서 불리면 sys_yesno 문서로 띄운다.
static int in_system;

void orig_f_37c08(cpu_t *c);
void f_37c08(cpu_t *c) {
    if (!ui_modern() || in_system) { orig_f_37c08(c); return; }
    uint32_t q = arg(c, 4), kind = arg(c, 5), sel = arg(c, 6);
    uint32_t pair = G(G_YESNO) + 8 * kind;
    ui_scratch_begin();
    orig_f_37c08(c);
    ui_scratch_end();
    ui_begin("sys_yesno");
    ui_bool("narrow", ui_screen_w() < 400);
    ui_str("q", gstr(q));
    ui_str("yes", gstr(R32_(&cpu, pair)));
    ui_str("no", gstr(R32_(&cpu, pair + 4)));
    ui_int("sel", (int)sel);
    // 터치: 예/아니오는 좌우로 고른다(이 상자를 부르는 쪽이 받는 키는 화면 7의 예/아니오와 같다고 본다. 지금 부르는 곳이 없어 시험은 못 했다).
    ui_nav("yn", (int)sel, 2, UI_KEY_LEFT, UI_KEY_RIGHT, UI_NAV_ACT);
    ui_end();
}

// 화면 7 그리기(vtable+4). 도움말(+0xc==3)은 분배기가 부르지 않으므로 아래 키 처리기 쪽에서 그린다.
void orig_f_35440(cpu_t *c);
void f_35440(cpu_t *c) {
    if (!ui_modern()) { orig_f_35440(c); return; }
    uint32_t scr = c->r[0], menu = R32(scr + S_MENU), game = R32(menu + M_GAME);
    int sub = (int)R32(scr + S_SUB), stage = (int)R32(scr + S_STAGE), dlg = (int)R32(scr + S_DLG);
    int mode = (int)R32(menu + M_MODE);
    // 불러오기 실행(대화 4): 원본은 화면을 검게 지우고 메뉴를 닫는다. 그 지우기는 그대로 살린다(이어지는 페이드가 검은 화면에서 시작).
    if (sub == 1 && stage != 0 && dlg == 4) { orig_f_35440(c); return; }
    int yn = (int)R32(scr + S_YN), cur = (int)R32(scr + S_CUR);
    in_system++;
    ui_scratch_begin();
    orig_f_35440(c);
    ui_scratch_end();
    in_system--;
    if (mode == 2) title_clean(menu);
    if (R32(menu + M_CLOSE)) return; // 이번 틱에 닫혔다
    if (ext_shop_active()) { ext_shop_doc(menu); return; } // 프리미엄 상점(restored/ui_premium.c)이 옵션 화면 위를 차지한다

    doc(scr, menu, game, sub, stage, dlg, cur, yn);
}

// ---- 저장과 불러오기를 나눈 옵션 목록(사용자 결정 2026-11-10) ----
// 원본 옵션 목록은 「저장/불러오기」 한 칸(커서 1)에서 들어가 저장·불러오기를 고르고(단계 0), 슬롯을 고르고, 예/아니오를 묻는다.
// 헷갈린다는 의견에 따라 새 UI에서는 목록을 「저장」「불러오기」「환경설정」「도움말」「게임종료」 다섯 줄로 보이고,
// 고르면 바로 그 슬롯 화면으로, 슬롯을 고르면 묻지 않고 바로 저장·불러오기를 한다. 상태는 여전히 원본이 바꾼다:
// 키 처리기를 감싸 원본 키를 덧보낸다(고르기 단계의 OK, 예/아니오의 예). 키보드·패드·터치 모두 같은 길이다.
// split(위에 선언): 원본 커서 1(저장/불러오기 칸)이 새 목록의 어느 줄인지(0 저장, 1 불러오기).
enum { K_UP = 0x1f, K_DOWN = 0x1e, K_LEFT = 0x1d, K_RIGHT = 0x1c, K_OK = 0x1b, K_CLR = 0x10, K_2 = 0x52, K_4 = 0x54, K_6 = 0x56, K_8 = 0x58, K_5 = 0x55,
       PICK_EXIT = 9 }; // 문서의 pick="opt" idx: 0~4 목록 줄, 9 옵션 나가기

void orig_f_34f48(cpu_t *c);

// 이번 틱에 누른 키(내부 코드, 0x61d4와 같은 판정)
static int key_now(uint32_t in) { return R32_(&cpu, in + 0xc) && R16_(&cpu, in + 2) == 1 ? (int)R16_(&cpu, in) : 0; }

// 원본 키 처리기를 키 code로 한 번 부른다(code 0이면 키 없이). 입력 객체는 되돌린다.
static void call_keys(cpu_t *c, uint32_t scr, uint32_t in, int code) {
    uint16_t k0 = R16_(&cpu, in), k2 = R16_(&cpu, in + 2);
    uint32_t kc = R32_(&cpu, in + 0xc);
    if (code) { W16_(&cpu, in, (uint32_t)code); W16_(&cpu, in + 2, 1); W32_(&cpu, in + 0xc, 1); }
    else W16_(&cpu, in + 2, 0);
    c->r[0] = scr; c->r[1] = in;
    orig_f_34f48(c);
    W16_(&cpu, in, k0); W16_(&cpu, in + 2, k2); W32_(&cpu, in + 0xc, kc);
}

// ---- 하위 화면 깊이 줄이기(사용자 결정 2026-11-10) ----
// 옵션의 하위 화면(저장·불러오기 슬롯, 환경설정, 도움말)에 "확인/이전" 단계가 필요 없게: 왼쪽 목록은 어느 하위 화면에서든
// 바로 누를 수 있고(pick), 환경설정 값은 바꾸는 즉시 적용·저장한다. 원본 상태 전환은 그대로 두고 원본 키를 덧보낸다.

// 하위 화면에서 목록(하위 0)으로. 슬롯은 CLR 두 번(슬롯 → 고르기 단계 → 목록), 그 밖은 한 번.
static void to_list(cpu_t *c, uint32_t scr, uint32_t in) {
    for (int i = 0; i < 3 && R32_(&cpu, scr + S_SUB) != 0; i++) call_keys(c, scr, in, K_CLR);
}
// 목록에서 새 목록의 줄 v(0 저장, 1 불러오기, 2 환경설정, 3 도움말, 4 게임종료)로 들어간다.
static void enter(cpu_t *c, uint32_t scr, uint32_t in, int v) {
    W32_(&cpu, scr + S_CUR, (uint32_t)(v <= 1 ? 1 : v));
    if (v <= 1) split = v;
    call_keys(c, scr, in, K_OK);
    if (v <= 1 && R32_(&cpu, scr + S_SUB) == 1 && R32_(&cpu, scr + S_STAGE) == 0) {
        W32_(&cpu, scr + S_CUR, (uint32_t)v);
        call_keys(c, scr, in, K_OK);
        if (R32_(&cpu, scr + S_SUB) == 1 && R32_(&cpu, scr + S_STAGE) == 0) call_keys(c, scr, in, K_CLR); // 저장할 수 없는 곳
    }
}

// 도움말로 들어간 틱: 원본은 이 틱에 그리기(0x35440)를 이미 지났고 도움말을 그리는 키 처리기도 다음 틱부터 그리므로,
// 이 틱에는 system 문서를 채우는 곳이 없다. 문서가 한 틱 닫혔다 다시 열리면 여는 움직임(.pop)이 다시 돌아 도움말에 들어갈 때마다
// 창이 깜빡이듯 떠올랐다(저장·불러오기·환경설정은 다음 틱에 그리기가 불려 끊기지 않는다). 그래서 이 틱에 문서를 채워 이어 준다.
static void help_enter_tick(uint32_t scr, uint32_t menu, uint32_t game) {
    if (R32_(&cpu, scr + S_SUB) != 3 || R32_(&cpu, menu + M_CLOSE)) return;
    doc(scr, menu, game, 3, 0, 0, 0, 0);
}

// 화면 7 키 처리(vtable+8). 도움말(+0xc==3)일 때는 이것이 매 틱 창과 글(0x10258)을 그리므로 덧그림 화면에서 부르고 문서를 채운다.
void f_34f48(cpu_t *c) {
    uint32_t scr = c->r[0], in = c->r[1];
    if (!ui_modern()) { orig_f_34f48(c); return; }
    uint32_t menu = R32(scr + S_MENU), game = R32(menu + M_GAME);
    int sub = (int)R32(scr + S_SUB), stage = (int)R32(scr + S_STAGE), cur = (int)R32(scr + S_CUR), dlg = (int)R32(scr + S_DLG);
    int mode = (int)R32(menu + M_MODE), k = key_now(in);
    bool up = k == K_UP || k == K_2, down = k == K_DOWN || k == K_8, ok = k == K_OK || k == K_5, clr = k == K_CLR;
    bool lr = k == K_LEFT || k == K_RIGHT || k == K_4 || k == K_6;

    // 프리미엄 상점: 떠 있는 동안 키와 터치는 상점이 받는다(원본에는 키 없이 한 틱만 돌려 상태를 유지한다).
    if (ext_shop_active()) { prem_seen = mono_ms(); ext_shop_input(in); call_keys(c, scr, in, 0); return; }

    // 왼쪽 목록을 눌렀다(터치·마우스). 대화(예/아니오, 메시지)나 종료 확인 중에는 받지 않는다.
    int pk = mode == 1 ? ui_pick("opt") : -1;
    // 목록 줄(nav "items", UI_NAV_DIRECT): 키를 한 칸씩 흉내 내지 않고 누른 줄로 바로 들어간다(아래 pick과 같은 길).
    // 전에는 원본 위아래 키로 옮겼는데, 가상 줄(프리미엄)과 저장/불러오기 나눔 사이에서 커서가 되돌아 돌아 목표에 닿지 못했다.
    if (pk < 0) { int nk = ui_pick("items"); if (nk >= 0) pk = nk; }
    // 게임종료 확인(하위 4) 중에 왼쪽 목록(프리미엄 줄 포함)을 누르면 확인을 취소하고(CLR: 원본은 목록으로) 그 줄로 간다.
    // 틀 안에서는 확인 막이 내용 자리만 덮어 목록이 보이므로, 보이는 줄은 바로 눌려야 한다(사용자 결정 2026-11-11).
    if (pk >= 0 && sub == 4 && dlg == 0) { call_keys(c, scr, in, K_CLR); sub = (int)R32_(&cpu, scr + S_SUB); stage = (int)R32_(&cpu, scr + S_STAGE); }
    if (pk >= 0 && pk != 5) prem_sel = 0;
    // 프리미엄 줄(가상 줄 5)은 원본 커서로 들어갈 수 없다. 다른 하위 화면(환경설정·슬롯 등)이 열려 있으면 먼저 목록으로 돌린 뒤 연다.
    // 전에는 목록 상태(sub 0)에서만 받아서, 하위 화면에서 누르면 아래 enter()가 원본 커서에 5를 써 빈 패널에서 멈췄다.
    if (pk == 5 && sub != 4 && dlg == 0) {
        if (sub != 0) to_list(c, scr, in);
        if (R32_(&cpu, scr + S_SUB) == 0) { prem_sel = 1; ext_shop_open(); }
        call_keys(c, scr, in, 0);
        return;
    }
    if (pk == 5) pk = -1; // 종료 확인·대화 중: 원본 커서에 없는 줄이므로 아래 일반 길로 보내지 않는다
    if (pk >= 0 && sub != 4 && dlg == 0) {
        int now = sub == 1 ? (stage == 2) : sub;
        if (pk == PICK_EXIT) { to_list(c, scr, in); call_keys(c, scr, in, K_CLR); return; }
        if (sub != 0 && now == pk) { call_keys(c, scr, in, 0); return; }
        if (sub != 0) to_list(c, scr, in);
        if (R32_(&cpu, scr + S_SUB) == 0) enter(c, scr, in, pk);
        help_enter_tick(scr, menu, game);
        return;
    }
    // 환경설정의 원작에 없는 줄(restored/cfgx.c): 원본 줄 0~3 아래의 가상 줄. 0 → … → 3 → 가상 줄들 → 0으로 돈다.
    // 그 줄에 있는 동안 위아래·좌우·OK(여는 줄)는 원본에 보내지 않는다(키 없이 한 틱). 값 줄의 OK와 CLR(나가기)은 원본이 받는다.
    if (sub != 2 || dlg != 0) cfgx_reset();
    else {
        int set;
        if (cfgx_key(up, down, lr, ok, cur, 3, &set)) {
            if (set >= 0) W32_(&cpu, scr + S_CUR, (uint32_t)set);
            call_keys(c, scr, in, 0);
            return;
        }
        if (ok || clr) cfgx_reset();
    }
    // 환경설정: 값을 바꾸면 바로 적용·저장한다(원본은 OK로 적용·저장하고 목록으로 나간다. 다시 들어가 같은 줄로).
    if (mode == 1 && sub == 2) {
        uint32_t before = R32(scr + S_CFG);
        orig_f_34f48(c);
        if (lr && R32_(&cpu, scr + S_SUB) == 2 && R32_(&cpu, scr + S_CFG) != before) {
            call_keys(c, scr, in, K_OK);
            if (R32_(&cpu, scr + S_SUB) == 0) { W32_(&cpu, scr + S_CUR, 2); call_keys(c, scr, in, K_OK); W32_(&cpu, scr + S_CUR, (uint32_t)cur); }
        }
        return;
    }

    if (sub == 3) { // 도움말: 좌우는 항목, 위아래는 글 상자 넘기기(원본의 쪽 넘김은 쓰지 않는다)
        bool left = k == K_LEFT || k == K_4, right = k == K_RIGHT || k == K_6;
        help_input(HELP_MAIN, left ? -1 : right ? 1 : 0, up ? -1 : down ? 1 : 0);
        ui_scratch_begin();
        if (left || right || up || down) call_keys(c, scr, in, 0); else orig_f_34f48(c);
        ui_scratch_end();
        if (R32_(&cpu, scr + S_SUB) != 3 || R32_(&cpu, menu + M_CLOSE)) return; // CLR로 목록에 돌아갔거나 #으로 닫혔다
        doc(scr, menu, game, 3, 0, 0, 0, 0);
        return;
    }
    if (mode == 1 && sub == 0) {
        // 프리미엄 상점 줄(가상): 원본 커서 4(게임종료) 아래. 위 끝(저장)에서 위로 가면 이 줄로 돈다.
        if (mono_ms() - prem_seen > 400) prem_sel = 0; // 옵션 화면을 떠났다 다시 열면(키 처리가 한참 끊김) 프리미엄 줄 선택은 잊는다
        prem_seen = mono_ms();
        if (prem_sel) {
            if (ok) ext_shop_open();
            else if (clr) { prem_sel = 0; orig_f_34f48(c); return; }
            else if (up) { prem_sel = 0; W32_(&cpu, scr + S_CUR, 4); }   // 게임종료 줄로. 터치로 상점에 들어왔으면 원본 커서가 딴 줄이라 4로 맞춘다
            else if (down) { prem_sel = 0; W32_(&cpu, scr + S_CUR, 1); split = 0; } // 맨 위(저장)로 돈다
            call_keys(c, scr, in, 0);
            return;
        }
        if ((cur == 4 && down) || (cur == 1 && split == 0 && up)) { prem_sel = 1; call_keys(c, scr, in, 0); return; }
        // 새 목록 안에서 저장↔불러오기 사이를 오가는 키는 원본에 보내지 않는다(원본 커서는 1 그대로).
        if (cur == 1 && ((up && split == 1) || (down && split == 0))) { split ^= 1; call_keys(c, scr, in, 0); return; }
        orig_f_34f48(c);
        int now = (int)R32_(&cpu, scr + S_CUR);
        if (now == 1 && cur != 1) split = up ? 1 : 0; // 아래(환경설정)에서 올라오면 불러오기, 위(게임종료)에서 돌아오면 저장
        // 저장/불러오기 칸에서 OK: 원본은 고르기 단계(0)로 간다. 그 자리에서 고른 쪽으로 OK를 덧보내 바로 슬롯 화면으로.
        if (ok && cur == 1 && R32_(&cpu, scr + S_SUB) == 1 && R32_(&cpu, scr + S_STAGE) == 0) {
            W32_(&cpu, scr + S_CUR, (uint32_t)split);
            call_keys(c, scr, in, K_OK);
            // 저장할 수 없는 곳이면 원본이 고르기 단계에 머문다. 고르기 화면을 보이지 않게 목록으로 돌아간다.
            if (R32_(&cpu, scr + S_SUB) == 1 && R32_(&cpu, scr + S_STAGE) == 0) call_keys(c, scr, in, K_CLR);
        }
        help_enter_tick(scr, menu, game);
        return;
    }
    if (sub == 1 && stage != 0 && dlg == 0) {
        orig_f_34f48(c);
        // 슬롯에서 OK: 원본은 예/아니오(대화 1)를 묻는다. 예로 바로 넘긴다.
        if (ok && R32_(&cpu, scr + S_DLG) == 1) { W32_(&cpu, scr + S_YN, 0); call_keys(c, scr, in, K_OK); }
        // 슬롯에서 CLR: 원본은 고르기 단계로 돌아간다. 고르기 화면은 없으므로 목록까지(그 줄에 커서).
        if (clr && mode == 1 && R32_(&cpu, scr + S_SUB) == 1 && R32_(&cpu, scr + S_STAGE) == 0) {
            split = stage == 2;
            call_keys(c, scr, in, K_CLR);
        }
        return;
    }
    orig_f_34f48(c);
}
