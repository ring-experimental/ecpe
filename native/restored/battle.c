// 넓은 화면의 전투 배경. 원본 0x3892c(맵 그리기)를 옮기고 0x205c4(갱신 격자 만들기)를 감쌌다.
//
// 전투 무대는 14px 타일 23칸(322px) 맵이다. 화면이 더 넓으면 맵 카메라 x가 음수가 되고 양옆이 빈다.
// 전투는 매 프레임 전부 그리지 않고 갱신 격자(맵 크기의 비트맵)에 표시된 타일, 곧 스프라이트가 지나간 자리만 다시 그린다.
// 캐릭터와 데미지 숫자, 효과는 화면 폭 비율로 놓이므로 빈 양옆에도 그려지는데, 그곳은 격자 밖이라 지워지지 않아 잔상이 남는다.
//
// 그래서 전투 맵을 읽을 때(0x1b1d0, 이 격자를 만드는 유일한 곳) 맵이 화면보다 좁으면 격자를 양옆으로 E칸씩 넓히고,
// 그 격자로 그릴 때는 넓힌 칸에 맵 끝을 거울처럼 접은 타일을 좌우로 뒤집어 그린다. 격자를 읽는 곳은 이 그리기 함수뿐이다(0x2077c).
// 폭이 322 이하이면 E가 0이라 원본과 같다. 필드는 다른 격자를 쓰므로 원본 그대로다.
#include "restored.h"

enum {
    TILE = 14,
    FN_NO_MAP = 0x10467c,       // (0, 0, 0)
    FN_RNG = 0x1144b0,          // () -> 난수기
    FN_RAND = 0x1067a8,         // (난수기, n) -> 0..n-1
    FN_DIRTY = 0x12077c,        // (격자, 열, 행) -> 이번이나 지난 프레임에 표시됐나
    FN_DIRTY_SAVE = 0x120764,   // (격자) 이번 표시를 지난 표시로 옮긴다
    FN_DIRTY_CLEAR = 0x1207b0,  // (격자) 이번 표시를 지운다
    FN_TILE = 0x12e0a8,         // (타일셋, x, y, 타일, gc)
    FN_TILE_OVER = 0x12e2ac,    // 0x2000이 붙은 타일
    TILE_OVER = 0x2000,
    TILE_HFLIP = 0x800,         // 타일 그림을 좌우로 뒤집는다(0x2df44). 0x2000 타일이 곁들이는 옆 칸의 방향도 같이 바뀐다(0x2e2f8)
    GC_SHAKE = 0x1000000,       // gc 첫 워드: 이번 한 번 화면을 흔든다

    // 맵 그리기 객체(장면 + 0x6c)
    MAP_GAME = 0x00, MAP_SCENE = 0x04, MAP_CAM_X = 0x18, MAP_CAM_Y = 0x1c, MAP_W = 0x20, MAP_H = 0x24, MAP_DATA = 0x28,
    MAPDATA_TILES = 0x0c,       // u16 타일, 행 우선
    GAME_GFX = 0x08, GAME_TILESET = 0x18c,
    GFX_CLIP_W = 0xe4, GFX_CLIP_H = 0xe8,
    SCENE_GRID = 0xb8,
    // 갱신 격자. 표시하는 쪽(0x20680)은 화면 좌표에 OFF_X를 더해 칸을 고른다.
    GRID_GFX = 0x04, GRID_COLS = 0x18, GRID_OFF_X = 0x20,
};

// 넓힌 격자와 그 격자로 처음 그린 맵 데이터. 같은 객체 자리가 다른 격자나 맵에 다시 쓰이면 원본대로 그린다.
static uint32_t wide_grid, wide_data;
static int wide_e;

// 0x205c4(격자, off_x, off_y, 열, 행[스택]): 맵을 읽을 때(0x1b1d0) 한 번 만든다. 격자의 gfx는 생성자(0x20568)가 +4에 넣었다.
// off_x는 맵 카메라 x이고 음수면 맵이 화면보다 좁다. 그때 맵은 화면 -off_x에서 시작하므로 양옆 여백을 덮도록 E칸씩 넓힌다.
void orig_f_205c4(cpu_t *c);
void f_205c4(cpu_t *c) {
    int off_x = (int)c->r[1], cols = (int)c->r[3];
    int scr_w = (int)R32(R32(c->r[0] + GRID_GFX) + GFX_CLIP_W);
    wide_grid = 0;
    if (off_x < 0) {
        int left = -off_x, right = scr_w - cols * TILE - left, side = left > right ? left : right;
        int e = (side + TILE - 1) / TILE;
        c->r[1] = (uint32_t)(off_x + e * TILE);
        c->r[3] = (uint32_t)(cols + 2 * e);
        wide_grid = c->r[0], wide_data = 0, wide_e = e;
    }
    orig_f_205c4(c);
}


// 0x3892c(맵, gc): 보이는 타일 가운데 격자에 표시된 것을 그린다. gc가 흔들기면 이번만 ±3px 옮긴다.
void f_3892c(cpu_t *c) {
    uint32_t map = c->r[0], gc = c->r[1];
    uint32_t game = R32(map + MAP_GAME), grid = R32(R32(map + MAP_SCENE) + SCENE_GRID);
    uint32_t gfx = R32(game + GAME_GFX), tileset = R32(game + GAME_TILESET);
    int scr_w = (int)R32(gfx + GFX_CLIP_W), scr_h = (int)R32(gfx + GFX_CLIP_H);
    int map_w = (int)R32(map + MAP_W), map_h = (int)R32(map + MAP_H);
    int cam_x = (int)R32(map + MAP_CAM_X), cam_y = (int)R32(map + MAP_CAM_Y);
    int col0 = 0, row0 = 0, x0 = 0, y0 = 0;
    if (cam_x >= 0) col0 = cam_x / TILE; else x0 = (scr_w - map_w * TILE) / 2;
    if (cam_y >= 0) row0 = cam_y / TILE; else y0 = (scr_h - map_h * TILE) / 2;

    // 0x205c4가 넓힌 격자이면 양옆 E칸도 그린다. 이때 맵은 격자가 표시하는 위치, 곧 화면 -카메라 x에 놓는다.
    // 원본은 카메라가 음수이면 맵만 가운데로 옮겨 그려 표시(카메라 기준)와 어긋난다. 폭 322 이하만 생각한 것이다.
    // ge는 격자 칸 번호의 밀림, e는 실제로 그릴 양옆 칸 수다.
    uint32_t data = R32(map + MAP_DATA);
    int ge = grid && grid == wide_grid && (int)R32(grid + GRID_COLS) == map_w + 2 * wide_e ? wide_e : 0, e = 0;
    if (ge && cam_x < 0) {
        if (!wide_data) wide_data = data;
        if (data == wide_data) e = ge;
    }
    if (e) x0 = e * TILE - (int)R32(grid + GRID_OFF_X);

    uint32_t saved_gc[5];
    if (gc) {
        for (int i = 0; i < 5; i++) saved_gc[i] = R32(gc + i * 4);
        if (saved_gc[0] & GC_SHAKE) {
            uint32_t rng = CALL(FN_RNG);
            int dx = (int)CALL(FN_RAND, rng, 7), dy = (int)CALL(FN_RAND, rng, 7);
            x0 += dx - 3;
            W32(gc, saved_gc[0] & ~(uint32_t)GC_SHAKE);
            y0 += dy - 3;
        }
    }

    if (!data) {
        CALL(FN_NO_MAP, 0, 0, 0);
    } else {
        for (int row = row0, y = y0; row < map_h; row++) {
            uint32_t line = data + MAPDATA_TILES + (uint32_t)(row * map_w) * 2;
            for (int col = col0 - e, x = x0 - e * TILE; col < map_w + e; col++, x += TILE) {
                if (x + TILE <= 0) continue;
                if (e && x >= scr_w) break;
                if (CALL(FN_DIRTY, grid, (uint32_t)(col + ge), (uint32_t)row)) {
                    // 접은 칸은 타일 그림도 뒤집어야 사선 절벽과 구름이 맵 끝에서 이어진다. 두 번 접힌 칸은 다시 바로 선다.
                    int src = fold(col, map_w);
                    uint32_t t = R16(line + (uint32_t)src * 2);
                    if (((col % (2 * map_w)) + 2 * map_w) % (2 * map_w) >= map_w) t ^= TILE_HFLIP;
                    CALL(t & TILE_OVER ? FN_TILE_OVER : FN_TILE, tileset, (uint32_t)x, (uint32_t)y, t, gc);
                }
                if (!e && x + TILE >= scr_w) break;
            }
            y += TILE;
            if (y >= scr_h) break;
        }
    }
    CALL(FN_DIRTY_SAVE, grid);
    CALL(FN_DIRTY_CLEAR, grid);
    if (gc) for (int i = 0; i < 5; i++) W32(gc + i * 4, saved_gc[i]);
}

