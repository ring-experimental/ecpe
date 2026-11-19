// 읽는 C로 옮긴 함수들의 공통 도구. 크로노스 윙 native/restored와 같은 방식이다.
// 주소는 게스트 주소(파일 오프셋 + 0x100000)이고, 옮긴 함수 목록은 list.txt에 있다.
#include "host.h"
#include "ui.h"

#define CALL(addr, ...) guest_call((addr), (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4)

// 0..w-1 밖의 칸을 끝에서 거울처럼 접는다. 넓은 화면에서 원본 데이터 밖을 같은 데이터로 채울 때 쓴다.
static inline int fold(int i, int w) {
    for (;;) {
        if (i < 0) i = -i - 1;
        else if (i >= w) i = 2 * w - i - 1;
        else return i;
    }
}
