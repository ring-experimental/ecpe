// 리마스터 일러스트: 합성 프레임 그리기 0x56ec(시트, 프레임, x, y, dx, dy, 뒤집기)를 감싼다.
// 새 UI가 그림을 떼어 내는 동안(scratch) UI 시트의 초상화 프레임 60~68이 그려지면, 같은 인자로 화면 전체 클립에서 한 번 더
// 시험 그리기를 해 프레임이 놓인 테두리를 재고 ui_illust_mark로 알린다. 그 자리를 떼는 ui_sprite가 고해상도 그림으로 바꾼다
// (host/ui.cpp illust_sprite). 실제 그리기는 원본 그대로 한 번 한다(원래 클립, trim이 그 화소로 크기를 잰다).
// 원작 그리기(F2)나 일러스트를 끈 때, 다른 시트·프레임은 그대로 지나간다.
#include "restored.h"
#include <string.h>

#define G32(a) R32_(&cpu, (a))
enum {
    APP_PTR = 0x14ee34, APP_GAME = 0x48,
    GAME_GFX = 0x08, GAME_MENU = 0x1d4, MENU_SHEET = 0x14,
    GFX_CLIP = 0xdc,            // {x, y, w, h}
    FRAME_FIRST = 60,           // 아크, 니하트, 제레미, 쟈이언트, 이오, 라즈파샤, 위지윅, 어린 니하트(67), 키리엔(68)
};

void orig_f_56ec(cpu_t *c);
void f_56ec(cpu_t *c) {
    int id = (int)c->r[1] - FRAME_FIRST;
    if (!ui_capturing || id < 0 || id >= UI_ILLUST_COUNT || !ui_illust() || !ui_illust_has(id)) { orig_f_56ec(c); return; }
    uint32_t G = G32(G32(APP_PTR) + APP_GAME);
    if (!G || c->r[0] != G32(G32(G + GAME_MENU) + MENU_SHEET)) { orig_f_56ec(c); return; }
    uint32_t regs[16], clip[4], gfx = G32(G + GAME_GFX);
    memcpy(regs, c->r, sizeof regs);
    int flip = (int)(G32(c->SP + 8) & 1);
    for (int i = 0; i < 4; i++) clip[i] = G32(gfx + GFX_CLIP + 4u * (uint32_t)i);
    W32_(&cpu, gfx + GFX_CLIP, 0); W32_(&cpu, gfx + GFX_CLIP + 4, 0);
    W32_(&cpu, gfx + GFX_CLIP + 8, (uint32_t)ui_screen_w()); W32_(&cpu, gfx + GFX_CLIP + 12, 320);
    ui_probe_begin();
    orig_f_56ec(c);
    int x, y, w, h;
    bool ok = ui_probe_end(&x, &y, &w, &h);
    for (int i = 0; i < 4; i++) W32_(&cpu, gfx + GFX_CLIP + 4u * (uint32_t)i, clip[i]);
    memcpy(c->r, regs, sizeof regs);
    orig_f_56ec(c);
    if (ok) ui_illust_mark(id, x, y, w, h, flip, (int)clip[0], (int)clip[1], (int)clip[2], (int)clip[3]);
}
