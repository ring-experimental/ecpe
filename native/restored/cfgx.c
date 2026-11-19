// 환경설정의 원작에 없는 줄: 화면(전체화면/창), 일러스트(리마스터/도트), 조작 설정, 기타 설정, 세이브 데이터.
//
// 전에는 뒤의 넷이 환경설정 제목 옆의 버튼이었다(터치·마우스로만 누르고 키보드는 F1/F3/F4, 게임패드는 조작 설정만 BACK).
// 게임패드로는 닿을 길도, 있다는 안내도 없어서(사용자 보고 2026-11-12) 원작 줄 아래의 줄로 내렸다: 위아래로 가서
// 좌우로 값을 바꾸거나 OK로 연다. 원본 커서로는 닿지 않는 가상 줄이라 커서를 여기서 따로 들고, 그 줄에 있는 동안의
// 위아래·좌우·OK는 원본에 보내지 않는다. 원본 줄과 한 고리로 돈다(0 → … → 마지막 → 가상 줄들 → 0).
#include "restored.h"
#include "cfgx.h"

enum { CX_DISPLAY, CX_ILLUST, CX_CONTROLS, CX_ETC, CX_SAVEDATA, CX_KINDS };
static int sel = -1;

static int kind_of(int i) {
    if (!ui_display_supported()) i++; // 「화면」 줄은 바꿀 수 있는 창에서만(Android·헤드리스에는 없다)
    return i >= 0 && i < CX_KINDS ? i : -1;
}
int cfgx_count(void) { return ui_display_supported() ? CX_KINDS : CX_KINDS - 1; }
int cfgx_sel(void) { if (sel >= cfgx_count()) sel = -1; return sel; }
void cfgx_reset(void) { sel = -1; }

const char *cfgx_name(int i) {
    static const char *const N[CX_KINDS] = {"화면", "일러스트", "조작 설정", "기타 설정", "세이브 데이터"};
    int k = kind_of(i);
    return k < 0 ? "" : N[k];
}
const char *cfgx_action(int i) {
    switch (kind_of(i)) {
    case CX_CONTROLS: return "settings";
    case CX_ETC: return "etc";
    case CX_SAVEDATA: return "savedata";
    default: return "";
    }
}
int cfgx_value(int i, const char **on_t, const char **off_t) {
    switch (kind_of(i)) {
    case CX_DISPLAY: *on_t = "전체화면"; *off_t = "창"; return ui_fullscreen();
    case CX_ILLUST: *on_t = "HD"; *off_t = "도트"; return ui_illust();
    default: *on_t = *off_t = ""; return 0;
    }
}

int cfgx_key(int up, int down, int lr, int ok, int cur, int last, int *set_cur) {
    int n = cfgx_count();
    *set_cur = -1;
    if (cfgx_sel() < 0) {
        if (down && cur == last) { sel = 0; return 1; }
        if (up && cur == 0) { sel = n - 1; return 1; }
        return 0;
    }
    if (up) { if (sel == 0) { sel = -1; *set_cur = last; } else sel--; return 1; }
    if (down) { if (sel == n - 1) { sel = -1; *set_cur = 0; } else sel++; return 1; }
    int k = kind_of(sel);
    if (lr) { // 값 줄만 바뀐다(바로 적용, 호스트가 ui.ini에 남긴다)
        if (k == CX_DISPLAY) ui_toggle_fullscreen();
        else if (k == CX_ILLUST) ui_toggle_illust();
        return 1;
    }
    // 여는 줄: 지금은 게스트 틱 안이라 바로 열지 않고 메인 루프로 미룬다(ui.h ui_action_later)
    if (ok && cfgx_action(sel)[0]) { ui_action_later(cfgx_action(sel)); return 1; }
    return 0; // 값 줄의 OK와 CLR(나가기)은 원본이 받는다
}
