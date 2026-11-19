// 항목별 글 공용(restored/help.c). 문서는 같은 값 이름(d.hsections, d.hparas, d.hpage …)과 ui/help.rcss를 쓴다.
#pragma once
enum { HELP_MAIN, HELP_INTRO, HELP_COUNT }; // 도움말(옵션·타이틀 공용), 게임속으로(타이틀)
// 지금 문서(ui_begin~ui_end 사이)에 그 글의 값을 채운다: hsections[t, cur, i], hparas[ty(0 보통, 1 ◈ 소제목, 2 ☞ 항목, 3 하위 줄), t],
// hsection, hpage(1부터), hpages, hprev/hnext, hprev_i/hnext_i.
void help_emit(int which);
// 입력: 이 틱의 좌우(dx: 항목), 위아래(dy: 글 상자 넘기기)와 문서의 pick="help"(항목 고르기).
void help_input(int which, int dx, int dy);
