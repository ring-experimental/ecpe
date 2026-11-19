// 게임패드 입력과 키 맵핑, 마지막에 쓴 조작 장치. 구조는 크로노스 윙 host/gamepad.c를 따른다.
#pragma once
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
void gamepad_init(void);
bool gamepad_event(const SDL_Event *e); // 패드 이벤트면 true. 키보드·터치 이벤트는 조작 장치만 기록하고 false.
void gamepad_tick(void);                // 설정 창 이동의 길게 누르기를 처리한다.
void gamepad_release_all(void);         // 눌린 게임 키를 모두 뗀다.
bool gamepad_ctrl(const char *line);    // 시험용 가상 패드 명령. 처리했으면 true.

// 마지막에 쓴 조작 장치. 키 안내 글과 터치 패드 표시가 이것을 따른다.
enum { SOURCE_KEYBOARD, SOURCE_TOUCH, SOURCE_PAD };
int gamepad_source(void);
void gamepad_set_source(int s);

bool gamepad_connected(void);
int gamepad_generation(void); // 맵핑, 연결, 패드 종류, 조작 장치가 바뀌면 늘어난다.
int gamepad_action_count(void);
const char *gamepad_action_name(int action); // UTF-8
const char *gamepad_action_key(int action);  // 맵핑된 버튼 표시. UTF-8
void gamepad_bind(int action, int input);    // 다른 동작이 쓰던 버튼이면 두 동작의 버튼을 맞바꾼다.
void gamepad_bind_reset(void);
const char *gamepad_config_id(int action);
const char *gamepad_config_value(int action);
void gamepad_config_read(const char *id, const char *value);
// 게임 키 코드(-5 OK, -16 CLEAR, '*' 등)에 맵핑된 버튼 표시. 맵핑이 없으면 NULL.
const char *gamepad_code_label(int code);
const char *gamepad_fixed_label(int which);        // 버튼 자리의 표시: 0 확인, 1 닫기(설정 창 고정 배치), 2 LB 3 RB 4 LT 5 RT
const char *gamepad_action_label(const char *id); // 동작 id("auto" 등)에 맵핑된 버튼 표시, 없으면 NULL
// 미니게임 배치: 부르면 잠시(0.25초) 패드 버튼이 그 배치의 원작 키를 보낸다(틱마다 다시 부른다).
// 라벨은 그 배치에서 키 코드('0'~'9', -5 OK, -16 나가기)를 누르는 버튼, 없으면 빈 글.
enum { GAMEPAD_LAYER_HERO = 1, GAMEPAD_LAYER_SLOT = 2 };
void gamepad_key_layer(int layer);
const char *gamepad_layer_label(int layer, int code);
#ifdef __cplusplus
}
#endif
