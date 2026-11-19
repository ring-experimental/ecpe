// 원작 메뉴를 대신 그리는 RmlUi 층. 게임 코드(restored/ui_*.c)가 매 프레임 화면에 보일 값을 넘기고, 모양은 ui/<문서>.rml과 ui/*.rcss가 정한다.
// 입력과 상태는 원본 코드가 그대로 처리한다. 여기서는 그리기만 바꾼다.
//
// 쓰는 법(restored/list.txt에 `<오프셋> ui_<이름>.c wrap`으로 원본 그리기 함수를 감싼다):
//   void orig_f_31b18(cpu_t *c);
//   void f_31b18(cpu_t *c) {
//       if (!ui_modern()) { orig_f_31b18(c); return; }   // F2로 원작 그리기와 바꿔 볼 수 있다
//       ui_begin("item");                                 // ui/item.rml, 데이터 모델 이름도 "item", 값은 `d.` 아래에 있다
//       ui_str("title", gstr(...)); ui_int("cursor", ...);
//       ui_list("items"); for (...) { ui_item(); ui_str("name", ...); ui_int("count", ...); ui_close(); } ui_close();
//       ui_end();
//   }
// ui_begin~ui_end를 부른 틱에만 그 문서가 보인다(표시 때마다 판정). 그리기 함수가 매 틱 불리지 않는 화면은 틱 함수 쪽에서 부른다.
// 키 이름은 매 프레임 같은 모양으로 채운다(없는 키를 문서가 읽으면 빈 값). 나중에 ui_begin한 문서가 위에 온다.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool ui_modern(void);
void ui_begin(const char *doc);
void ui_end(void);
void ui_force(void);            // ui_begin 뒤에 부르면 그 문서는 새 UI가 꺼져 있어도(F2 원작 그리기) 보인다(부활석 창 등 원작에 없는 화면용)
void ui_hide(const char *doc);   // 이번 틱에 이미 채운 그 문서를 감춘다(위에 뜬 창이 아래 화면을 대신할 때. 다음 틱에 다시 채우면 다시 보인다)
void ui_below(void);            // ui_begin 뒤에 부르면 그 문서는 채운 차례와 상관없이 다른 문서보다 아래에 깔린다(필드 메뉴 틀)
void ui_int(const char *key, int v);
void ui_bool(const char *key, bool v);
void ui_str(const char *key, const char *cp949);            // 게임 문자열(CP949). NULL이면 빈 글.
void ui_strn(const char *key, const char *cp949, int len);
void ui_text(const char *key, const char *utf8);            // 소스에 적은 글(UTF-8)
void ui_fmt(const char *key, const char *utf8_fmt, ...);
void ui_list(const char *key);  // 배열 시작. ui_close로 닫는다.
void ui_item(void);             // 배열 안의 객체 하나 시작. ui_close로 닫는다.
void ui_obj(const char *key);   // 이름 붙은 객체 시작. ui_close로 닫는다.
void ui_close(void);

// 게임 그림을 텍스처로 떼어 낸다. 화면 버퍼를 잠시 빌려 원본 그리기 함수를 부르고 되돌려 놓는다.
//   ui_scratch_begin();                      // 화면을 보관하고 투명색(자홍 0xf81f)으로 채운다. DrawString 기록도 시작한다.
//   CALL(원본 스프라이트 그리기, ..., x, y);  // 또는 원본 그리기 함수 전체
//   ui_sprite("icon", x, y, w, h);           // 그 자리를 떼어 키 "icon"에 텍스처 이름을 넣는다(<img data-attr-src="d.icon"/>, 1px = 1dp).
//   ui_scratch_clear();                      // 다음 그림을 위해 다시 투명색으로
//   ui_scratch_end();                        // 화면을 되돌린다
// ui_sprite_trim은 투명색이 아닌 화소의 테두리로 줄인다. 다 투명이면 키에 빈 글이 들어간다. 키_w, 키_h에 크기(px)가 들어간다.
void ui_scratch_begin(void);
void ui_scratch_clear(void);
void ui_scratch_end(void);
void ui_sprite(const char *key, int x, int y, int w, int h);
void ui_sprite_trim(const char *key, int x, int y, int w, int h);
bool ui_scratch_dump(const char *path); // 진단: scratch에 그린 것(테두리로 줄임)을 PPM으로
// ui_scratch_begin 뒤에 원본이 DrawString으로 그린 글. 구조체를 읽는 대신 원본이 만든 글을 그대로 받을 때 쓴다.
int ui_cap_count(void);
const char *ui_cap_text(int i, int *x, int *y, unsigned *rgb565); // CP949, 다음 ui_scratch_begin까지 유효
int ui_screen_w(void); // 게임 화면 폭(px = dp)

// 리마스터 일러스트(환경설정 '일러스트', ui.ini illust, F4). 원본 초상화 프레임 60~66(아크, 니하트, 제레미, 쟈이언트, 이오,
// 라즈파샤, 위지윅 = id 0~6)을 scratch에 그리면 restored/illust.c가 그 자리를 ui_illust_mark로 알리고, 그 자리를 떼는 ui_sprite는
// 고해상도 그림(ui/illust_<이름>.png)으로 바뀐다. 끄면 원작 도트 그대로다.
enum { UI_ILLUST_COUNT = 9 }; // 프레임 60~68(67 어린 니하트·68 키리엔은 원화 자료가 없어 도트에서 다시 그렸다)
// 원작에 초상이 없는 인물(겐나드, 모아드, 아케인, 텔러, 하멜린 = id 7~11): restored/ui_dialog.c가 화자 이름으로 골라 대사창 얼굴 칸에
// 통째로 띄운다. 그림(ui/illust_<이름>.png)은 오른쪽을 보게 두고 8배 크기로 만든다(문서 크기 = 화소 / 8).
enum { UI_ILLUST_NPC_FIRST = 9, UI_ILLUST_TOTAL = 14 };
bool ui_illust_size(int id, int *w, int *h);               // 문서 크기(dp). 그림이 없으면 false
bool ui_illust_image(const char *key, int id, int flip);   // 키에 그림 이름, 키_w·키_h에 문서 크기. 없으면 false(키는 빈 글)
bool ui_illust(void);
bool ui_illust_has(int id);
void ui_toggle_illust(void);
// 화면 모드(데스크톱만, 환경설정 「화면」 줄, F11, Alt+Enter). ui.ini fullscreen·window에 남는다.
bool ui_peek_display(const char *path, bool *full, int *w, int *h); // 창을 만들기 전에 저장된 값을 읽는다. 모드가 있으면 true
bool ui_display_supported(void); // 바꿀 수 있는 창인가(Android, 헤드리스는 아님)
bool ui_fullscreen(void);
void ui_set_fullscreen(bool on);
void ui_toggle_fullscreen(void);
void ui_illust_mark(int id, int x, int y, int w, int h, int flip, int clip_x, int clip_y, int clip_w, int clip_h);
void ui_probe_begin(void);                                // 화면을 보관하고 투명색으로(시험 그리기)
bool ui_probe_end(int *x, int *y, int *w, int *h);         // 시험 그리기의 테두리를 재고 화면을 되돌린다

// 키 별칭: 새 배치의 방향이 원본 키 처리와 다를 때(세로로 늘어놓은 탭을 원본은 좌우로 넘기는 등) 그 화면이 떠 있는 동안
// from 키를 to 키로 바꿔 원본에 넘긴다. 원래 키(to)는 그대로 동작한다. ui_begin~ui_end 사이에 매 틱 적는다.
// 원작 그리기(classic)에서는 문서를 채우지 않으므로 별칭도 없다.
enum { UI_KEY_UP = -1, UI_KEY_DOWN = -2, UI_KEY_LEFT = -3, UI_KEY_RIGHT = -4 };
void ui_key_alias(int from, int to);
// 세로 배치를 원본의 좌우 키로(↑=←, ↓=→, 숫자 2=4, 8=6), 가로 배치를 원본의 상하 키로(←=↑, →=↓, 4=2, 6=8)
void ui_key_alias_v2h(void);
void ui_key_alias_h2v(void);

// ---- 터치(직접 누르기) ----
// 입력은 여전히 원본 코드가 받는다. 터치는 "원본 키를 대신 눌러 주는" 방식이다. 문서 요소에 다음 속성을 단다.
//   key="ok|clear|star|hash|lsk|up|down|left|right|0~9"  누르면(손가락을 뗄 때) 그 원본 키를 한 번 보낸다(별칭 없이 그대로).
//                                    data-attr-key="k.key"처럼 값으로 넣어도 된다. 빈 값이면 누를 수 없다.
//   nav="<이름>" data-attr-idx="<번호>"  ui_nav로 알린 목록의 그 번호 항목. 누르면 커서를 그 번호로 옮기고
//                                    (틱마다 prev/next 키를 하나씩 보내 원본 커서가 닿을 때까지) 닿으면 OK(UI_NAV_ACT 목록만).
//   action="settings|etc"             호스트 동작(조작 설정 창, 기타 설정 창 열기).
//   pick="<이름>" idx="<번호>"           래퍼가 ui_pick으로 가져가 직접 처리한다(여러 단계 이동). nav와 같이 달면 nav가 알려진 동안은 nav.
//   scroll="<이름>"                   overflow-y: auto 글 상자. 끌기·휠·오른쪽 스틱으로 넘긴다(ui_scroll).
//                                    nav 목록을 이 상자 안에 다 그리면: 줄 위에서 끌어도 넘어가고, 커서가 바뀌면(키·패드) 그 줄이 보이게 호스트가 넘긴다.
//   hold="7"                          누르고 있는 동안 그 원본 키도 눌린 채로 둔다(별칭 적용). 전투 적 이름표처럼 누르는 동안만 뜻이 있는 키.
// 키 이름표는 장치에 따라 바꿔 보인다: `{{ 'ok' | key }}`, `{{ k.key | key }}`(키보드면 Enter, 패드면 A, 터치면 원래 글).
// 터치로 조작 중이면 모든 문서 body에 `touch` 클래스가, 패드면 `pad` 클래스가 붙는다(base.rcss가 키 안내를 버튼으로 바꾼다).
enum { UI_KEY_OK = -5, UI_KEY_LSK = -6, UI_KEY_CLEAR = -16 };
enum {
    UI_NAV_WRAP = 1,    // 끝에서 처음으로 넘어간다(가까운 쪽으로 돈다)
    UI_NAV_ACT = 2,     // 누르면 그 항목으로 가서 OK(탭처럼 고르기만 하는 줄은 빼고 단다)
    UI_NAV_VERT = 4,    // 세로로 늘어선 목록: 위아래로 끌면 한 줄씩 넘긴다
    UI_NAV_HORZ = 8,    // 가로로 늘어선 목록: 좌우로 끌면 넘긴다
    UI_NAV_DIRECT = 16, // 누르면 키를 흉내 내지 않고 ui_pick(목록 이름)으로 번호만 넘긴다. 래퍼가 원본 커서를 직접 쓰고,
                        // UI_NAV_ACT 목록이면 고른 줄을 누른 것(이미 그 줄)일 때 OK도 래퍼가 보낸다(ui_key_tap).
};
// 이 문서의 목록 하나를 알린다(ui_begin~ui_end 사이, 그 상태에서 고를 수 있는 동안 매 틱). cursor는 0..count-1,
// prev/next는 원본이 받는 키(UI_KEY_UP 등, 별칭 적용 전 원본 키)다. 커서가 없는 상태(빈 목록)면 cursor = -1.
void ui_nav(const char *name, int cursor, int count, int prev_key, int next_key, int flags);
// 터치 패드 표시. 기본은 "문서에 누를 것(key/nav)이 보이면 숨기고, 없으면 필드 패드". 원본이 직접 그리는 선택
// (전투 대상 화살표 등)은 UI_PAD_DIR, 입력을 받지 않는 연출 동안 숨기려면 UI_PAD_NONE을 매 틱 적는다.
// UI_PAD_KEYS: OK와 취소(CLEAR)만(전투 내내. 메뉴를 문서로 눌러도 OK·취소가 늘 같은 자리에 있게).
enum { UI_PAD_AUTO = 0, UI_PAD_NONE = 1, UI_PAD_DIR = 2, UI_PAD_FIELD = 3, UI_PAD_KEYS = 4 };
void ui_pad(int mode);

// 호스트(main.c) 쪽
struct SDL_Renderer;
struct SDL_Window;
union SDL_Event;
bool ui_init(struct SDL_Renderer *ren, const char *ui_dir);
void ui_init_input(struct SDL_Window *win, const char *config_path, bool force_pad); // 터치 패드·설정(ui_init 뒤)
bool ui_event(const union SDL_Event *e); // 터치, 마우스, 설정 창 키. 처리했으면 true
void ui_input_tick(void);                // 메인 루프마다: 터치로 쌓인 키를 틱마다 하나씩 보낸다
bool ui_modal(void);                     // 조작 설정 창이 떠 있다(게임 입력을 막는다)
void ui_toggle_settings(void);
void ui_toggle_etc(void);                 // 기타 설정 창(전투 보상 배율). 문서의 action="etc", F3
// 세이브 데이터 내보내기/불러오기(host/savefile.c, 창은 ui/savedata.rml). 문서의 action="savedata".
void ui_toggle_savedata(void);
void ui_savedata_say(const char *msg);          // 결과 알림(UTF-8)
void ui_savedata_ask(int in_file, int have);    // 덮어쓰기 경고: 파일의 슬롯 수, 지금 슬롯 수. 답은 savefile_import_answer
void savefile_choose(bool import);              // 운영체제 파일 창을 연다
void savefile_import_answer(bool yes);
bool savefile_dialog_open(void);
bool savefile_poll(void);                       // 메인 루프마다. true면 게임을 다시 시작해야 한다(불러오기 끝)
int savefile_slot_count(void);                  // 저장 폴더의 슬롯 파일 수
void savefile_set_window(struct SDL_Window *w);
int ui_rate(int kind);                   // 기타 설정의 배율(%): 0 경험치, 1 골드, 2 드랍률. 기본 100
void ui_release_all(void);               // 눌린 패드 버튼을 모두 뗀다(백그라운드로 갈 때)
bool ui_pad_capturing(void);             // 게임패드 키 설정이 다음 버튼을 기다린다
bool ui_pad_capture(int input);
enum { UI_NAV_UP, UI_NAV_DOWN, UI_NAV_LEFT, UI_NAV_RIGHT, UI_NAV_OK, UI_NAV_BACK };
void ui_pad_nav(int nav);                // 설정 창을 게임패드로 움직인다
// 대사 자동 넘김(AUTO, ui.ini에 남는다)과 빨리 넘기기(SKIP, 선택지가 뜨거나 대사가 끝나면 꺼진다).
// 문서의 action="auto"/"skip", 키보드 Tab/Ctrl, 게임패드(키 설정에서 붙인 버튼). restored/ui_dialog.c가 읽어 OK를 보낸다.
// 자동 전투(R)·전투 2배속(F): restored/autobattle.c가 읽는다. 문서의 action="bauto"/"bfast", 게임패드는 키 설정에서 붙인 버튼.
// ui_human_ms는 사람이 마지막으로 게임 키를 넣은 때(now_ms 기준). 래퍼가 ui_key_tap으로 넣은 키는 세지 않는다.
bool ui_battle_auto(void);
bool ui_battle_fast(void);
void ui_toggle_battle_auto(void);
void ui_toggle_battle_fast(void);
void ui_note_human(void);
uint64_t ui_human_ms(void);
bool ui_dialog_auto(void);
bool ui_dialog_skip(void);
void ui_toggle_auto(void);
void ui_toggle_skip(void);
// scroll="<이름>" 글 상자를 dp만큼 넘긴다(name이 NULL이면 위에 보이는 첫 글 상자). 없으면 false.
// 손가락 끌기, 휠, 게임패드 오른쪽 스틱은 호스트가 알아서 넘긴다.
bool ui_scroll(const char *name, float dp);
// scroll="<이름>" 상자 안의 idx="<번호>" 요소가 보이게 최소한으로 넘긴다(키로 커서를 옮긴 목록). 없으면 false.
bool ui_scroll_show(const char *name, int idx);
// 세로 목록 공통 조작(ui/README.md '세로 목록'). main.c send_key가 새로 누른 키마다 부른다: 지금 고르는 세로 목록의 끝에서
// 더 나가는 키면 반대쪽 끝으로 옮기고 true(그 키는 원본에 보내지 않는다). 누르고 있는 동안의 반복은 원본이 하므로 돌지 않는다.
bool ui_wrap_key(int code);
// 게임패드 어깨 버튼(which 0 LB, 1 RB, 2 LT, 3 RT, code는 그 버튼에 묶인 원본 키). 필드 메뉴 틀이 떠 있으면 틀이 이긴다(위 ui_input_source 앞 설명).
// 그 밖의 메뉴에서 LT/RT는 쪽 넘김, LB/RB는 문서가 shoulder="prev|next"로 정한 요소(탭·캐릭터 넘기기)를 누른다.
// 화면이 그 원본 키를 쓰면(보이는 key/hold/keyhint) false: 원본 키로 보낸다.
bool ui_pad_shortcut(int which, int code);
// 게임패드 버튼 빌려 쓰기: 보이는 문서의 요소에 padhot="<동작 id>"(host/gamepad.c ACTIONS의 id: star, hash, menu, nine …)를 달면
// 그 요소가 보이는 동안 그 동작에 묶인 버튼은 원본 키를 보내지 않고 그 요소(action/pick/key)를 누른다. 원본이 그 키를 쓰지 않는
// 화면에서만 단다(대사 중 X·Y = AUTO·SKIP, 전투 중 Y·START = AUTO·x2, 필드 R3 = 퀵세이브). 이름표는 `{{ 'star' | key }}`나
// 원본 키로 가릴 수 없는 버튼이면 `{{ 'menu' | padkey }}`(패드일 때만 글이 나온다). 패드 전용 안내는 .ponly로 감싼다(base.rcss).
bool ui_pad_hot(const char *action_id);
// hotkey="<원본 키 이름>"(ok clear up down 0 star …): 그 요소가 보이는 동안 그 원본 키(키보드·패드 어느 쪽이든)는 원본에 가지 않고
// 요소(pick/action/key)를 누른다. 원본이 그 키를 쓰지 않는 화면이나, 원본 입력을 통째로 가로채는 자체 창(대사 기록)에 쓴다.
bool ui_hot_key(int code);
// 호스트 동작(문서의 action과 같은 이름: settings, etc, savedata …)을 다음 메인 루프에서 한다. 래퍼는 게스트 틱 안에서 돌므로
// 설정 창을 바로 열면 안 된다(눌린 패드 버튼을 떼며 게스트를 다시 들어간다).
void ui_action_later(const char *action);
// 필드 메뉴 틀(restored/fm_shell.c, 문서 fm_shell)이 떠 있으면 틀이 먼저다: LB/RB는 [rail-shoulder="prev|next"], LT/RT는
// [char-shoulder="prev|next"] 요소를 누른다(LT/RT는 그 요소가 없으면 아래 규칙). 키보드 Tab/Shift+Tab은 pick "railstep" 1/0,
// [ ]는 pick "charstep" 0/1, 마우스 오른쪽 버튼은 CLR(메뉴 장면에서만).
enum { UI_SRC_KEYBOARD = 0, UI_SRC_TOUCH = 1, UI_SRC_PAD = 2 };
int ui_input_source(void);               // 마지막에 쓴 조작 장치(키 이름표가 따르는 것)
bool ui_doc_visible(const char *name);   // 그 문서가 지금 화면에 떠 있다(지난 틱에 채웠다)
void ui_push_pick(const char *name, int idx); // 키보드 단축키 등 문서 밖에서 pick 요소를 누른 것으로 친다(F5 퀵세이브)
int ui_pick(const char *name);            // pick="<이름>" 요소를 눌렀으면 그 idx(한 번 가져가면 지운다), 없으면 -1
bool ui_key_tap(int code);               // 대기열이 비었으면 원본 키 하나를 넣는다(true). 래퍼가 쓰는 자동 입력
bool ui_tap_selector(const char *sel);   // 시험용(ctrl `tap <선택자>`): 보이는 문서에서 찾은 요소를 누른 것으로 친다
void ui_set_modern(bool on);
void ui_reload(void);                 // 문서와 스타일을 다시 읽는다(개발용, ctrl `uireload`)
void ui_present(void);                // 게임이 한 프레임을 표시했다: 이번 틱에 채운 문서만 남긴다
bool ui_active(void);
int ui_key_map(int code);             // 지금 보이는 문서의 키 별칭을 적용한 키(main.c send_key가 누를 때 부른다)
void ui_render(float x, float y, float w, float h); // 게임 화면이 놓인 자리(출력 화소)에 그린다
void ui_cap_add(int x, int y, unsigned color, const char *s, int len); // gfx.c DrawString이 부른다
extern bool ui_capturing;
#ifdef __cplusplus
}
#endif
