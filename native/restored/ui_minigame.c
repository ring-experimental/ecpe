// 미니게임 트리플 슬롯(장면 15, 타이틀 '게임시작 › 트리플슬롯'과 필드 카지노 '슬롯머신')을 넓은 화면 문서(ui/slot.rml)로 그린다.
// 주소는 파일 오프셋(게스트 = +0x100000). 사용자 요청 2026-11-12.
//
// ---- 원본 구조(조사) ----
//  장면 15 틱 0x10af0: 입력 0x61d4로 이 틱의 키를 받아 미니게임 객체 S = [W+0x1d8]로 분배 0x26db8(S, 키)을 부른다(매 틱).
//  0x26db8은 상태 S+0x14로 갈라진다(표 0x48e88). 0xb~0x10은 벼베기(저들의영웅), 슬롯은 아래 여섯 상태다. 각 상태 함수가 키 처리와 그리기를 함께 한다.
//    0x15 베팅 0x271f8   ↑(2) 베팅 1000(최대, 코인 넘지 않게), ↓(8) 1(최소), →(6) +1, ←(4) −1(1 아래로 안 감),
//                        OK(5) 베팅이 있으면 코인에서 빼고 0x16. CLR → 0x19, *·# → 0x18(돌아올 상태 S+0x18).
//    0x16 회전   0x272a4   1/2/3(0x51~0x53) 그 릴 멈춤(0x267b4), 매 틱 릴 진행(0x26860, 0x26820). 셋 다 서면 배당 0x2694c:
//                        경품 표지(M+0x14)가 있고 필드에서 연 것이면 0x1a, 아니면 0x17. CLR → 0x19, *·# → 0x18.
//    0x17 결과   0x27400   WIN(시트 S+0x40 프레임 8) 또는 BAD(9)를 릴 위에. OK → 코인 += 배당, 베팅 = min(베팅, 코인), 0x15.
//    0x18 도움말 0x2757c → 0x274c8   배당표 글(GOT 0x3b8)을 쪽 넘기기 0x10294로 5줄씩(S+0x30 쪽), ←→ 쪽, CLR·*·# 돌아가기.
//    0x19 종료   0x275f0   「끝내시겠습니까?」(GOT 0x3bc). OK → 끝(필드면 코인을 파티 +0xa8+4에 돌려놓는다), CLR 돌아가기.
//    0x1a 경품   0x27328   필드 카지노에서 7·7·7. 받은 횟수([파티 +0xa8+0xc])로 도신의 동전/주사위/카드, 그 뒤는 미라클 포션. OK → 0x17.
//  패널 그리기 0x27bc8는 S+0x10(다시 그림 표지, 0x26e3c가 켠다)이 1일 때만 창 0x28030 + 본문 0x27bf0을 그린다.
//  모델 M = S+0xc: +0..2 릴 위치(16칸 띠 GOT 0x384/0x38c/0x388의 번호), +3..5 릴 단계(0 회전, 1~9 서는 중, 10 섬),
//    +8 코인, +0xc 배당(결과), +0x10 베팅, +0x14 경품 표지.
//  릴 칸 그리기 0x28114(S, x, y, 33, 42, 기호): 기호 0 겐나드(시트 S+0x48 프레임 0), 8 「7」(S+0x40 프레임 7),
//    1~7 UI 시트(메뉴 +0x14) 프레임 GOT 0x418 표(54, 57, 56, 52, 53, 51, 55: 직업 순서의 작은 초상 51~57).
//    서는 중 단계 3/5/6/8은 칸을 15/10/20/5 내려 그리고 다음 기호를 30 위에 함께 그린다(릴이 굴러 멈추는 모습).
//  배당 0x2697c: 세 기호를 큰 순으로 늘어놓은 세 자리 수로 판정. 쌍은 (1 라즈파샤, 2 자이언트) (3 이오, 4 니하트) (5 제레미, 6 아크),
//    7 위지윅, 8 「7」, 0 겐나드. 도움말 글의 머리글자(라 자 이 니 제 아 위 7 ?)가 이 기호다.
//
// ---- 새 UI ----
//  원본 분배를 그대로 부르되 그림은 화면 밖(scratch)에 버리고, 상태를 읽어 문서를 매 틱 채운다(키 처리·소리·코인 계산은 원본).
//  원작은 240폭 작은 창 하나에 모든 것을 넣고 배당표는 2쪽 도움말로 따로 보였다. 넓은 화면에서는 기계(코인·릴·베팅·결과)를
//  왼쪽에, 배당표를 오른쪽에 늘 보인다(도움말 상태는 그 표를 앞으로 꺼낸다). 좁으면 원작처럼 도움말 상태에서만 표를 띄운다.
//  모든 원본 키는 문서의 버튼으로도 누른다(릴 아래 멈춤 1/2/3, 베팅 최소·−1·+1·최대, 돌리기/계속, 도움말, 나가기).
//  릴 그림은 원본 칸 그리기와 같은 시트·프레임을 scratch에 그려 떼고, 일러스트 HD가 켜져 있으면 인물(파티 7명, 겐나드)을 리마스터 그림으로 바꾼다.
#include "restored.h"
#include "gamepad.h"
#include <string.h>
#include <stdio.h>

enum {
    S_W = 0x00, S_MODEL = 0x0c, S_STATE = 0x14, S_RET = 0x18, S_PAGE = 0x30, S_SHEET = 0x40, S_GENNAD = 0x48,
    W_MENU = 0x1d4, W_SCENE = 0x0c, W_MINIGAME = 0x1d8, SCENE_SLOT = 15, MENU_SHEET = 0x14, W_SCENE_RET = 0x10, W_PARTY = 0x1c0, PARTY_CASINO = 0xa8,
    M_POS = 0x00, M_PHASE = 0x03, M_COIN = 0x08, M_WIN = 0x0c, M_BET = 0x10,
    ST_BET = 0x15, ST_SPIN = 0x16, ST_RESULT = 0x17, ST_HELP = 0x18, ST_EXIT = 0x19, ST_PRIZE = 0x1a,
    GOT_STRIP0 = 0x384, GOT_STRIP2 = 0x388, GOT_STRIP1 = 0x38c, GOT_HELP = 0x3b8, GOT_ASK = 0x3bc, GOT_TITLE = 0x40c,
    GOT_SYMFRAME = 0x418, GOT_COIN = 0x3e4,
    FN_SPR = 0x1056ec,      // (시트, 프레임, x, y, 0, 0, 뒤집기)
    BET_MAX = 1000, PHASE_STOP = 10, NSYM = 9,
};

static inline uint32_t rd(uint32_t a) { return R32_(&cpu, a); }
static inline uint32_t got(uint32_t off) { return rd(GOT_ADDR(off)); }

static bool slot_state(int s) { return s == ST_BET || s == ST_SPIN || s == ST_RESULT || s == ST_PRIZE; }
static bool is_slot(uint32_t S) {
    int s = (int)rd(S + S_STATE);
    if (slot_state(s)) return true;
    return (s == ST_HELP || s == ST_EXIT) && slot_state((int)rd(S + S_RET));
}

// 기호 → 리마스터 일러스트 번호(ui.h: 파티 0~6 = 아크 니하트 제레미 자이언트 이오 라즈파샤 위지윅, 9 겐나드). 「7」은 없다.
static const int SYM_ILLUST[NSYM] = {9, 5, 3, 4, 1, 2, 0, 6, -1};

// 기호 하나를 원본 칸 그리기(0x28114)와 같은 시트·프레임으로 scratch에 그려 뗀다. 키에 그림, 키_dw·키_dh에 문서 크기(dp)를 넣는다.
// 도트 그림(키_hd 0)은 문서가 키_w·키_h에 배율을 곱한다(릴 2배, 배당표 1/2배). 일러스트 HD가 켜져 있으면(키_hd 1) 인물은 리마스터 그림을
// bw×bh 칸에 꽉 차게 키_dw·키_dh로 준다(넘치는 쪽은 칸이 자른다).
static void sym_sprite(uint32_t S, int sym, const char *key, int bw, int bh) {
    char kw[32], kh[32], khd[32];
    snprintf(kw, sizeof kw, "%s_dw", key); snprintf(kh, sizeof kh, "%s_dh", key); snprintf(khd, sizeof khd, "%s_hd", key);
    int id = sym >= 0 && sym < NSYM ? SYM_ILLUST[sym] : -1, w, h;
    if (id >= 0 && ui_illust() && ui_illust_size(id, &w, &h) && w > 0 && h > 0 && ui_illust_image(key, id, 0)) {
        float sc = (float)bw / w > (float)bh / h ? (float)bw / w : (float)bh / h;
        ui_int(kw, (int)(w * sc + 0.5f)); ui_int(kh, (int)(h * sc + 0.5f)); ui_int(khd, 1);
        return;
    }
    ui_int(kw, 0); ui_int(kh, 0); ui_int(khd, 0);
    if (sym < 0 || sym >= NSYM) {
        snprintf(kw, sizeof kw, "%s_w", key); snprintf(kh, sizeof kh, "%s_h", key);
        ui_text(key, ""); ui_int(kw, 0); ui_int(kh, 0);
        return;
    }
    uint32_t sheet, frame;
    if (sym == 0) { sheet = rd(S + S_GENNAD); frame = 0; }
    else if (sym == 8) { sheet = rd(S + S_SHEET); frame = 7; }
    else { sheet = rd(rd(rd(S + S_W) + W_MENU) + MENU_SHEET); frame = R8_(&cpu, got(GOT_SYMFRAME) + (uint32_t)sym); }
    ui_scratch_clear();
    CALL(FN_SPR, sheet, frame, 60, 60, 0, 0, 0);
    ui_sprite_trim(key, 0, 0, 120, 120);
}

static int strip_sym(int reel, int pos) {
    static const uint32_t off[3] = {GOT_STRIP0, GOT_STRIP1, GOT_STRIP2};
    return (int)(int8_t)R8_(&cpu, got(off[reel]) + (uint32_t)(pos & 15));
}

// 서는 중 단계별로 칸을 내려 그리는 양(원본 0x27bf0 표 0x48b24): 이 단계에서는 다음 기호가 30 위에 함께 보인다
static int phase_drop(int ph) {
    switch (ph) { case 3: return 15; case 5: return 10; case 6: return 20; case 8: return 5; }
    return 0;
}

// 배당표: 도움말 글(GOT 0x3b8)의 줄을 그대로 쓰되 머리글자를 기호 그림으로 바꾼다. 첫 줄(겐나드는 무조건 꽝)은 글 그대로.
// 머리글자: 라 자 이 니 제 아 위(CP949) → 기호 1~7, '7' → 8, '?' → 아무거나(-1). 나머지 글자는 그대로 둔다.
static int initial_sym(const unsigned char *p, int *len) {
    static const char *const K[7] = {"\xb6\xf3", "\xc0\xda", "\xc0\xcc", "\xb4\xcf", "\xc1\xa6", "\xbe\xc6", "\xc0\xa7"}; // 라 자 이 니 제 아 위
    if (p[0] == '7') { *len = 1; return 8; }
    if (p[0] == '?') { *len = 1; return -1; }
    if ((p[0] & 0x80) && p[1]) {
        for (int i = 0; i < 7; i++) if (p[0] == (unsigned char)K[i][0] && p[1] == (unsigned char)K[i][1]) { *len = 2; return i + 1; }
    }
    *len = 0;
    return -2;
}

enum { PAY_ROWS = 12, PAY_HALF = 2 };
// 한 칸("라라자x2 "): 기호 셋과 배수 글. 기호를 셋 못 읽으면 글 그대로(ok = 0).
typedef struct { int ok, sym[3]; char mult[16]; char raw[48]; } pay_t;
static void parse_half(const char *s, int n, pay_t *o) {
    memset(o, 0, sizeof *o);
    while (n > 0 && s[n - 1] == ' ') n--;
    snprintf(o->raw, sizeof o->raw, "%.*s", n, s);
    const unsigned char *p = (const unsigned char *)s, *e = p + n;
    int k = 0;
    while (p < e && k < 3) {
        if (*p == ' ') { p++; continue; }
        int l;
        int sym = initial_sym(p, &l);
        if (sym == -2) break;
        o->sym[k++] = sym;
        p += l;
    }
    while (p < e && *p == ' ') p++;
    if (k == 3 && p < e && *p == 'x') { o->ok = 1; snprintf(o->mult, sizeof o->mult, "%.*s", (int)(e - p), (const char *)p); }
}

static void emit_paytable(uint32_t S) {
    const char *t = gstr(got(GOT_HELP));
    char head[96] = "";
    pay_t rows[PAY_ROWS][PAY_HALF];
    int nr = 0, first = 1;
    const char *p = t;
    while (*p && *p != '\t' && nr < PAY_ROWS) {
        const char *e = p;
        while (*e && *e != '\n' && *e != '\t') e++;
        if (first) { snprintf(head, sizeof head, "%.*s", (int)(e - p), p); first = 0; }
        else {
            const char *bar = memchr(p, '|', (size_t)(e - p));
            if (bar) { parse_half(p, (int)(bar - p), &rows[nr][0]); parse_half(bar + 1, (int)(e - bar - 1), &rows[nr][1]); }
            else { parse_half(p, (int)(e - p), &rows[nr][0]); memset(&rows[nr][1], 0, sizeof rows[nr][1]); }
            nr++;
        }
        p = *e == '\n' ? e + 1 : e;
    }
    // 머리 줄: "겐나드는 무조건 꽝" — 겐나드 그림을 앞에 둔다(기호 0)
    ui_str("pay_head", head);
    sym_sprite(S, 0, "gen", 22, 22);
    ui_list("pay");
    for (int r = 0; r < PAY_ROWS; r++) {
        ui_item();
        ui_int("use", r < nr);
        for (int h = 0; h < PAY_HALF; h++) {
            pay_t *q = r < nr ? &rows[r][h] : NULL;
            char k[16];
            snprintf(k, sizeof k, "ok%d", h); ui_int(k, q && q->ok);
            snprintf(k, sizeof k, "m%d", h); ui_str(k, q ? q->mult : "");
            snprintf(k, sizeof k, "raw%d", h); ui_str(k, q && !q->ok ? q->raw : "");
            for (int j = 0; j < 3; j++) {
                snprintf(k, sizeof k, "a%d%d", h, j); ui_int(k, q && q->ok && q->sym[j] < 0); // 아무거나(?)
                snprintf(k, sizeof k, "s%d%d", h, j);
                sym_sprite(S, q && q->ok ? q->sym[j] : -1, k, 16, 20);
            }
        }
        ui_close();
    }
    ui_close();
}

// 필드 카지노 경품 글(0x27328과 같은 판정)
static uint32_t prize_text(uint32_t S) {
    uint32_t casino = rd(rd(S + S_W) + W_PARTY) + PARTY_CASINO;
    switch ((int)rd(casino + 0xc)) {
    case 0: return got(0x3b0);
    case 1: return got(0x3b4);
    case 2: return got(0x3ac);
    }
    return got(0x3a8);
}

// =====================================================================
// 미니게임 저들의영웅(벼베기, 장면 16, 타이틀 '게임시작 › 저들의영웅'과 필드). 사용자 요청 2026-11-12.
//
// ---- 원본 구조(조사) ----
//  상태 S+0x14: 0xb 준비 0x26e44(OK·5 → 0xc), 0xc 진행 0x26e84, 0xd 동작 연출 0x26ee0, 0xe 게임 끝 0x26f14(OK·5 → 0xb),
//    0xf 도움말 0x2757c → 0x27144(글 GOT 0x3a4, 5줄 쪽 넘기기), 0x10 종료 확인 0x275f0. 넷 다 CLR → 0x10, *·(0x1a) → 0xf(돌아올 상태 S+0x18).
//  모델 M = [S+8]: +0 최고 점수, +4 점수(최대 9999), +8 방식(0xd 보통, 0xc 허리펴기, 0xe 술마시기 = 시트 S+0x40 프레임),
//    +9 시간(진행 틱마다 +1, 33을 넘으면 실수), +0xa 연속(빠른 성공 수), +0xb 보너스(벨 때 점수 +보너스+1, 0~9),
//    +0xc 실수(5면 끝), +0xd 결과(1 성공 2 실패), +0xe 연출 프레임, +0xf 동작(0 없음 1 베기 2 허리펴기 3 술마시기 4 실수),
//    +0x10..0x13 숫자 넷(새 숫자는 왼쪽에 들어오고 하나씩 오른쪽으로 밀린다, 0x26704).
//  판정 0x26600(M, 키): 보통이면 맨 오른쪽 숫자(+0x13), 허리펴기면 OK(10), 술마시기면 넷에 없는 숫자. 아니면 실수.
//  연출 0x26658: 프레임 1에서 다음 방식(10% 허리펴기, 10% 술마시기, 나머지는 그대로), 프레임 3에서 베기면 점수·새 숫자,
//    허리펴기·술마시기면 보통으로, 실수면 실수 +1(5면 0xe).
//  보너스 0x26744: 시간 14 이하로 성공하면 연속 +1, 30번째마다 실수가 있으면 실수 −1, 없으면 보너스 +1. 늦거나 실수면 연속·보너스 0.
//  게임 끝에서 OK: 필드에서 연 것이고 최고 기록을 넘으면 100/300/500/1000/2000점 문턱을 처음 넘은 하나를 준다(GOT 0x390~0x3a0 글).
//  제목 0x28030: 최고 점수 2000↑ 「저 들의 영웅」, 1000↑, 500↑, 300↑, 100↑, 그 아래 「모두의 벼베기」(GOT 0x3f4~0x408).
//  그림 0x276b8(S): 판 0x27a4c(논 타일 7×3, 숫자 칸, 실수 표시, 시간 막대) + 상태별(점수 글, 방식 그림, 인물 시트 S+0x44, 연속·보너스 0x27900).
//    판 원점 (S+0x28, S+0x2c), 논은 (원점 + (4, 0x33)) 98×42, 인물 기준 (S+0x20, S+0x24).
//
// ---- 새 UI(ui/hero.rml) ----
//  키 처리·판정·점수·보상은 원본. 원본 그림은 화면 밖에 버리고, 논(인물·연출 포함)만 원본 그리기로 떼어 3배로 보인다.
//  원작 240폭 창의 작은 칸 대신 점수·최고·실수(5칸)·연속·보너스·시간 막대·숫자 넷(누를 칸 강조)·지금 방식의 할 일을 크게 보이고,
//  오른쪽에 숫자 키패드(터치·마우스, 진행 중에만 누를 수 있다)와 허리펴기(OK) 버튼을 둔다. 규칙은 도움말 상태에서 창으로.
// =====================================================================
enum {
    SCENE_HERO = 16,
    HS_READY = 0xb, HS_PLAY = 0xc, HS_ACT = 0xd, HS_OVER = 0xe, HS_HELP = 0xf, HS_EXIT = 0x10,
    S_REDRAW = 0x10, S_ORG_X = 0x28, S_ORG_Y = 0x2c,
    HM_BEST = 0x00, HM_SCORE = 0x04, HM_MODE = 0x08, HM_TIME = 0x09, HM_COMBO = 0x0a, HM_BONUS = 0x0b, HM_MISS = 0x0c,
    HM_ANIM = 0x0e, HM_ACT = 0x0f, HM_DIGITS = 0x10,
    MODE_STRETCH = 0xc, MODE_NORMAL = 0xd, MODE_DRINK = 0xe,
    HERO_TIME_MAX = 33, HERO_MISS_MAX = 5, HERO_FAST = 14, HERO_COMBO_STEP = 30,
    FN_HERO_DRAW = 0x1276b8,
    GOT_HERO_HELP = 0x3a4, GOT_HERO_OVER = 0x3d0, GOT_HERO_READY = 0x3c8,
};
static bool hero_state(int s) { return s >= HS_READY && s <= HS_OVER; }
static bool is_hero(uint32_t S) {
    int s = (int)rd(S + S_STATE);
    if (hero_state(s)) return true;
    return (s == HS_HELP || s == HS_EXIT) && hero_state((int)rd(S + S_RET));
}
void orig_f_26db8(cpu_t *c);
static inline int m8(uint32_t M, int off) { return (int)(int8_t)R8_(&cpu, M + (uint32_t)off); }

// 제목(0x28030과 같은 문턱)
static uint32_t hero_title(int best) {
    return got(best >= 2000 ? 0x3f4 : best >= 1000 ? 0x3f8 : best >= 500 ? 0x3fc : best >= 300 ? 0x400 : best >= 100 ? 0x404 : 0x408);
}
// 게임 끝 보상 글(0x26f14와 같은 판정). 없으면 0.
static uint32_t hero_reward(uint32_t S, int best, int score) {
    if (rd(rd(S + S_W) + W_SCENE_RET) == 3 || score <= best) return 0; // 타이틀에서 연 것은 보상이 없다
    if (best <= 99) return score > 99 ? got(0x390) : 0;
    if (best < 300 && score > 299) return got(0x394);
    if (best < 500 && score > 499) return got(0x398);
    if (best < 1000 && score > 999) return got(0x39c);
    if (best < 2000 && score > 1999) return got(0x3a0);
    return 0;
}

// 게임패드 숫자 입력(사용자 요청 2026-11-12): 진행·연출 상태 동안 host/gamepad.c의 숫자 배치(GAMEPAD_LAYER_HERO)를 켜서 버튼마다 숫자(와 허리펴기)를
// 하나씩 보낸다. 문서는 숫자 칸·키패드에 그 버튼 이름을 붙인다. 처음엔 키패드 커서(방향으로 고르고 A)였지만 시간 막대(약 1초) 안에
// 칸을 옮기기 어려워 바꿨다(사용자 의견).
static void hero_frame(uint32_t S, cpu_t *c) {
    ui_scratch_begin();
    orig_f_26db8(c);
    ui_scratch_end();
    uint32_t W = rd(S + S_W);
    if (rd(W + W_SCENE) != SCENE_HERO || rd(W + W_MINIGAME) != S || !is_hero(S)) return; // 끝낸 틱(시트가 풀렸다)

    cpu_t saved = *c;
    uint32_t M = rd(S + 8);
    int st = (int)rd(S + S_STATE), ret = (int)rd(S + S_RET);
    int base = (st == HS_HELP || st == HS_EXIT) ? ret : st;
    int best = (int)rd(M + HM_BEST), score = (int)rd(M + HM_SCORE);
    int mode = m8(M, HM_MODE), tm = m8(M, HM_TIME), act = m8(M, HM_ACT);
    int miss = m8(M, HM_MISS), combo = m8(M, HM_COMBO), bonus = m8(M, HM_BONUS);
    if (tm < 0) tm = 0;
    if (tm > HERO_TIME_MAX) tm = HERO_TIME_MAX;

    ui_begin("hero");
    ui_int("narrow", ui_screen_w() < 400);
    ui_str("title", gstr(hero_title(best)));
    ui_int("st", st);
    ui_bool("help", st == HS_HELP);
    ui_bool("exit", st == HS_EXIT);
    ui_bool("ready", base == HS_READY);
    ui_bool("play", base == HS_PLAY || base == HS_ACT);
    ui_bool("over", base == HS_OVER);
    ui_bool("can_key", st == HS_PLAY); // 진행 틱에만 원본이 숫자를 받는다(연출 중에는 버린다)
    // 패드 숫자 배치는 진행·연출 동안만. 도움말·종료 창 위와 준비·게임 끝에서는 원래 버튼(A 결정, B 아니오 등)
    bool pad = gamepad_source() == SOURCE_PAD;
    bool layer = st == HS_PLAY || st == HS_ACT;
    if (layer) gamepad_key_layer(GAMEPAD_LAYER_HERO);
    ui_bool("layer", pad && layer);
    ui_text("kclr", gamepad_layer_label(GAMEPAD_LAYER_HERO, -16));
    ui_bool("pad", pad);
    // 버튼 이름: k0..k9 숫자, kok 허리펴기
    for (int i = 0; i < 10; i++) { char k[8]; snprintf(k, sizeof k, "k%d", i); ui_text(k, gamepad_layer_label(GAMEPAD_LAYER_HERO, '0' + i)); }
    ui_text("kok", gamepad_layer_label(GAMEPAD_LAYER_HERO, -5));
    ui_int("score", score);
    ui_int("best", best);
    ui_bool("record", base == HS_OVER && score > best);
    ui_int("bonus", bonus);
    ui_int("combo", combo);
    ui_int("combo_left", HERO_COMBO_STEP - (combo % HERO_COMBO_STEP));
    ui_int("time_pct", (HERO_TIME_MAX - tm) * 100 / HERO_TIME_MAX);
    ui_bool("time_fast", tm <= HERO_FAST);
    ui_int("mode", mode == MODE_STRETCH ? 1 : mode == MODE_DRINK ? 2 : 0);
    // 동작 결과(연출 동안): 1 베기, 2 허리펴기, 3 술마시기, 4 실수
    ui_int("act", base == HS_ACT ? act : 0);
    ui_list("miss");
    for (int i = 0; i < HERO_MISS_MAX; i++) { ui_item(); ui_bool("on", i < miss); ui_close(); }
    ui_close();
    ui_list("digits");
    for (int i = 0; i < 4; i++) {
        ui_item();
        ui_int("v", m8(M, HM_DIGITS + i));
        ui_bool("tgt", mode == MODE_NORMAL && i == 3);
        ui_text("k", gamepad_layer_label(GAMEPAD_LAYER_HERO, '0' + m8(M, HM_DIGITS + i)));
        ui_close();
    }
    ui_close();
    ui_str("ready_t", gstr(got(GOT_HERO_READY)));
    ui_str("over_t", gstr(got(GOT_HERO_OVER)));
    uint32_t rw = base == HS_OVER ? hero_reward(S, best, score) : 0;
    ui_str("reward", rw ? gstr(rw) : "");
    ui_str("ask", gstr(got(GOT_ASK)));
    ui_bool("field", rd(W + W_SCENE_RET) != 3);
    // 보상 표(필드에서만 받는다): 문턱과 글
    static const int TH[5] = {100, 300, 500, 1000, 2000};
    ui_list("rewards");
    for (int i = 0; i < 5; i++) {
        ui_item(); ui_int("th", TH[i]); ui_str("t", gstr(got(0x390 + 4 * (uint32_t)i))); ui_bool("got", best >= TH[i]); ui_close();
    }
    ui_close();
    static const int RT[6] = {0, 100, 300, 500, 1000, 2000};
    ui_list("ranks");
    for (int i = 0; i < 6; i++) {
        ui_item(); ui_int("th", RT[i]); ui_str("t", gstr(got(0x408 - 4 * (uint32_t)i)));
        ui_bool("cur", best >= RT[i] && (i == 5 || best < RT[i + 1])); ui_close();
    }
    ui_close();

    // 논: 원본 그리기(0x276b8)를 다시 불러 떼어 낸다. 도움말·종료 창에서는 그 아래 상태로 그린다.
    // 다시 그림 표지(S+0x10)가 서 있으면 창 틀을 그리고 표지를 내리므로 그동안 0으로 둔다(원본 틱이 이미 처리했다).
    uint32_t redraw = rd(S + S_REDRAW);
    W32_(&cpu, S + S_REDRAW, 0);
    if (st != base) W32_(&cpu, S + S_STATE, (uint32_t)base);
    uint8_t act_keep = R8_(&cpu, M + HM_ACT);
    ui_scratch_begin();
    CALL(FN_HERO_DRAW, S);
    int ox = (int)rd(S + S_ORG_X), oy = (int)rd(S + S_ORG_Y);
    ui_sprite("stage", ox + 4, oy + 0x33, 98, 42);
    ui_scratch_end();
    W8_(&cpu, M + HM_ACT, act_keep); // 게임 끝 그리기는 동작을 4로 적는다. 원본 틱이 한 일만 남기려고 되돌린다
    if (st != base) W32_(&cpu, S + S_STATE, (uint32_t)st);
    W32_(&cpu, S + S_REDRAW, redraw);

    ui_pad(UI_PAD_NONE); // 화면 패드 대신 문서의 키패드·버튼
    ui_end();
    *c = saved;
}

void f_26db8(cpu_t *c) {
    uint32_t S = c->r[0];
    if (ui_modern() && S && is_hero(S)) { hero_frame(S, c); return; }
    if (!ui_modern() || !S || !is_slot(S)) { orig_f_26db8(c); return; }

    // 원본(키 처리·상태 전이·코인 계산·소리)은 그대로. 그림은 화면 밖에 버린다.
    ui_scratch_begin();
    orig_f_26db8(c);
    ui_scratch_end();
    // 종료 확인에서 OK면 원본이 이 틱에 시트를 풀고(0x26d5c) 장면을 돌린다(W+0xc). 풀린 시트로 그림을 떼면 멈추므로 문서를 채우지 않는다.
    uint32_t W = rd(S + S_W);
    if (rd(W + W_SCENE) != SCENE_SLOT || rd(W + W_MINIGAME) != S || !is_slot(S)) return;

    cpu_t saved = *c;
    uint32_t M = rd(S + S_MODEL);
    int st = (int)rd(S + S_STATE), ret = (int)rd(S + S_RET);
    int base = (st == ST_HELP || st == ST_EXIT) ? ret : st; // 기계 화면에 보일 상태(창이 떠 있으면 그 아래 상태)
    int coin = (int)rd(M + M_COIN), bet = (int)rd(M + M_BET), win = (int)rd(M + M_WIN);
    int narrow = ui_screen_w() < 400;

    ui_begin("slot");
    ui_int("narrow", narrow);
    ui_str("title", gstr(got(GOT_TITLE)));
    ui_str("coin_t", gstr(got(GOT_COIN)));
    ui_int("st", st);
    ui_int("base", base);
    ui_bool("help", st == ST_HELP);
    ui_bool("exit", st == ST_EXIT);
    ui_bool("bet_st", base == ST_BET);
    ui_bool("spin_st", base == ST_SPIN);
    ui_bool("res_st", base == ST_RESULT);
    ui_bool("prize_st", base == ST_PRIZE);
    ui_bool("field", rd(rd(S + S_W) + W_SCENE_RET) != 3); // 타이틀에서 연 것(3)이 아니면 필드 카지노
    // 원본 표시 그대로: 베팅 중에는 건 만큼 뺀 코인, 결과에서는 베팅 칸 0
    ui_int("coin", base == ST_BET ? coin - bet : coin);
    ui_int("bet", base == ST_RESULT ? 0 : bet);
    ui_int("win", win);
    ui_int("bet_max", BET_MAX);
    ui_bool("can_spin", base == ST_BET && bet > 0);
    ui_bool("can_up", base == ST_BET && bet < BET_MAX && bet < coin);
    ui_bool("can_down", base == ST_BET && bet > 1);
    // 게임패드: 릴이 도는 동안 X·Y·B가 릴 1·2·3 멈춤(원래 패드에는 2가 없다), 나가기는 START(GAMEPAD_LAYER_SLOT).
    // 베팅은 원래 D패드 방향, 돌리기는 A. 문서는 패드로 조작 중이면 버튼 이름을 붙인다.
    bool pad = gamepad_source() == SOURCE_PAD, layer = st == ST_SPIN;
    if (layer) gamepad_key_layer(GAMEPAD_LAYER_SLOT);
    ui_bool("pad", pad);
    ui_bool("layer", pad && layer);
    ui_text("kclr", gamepad_layer_label(GAMEPAD_LAYER_SLOT, -16));

    ui_scratch_begin();
    // 릴 셋
    ui_list("reels");
    int stopped = 0;
    for (int i = 0; i < 3; i++) {
        int pos = (int)(int8_t)R8_(&cpu, M + M_POS + (uint32_t)i), ph = (int)(int8_t)R8_(&cpu, M + M_PHASE + (uint32_t)i);
        int drop = phase_drop(ph);
        ui_item();
        ui_int("i", i);
        ui_fmt("k", "%d", i + 1);
        ui_text("pk", gamepad_layer_label(GAMEPAD_LAYER_SLOT, '1' + i)); // 패드로 멈추는 버튼
        sym_sprite(S, strip_sym(i, pos), "a", narrow ? 56 : 66, narrow ? 76 : 84);
        sym_sprite(S, drop ? strip_sym(i, pos + 1) : -1, "b", narrow ? 56 : 66, narrow ? 76 : 84);
        ui_int("drop", drop * 2);              // 문서는 원본 칸(33×42)의 두 배로 그린다
        ui_bool("two", drop != 0);
        ui_bool("spin", base == ST_SPIN && ph != PHASE_STOP);
        ui_bool("can_stop", st == ST_SPIN && ph == 0);
        ui_close();
        stopped += ph == PHASE_STOP;
    }
    ui_close();
    ui_int("stopped", stopped);
    // 결과 그림(WIN 프레임 8 / BAD 9, 0x27400)
    if (base == ST_RESULT) {
        ui_scratch_clear();
        CALL(FN_SPR, rd(S + S_SHEET), win ? 8 : 9, 100, 60, 0, 0, 0);
        ui_sprite_trim("res", 0, 0, 220, 140);
    } else { ui_text("res", ""); ui_int("res_w", 0); ui_int("res_h", 0); }
    emit_paytable(S);
    ui_scratch_end();

    ui_str("prize", base == ST_PRIZE ? gstr(prize_text(S)) : "");
    ui_str("ask", gstr(got(GOT_ASK)));
    ui_int("page", (int)rd(S + S_PAGE));
    // 결정 버튼의 이름: 상태마다 OK가 하는 일
    ui_text("ok_t", base == ST_BET ? "돌리기" : base == ST_RESULT ? "계속" : base == ST_PRIZE ? "받기" : "돌리기");
    ui_bool("ok_on", st == base && (base == ST_BET ? bet > 0 : base == ST_RESULT || base == ST_PRIZE));
    ui_pad(UI_PAD_AUTO);
    ui_end();
    *c = saved;
}
