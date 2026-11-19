// 넓은 화면의 필드 뒷배경. 원본 0x381a0을 옮겼다.
//
// 필드 맵은 투명 타일(비트 15) 자리에 뒷배경 레이어(하늘 따위)를 비춰 그린다. 필드는 매 프레임 화면 전체를 그리지 않으므로
// 뒷배경이 닿지 않는 투명 자리는 지난 프레임이 남는다. 뒷배경은 화면 왼쪽 x=0부터 그 레이어 폭(예: 25칸, 350px)만큼만 그려지는데,
// 240 폭에서는 넉넉하지만 넓은 화면에서는 오른쪽이 모자라 스크롤할 때 절벽 같은 타일이 끌린 잔상이 남는다.
// 레이어 폭이나 높이를 넘는 칸은 레이어를 되풀이해 그리는 범위 끝까지 채운다. 원본이 덮던 칸은 그대로다.
// 거울로 접으면 타일 그림 자체는 뒤집히지 않아 구름이 칸마다 끊기므로 되풀이를 쓴다.
#include "restored.h"

static int wrap(int i, int n) { return (i % n + n) % n; }

enum {
    TILE = 14,
    FN_SET_CLIP = 0x103900,     // (gfx, x, y, w, h)
    FN_RESTORE_CLIP = 0x103994, // (gfx)
    FN_SEE_THROUGH = 0x12ed24,  // (필드, gc, x, y) -> 이 자리 앞 타일이 투명한가
    FN_DRAW_TILE = 0x12e068,    // (gc, x, y, 타일, 맵 그리기 정보)

    LAYER_OWNER = 0x00, LAYER_COLS = 0x10, LAYER_ROWS = 0x14, LAYER_DATA = 0x18,
    OWNER_GAME = 0x00, OWNER_MAP_W = 0x78, OWNER_MAP_H = 0x7c, // 앞 맵의 픽셀 크기
    GAME_GFX = 0x08, GAME_VIEW = 0x568,                         // 필드 보기 사각형(x, y, w, h)
};

// 0x381a0(레이어, gc, x, y, 오른쪽, 아래, 맵 그리기 정보): x, y는 앞 맵의 첫 타일 위치(1칸 미만 음수일 수 있다).
void f_381a0(cpu_t *c) {
    uint32_t layer = c->r[0], gc = c->r[1];
    int xoff = (int)c->r[2], yoff = (int)c->r[3];
    int right = (int)R32(c->SP), bottom = (int)R32(c->SP + 4);
    uint32_t info = R32(c->SP + 8);
    uint32_t owner = R32(layer + LAYER_OWNER), game = R32(owner + OWNER_GAME), gfx = R32(game + GAME_GFX);
    int view_w = (int)R32(game + GAME_VIEW + 8);
    int cols = (int)R32(layer + LAYER_COLS), rows = (int)R32(layer + LAYER_ROWS);
    uint32_t data = R32(layer + LAYER_DATA);

    if (right > (int)R32(owner + OWNER_MAP_W) + xoff) right = (int)R32(owner + OWNER_MAP_W) + xoff;
    if (bottom > (int)R32(owner + OWNER_MAP_H) + yoff) bottom = (int)R32(owner + OWNER_MAP_H) + yoff;

    // 레이어가 보기보다 넓으면 가운데 부분부터 그린다.
    int col0 = (cols * TILE - view_w) / 2;
    col0 = col0 < 0 ? 0 : col0 / TILE;
    int x0 = 0, y0 = 0, row = 0;
    if (xoff < 0) { int q = xoff / TILE; x0 = q * TILE; col0 += q; }
    if (yoff < 0) { row = yoff / TILE; y0 = row * TILE; }

    CALL(FN_SET_CLIP, gfx, (uint32_t)xoff, (uint32_t)yoff, (uint32_t)(right - xoff), (uint32_t)(bottom - yoff));
    for (int y = y0; row < rows || y < bottom; row++, y += TILE) {
        for (int col = col0, x = x0; col < cols || x < right; col++, x += TILE) {
            if (CALL(FN_SEE_THROUGH, owner, gc, (uint32_t)x, (uint32_t)y)) {
                uint32_t t = R16(data + (uint32_t)(wrap(row, rows) * cols + wrap(col, cols)) * 2);
                CALL(FN_DRAW_TILE, gc, (uint32_t)x, (uint32_t)y, t, info);
            }
            if (x >= right) break;
        }
        if (y >= bottom) break;
    }
    CALL(FN_RESTORE_CLIP, gfx);
}
