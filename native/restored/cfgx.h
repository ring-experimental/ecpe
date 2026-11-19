// 환경설정의 원작에 없는 줄(타이틀 ui_title.c와 필드 옵션 ui_system.c가 같이 쓴다). restored/cfgx.c
#pragma once
int cfgx_count(void);            // 지금 둘 수 있는 줄 수(「화면」은 데스크톱 창에서만)
int cfgx_sel(void);              // 커서가 있는 가상 줄(0..), 없으면 -1
void cfgx_reset(void);           // 환경설정을 벗어났다
// 이 틱의 키. cur는 원본 커서, last는 새 목록에서 원본 줄의 마지막 번호. 1이면 가져갔다(원본에는 키 없이 한 틱).
// 가상 줄에서 원본 줄로 돌아가면 *set_cur에 갈 원본 커서(아니면 -1).
int cfgx_key(int up, int down, int lr, int ok, int cur, int last, int *set_cur);
const char *cfgx_name(int i);    // UTF-8
const char *cfgx_action(int i);  // 여는 줄이면 호스트 action 이름("settings" 등), 값 줄이면 ""
int cfgx_value(int i, const char **on_t, const char **off_t); // 값 줄: 켜짐(1)/꺼짐(0)과 두 값의 이름
