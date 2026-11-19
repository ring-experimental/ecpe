// 게임패드 입력. 게임에는 키보드와 같은 키 코드로 넘기고(host_key), 조작 설정 창에서는 고정 배치로 창을 움직인다.
// 맵핑을 잘못 바꿔도 설정 창을 다룰 수 있도록 창 조작은 맵핑을 따르지 않는다. 크로노스 윙 host/gamepad.c에서 가져와
// 이 게임의 키(숫자 키패드 기능)에 맞게 동작 표를 바꾸고 도움말 글 바꾸기는 뺐다(이 게임의 키 안내는 새 UI 문서가 그린다).
#include "host.h"
#include "ui.h"
#include "gamepad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_key(int type, int code);

enum { TRIGGER_L = SDL_GAMEPAD_BUTTON_COUNT, TRIGGER_R, INPUT_COUNT, NO_INPUT = -1 };
enum { ACT_KEY, ACT_SETTINGS, ACT_AUTO, ACT_SKIP, ACT_BAUTO, ACT_BFAST };
enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_COUNT }; // ACTIONS의 앞 네 줄과 순서가 같다.
enum {
    A_UP, A_DOWN, A_LEFT, A_RIGHT, A_OK, A_CANCEL, A_MENU, A_STAR, A_HASH, A_ZERO, A_SEVEN,
    A_ONE, A_THREE, A_SIX, A_NINE, A_LSK, A_SETTINGS, A_AUTO, A_SKIP, A_BAUTO, A_BFAST, ACTION_COUNT,
};
enum {
    AXIS_ON = 16000, AXIS_OFF = 10000, MAX_PADS = 8,
    NAV_DELAY_MS = 400, NAV_REPEAT_MS = 120,
    KEY_CLEAR = -16,
};

// 게임이 받는 키와 그 쓰임(조사 work/ui/survey/*.md). 2/4/5/6/8은 방향과 OK 대용이라 따로 두지 않는다.
// 취소와 메뉴는 같은 CLEAR다. 필드에서 CLEAR가 메뉴를 열어서, 패드에서 흔히 메뉴 자리인 START에도 둔다.
static const struct { const char *id, *name; int kind, code, def; } ACTIONS[ACTION_COUNT] = {
    [A_UP] = {"up", "위", ACT_KEY, -1, SDL_GAMEPAD_BUTTON_DPAD_UP},
    [A_DOWN] = {"down", "아래", ACT_KEY, -2, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    [A_LEFT] = {"left", "왼쪽", ACT_KEY, -3, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    [A_RIGHT] = {"right", "오른쪽", ACT_KEY, -4, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    [A_OK] = {"ok", "확인, 대화", ACT_KEY, -5, SDL_GAMEPAD_BUTTON_SOUTH},
    [A_CANCEL] = {"cancel", "취소, 메뉴", ACT_KEY, KEY_CLEAR, SDL_GAMEPAD_BUTTON_EAST},
    [A_MENU] = {"menu", "메뉴", ACT_KEY, KEY_CLEAR, SDL_GAMEPAD_BUTTON_START},
    [A_STAR] = {"star", "* 아이템, 핫슬롯", ACT_KEY, '*', SDL_GAMEPAD_BUTTON_WEST},
    [A_HASH] = {"hash", "# 지도, 메뉴 닫기", ACT_KEY, '#', SDL_GAMEPAD_BUTTON_NORTH},
    [A_ZERO] = {"zero", "0 조합, 해제, 이착륙", ACT_KEY, '0', SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    [A_SEVEN] = {"seven", "7 상세, 적 이름", ACT_KEY, '7', SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    [A_ONE] = {"one", "1 적 정보", ACT_KEY, '1', TRIGGER_L},
    [A_THREE] = {"three", "3 단축 스킬", ACT_KEY, '3', TRIGGER_R},
    [A_SIX] = {"six", "6 단축 스킬", ACT_KEY, '6', SDL_GAMEPAD_BUTTON_LEFT_STICK},
    [A_NINE] = {"nine", "9 단축 스킬", ACT_KEY, '9', SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    [A_LSK] = {"lsk", "왼쪽 소프트키", ACT_KEY, -6, NO_INPUT},
    [A_SETTINGS] = {"settings", "조작 설정 창", ACT_SETTINGS, 0, SDL_GAMEPAD_BUTTON_BACK},
    // 대사 자동·빨리 넘기기. 기본 버튼이 비어 있다(남는 버튼이 없다). 키 설정에서 붙인다.
    [A_AUTO] = {"auto", "대사 자동 넘김(AUTO)", ACT_AUTO, 0, NO_INPUT},
    [A_SKIP] = {"skip", "대사 빨리 넘기기(SKIP)", ACT_SKIP, 0, NO_INPUT},
    // 자동 전투·전투 2배속. 이것도 키 설정에서 붙인다.
    [A_BAUTO] = {"bauto", "자동 전투(AUTO)", ACT_BAUTO, 0, NO_INPUT},
    [A_BFAST] = {"bfast", "전투 2배속(x2)", ACT_BFAST, 0, NO_INPUT},
};

static int binds[ACTION_COUNT];
static SDL_Gamepad *pads[MAX_PADS];
static SDL_GamepadType pad_type = SDL_GAMEPAD_TYPE_STANDARD;
static int source;
static int generation;
static bool input_down[INPUT_COUNT], stick_held[DIR_COUNT], stick_sent[DIR_COUNT], trigger_held[2];
static int action_count[ACTION_COUNT];
static int nav_dir = -1;
static int right_y; // 오른쪽 스틱 세로(글 상자 넘기기)
static uint64_t nav_next;

static int default_source(void) {
#ifdef __ANDROID__
    return SOURCE_TOUCH;
#else
    return SOURCE_KEYBOARD;
#endif
}

int gamepad_source(void) { return source; }
void gamepad_set_source(int s) { if (s != source) { source = s; generation++; } }

void gamepad_init(void) {
    source = default_source();
    gamepad_bind_reset();
}

void gamepad_bind_reset(void) {
    for (int i = 0; i < ACTION_COUNT; i++) binds[i] = ACTIONS[i].def;
    generation++;
}

void gamepad_bind(int action, int input) {
    if (action < 0 || action >= ACTION_COUNT) return;
    gamepad_release_all();
    for (int i = 0; i < ACTION_COUNT; i++) if (i != action && input != NO_INPUT && binds[i] == input) binds[i] = binds[action];
    binds[action] = input;
    generation++;
}

static int pad_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_PADS; i++) if (pads[i]) n++;
    return n;
}

bool gamepad_connected(void) { return pad_count() > 0; }
int gamepad_generation(void) { return generation; }
int gamepad_action_count(void) { return ACTION_COUNT; }
const char *gamepad_action_name(int a) { return ACTIONS[a].name; }
const char *gamepad_config_id(int a) { return ACTIONS[a].id; }

static bool type_is_playstation(void) {
    return pad_type == SDL_GAMEPAD_TYPE_PS3 || pad_type == SDL_GAMEPAD_TYPE_PS4 || pad_type == SDL_GAMEPAD_TYPE_PS5;
}

static bool type_is_nintendo(void) {
    return pad_type >= SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO && pad_type <= SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;
}

static const char *input_label(int input) {
    int family = type_is_playstation() ? 1 : type_is_nintendo() ? 2 : 0;
    switch (input) {
    case NO_INPUT: return "없음";
    case TRIGGER_L: { static const char *n[] = {"LT", "L2", "ZL"}; return n[family]; }
    case TRIGGER_R: { static const char *n[] = {"RT", "R2", "ZR"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: { static const char *n[] = {"LB", "L1", "L"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: { static const char *n[] = {"RB", "R1", "R"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: { static const char *n[] = {"LS", "L3", "LS"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: { static const char *n[] = {"RS", "R3", "RS"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_BACK: { static const char *n[] = {"BACK", "SHARE", "-"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_START: { static const char *n[] = {"START", "OPTIONS", "+"}; return n[family]; }
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "↑";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "↓";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "←";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "→";
    case SDL_GAMEPAD_BUTTON_MISC1: return "추가 버튼";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "P1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "P2";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "P3";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "P4";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "터치패드";
    case SDL_GAMEPAD_BUTTON_SOUTH: case SDL_GAMEPAD_BUTTON_EAST: case SDL_GAMEPAD_BUTTON_WEST: case SDL_GAMEPAD_BUTTON_NORTH:
        switch (SDL_GetGamepadButtonLabelForType(pad_type, (SDL_GamepadButton)input)) {
        case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
        case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
        case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "×";
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "○";
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "□";
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "△";
        default: return "?";
        }
    default: return "?";
    }
}

// 키 설정 창에 보일 버튼. AUTO·SKIP·자동 전투·2배속은 따로 붙인 버튼이 없어도 그 화면에서 다른 동작의 버튼을 빌려 쓴다
// (문서의 padhot: 대사 중 * # 버튼, 전투 중 # 버튼과 메뉴 버튼). 「없음」으로 보이면 쓸 수 없는 줄 알게 되므로(사용자 보고 2026-11-12)
// 빌려 쓰는 버튼과 그 장면을 적는다. 빌린 쪽 동작의 버튼을 바꾸면 따라 바뀐다.
const char *gamepad_action_key(int a) {
    static const struct { int act, from; const char *when; } BORROW[] = {
        {A_AUTO, A_STAR, "대사"}, {A_SKIP, A_HASH, "대사"}, {A_BAUTO, A_HASH, "전투"}, {A_BFAST, A_MENU, "전투"}};
    static char buf[4][48];
    if (binds[a] == NO_INPUT)
        for (int i = 0; i < 4; i++)
            if (BORROW[i].act == a && binds[BORROW[i].from] != NO_INPUT) {
                snprintf(buf[i], sizeof buf[i], "%s (%s)", input_label(binds[BORROW[i].from]), BORROW[i].when);
                return buf[i];
            }
    return input_label(binds[a]);
}

const char *gamepad_code_label(int code) {
    for (int a = 0; a < ACTION_COUNT; a++)
        if (ACTIONS[a].kind == ACT_KEY && ACTIONS[a].code == code && binds[a] != NO_INPUT) return input_label(binds[a]);
    return NULL;
}

// 설정 창의 고정 배치(맵핑을 따르지 않는다): 0 확인(아래 버튼), 1 닫기(오른쪽 버튼)
// 2~5는 어깨 버튼 자리(LB RB LT RT): 메뉴의 탭·캐릭터 넘김은 묶은 동작이 아니라 버튼 자리로 정한다(input_set).
const char *gamepad_fixed_label(int which) {
    static const int IN[] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, TRIGGER_L, TRIGGER_R};
    return which >= 0 && which < 6 ? input_label(IN[which]) : "";
}

const char *gamepad_action_label(const char *id) {
    for (int a = 0; a < ACTION_COUNT; a++) if (!strcmp(ACTIONS[a].id, id)) return binds[a] == NO_INPUT ? NULL : input_label(binds[a]);
    return NULL;
}

const char *gamepad_config_value(int a) {
    int b = binds[a];
    if (b == NO_INPUT) return "none";
    if (b == TRIGGER_L) return "lefttrigger";
    if (b == TRIGGER_R) return "righttrigger";
    return SDL_GetGamepadStringForButton((SDL_GamepadButton)b);
}

void gamepad_config_read(const char *id, const char *value) {
    for (int a = 0; a < ACTION_COUNT; a++) {
        if (strcmp(ACTIONS[a].id, id)) continue;
        int input = !strcmp(value, "none") ? NO_INPUT : !strcmp(value, "lefttrigger") ? TRIGGER_L : !strcmp(value, "righttrigger") ? TRIGGER_R : (int)SDL_GetGamepadButtonFromString(value);
        if (input == SDL_GAMEPAD_BUTTON_INVALID || input == SDL_GAMEPAD_BUTTON_GUIDE) return;
        gamepad_bind(a, input);
        return;
    }
}

static void fire(int action, bool down) {
    if (ACTIONS[action].kind == ACT_SETTINGS) { if (down) ui_toggle_settings(); return; }
    if (ACTIONS[action].kind == ACT_AUTO) { if (down) ui_toggle_auto(); return; }
    if (ACTIONS[action].kind == ACT_SKIP) { if (down) ui_toggle_skip(); return; }
    if (ACTIONS[action].kind == ACT_BAUTO) { if (down) ui_toggle_battle_auto(); return; }
    if (ACTIONS[action].kind == ACT_BFAST) { if (down) ui_toggle_battle_fast(); return; }
    // 화면이 이 버튼을 빌려 쓰면(문서의 padhot, ui.h) 원본 키를 보내지 않는다. 누를 때 가로챘으면 뗄 때도 보내지 않는다.
    static bool hot[ACTION_COUNT];
    if (down) { if (ui_pad_hot(ACTIONS[action].id)) { hot[action] = true; return; } }
    else if (hot[action]) { hot[action] = false; return; }
    host_key(down ? 2 : 3, ACTIONS[action].code);
}

static void action_set(int action, bool down) {
    if (down) { if (action_count[action]++ == 0) fire(action, true); }
    else if (action_count[action] > 0 && --action_count[action] == 0) fire(action, false);
}

void gamepad_release_all(void) {
    for (int i = 0; i < INPUT_COUNT; i++) input_down[i] = false;
    for (int i = 0; i < DIR_COUNT; i++) stick_sent[i] = false;
    for (int i = 0; i < ACTION_COUNT; i++) if (action_count[i] > 0) { action_count[i] = 0; fire(i, false); }
    nav_dir = -1;
}

static void nav_press(int dir) {
    ui_pad_nav(dir == DIR_UP ? UI_NAV_UP : dir == DIR_DOWN ? UI_NAV_DOWN : dir == DIR_LEFT ? UI_NAV_LEFT : UI_NAV_RIGHT);
    nav_dir = dir;
    nav_next = SDL_GetTicks() + NAV_DELAY_MS;
}

static void note_pad_used(SDL_JoystickID which) {
    SDL_Gamepad *p = SDL_GetGamepadFromID(which);
    SDL_GamepadType type = p ? SDL_GetGamepadType(p) : pad_type;
    if (type != pad_type) { pad_type = type; generation++; }
    gamepad_set_source(SOURCE_PAD);
}

static int dpad_dir(int input) {
    switch (input) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return DIR_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return DIR_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return DIR_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return DIR_RIGHT;
    default: return -1;
    }
}

// 미니게임 배치(restored/ui_minigame.c가 진행 틱마다 gamepad_key_layer(배치)로 켠다): 원작 숫자 키를 버튼마다 하나씩 붙인다.
// 묶은 동작보다 먼저 보고, 누를 때 보낸 키를 뗄 때 그대로 뗀다(그 사이 배치가 꺼져도 짝이 맞게). 배치에 없는 버튼은 원래 동작.
//  저들의영웅: D패드 ↑1 →2 ↓3 ←4, 얼굴 버튼 위부터 시계 방향 5 6 7 8, LB 9, RB 0, LT·RT OK(허리펴기).
//  트리플 슬롯(릴이 도는 동안): 얼굴 버튼 왼쪽·위·오른쪽(X Y B) = 릴 1 2 3(화면의 릴 순서). A는 그대로 OK.
// 두 배치 모두 B를 쓰므로 나가기(CLR)는 START로 한다(gamepad_layer_label(배치, -16)).
static Uint64 layer_until;
static int layer_now;
static int layer_sent[INPUT_COUNT];
void gamepad_key_layer(int layer) { layer_now = layer; layer_until = SDL_GetTicks() + 250; }
static int layer_code(int layer, int input) {
    if (layer == GAMEPAD_LAYER_HERO) switch (input) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return '1';
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return '2';
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return '3';
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return '4';
    case SDL_GAMEPAD_BUTTON_NORTH: return '5';
    case SDL_GAMEPAD_BUTTON_EAST: return '6';
    case SDL_GAMEPAD_BUTTON_SOUTH: return '7';
    case SDL_GAMEPAD_BUTTON_WEST: return '8';
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return '9';
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return '0';
    case TRIGGER_L: case TRIGGER_R: return -5;
    default: return 0;
    }
    if (layer == GAMEPAD_LAYER_SLOT) switch (input) {
    case SDL_GAMEPAD_BUTTON_WEST: return '1';
    case SDL_GAMEPAD_BUTTON_NORTH: return '2';
    case SDL_GAMEPAD_BUTTON_EAST: return '3';
    default: return 0;
    }
    return 0;
}
const char *gamepad_layer_label(int layer, int code) {
    if (code == -16) return input_label(SDL_GAMEPAD_BUTTON_START);
    for (int i = 0; i < INPUT_COUNT; i++) if (layer_code(layer, i) == code) return input_label(i);
    return "";
}

static void input_set(int input, bool down) {
    if (down && ui_pad_capture(input)) return;
    if (ui_modal()) {
        int dir = dpad_dir(input);
        if (!down) { if (dir >= 0 && dir == nav_dir) nav_dir = -1; return; }
        if (dir >= 0) nav_press(dir);
        else if (input == SDL_GAMEPAD_BUTTON_SOUTH) ui_pad_nav(UI_NAV_OK);
        else if (input == SDL_GAMEPAD_BUTTON_EAST || input == binds[A_SETTINGS]) ui_pad_nav(UI_NAV_BACK);
        return;
    }
    if (down == input_down[input]) return;
    input_down[input] = down;
    if (!down && layer_sent[input]) { host_key(3, layer_sent[input]); layer_sent[input] = 0; return; }
    if (down && SDL_GetTicks() < layer_until && layer_code(layer_now, input)) { layer_sent[input] = layer_code(layer_now, input); host_key(2, layer_sent[input]); return; }
    // 어깨 버튼: 메뉴에서는 LB/RB 탭, LT/RT 쪽 넘김(Xbox 접근성 지침 112의 Sea of Thieves 배치). 버튼 자리로 정하고(묶은 동작이 아니라),
    // 화면이 그 버튼에 묶인 원본 키를 쓰면 원본 키로 보낸다(ui_pad_shortcut). 누를 때 가로챘으면 뗄 때도 보내지 않는다.
    static bool taken[INPUT_COUNT];
    int sh = input == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER ? 0 : input == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER ? 1 : input == TRIGGER_L ? 2 : input == TRIGGER_R ? 3 : -1;
    if (sh >= 0) {
        if (!down && taken[input]) { taken[input] = false; return; }
        if (down) {
            int code = 0;
            for (int a = 0; a < ACTION_COUNT; a++) if (binds[a] == input && ACTIONS[a].kind == ACT_KEY) code = ACTIONS[a].code;
            if (ui_pad_shortcut(sh, code)) { taken[input] = true; return; }
        }
    }
    for (int a = 0; a < ACTION_COUNT; a++) if (binds[a] == input) { action_set(a, down); break; }
}

static void stick_set(int dir, bool held) {
    if (stick_held[dir] == held) return;
    stick_held[dir] = held;
    if (held) {
        if (ui_pad_capturing()) return;
        if (ui_modal()) nav_press(dir);
        else { stick_sent[dir] = true; action_set(A_UP + dir, true); }
    } else {
        if (dir == nav_dir) nav_dir = -1;
        if (stick_sent[dir]) { stick_sent[dir] = false; action_set(A_UP + dir, false); }
    }
}

static bool past_threshold(bool held, int value) { return value >= (held ? AXIS_OFF : AXIS_ON); }

static void axis_event(const SDL_GamepadAxisEvent *e) {
    int v = e->value;
    switch (e->axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
        stick_set(DIR_LEFT, past_threshold(stick_held[DIR_LEFT], -v));
        stick_set(DIR_RIGHT, past_threshold(stick_held[DIR_RIGHT], v));
        break;
    case SDL_GAMEPAD_AXIS_LEFTY:
        stick_set(DIR_UP, past_threshold(stick_held[DIR_UP], -v));
        stick_set(DIR_DOWN, past_threshold(stick_held[DIR_DOWN], v));
        break;
    case SDL_GAMEPAD_AXIS_RIGHTY: right_y = v; break;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
        int t = e->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        bool held = past_threshold(trigger_held[t], v);
        if (held == trigger_held[t]) break;
        trigger_held[t] = held;
        if (held) note_pad_used(e->which);
        input_set(t ? TRIGGER_R : TRIGGER_L, held);
        break;
    }
    default: break;
    }
    if (e->axis == SDL_GAMEPAD_AXIS_LEFTX || e->axis == SDL_GAMEPAD_AXIS_LEFTY)
        if (v >= AXIS_ON || v <= -AXIS_ON) note_pad_used(e->which);
}

static void pad_removed(SDL_JoystickID which) {
    SDL_Gamepad *p = SDL_GetGamepadFromID(which);
    for (int i = 0; i < MAX_PADS; i++) if (pads[i] == p) { SDL_CloseGamepad(p); pads[i] = NULL; }
    gamepad_release_all();
    for (int i = 0; i < DIR_COUNT; i++) stick_held[i] = false;
    trigger_held[0] = trigger_held[1] = false;
    if (!gamepad_connected() && source == SOURCE_PAD) source = default_source();
    generation++;
}

static void pad_added(SDL_JoystickID which) {
    for (int i = 0; i < MAX_PADS; i++) if (!pads[i]) {
        pads[i] = SDL_OpenGamepad(which);
        if (!pads[i]) { SDL_Log("게임패드 열기 실패: %s", SDL_GetError()); return; }
        pad_type = SDL_GetGamepadType(pads[i]);
        SDL_Log("게임패드 연결: %s", SDL_GetGamepadName(pads[i]));
        generation++;
        return;
    }
}

bool gamepad_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_GAMEPAD_ADDED: pad_added(e->gdevice.which); return true;
    case SDL_EVENT_GAMEPAD_REMOVED: pad_removed(e->gdevice.which); return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        bool down = e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        if (e->gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) return true; // 시스템이 쓰는 버튼이다.
        if (down) note_pad_used(e->gbutton.which);
        input_set(e->gbutton.button, down);
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: axis_event(&e->gaxis); return true;
    case SDL_EVENT_KEY_DOWN: // Android BACK과 음량 키는 키보드로 치지 않는다.
        if (e->key.scancode != SDL_SCANCODE_AC_BACK && e->key.scancode != SDL_SCANCODE_VOLUMEUP && e->key.scancode != SDL_SCANCODE_VOLUMEDOWN
            && e->key.scancode != SDL_SCANCODE_F1 && e->key.scancode != SDL_SCANCODE_F2 && e->key.scancode != SDL_SCANCODE_F11)
            gamepad_set_source(SOURCE_KEYBOARD);
        return false;
    case SDL_EVENT_FINGER_DOWN:
        if (e->tfinger.touchID != SDL_MOUSE_TOUCHID) gamepad_set_source(SOURCE_TOUCH);
        return false;
    default: return false;
    }
}

void gamepad_tick(void) {
    // 오른쪽 스틱: 보이는 글 상자(scroll 속성, 도움말 등)를 기울인 만큼 넘긴다. 데드존 밖에서 틱마다 최대 6dp.
    static uint64_t last;
    uint64_t t = SDL_GetTicks();
    if (right_y > AXIS_OFF || right_y < -AXIS_OFF) {
        float amt = (float)(right_y > 0 ? right_y - AXIS_OFF : right_y + AXIS_OFF) / (32767 - AXIS_OFF);
        float dt = last ? (float)(t - last) : 16;
        if (dt > 50) dt = 50;
        if (ui_scroll(NULL, amt * dt * 0.4f)) gamepad_set_source(SOURCE_PAD);
    }
    last = t;
    if (nav_dir < 0) return;
    if (!ui_modal()) { nav_dir = -1; return; }
    uint64_t now = SDL_GetTicks();
    if (now < nav_next) return;
    int dir = nav_dir;
    nav_press(dir);
    nav_next = now + NAV_REPEAT_MS;
}

// 시험용 가상 패드. vpad xbox|ps, vbtn <이름> [up], vaxis <이름> <값>, vdetach. 이름은 SDL 매핑 이름이다.
static SDL_JoystickID virtual_pad;

bool gamepad_ctrl(const char *line) {
    char a[32], b[32];
    if (!strncmp(line, "vpad ", 5)) {
        SDL_VirtualJoystickDesc d;
        SDL_INIT_INTERFACE(&d);
        bool ps = !strcmp(line + 5, "ps");
        d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        d.naxes = SDL_GAMEPAD_AXIS_COUNT;
        d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        d.vendor_id = ps ? 0x054c : 0x045e;
        d.product_id = ps ? 0x0ce6 : 0x028e;
        d.name = ps ? "virtual ps" : "virtual xbox";
        virtual_pad = SDL_AttachVirtualJoystick(&d);
        if (!virtual_pad) SDL_Log("가상 패드 실패: %s", SDL_GetError());
        return true;
    }
    if (!strcmp(line, "vdetach")) { if (virtual_pad) SDL_DetachVirtualJoystick(virtual_pad); virtual_pad = 0; return true; }
    SDL_Joystick *j = virtual_pad ? SDL_GetJoystickFromID(virtual_pad) : NULL;
    if (!strncmp(line, "vbtn ", 5)) {
        int n = sscanf(line, "vbtn %31s %31s", a, b);
        if (j && n >= 1) SDL_SetJoystickVirtualButton(j, SDL_GetGamepadButtonFromString(a), n != 2);
        return true;
    }
    int v;
    if (sscanf(line, "vaxis %31s %d", a, &v) == 2) {
        if (j) SDL_SetJoystickVirtualAxis(j, SDL_GetGamepadAxisFromString(a), (Sint16)v);
        return true;
    }
    return false;
}
