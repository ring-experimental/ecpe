// 배경음과 효과음을 함께 낸다. 크로노스 윙 restored/sound.c와 같은 생각이다.
//
// 원본 소리 관리자(객체 = [[game]+0x3c], 0xad04에서 만든다)는 클립 하나(+4)로 모든 소리를 낸다. 소리 번호(0..0x1a)는
// 0..0xe 배경음·징글(MIDI), 0xf..0x15 효과음(0x11만 MIDI, 나머지는 파형), 0x16..0x1a 이벤트 음악(MIDI)이다.
//   - 재생 요청 0x11aec(game, 번호, 1, 플래그) → 0x11ae60이 지금 소리를 끄고(0xaedc, +0x3c 비트 1이 켜져 있을 때)
//     설명(번호, 플래그, 크기, 데이터)을 +0x18에 둔다. 두 틱 뒤 0xaeb0 → 0xadf0이 ClipPutData + Play.
//   - 정지 0x11b90 → 0xaedc가 클립을 멈추고 +0x18을 -1로 지운다.
// 필드에서는 효과음(0xf)을 낼 때마다 배경음(4)을 끄고, 효과음 뒤 정지(0x12babc), 다음 틱에 배경음을 다시 요청한다.
// 그대로 따르면 배경음이 효과음마다 끊겼다가 처음부터 다시 나온다.
//
// 그래서 효과음 처리 안의 정지는 배경음에 적용하지 않는다(audio_sfx_scope). 같은 곡을 다시 요청하면 호스트는 곡을 잇는다.
// 효과음이 지금 소리일 때 들어온 정지는 효과음만 끈다. 원본에서는 이 정지 뒤가 무음이었으므로, 그 뒤 잠시 안에
// 배경음 요청이 없으면(이벤트가 무음을 원한 경우) 그때 배경음을 끈다. 그 밖의 정지는 배경음까지 실제로 끈다.
#include "restored.h"

void orig_f_ae60(cpu_t *c);
void orig_f_adf0(cpu_t *c);
void orig_f_11b90(cpu_t *c);
void orig_f_11aec(cpu_t *c);
void orig_f_aeb0(cpu_t *c);

enum {
    MGR_CUR = 0x18,          // 지금 소리 설명(+0 번호, -1이면 없음)
    SFX_FIRST = 0xf, SFX_LAST = 0x15,
    ORPHAN_TICKS = 20,       // 효과음 정지 뒤 배경음 요청을 기다리는 틱 수(약 1초)
};

static bool is_sfx(uint32_t id) { return id >= SFX_FIRST && id <= SFX_LAST; }
static int orphan; // 0이 아니면 남은 틱. 0이 되면 배경음을 끈다.

// 재생 요청 안쪽(0xae60): 새 소리가 효과음이면 앞 소리를 끄는 정지는 효과음끼리만 적용한다.
void f_ae60(cpu_t *c) {
    bool sfx = c->r[1] && is_sfx(R32(c->r[1]));
    if (sfx) audio_sfx_scope(true);
    orig_f_ae60(c);
    if (sfx) audio_sfx_scope(false);
}

// 실제 재생(0xadf0): 효과음이면 MIDI도 효과음 합성기로 보낸다(배경음을 갈아 끼우지 않는다).
void f_adf0(cpu_t *c) {
    bool sfx = is_sfx(R32(c->r[0] + MGR_CUR));
    if (sfx) audio_sfx_scope(true);
    orig_f_adf0(c);
    if (sfx) audio_sfx_scope(false);
}

// 재생 요청(0x11aec): 효과음이 아닌 소리를 요청하면 기다리던 배경음 끄기를 거둔다.
void f_11aec(cpu_t *c) {
    if (!is_sfx(c->r[1])) orphan = 0;
    orig_f_11aec(c);
}

// 정지(0x11b90).
void f_11b90(cpu_t *c) {
    uint32_t mgr = R32(R32(c->r[0]) + 0x3c);
    bool sfx = mgr && is_sfx(R32(mgr + MGR_CUR));
    if (sfx) {
        audio_sfx_scope(true);
        orig_f_11b90(c);
        audio_sfx_scope(false);
        orphan = ORPHAN_TICKS;
        return;
    }
    orig_f_11b90(c);
    orphan = 0;
    audio_music_stop(); // 클립에 마지막으로 넣은 것이 효과음이어도 배경음을 끈다
}

// 관리자 틱(0xaeb0, 0x6720에서 매 틱).
void f_aeb0(cpu_t *c) {
    if (orphan && --orphan == 0) audio_music_stop();
    orig_f_aeb0(c);
}
