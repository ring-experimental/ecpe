// 판 선택과 주소 변환(ver.h). 판마다 다른 것은 모두 native/ver/<AID>.map에 있고 여기에는 판 이름이나 숫자를 두지 않는다.
// 기준 판(010100D5)도 빈 표 하나다: 어떤 종류든 표에 줄이 없으면 그 종류는 기준 판과 같다고 본다.
#include "ver.h"
#include <stdio.h>
#include <stdlib.h>

static const ver_table_t *cur;
static uint32_t got_base;

bool ver_select(const uint8_t *image, uint32_t size) {
    cur = NULL;
    for (int i = 0; ver_tables[i]; i++)
        if (ver_tables[i]->image_size == size) cur = ver_tables[i];
    if (!cur) return false;
    // GOT 시작: 재배치 표 뒤 기준점 + 머리의 첫 값(host/main.c load_image와 같은 식). 적재한 이미지에서 읽는다.
    uint32_t n = (uint32_t)image[0x70] | (uint32_t)image[0x71] << 8 | (uint32_t)image[0x72] << 16 | (uint32_t)image[0x73] << 24;
    uint32_t base = 0x74 + n * 4, g = (uint32_t)image[base] | (uint32_t)image[base + 1] << 8 | (uint32_t)image[base + 2] << 16 | (uint32_t)image[base + 3] << 24;
    got_base = 0x100000u + base + g;
    return true;
}
const char *ver_name(void) { return cur ? cur->aid : "?"; }
uint32_t ver_got_base(void) { return got_base; }

static _Noreturn void miss(const char *what, uint32_t a) {
    fprintf(stderr, "판 %s: %s %#x의 대응이 없다(native/ver/%s.map)\n", ver_name(), what, a, ver_name());
    abort();
}

static int fn_le(uint32_t a) { // a 이하인 가장 큰 기준 함수
    int lo = 0, hi = cur->nfn - 1, r = -1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (cur->fn[m].a <= a) { r = m; lo = m + 1; } else hi = m - 1; }
    return r;
}

static const ver_pair_t *pair_find(const ver_pair_t *t, int n, uint32_t a) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (t[m].a == a) return &t[m];
        if (t[m].a < a) lo = m + 1; else hi = m - 1;
    }
    return NULL;
}

uint32_t VF(uint32_t a) {
    uint32_t t = a & 1, x = a & ~1u;
    const ver_pair_t *h = pair_find(cur->arm, cur->narm, x);
    if (h) return h->b;
    if (!cur->nfn) return a;
    int k = fn_le(x);
    if (k < 0 || cur->fn[k].a != x) miss("함수", x);
    return cur->fn[k].b | t;
}

uint32_t VFIN(uint32_t a) {
    if (!cur->nfn) return a;
    int k = fn_le(a);
    if (k < 0 || !cur->fn[k].same) miss("함수 안 주소", a);
    return cur->fn[k].b + (a - cur->fn[k].a);
}

uint32_t VFEND(uint32_t a) {
    if (!cur->nfn) return a;
    int k = fn_le(a);
    if (k >= 0 && cur->fn[k].a == a) return cur->fn[k].b;
    if (k + 1 >= cur->nfn) miss("함수 끝", a);
    return cur->fn[k + 1].b - (cur->fn[k + 1].a - a); // 다음 함수 시작에서 거꾸로 센다
}

uint32_t VGOT(uint32_t off) {
    const ver_pair_t *p = pair_find(cur->got, cur->ngot, off);
    return p ? p->b : off;
}

uint32_t VD(uint32_t a) {
    const ver_pair_t *p = pair_find(cur->data, cur->ndata, a);
    if (p) return p->b;
    for (int i = 0; i < cur->nbss; i++) // GOT·재배치에 없는 BSS 전역: 구간마다 한 덩어리로 밀린다
        if (a >= cur->bss[i].lo && a < cur->bss[i].hi) return a + (uint32_t)cur->bss[i].delta;
    if (!cur->ndata) return a;
    miss("데이터", a);
}

uint32_t VG(uint32_t off) {
    int32_t d = 0;
    for (int i = 0; i < cur->ngofs; i++) // 이 오프셋 앞에 끼어든 필드 크기를 모두 더한다
        if (off >= cur->gofs[i].from) d += cur->gofs[i].delta;
    return off + (uint32_t)d;
}
