// 필드 메뉴 틀(셸). 모드 1(필드에서 연 일반 메뉴)의 화면 0~7에 같은 틀을 둔다: 왼쪽 레일(메뉴 7개 + 소지금·시간·장소),
// 캐릭터 화면(스킬·장비·스테이터스) 위의 캐릭터 띠, 모든 화면 아래의 고정 버튼 띠. 설명은 restored/fm_shell.c 머리와 ui/README.md '필드 메뉴 틀'.
#pragma once
#include <stdint.h>

// 이번 틱에 틀을 그렸다(f_2faa4가 화면 그리기 앞에서 정한다). 화면 래퍼는 이것으로 틀 배치를 고른다.
int fm_shell_on(void);
// 분배 0x2faa4가 부른다: 틀 문서를 채우고(화면 문서보다 아래에 깔린다), 화면 그리기가 끝나면 fm_shell_end.
void fm_shell_emit(uint32_t menu);
void fm_shell_end(void);
// 화면 문서에 틀 배치 값(shell, chars)을 적는다(ui_begin~ui_end 사이).
void fm_doc_frame(uint32_t menu);
// 틀 안에 뜨는 자체 창(강화·귀환·망각의 물약·프리미엄 상점)의 문서에 틀 값을 적고, 틀 안이면 아래 화면 문서를 감춘다(ui_begin~ui_end 사이).
// 틀 안이면 1(그때 창은 고정 버튼 띠 <fmbar/>를 쓰고 fm_bar_*로 채운다).
int fm_overlay_frame(uint32_t menu);

// 원본 필드 메뉴 키 분배 0x2fa60을 지금 동기로 부른다(게임 키 코드: 위 0x1f 아래 0x1e 왼 0x1d 오른 0x1c OK 0x1b CLR 0x10,
// 숫자 0x50+n, * 0x4a, # 0x43). 틀의 훅(이전 규칙, 화면 몰기)은 건너뛴다. 레지스터를 덮으므로 부르는 쪽이 cpu를 챙긴다.
void fm_key_raw(uint32_t menu, int gcode);
enum { FMK_UP = 0x1f, FMK_DOWN = 0x1e, FMK_LEFT = 0x1d, FMK_RIGHT = 0x1c, FMK_OK = 0x1b, FMK_CLR = 0x10, FMK_STAR = 0x4a,
       FMK_0 = 0x50, FMK_4 = 0x54, FMK_5 = 0x55, FMK_6 = 0x56 };

// 마지막으로 본 캐릭터(파티 칸 0~4). 없거나 파티에서 빠졌으면 첫 칸.
int fm_last_char(uint32_t game);

// 스킬 화면을 '목록' 상태(2, 동작 = 사용)로 몰아 둔다(분류·동작 고르기 단계를 화면에서 없앤다). 스킬 그리기 앞에서 부른다.
void fm_skill_normalize(uint32_t menu);
// 「단축키」 버튼: 목록 → 동작 '장착' → 3/6/9 대기(상태 5)까지 한 번에.
void fm_skill_hotkey(uint32_t menu);
// 분류(0 일반, 1 조합, 2 필살기)를 바꾸고 목록 상태로 둔다(분류 고르기 상태로 CLR → 분류 → 다시 목록).
void fm_skill_set_cat(uint32_t menu, int cat);

// ---- 고정 버튼 띠(문서의 <fmbar/>, 값 d.bar) ----
// 오른쪽 끝 「이전」(홈은 「닫기」), 그 왼쪽 「결정」은 늘 같은 자리. 왼쪽 보조 버튼은 화면마다 칸 수·순서가 고정이다.
// key는 누르면 보낼 원본 키(호스트 코드: UI_KEY_OK, '0', '*' …), fn이 있으면 그것을 부른다(여러 단계를 한 번에 하는 버튼).
// kl은 키 이름표로 보일 키 이름(문서의 `| key`, "ok" "clear" "0" "star" …). 빈 글이면 이름표가 없다.
enum { FMB_EX = 4 };
typedef void (*fm_fn)(uint32_t menu);
void fm_bar_begin(void);
void fm_bar_ex(int i, const char *label, int on, int key, fm_fn fn, const char *kl);
void fm_bar_ok(const char *label, int on);
// 방향 키 안내(UTF-8, "←→ 분류"처럼 짧게). 보조 버튼 왼쪽에 키보드·게임패드일 때만 보인다. fm_bar_begin이 지운다.
void fm_bar_hint(const char *utf8);
void fm_bar_emit(uint32_t menu); // ui_obj("bar")를 쓰고 누름 처리용 표를 남긴다
