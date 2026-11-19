// 간접 호출, 원본 ARM 도우미, 타이머.
#include "host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

cpu_t cpu;
static int nfn;
uint32_t host_last_call;
FILE *api_trace; // WIE `wipic call` 로그와 같은 형식(id, lr)

static int cmpf(const void *k, const void *e) {
    uint32_t a = *(const uint32_t *)k, b = ((const fn_entry_t *)e)->addr;
    return a < b ? -1 : a > b;
}

void guest_fault(cpu_t *c, uint32_t a) {
    fprintf(stderr, "게스트 메모리 오류: pc %#x 주소 %#x, LR %#x, SP %#x, r0-3 %#x %#x %#x %#x, 마지막 간접 호출 %#x\n",
            c->pc, a, c->LR, c->SP, c->r[0], c->r[1], c->r[2], c->r[3], host_last_call);
    abort();
}

// 0x40024 memset, 0x400d8 memcpy, 0x40210 64비트 나눗셈의 C 대응(크로노스 윙과 같은 툴체인 코드). setjmp/longjmp는 번역 코드가 부르지 않는다.
const uint32_t arm_helper_addrs[] = {0x140024, 0x1400d8, 0x140210, 0};
// 인터프리터 빌드가 채운다(interp/interp.c, 웹 빌드). 비어 있으면 번역 코드의 fn_table을 쓴다.
void (*guest_exec)(cpu_t *, uint32_t);

bool arm_helper(cpu_t *c, uint32_t a) {
    uint32_t *r = c->r;
    static uint32_t h[3];
    if (!h[0]) for (int i = 0; i < 3; i++) h[i] = VF(arm_helper_addrs[i]); // 판마다 자리가 다르다(host/ver.h)
    a = a == h[0] ? 0x140024 : a == h[1] ? 0x1400d8 : a == h[2] ? 0x140210 : 0;
    switch (a) {
    case 0x140024: CK(r[0], r[2]); memset(c->m + r[0], (int)r[1], r[2]); return true;
    case 0x1400d8: CK(r[0], r[2]); CK(r[1], r[2]); memmove(c->m + r[0], c->m + r[1], r[2]); return true;
    case 0x140210: {
        int64_t n = (int64_t)(((uint64_t)r[1] << 32) | r[0]), d = (int64_t)(((uint64_t)r[3] << 32) | r[2]);
        int64_t q = d ? n / d : 0, m = d ? n % d : n;
        r[0] = (uint32_t)q; r[1] = (uint32_t)((uint64_t)q >> 32);
        r[2] = (uint32_t)m; r[3] = (uint32_t)((uint64_t)m >> 32);
        return true;
    }
    }
    return false;
}

void call_ind(cpu_t *c, uint32_t t) {
    host_last_call = t;
    if (t >= API_BASE) {
        uint32_t table = (t >> 16) & 0xfff, index = (t & 0xffff) >> 1;
        if (api_trace) fprintf(api_trace, "%#x %#x\n", table << 16 | index, c->LR);
        ts_api(c, table, index);
        api_dispatch(c, table, index);
        return;
    }
    if (arm_helper(c, t & ~1u)) return;
    if (guest_exec) { guest_exec(c, t); return; }
    if (!nfn) while (fn_table[nfn].addr) nfn++;
    uint32_t a = t & ~1u;
    const fn_entry_t *e = bsearch(&a, fn_table, nfn, sizeof(fn_entry_t), cmpf);
    if (!e) {
        fprintf(stderr, "번역되지 않은 코드로 간접 호출: pc %#x 대상 %#x, LR %#x, r0-7 %#x %#x %#x %#x %#x %#x %#x %#x, SP %#x, [SP] %#x\n", c->pc, t, c->LR,
                c->r[0], c->r[1], c->r[2], c->r[3], c->r[4], c->r[5], c->r[6], c->r[7], c->SP, R32(c->SP));
        abort();
    }
    e->fn(c);
}

uint32_t guest_call(uint32_t addr, const uint32_t *args, int n) {
    cpu_t *c = &cpu;
    uint32_t saved_sp = c->SP, saved_lr = c->LR;
    int extra = n > 4 ? n - 4 : 0;
    c->SP = (c->SP - extra * 4) & ~7u;
    for (int i = 0; i < n; i++) {
        if (i < 4) c->r[i] = args[i];
        else W32(c->SP + (i - 4) * 4, args[i]);
    }
    c->LR = 0xfffffffe;
    if (saved_lr == 0 || c->SP >= STACK_TOP - 64) ts_root();
    call_ind(c, addr | 1);
    c->SP = saved_sp;
    c->LR = saved_lr;
    return c->r[0];
}

// 타이머
typedef struct { uint32_t timer, cb, a0, a1; uint64_t due, seq; bool defer; bool used; } pend_t;
static pend_t pend[256];
static uint64_t seq;

uint64_t now_ms(void) { return SDL_GetTicks(); }

void timers_reset(void) { memset(pend, 0, sizeof pend); }

void timer_unset(uint32_t timer) {
    for (int i = 0; i < 256; i++)
        if (pend[i].used && !pend[i].defer && pend[i].timer == timer) pend[i].used = false;
}

void timer_def(uint32_t timer, uint32_t cb) {
    timer_unset(timer);
    W32_(&cpu, timer, cb);
}

static pend_t *pend_new(void) {
    for (int i = 0; i < 256; i++)
        if (!pend[i].used) { pend[i].used = true; pend[i].seq = seq++; return &pend[i]; }
    fprintf(stderr, "타이머 슬롯 부족\n");
    abort();
}

// 전투 2배속(restored/autobattle.c의 ab_speed_pct, 100이면 그대로). 게임은 틱 수로 진행을 세지만 틱 간격은 게임 시계(api 28)로
// 한 틱 목표 시간에서 걸린 시간을 빼 타이머로 건다. 그래서 타이머 간격만 줄이면 게임이 "일찍 왔다"고 보고 다시 늘려 1.5배밖에 안 나온다.
// 게임 시계를 배율만큼 빠르게 흐르게 하고(game_time_ms) 타이머 간격(게임 시계 기준)을 실제 시간으로 환산해(scaled) 둘이 맞게 한다.
int ab_speed_pct(void); // restored/autobattle.c
static uint64_t scaled(uint64_t ms) {
    int pct = ab_speed_pct();
    return pct > 100 ? ms * 100 / (uint64_t)pct : ms;
}

// 게임 시계: 배율이 바뀐 때마다 그때까지의 값을 이어 붙인다(시계가 거꾸로 가거나 뛰지 않게).
uint64_t game_time_ms(void) {
    static uint64_t vt, rt;
    static int pct = 100;
    uint64_t now = now_ms();
    if (!rt) vt = rt = now;
    int p = ab_speed_pct();
    if (p != pct) { vt += (now - rt) * (uint64_t)pct / 100; rt = now; pct = p; }
    return vt + (now - rt) * (uint64_t)pct / 100;
}

int timer_set(uint32_t timer, uint64_t ms, uint32_t param) {
    ms = scaled(ms);
    for (int i = 0; i < 256; i++)
        if (pend[i].used && !pend[i].defer && pend[i].timer == timer) return -5;
    pend_t *p = pend_new();
    p->defer = false;
    p->timer = timer;
    p->cb = R32_(&cpu, timer);
    p->a0 = timer;
    p->a1 = param;
    p->due = now_ms() + ms;
    return 0;
}

void defer_call(uint32_t cb, uint32_t a0, uint32_t a1, uint64_t ms) {
    ms = scaled(ms);
    pend_t *p = pend_new();
    p->defer = true;
    p->timer = 0;
    p->cb = cb;
    p->a0 = a0;
    p->a1 = a1;
    p->due = now_ms() + ms;
}

void timers_run(void) {
    uint64_t now = now_ms();
    for (;;) {
        pend_t *best = NULL;
        for (int i = 0; i < 256; i++)
            if (pend[i].used && pend[i].due <= now && (!best || pend[i].due < best->due || (pend[i].due == best->due && pend[i].seq < best->seq)))
                best = &pend[i];
        if (!best) return;
        uint32_t args[2] = {best->a0, best->a1}, cb = best->cb;
        best->used = false;
        guest_call(cb, args, 2);
    }
}

int64_t timers_next_ms(void) {
    int64_t best = -1, now = (int64_t)now_ms();
    for (int i = 0; i < 256; i++)
        if (pend[i].used) {
            int64_t d = (int64_t)pend[i].due - now;
            if (d < 0) d = 0;
            if (best < 0 || d < best) best = d;
        }
    return best;
}

// 진단(make TRFLAGS="-DTRACE_PC -DWATCH_VAL=506"): 값이 처음 읽힌 pc마다 한 줄.
#include <stdio.h>
extern uint64_t frames;
void watch_hit(cpu_t *c, uint32_t a, uint32_t v) {
    static uint32_t seen[4096]; static int n;
    uint32_t key = c->pc ^ (v << 20);
    for (int i = 0; i < n; i++) if (seen[i] == key) return;
    if (n < 4096) seen[n++] = key;
    fprintf(stderr, "watch pc %#x addr %#x val %u frame %llu\n", c->pc, a, v, (unsigned long long)frames);
}
