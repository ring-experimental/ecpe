// 게스트 힙. 주소순 빈 목록, 인접 블록 병합. 머리 8바이트에 크기를 둔다.
#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
#else
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static uint32_t free_head;
#define HDR 8u

// 32비트 주소 전체를 예약하고 게스트 영역만 연다. 번역 코드가 접근마다 범위를 검사하지 않아도 잘못된 주소는 보호 페이지에서 멈춘다.
#define GUEST_RESERVE 0x100010000ull

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
static struct sigaction old_segv, old_bus;

static void on_fault(int sig, siginfo_t *si, void *uc) {
    (void)uc;
    uint64_t off = (uint64_t)(uintptr_t)si->si_addr - (uint64_t)(uintptr_t)cpu.m;
    if (off < GUEST_RESERVE) fprintf(stderr, "게스트 메모리 오류: 주소 %#llx, 마지막 간접 호출 %#x\n", (unsigned long long)off, host_last_call);
#ifdef __ANDROID__
    // 진단: 게스트 메모리와 레지스터를 저장 폴더에 남긴다. 어떤 객체가 원인인지는 tombstone에 없다.
    if (off < GUEST_RESERVE) {
        char path[1200];
        snprintf(path, sizeof path, "%s/fault.bin", save_dir);
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            (void)!write(fd, cpu.r, sizeof cpu.r);
            (void)!write(fd, cpu.m + GUEST_LOW, GUEST_MEM - GUEST_LOW);
            close(fd);
        }
    }
#endif
    // 이전 처리기로 돌린다. 복귀하면 같은 자리에서 다시 죽고, Android는 debuggerd가 tombstone을 남긴다.
    sigaction(sig, sig == SIGBUS ? &old_bus : &old_segv, NULL);
}
#endif

void guest_mem_alloc(void) {
#ifdef __EMSCRIPTEN__
    // 보호 페이지가 없다. 인터프리터와 읽는 C는 접근마다 범위를 검사한다(thumbrt.h의 CK).
    uint8_t *p = calloc(1, GUEST_MEM);
    if (!p) { fprintf(stderr, "게스트 메모리 할당 실패\n"); exit(1); }
#elif defined(_WIN32)
    uint8_t *p = VirtualAlloc(NULL, GUEST_RESERVE, MEM_RESERVE, PAGE_NOACCESS);
    if (!p || !VirtualAlloc(p + GUEST_LOW, GUEST_MEM - GUEST_LOW, MEM_COMMIT, PAGE_READWRITE)) { fprintf(stderr, "게스트 메모리 예약 실패\n"); exit(1); }
#else
    uint8_t *p = mmap(NULL, GUEST_RESERVE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED || mprotect(p + GUEST_LOW, GUEST_MEM - GUEST_LOW, PROT_READ | PROT_WRITE)) { perror("게스트 메모리 예약 실패"); exit(1); }
    struct sigaction sa = {0};
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, &old_segv);
    sigaction(SIGBUS, &sa, &old_bus);
#endif
    cpu.m = p;
    cpu.memsz = GUEST_MEM;
}

// 게임을 다시 시작할 때 이미지, BSS, 스택, 힙을 처음 상태(0)로 되돌린다.
void guest_mem_clear(void) { memset(cpu.m + GUEST_LOW, 0, GUEST_MEM - GUEST_LOW); }

void heap_init(void) {
    cpu_t *c = &cpu;
    free_head = HEAP_START;
    W32(HEAP_START, GUEST_MEM - HEAP_START);
    W32(HEAP_START + 4, 0);
}

uint32_t heap_alloc(uint32_t size) {
    cpu_t *c = &cpu;
    uint32_t need = (size + HDR + 7) & ~7u, prev = 0, p = free_head;
    while (p) {
        uint32_t sz = R32(p), next = R32(p + 4);
        if (sz >= need) {
            uint32_t rest = sz - need, follow = next;
            if (rest >= 16) {
                follow = p + need;
                W32(follow, rest);
                W32(follow + 4, next);
            } else {
                need = sz;
            }
            if (prev) W32(prev + 4, follow); else free_head = follow;
            W32(p, need);
            W32(p + 4, 0x4c4c4f43);
            memset(c->m + p + HDR, 0, need - HDR);
            return p + HDR;
        }
        prev = p;
        p = next;
    }
    return 0;
}

void heap_free(uint32_t addr) {
    cpu_t *c = &cpu;
    if (!addr) return;
    uint32_t b = addr - HDR, sz = R32(b), prev = 0, p = free_head;
    while (p && p < b) { prev = p; p = R32(p + 4); }
    if (p && b + sz == p) { sz += R32(p); p = R32(p + 4); }
    W32(b, sz);
    W32(b + 4, p);
    if (prev) {
        if (prev + R32(prev) == b) { W32(prev, R32(prev) + sz); W32(prev + 4, p); }
        else W32(prev + 4, b);
    } else {
        free_head = b;
    }
}

// WIPI 메모리 핸들: [h]=h+4, [h+4]=크기, 데이터는 h+12.
uint32_t handle_alloc(uint32_t size) {
    cpu_t *c = &cpu;
    uint32_t h = heap_alloc(size + 12);
    if (!h) return 0;
    W32(h, h + 4);
    W32(h + 4, size);
    return h;
}

void handle_free(uint32_t h) { heap_free(h); }
