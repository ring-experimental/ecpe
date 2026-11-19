// 넓은 화면의 필드 카메라. 원본 0x2f3e4를 감쌌다.
//
// 0x2f3e4(필드, x, y)는 카메라를 (x, y)가 보기 가운데에 오게 놓고(0x2f3c0), 게임 상태 플래그(게임 + 0x20)의 0x10이 켜져 있을 때만
// 맵 안으로 가둔다(0x2ea20). 맵에 들어설 때는 이 플래그가 꺼져 있어서 카메라가 주인공 가운데 그대로 남고, 걷기 시작해야 가둬진다.
// 240 폭에서는 문이 맵 끝에서 120px 넘게 떨어져 있으면 맵 밖이 보이지 않지만, 넓은 화면에서는 보기의 절반(예: 256px)이
// 맵 끝을 넘어가 한쪽이 검게 비었다가 첫 걸음에 화면이 튄다(수도 아멜고의 아크의 방에서 나올 때 오른쪽 60px).
//
// 원본 폭(240)이었다면 맵 밖이 보이지 않았을 카메라만 가로로 가둔다. 240 폭에서는 조건이 그대로라 아무것도 바꾸지 않는다.
// 세로는 화면 높이가 원본과 같으므로 건드리지 않는다.
#include "restored.h"

#define GAME_VIEW_W VG(GAME_VIEW_W_D5) // 게임 객체 필드: 판마다 다르다(host/ver.h)
enum {
    ORIG_W = 240,
    FIELD_GAME = 0x00, FIELD_CAM_X = 0x18, FIELD_MAP_W = 0x78, // 맵 폭(px)
    GAME_FLAGS = 0x20, FLAG_CLAMP = 0x10,
    GAME_VIEW_W_D5 = 0x568 + 8,
};

void orig_f_2f3e4(cpu_t *c);
void f_2f3e4(cpu_t *c) {
    uint32_t field = c->r[0];
    orig_f_2f3e4(c);
    uint32_t game = R32(field + FIELD_GAME);
    if (R32(game + GAME_FLAGS) & FLAG_CLAMP) return; // 원본이 이미 가뒀다
    int view_w = (int)R32(game + GAME_VIEW_W), map_w = (int)R32(field + FIELD_MAP_W);
    int cam = (int)R32(field + FIELD_CAM_X), cam240 = cam + (view_w - ORIG_W) / 2;
    if (view_w <= ORIG_W || cam240 < 0 || cam240 > map_w - ORIG_W) return;
    // 0x2ea20과 같은 규칙: 맵이 보기보다 좁으면 가운데, 아니면 0..맵 폭 - 보기 폭. 짝수로 맞춘다.
    if (map_w < view_w) cam = (map_w - view_w) / 2;
    else if (cam < 0) cam = 0;
    else if (cam > map_w - view_w) cam = map_w - view_w;
    W32(field + FIELD_CAM_X, (uint32_t)cam & ~1u);
}
