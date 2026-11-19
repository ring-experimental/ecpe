// 읽는 C로 옮긴 함수들의 공통 도구. 크로노스 윙 native/restored와 같은 방식이다.
// 주소는 게스트 주소(파일 오프셋 + 0x100000)이고, 옮긴 함수 목록은 list.txt에 있다.
#include "host.h"
#include "ui.h"

// 주소는 기준 판(010100D5)의 것으로 쓴다. 다른 판에서는 host/ver.h가 바꾼다:
//   CALL(기준 판 함수 주소, ...)    함수 주소를 지금 판으로 바꿔 부른다.
//   CALLP(게스트 포인터, ...)       메모리에서 읽은 포인터(가상 함수 표 등)는 이미 지금 판의 주소라 그대로 부른다.
//   GOT_ADDR(칸), APP_ADDR, VG(게임 객체 오프셋), VD(데이터 주소), VFIN(함수 안 주소)
#define CALL(addr, ...) guest_call(VF(addr), (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4)
#define CALLP(addr, ...) guest_call((addr), (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4)
#define APP_ADDR VD(0x14ee34u) // 앱 객체 포인터를 가진 BSS 전역(앱 +0x48이 게임 객체)

// 0..w-1 밖의 칸을 끝에서 거울처럼 접는다. 넓은 화면에서 원본 데이터 밖을 같은 데이터로 채울 때 쓴다.
static inline int fold(int i, int w) {
    for (;;) {
        if (i < 0) i = -i - 1;
        else if (i >= w) i = 2 * w - i - 1;
        else return i;
    }
}
