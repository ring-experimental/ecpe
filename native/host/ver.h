// 원본 판(빌드)마다 다른 주소를 맞춘다. restored/와 host/는 기준 판(KTF 010100D5, 01.00.00)의 주소로 쓰고,
// 다른 판을 돌릴 때는 아래 함수가 그 판의 주소로 바꾼다. 기준 판에서는 모두 그대로 돌려준다.
//   VF(주소)   함수 주소(게스트 절대 주소, 0x100000 + 파일 오프셋). 감싼 함수, CALL, 시작 함수.
//   VFIN(주소) 함수 안의 주소. 몸이 같은 함수만 된다. VFEND(주소)는 함수 범위의 끝(다음 함수 시작에서 거꾸로 센다).
//   VGOT(칸)   GOT 칸 오프셋(GOT 시작 기준). GOT_ADDR(칸) = 그 칸의 게스트 주소.
//   VD(주소)   이미지·BSS 데이터 주소(게스트 절대 주소).
//   VG(오프셋) 게임 객체(0x14ee34가 가리키는 앱의 +0x48) 필드 오프셋.
// 표에 그 종류의 줄이 없으면 기준 판과 같다고 보고 그대로 돌려준다. 줄이 있는데 대응을 못 찾으면 멈춘다(잘못 돌지 않게).
// 판마다 다른 것은 모두 native/ver/<AID>.map에 있다(기준 판도 빈 표 하나). 대응은 work/tools/vermap.py가 두 client.bin에서 만들고,
// 손으로 정한 규칙(gofs, bss 줄)은 같은 파일에 적는다. 빌드 때 work/tools/vergen.py가 C 표로 바꾼다.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct { uint32_t a, b; } ver_pair_t;
typedef struct { uint32_t a, b; uint8_t same; } ver_fn_t;
typedef struct { uint32_t from; int32_t delta; } ver_gofs_t;
typedef struct { uint32_t lo, hi; int32_t delta; } ver_bss_t;
typedef struct {
    const char *aid;
    uint32_t image_size;
    const ver_fn_t *fn; int nfn;
    const ver_pair_t *arm; int narm;
    const ver_pair_t *got; int ngot;
    const ver_pair_t *data; int ndata;
    const ver_gofs_t *gofs; int ngofs;
    const ver_bss_t *bss; int nbss;
} ver_table_t;

extern const ver_table_t *const ver_tables[]; // 생성 표(build/ver_tables.c)

// 실행 이미지(client.bin)로 판을 고른다. 모르는 판이면 false.
bool ver_select(const uint8_t *image, uint32_t size);
const char *ver_name(void);  // "010100D5" 등

uint32_t VF(uint32_t a);
uint32_t VFIN(uint32_t a);
uint32_t VFEND(uint32_t a); // 함수 범위의 끝(다음 함수 시작 쪽에서 센다)
uint32_t VGOT(uint32_t off);
uint32_t VD(uint32_t a);
uint32_t VG(uint32_t off);
uint32_t ver_got_base(void);
#define GOT_ADDR(off) (ver_got_base() + VGOT(off))
