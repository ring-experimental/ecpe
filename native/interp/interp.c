// Thumb-1 인터프리터. 기계 번역 C(tr.c) 대신 client.bin의 코드를 직접 실행한다. 원본에서 나온 내용은 들어 있지 않다.
// 번역 코드와 같은 규칙을 따른다: 함수 호출(bl)과 간접 분기의 대상이 restored/list.txt의 함수면 읽는 C를 부르고,
// 0xf0000000 이상은 WIPI API, ARM 도우미 세 개는 runtime.c의 C 대응으로 보낸다.
#include "host.h"
#include <stdio.h>
#include <stdlib.h>

extern const fn_entry_t interp_hooks[]; // 생성 파일(interpgen.py). 주소 0으로 끝난다.
extern const uint32_t arm_helper_addrs[];
bool arm_helper(cpu_t *c, uint32_t a);
void call_ind(cpu_t *c, uint32_t t);

enum { TEXT_MAX = 0x100000, HELPER = 0xffff };
static uint16_t hook_at[TEXT_MAX / 2]; // 0 없음, HELPER, 그 밖은 interp_hooks 순번 + 1
static bool ready;

#ifdef INTERP_PROF
// -DINTERP_PROF: 명령 주소별 실행 횟수를 CHRONOS_INTERP_PROF 파일에 남긴다(uint64, 2바이트 단위).
static uint64_t prof[TEXT_MAX / 2];
static void prof_dump(void) {
    const char *p = getenv("CHRONOS_INTERP_PROF");
    FILE *f = p ? fopen(p, "wb") : NULL;
    if (f) { fwrite(prof, sizeof prof, 1, f); fclose(f); }
}
#endif

static void setup(void) {
    ready = true;
#ifdef INTERP_PROF
    atexit(prof_dump);
#endif
    // 표의 주소는 기준 판 것이다. 지금 판의 자리로 바꿔 건다(host/ver.h).
    for (int i = 0; interp_hooks[i].addr; i++) hook_at[(VF(interp_hooks[i].addr) - IMAGE_LOAD) >> 1] = (uint16_t)(i + 1);
    for (int i = 0; arm_helper_addrs[i]; i++) hook_at[(VF(arm_helper_addrs[i]) - IMAGE_LOAD) >> 1] = HELPER;
}

static inline bool cond(const flags_t *f, uint32_t cc) {
    switch (cc) {
    case 0: return f->z;
    case 1: return !f->z;
    case 2: return f->cf;
    case 3: return !f->cf;
    case 4: return f->n;
    case 5: return !f->n;
    case 6: return f->v;
    case 7: return !f->v;
    case 8: return f->cf && !f->z;
    case 9: return !f->cf || f->z;
    case 10: return f->n == f->v;
    case 11: return f->n != f->v;
    case 12: return !f->z && f->n == f->v;
    default: return f->z || f->n != f->v;
    }
}

static void bad(cpu_t *c, uint32_t pc, uint32_t op) {
    fprintf(stderr, "인터프리터: 처리하지 못하는 명령 pc %#x op %#06x, LR %#x, SP %#x\n", pc, op, c->LR, c->SP);
    abort();
}

// t의 함수를 부르고 그 함수가 들어올 때의 LR로 돌아가면 끝난다. 재귀 호출의 안쪽 복귀와 구분하려고 SP도 본다.
// skip_hook이면 첫 대상의 읽는 C 교체를 건너뛴다(wrap 함수가 원본을 부를 때).
void interp_call(cpu_t *c, uint32_t t, bool skip_hook) {
    if (!ready) setup();
    uint32_t *const r = c->r;
    const uint32_t ret = c->LR & ~1u, sp0 = c->SP;
    flags_t fl = {0};
    uint32_t pc, op;
    bool arm = false;

dispatch:
    t &= ~1u;
    if (t == ret && r[13] >= sp0) return;
    if (t >= API_BASE) {
        uint32_t back = c->LR;
        call_ind(c, t | 1);
        t = back;
        goto dispatch;
    }
    if (t - IMAGE_LOAD >= TEXT_MAX) { c->pc = pc; guest_fault(c, t); }
    {
        uint32_t h = hook_at[(t - IMAGE_LOAD) >> 1];
        if (h == HELPER) {
            uint32_t back = c->LR;
            host_last_call = t;
            arm_helper(c, t);
            arm = false;
            t = back;
            goto dispatch;
        }
        if (arm) {
            fprintf(stderr, "인터프리터: ARM 코드로 분기 pc %#x 대상 %#x, LR %#x\n", pc, t, c->LR);
            abort();
        }
        if (h && !skip_hook) {
            uint32_t back = c->LR;
            interp_hooks[h - 1].fn(c);
            t = back;
            goto dispatch;
        }
        skip_hook = false;
    }
    pc = t;

    for (;;) {
        if (pc - IMAGE_LOAD >= TEXT_MAX - 4) guest_fault(c, pc);
        op = c->m[pc] | (uint32_t)c->m[pc + 1] << 8;
        c->pc = pc;
        c->icount++;
#ifdef INTERP_PROF
        prof[(pc - IMAGE_LOAD) >> 1]++;
#endif
        const uint32_t rd = op & 7, rs = (op >> 3) & 7;
        switch (op >> 11) {
        case 0: { // lsls rd, rs, #imm. #0은 movs다.
            uint32_t s = (op >> 6) & 31;
            if (s) r[rd] = lsl_(&fl, r[rs], s); else { r[rd] = r[rs]; NZ(r[rd]); }
            break; }
        case 1: { uint32_t s = (op >> 6) & 31; r[rd] = lsr_(&fl, r[rs], s ? s : 32); break; }
        case 2: { uint32_t s = (op >> 6) & 31; r[rd] = asr_(&fl, r[rs], s ? s : 32); break; }
        case 3: { // adds/subs rd, rs, rn|#imm3
            uint32_t b = (op >> 6) & 7;
            if (!(op & 0x400)) b = r[b];
            r[rd] = op & 0x200 ? addc(&fl, r[rs], ~b, 1) : addc(&fl, r[rs], b, 0);
            break; }
        case 4: r[(op >> 8) & 7] = op & 255; NZ(op & 255); break;
        case 5: addc(&fl, r[(op >> 8) & 7], ~(op & 255), 1); break;
        case 6: r[(op >> 8) & 7] = addc(&fl, r[(op >> 8) & 7], op & 255, 0); break;
        case 7: r[(op >> 8) & 7] = addc(&fl, r[(op >> 8) & 7], ~(op & 255), 1); break;
        case 8:
            if (!(op & 0x400)) {
                uint32_t a = r[rd], b = r[rs];
                switch ((op >> 6) & 15) {
                case 0: r[rd] = a & b; NZ(r[rd]); break;
                case 1: r[rd] = a ^ b; NZ(r[rd]); break;
                case 2: r[rd] = lsl_(&fl, a, b); break;
                case 3: r[rd] = lsr_(&fl, a, b); break;
                case 4: r[rd] = asr_(&fl, a, b); break;
                case 5: r[rd] = addc(&fl, a, b, fl.cf); break;
                case 6: r[rd] = addc(&fl, a, ~b, fl.cf); break;
                case 7: r[rd] = ror_(&fl, a, b); break;
                case 8: { uint32_t x = a & b; NZ(x); break; }
                case 9: r[rd] = addc(&fl, ~b, 0, 1); break;
                case 10: addc(&fl, a, ~b, 1); break;
                case 11: addc(&fl, a, b, 0); break;
                case 12: r[rd] = a | b; NZ(r[rd]); break;
                case 13: r[rd] = a * b; NZ(r[rd]); break;
                case 14: r[rd] = a & ~b; NZ(r[rd]); break;
                case 15: r[rd] = ~b; NZ(r[rd]); break;
                }
            } else { // 높은 레지스터 연산과 bx
                uint32_t d = rd | ((op >> 4) & 8), s = (op >> 3) & 15;
                uint32_t v = s == 15 ? pc + 4 : r[s];
                switch ((op >> 8) & 3) {
                case 0:
                    if (d == 15) { t = pc + 4 + v; goto dispatch; }
                    r[d] += v;
                    break;
                case 1: addc(&fl, r[d], ~v, 1); break;
                case 2:
                    if (d == 15) { t = v; goto dispatch; }
                    r[d] = v;
                    break;
                case 3:
                    if (s == 15) { v = (pc + 4) & ~3u; arm = true; }
                    if (op & 0x80) r[14] = (pc + 2) | 1;
                    t = v;
                    goto dispatch;
                }
            }
            break;
        case 9: r[(op >> 8) & 7] = R32_(c, ((pc + 4) & ~3u) + (op & 255) * 4); break;
        case 10: case 11: {
            uint32_t ea = r[rs] + r[(op >> 6) & 7];
            switch ((op >> 9) & 7) {
            case 0: W32_(c, ea, r[rd]); break;
            case 1: W16_(c, ea, r[rd]); break;
            case 2: W8_(c, ea, r[rd]); break;
            case 3: r[rd] = (uint32_t)(int32_t)(int8_t)R8_(c, ea); break;
            case 4: r[rd] = R32_(c, ea); break;
            case 5: r[rd] = R16_(c, ea); break;
            case 6: r[rd] = R8_(c, ea); break;
            case 7: r[rd] = (uint32_t)(int32_t)(int16_t)R16_(c, ea); break;
            }
            break; }
        case 12: W32_(c, r[rs] + ((op >> 6) & 31) * 4, r[rd]); break;
        case 13: r[rd] = R32_(c, r[rs] + ((op >> 6) & 31) * 4); break;
        case 14: W8_(c, r[rs] + ((op >> 6) & 31), r[rd]); break;
        case 15: r[rd] = R8_(c, r[rs] + ((op >> 6) & 31)); break;
        case 16: W16_(c, r[rs] + ((op >> 6) & 31) * 2, r[rd]); break;
        case 17: r[rd] = R16_(c, r[rs] + ((op >> 6) & 31) * 2); break;
        case 18: W32_(c, r[13] + (op & 255) * 4, r[(op >> 8) & 7]); break;
        case 19: r[(op >> 8) & 7] = R32_(c, r[13] + (op & 255) * 4); break;
        case 20: r[(op >> 8) & 7] = ((pc + 4) & ~3u) + (op & 255) * 4; break;
        case 21: r[(op >> 8) & 7] = r[13] + (op & 255) * 4; break;
        case 22: case 23:
            if ((op & 0xff00) == 0xb000) {
                if (op & 0x80) r[13] -= (op & 127) * 4; else r[13] += (op & 127) * 4;
            } else if ((op & 0xfe00) == 0xb400) { // push
                uint32_t n = __builtin_popcount(op & 0x1ff), a = r[13] - 4 * n;
                r[13] = a;
                for (int i = 0; i < 8; i++)
                    if (op & (1u << i)) { W32_(c, a, r[i]); a += 4; }
                if (op & 0x100) W32_(c, a, r[14]);
            } else if ((op & 0xfe00) == 0xbc00) { // pop
                uint32_t a = r[13];
                for (int i = 0; i < 8; i++)
                    if (op & (1u << i)) { r[i] = R32_(c, a); a += 4; }
                if (op & 0x100) { t = R32_(c, a); r[13] = a + 4; goto dispatch; }
                r[13] = a;
            } else bad(c, pc, op);
            break;
        case 24: { // stmia rn!, {..}
            uint32_t n = (op >> 8) & 7, a = r[n];
            for (int i = 0; i < 8; i++)
                if (op & (1u << i)) { W32_(c, a, r[i]); a += 4; }
            r[n] = a;
            break; }
        case 25: { // ldmia rn!, {..}. 목록에 rn이 있으면 되쓰지 않는다.
            uint32_t n = (op >> 8) & 7, a = r[n];
            for (int i = 0; i < 8; i++)
                if (op & (1u << i)) { r[i] = R32_(c, a); a += 4; }
            if (!(op & (1u << n))) r[n] = a;
            break; }
        case 26: case 27: {
            uint32_t cc = (op >> 8) & 15;
            if (cc >= 14) bad(c, pc, op);
            if (cond(&fl, cc)) { pc = pc + 4 + (uint32_t)((int32_t)(int8_t)(op & 255) * 2); continue; }
            break; }
        case 28: pc = pc + 4 + (uint32_t)(((int32_t)(op << 21) >> 21) * 2); continue;
        case 30: { // bl 앞 절반. 뒤 절반이 바로 오면 한 번에 처리한다.
            uint32_t hi = pc + 4 + (uint32_t)(((int32_t)(op << 21) >> 21) << 12);
            uint32_t op2 = c->m[pc + 2] | (uint32_t)c->m[pc + 3] << 8;
            if ((op2 >> 11) == 31) {
                c->icount++;
                r[14] = (pc + 4) | 1;
                t = hi + ((op2 & 0x7ff) << 1);
                goto dispatch;
            }
            r[14] = hi;
            break; }
        case 31: { // bl 뒤 절반만 따로 온 경우
            uint32_t next = (pc + 2) | 1;
            t = r[14] + ((op & 0x7ff) << 1);
            r[14] = next;
            goto dispatch; }
        default: bad(c, pc, op);
        }
        pc += 2;
    }
}

void interp_exec(cpu_t *c, uint32_t t) { interp_call(c, t, false); }
