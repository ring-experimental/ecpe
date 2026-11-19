// 넓은 화면의 연출용 좁은 맵. 원본 필드 맵 그리기 0x2ee2c와 맵 밖 여백 칠하기 0x2f7c4를 감쌌다.
//
// 이벤트 장면 가운데 240 폭에 맞춘 좁은 맵(22~23칸, 308~322px)을 쓰는 곳이 있다. 넓은 화면에서는 맵이 가운데 놓이고 양옆 여백은
// 0x2f7c4가 검게 칠한다. 두 장면에서 이것이 눈에 띄게 어긋난다.
//  * 불꽃놀이(맵 88, 스크립트 5의 명령 0xb4~0xb7): 불꽃은 보기 사각형 전체를 클립으로 그리고, 꺼질 때는 화면을 지우지 않고 점을
//    밤하늘 색까지 어둡게 덧칠해 없앤다(0x14a30/0x14a68, 맵은 G+0x1c 비트 0이 켜진 틱에만 다시 그린다). 맵 밖 검은 여백에 떨어진 점은
//    하늘색 점으로 남는다. 이 맵은 가로로 되풀이되는 그림(밤하늘, 성벽)이라 여백을 검게 칠하지 않고 맵을 좌우로 되풀이해 채운다.
//    카메라를 맵 폭만큼 옮겨 원본 그리기를 한 번씩 더 부르는 방식이라 타일 종류와 상관없이 원본과 같은 그림이 나온다.
//  * 연출 뒷배경(장면 상태 5, 노을 장면의 맵 72): 뒷배경 레이어를 먼저 깔고 맵은 투명 타일을 건너뛰며 그린다. backdrop.c가 뒷배경을
//    보기 폭 끝까지 채워도 그 뒤에 여백 칠하기가 양옆을 검게 덮었다. 이 상태에서는 양옆을 칠하지 않는다(맵을 되풀이하면 가운데 절벽이
//    옆에 또 나오므로 뒷배경만 보인다).
// 그 밖의 좁은 맵(실내 등)은 원본대로 검은 여백이다. 맵이 보기보다 넓으면(240 폭 포함) 아무것도 바꾸지 않는다.
#include "restored.h"
#include <string.h>

#define GAME_VIEW VG(GAME_VIEW_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
enum {
    FIELD_GAME = 0x00, FIELD_CAM_X = 0x18, FIELD_MAP_W = 0x78,
    GAME_STATE = 0x18, GAME_MAP = 0x2d, GAME_VIEW_D5 = 0x568, // 보기 사각형(x, y, w, h)
    STATE_BACKDROP = 5,
    GC_COUNT = 0x18, // 0x2ee2c가 부를 때마다 올리는 값
    MAP_FIREWORKS = 88,
};

bool widemap_no_side_fill; // 켜 둔 동안 0x2f7c4가 좌우 여백을 칠하지 않는다(backdrop.c도 쓴다)

static bool tiles_sideways(int map) { return map == MAP_FIREWORKS; }

// 0x2f7c4(카메라 x, 카메라 y, 맵 폭, 맵 높이, [sp] 보기 사각형): 카메라가 보기 왼쪽·위보다 앞이거나 맵 끝이 보기 안이면 그 여백을 칠한다.
void orig_f_2f7c4(cpu_t *c);
void f_2f7c4(cpu_t *c) {
    if (widemap_no_side_fill) { // 좌우는 가득 찬 것으로 알린다. 위아래는 원본대로
        uint32_t view = R32(c->SP);
        int vx = (int)R32(view), vw = (int)R32(view + 8);
        c->r[0] = (uint32_t)vx;
        c->r[2] = (uint32_t)(2 * vx + vw);
    }
    orig_f_2f7c4(c);
}

// 0x2ee2c(필드, gc, 투명 타일 건너뛰기)
void orig_f_2ee2c(cpu_t *c);
void f_2ee2c(cpu_t *c) {
    uint32_t field = c->r[0], gc = c->r[1], game = R32(field + FIELD_GAME);
    int view_w = (int)R32(game + GAME_VIEW + 8), map_w = (int)R32(field + FIELD_MAP_W);
    int cam = (int)R32(field + FIELD_CAM_X);
    bool backdrop = R32(game + GAME_STATE) == STATE_BACKDROP;
    if (map_w <= 0 || map_w >= view_w || cam >= 0 || !(backdrop || tiles_sideways(R8(game + GAME_MAP)))) { orig_f_2ee2c(c); return; }

    uint32_t regs[16];
    memcpy(regs, c->r, sizeof regs);
    widemap_no_side_fill = true;
    if (!backdrop) {
        // 맵은 화면 -cam 자리에 놓인다. k번째 사본은 -cam + k * map_w.
        uint32_t count = R32(gc + GC_COUNT);
        for (int k = 1; -cam - k * map_w > -map_w || -cam + k * map_w < view_w; k++) {
            for (int side = -1; side <= 1; side += 2) {
                int left = -cam + side * k * map_w;
                if (left + map_w <= 0 || left >= view_w) continue;
                W32(field + FIELD_CAM_X, (uint32_t)(cam - side * k * map_w));
                memcpy(c->r, regs, sizeof regs);
                orig_f_2ee2c(c);
            }
        }
        W32(field + FIELD_CAM_X, (uint32_t)cam);
        W32(gc + GC_COUNT, count);
        memcpy(c->r, regs, sizeof regs);
    }
    orig_f_2ee2c(c);
    widemap_no_side_fill = false;
}

// 0xcc94(G, a, b): 연출 뒷배경 시작(스크립트 명령 0x93). 그리기 틀을 화면 가운데 146줄 띠로 줄인다(0x389c).
// 띠 밖은 그 뒤로 아무도 그리지 않으므로, 앞 장면이 페이드 없이 넘어오면 위아래에 앞 화면이 남는다. 틀을 줄이기 전에 화면을 지운다.
enum { GAME_GFX = 0x08, GFX_FRAME_H = 0x48, GFX_BLACK = 0x34, BAND_H = 146, FN_FILL = 0x10467c };
void orig_f_cc94(cpu_t *c);
void f_cc94(cpu_t *c) {
    uint32_t gfx = R32(c->r[0] + GAME_GFX);
    if ((int)R32(gfx + GFX_FRAME_H) > BAND_H) {
        uint32_t regs[16];
        memcpy(regs, c->r, sizeof regs);
        CALL(FN_FILL, 0, 0, R32(gfx + GFX_BLACK)); // 사각형 0 = 틀 전체
        memcpy(c->r, regs, sizeof regs);
    }
    orig_f_cc94(c);
}
