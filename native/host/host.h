// 네이티브 호스트 공용 선언. WIPI API 동작은 WIE(MIT, dlunch)의 wie-wipi-c를 참고했다.
// 구조는 크로노스 윙 네이티브 빌드(크로노스 윙 저장소의 native/)와 같다.
#pragma once
#include "thumbrt.h"
#include "ver.h"
#include <stdbool.h>

#define IMAGE_LOAD 0x100000u
#define HOST_TABLES 0x220000u
#define STACK_TOP 0x400000u
#define HEAP_START 0x400000u
#define GUEST_MEM 0x4000000u
#define GUEST_LOW 0x100000u
#define API_BASE 0xf0000000u

extern cpu_t cpu;
extern char game_dir[1024];
extern char save_dir[1024];

// 게스트 호출. 인자는 r0-r3, 그 뒤는 스택으로 넘긴다.
uint32_t guest_call(uint32_t addr, const uint32_t *args, int n);

static inline uint32_t arg(cpu_t *c, int i) { return i < 4 ? c->r[i] : R32_(c, c->SP + (i - 4) * 4); }
static inline char *gstr(uint32_t a) { return (char *)cpu.m + a; }

void api_dispatch(cpu_t *c, uint32_t table, uint32_t index);
void api_install(void);

// 메모리
extern uint32_t host_last_call;
void guest_mem_alloc(void);
void guest_mem_clear(void);
void heap_init(void);
uint32_t heap_alloc(uint32_t size);
void heap_free(uint32_t addr);
uint32_t handle_alloc(uint32_t size);
void handle_free(uint32_t h);
static inline uint32_t handle_data(uint32_t h) { return R32_(&cpu, h) + 8; }

// 타이머
void timer_def(uint32_t timer, uint32_t cb);
int timer_set(uint32_t timer, uint64_t ms, uint32_t param);
void timer_unset(uint32_t timer);
void defer_call(uint32_t cb, uint32_t a0, uint32_t a1, uint64_t ms);
void timers_run(void);
void timers_reset(void);
int64_t timers_next_ms(void);
uint64_t now_ms(void);
uint64_t game_time_ms(void); // 게임이 읽는 시계(api 28). 전투 2배속에서는 배율만큼 빠르다

// 표시
void video_present(uint32_t fb_handle);
extern int screen_w, screen_h;
void request_exit(void);

// 글꼴(font.c). WIE DrawString과 같은 글리프 표(빌드 때 glyphdump가 만든다)로 그린다.
bool font_load(const char *path);
int font_string_width(const uint8_t *s, int len);
void font_draw(uint16_t *px, int bpl_px, int w, int h, int x, int baseline, const uint8_t *s, int len, uint16_t color);

// 소리
void audio_init(const char *soundfont);
uint32_t audio_load(const uint8_t *data, uint32_t size);
void audio_play(uint32_t handle, bool repeat);
void audio_stop(uint32_t handle);
void audio_free(uint32_t handle);
void audio_pause(bool paused);
void audio_reset(void);
void audio_sfx_scope(bool in_sfx); // 게임의 효과음 처리 안: 클립 정지를 배경음에 적용하지 않고, MIDI는 효과음 합성기로
void audio_music_stop(void);       // 배경음을 실제로 끈다

// 그래픽 API (표 3)
void gfx_api(cpu_t *c, uint32_t index);
uint32_t gfx_screen_handle(void);
void gfx_reset(void);
void api_reset(void);

// 진단용 호출 나무(calltree.c)
void ts_api(cpu_t *c, uint32_t table, uint32_t index);
void ts_root(void);
void ts_request(const char *file);
void ts_present(void);
