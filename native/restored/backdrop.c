// 넓은 화면의 연출용 뒷배경. 원본 0x38418을 감쌌다.
//
// 필드 장면 상태(장면 + 0x18)가 5이면(이벤트 스크립트 명령 0xcc94가 켠다) 필드는 뒷배경 레이어를 먼저 통째로 깔고(0xcdf0 -> 0x38418)
// 그 위에 맵을 투명 타일을 건너뛰며 그린다(0xcab0, 0x2ee2c의 세 번째 인자 1). 이 상태에서는 0x381a0을 쓰지 않는다.
// 0x38418은 레이어를 보기 사각형 가운데에 놓고 레이어 폭(예: 25칸, 350px)만 그린다. 240 폭에서는 보기를 다 덮지만
// 넓은 화면에서는 양옆이 비고, 그 자리의 투명 타일에는 지난 프레임이 남는다.
// 레이어가 보기보다 좁으면 원본을 부르기 전에 양옆 빈 칸을 레이어를 되풀이해 채운다(fieldbg.c와 같은 이유로 거울 대신 되풀이).
// 그 뒤 원본이 가운데 레이어와 빛줄기, 그림을 그대로 그린다. 레이어가 보기 이상으로 넓으면 원본만 부른다.
#include "restored.h"

static int wrap(int i, int n) { return (i % n + n) % n; }

#define GAME_VIEW VG(GAME_VIEW_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
enum {
    TILE = 14,
    FN_SET_CLIP = 0x103900,     // (gfx, x, y, w, h)
    FN_RESTORE_CLIP = 0x103994, // (gfx)
    FN_DRAW_TILE = 0x12e068,    // (gc, x, y, 타일, 맵 그리기 정보)

    LAYER_OWNER = 0x00, LAYER_COLS = 0x10, LAYER_ROWS = 0x14, LAYER_DATA = 0x18,
    OWNER_GAME = 0x00,
    GAME_GFX = 0x08, GAME_VIEW_D5 = 0x568, // 필드 보기 사각형(x, y, w, h)
    GFX_TILE_MODE = 0x34,               // 원본이 타일을 그리는 동안 바꿔 두는 값(0x3f0c)
    TILE_MODE = 0xd0745800,
    HORIZON = 210,                      // 레이어의 이 줄이 보기 높이의 83/128 자리에 온다
};

// 0x38418(레이어, gc, 밝기)
void orig_f_38418(cpu_t *c);
void f_38418(cpu_t *c) {
    uint32_t layer = c->r[0], gc = c->r[1], arg2 = c->r[2];
    uint32_t game = R32(R32(layer + LAYER_OWNER) + OWNER_GAME), gfx = R32(game + GAME_GFX);
    int vx = (int)R32(game + GAME_VIEW), vy = (int)R32(game + GAME_VIEW + 4);
    int vw = (int)R32(game + GAME_VIEW + 8), vh = (int)R32(game + GAME_VIEW + 12);
    int cols = (int)R32(layer + LAYER_COLS), rows = (int)R32(layer + LAYER_ROWS);
    uint32_t data = R32(layer + LAYER_DATA);

    if (data && cols > 0 && cols * TILE < vw) {
        int left = vx + (vw - cols * TILE) / 2; // 원본이 레이어 0번 칸을 놓는 x
        int cam_y = HORIZON - (vh * 83 >> 7), row = cam_y / TILE, y = row * TILE - cam_y;
        if (row < 0) { y -= row * TILE; row = 0; }
        y += vy;
        uint32_t saved = R32(gfx + GFX_TILE_MODE);
        W32(gfx + GFX_TILE_MODE, TILE_MODE);
        CALL(FN_SET_CLIP, gfx, (uint32_t)vx, (uint32_t)vy, (uint32_t)vw, (uint32_t)vh);
        for (; row < rows; row++, y += TILE) {
            if (y >= vy + vh) break;
            uint32_t line = data + (uint32_t)(row * cols) * 2;
            for (int col = -1, x = left - TILE; x + TILE > vx; col--, x -= TILE)
                CALL(FN_DRAW_TILE, gc, (uint32_t)x, (uint32_t)y, R16(line + (uint32_t)wrap(col, cols) * 2), 0);
            for (int col = cols, x = left + cols * TILE; x < vx + vw; col++, x += TILE)
                CALL(FN_DRAW_TILE, gc, (uint32_t)x, (uint32_t)y, R16(line + (uint32_t)wrap(col, cols) * 2), 0);
        }
        CALL(FN_RESTORE_CLIP, gfx);
        W32(gfx + GFX_TILE_MODE, saved);
    }
    // 원본은 레이어를 그리기 전에 맵 밖 여백(0x2ec68 -> 0x2f7c4, 맵 폭 기준)을 검게 칠하고, 레이어 뒤에 노을 빛 그라데이션을
    // 보기 폭 전체에 섞어 얹는다. 좌우를 칠하게 두면 위에서 채운 칸이 지워지고 검은 바탕에 그라데이션만 남아 가운데와 어긋난다.
    extern bool widemap_no_side_fill;
    bool filled = data && cols > 0 && cols * TILE < vw;
    if (filled) widemap_no_side_fill = true;
    c->r[0] = layer, c->r[1] = gc, c->r[2] = arg2;
    orig_f_38418(c);
    if (filled) widemap_no_side_fill = false;
}
