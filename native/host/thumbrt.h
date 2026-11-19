// 기계 번역 코드의 런타임. 메모리는 c->m + 게스트 주소다.
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
typedef struct cpu { uint32_t pc; uint32_t r[16]; uint8_t *m; uint64_t icount; int fault; jmp_buf jb; uint32_t memsz; } cpu_t;
typedef struct { int n,z,cf,v; } flags_t;
#define SP r[13]
#define LR r[14]
typedef void (*fn_t)(cpu_t*);
typedef struct { uint32_t addr; fn_t fn; } fn_entry_t;
extern const fn_entry_t fn_table[];
void call_ind(cpu_t*c,uint32_t t);
void guest_fault(cpu_t*c,uint32_t a);
static inline void chk(cpu_t*c,uint32_t a,uint32_t n){ if(a<0x100000u||a+n>c->memsz)guest_fault(c,a); }
#define CK(a,n) chk(c,(a),(n))
#ifdef WATCH_VAL
// 진단: 읽은 값이 WATCH_VAL인 곳의 pc를 남긴다(runtime.c watch_hit).
void watch_hit(cpu_t*c,uint32_t a,uint32_t v);
#define WATCH(c,a,v) do{ if((v)==WATCH_VAL||(v)==WATCH_VAL/2) watch_hit(c,a,v); }while(0)
#else
#define WATCH(c,a,v) ((void)0)
#endif
static inline uint32_t R32_(cpu_t*c,uint32_t a){CK(a,4);uint32_t v;memcpy(&v,c->m+a,4);WATCH(c,a,v);return v;}
static inline uint32_t R16_(cpu_t*c,uint32_t a){CK(a,2);uint16_t v;memcpy(&v,c->m+a,2);WATCH(c,a,v);return v;}
static inline uint32_t R8_(cpu_t*c,uint32_t a){CK(a,1);return c->m[a];}
static inline void W32_(cpu_t*c,uint32_t a,uint32_t x){CK(a,4);memcpy(c->m+a,&x,4);}
static inline void W16_(cpu_t*c,uint32_t a,uint32_t x){CK(a,2);uint16_t v=x;memcpy(c->m+a,&v,2);}
static inline void W8_(cpu_t*c,uint32_t a,uint32_t x){CK(a,1);c->m[a]=x;}
#if defined(TR_LOCAL_REGS) && !defined(TRACE_PC)
// 번역 코드는 범위 검사 대신 보호 페이지(guest_mem_alloc)에 맡긴다. 32비트 주소는 예약 영역을 벗어날 수 없다.
static inline uint32_t RM32(const uint8_t*m,uint32_t a){uint32_t v;memcpy(&v,m+a,4);return v;}
static inline uint32_t RM16(const uint8_t*m,uint32_t a){uint16_t v;memcpy(&v,m+a,2);return v;}
static inline void WM32(uint8_t*m,uint32_t a,uint32_t x){memcpy(m+a,&x,4);}
static inline void WM16(uint8_t*m,uint32_t a,uint32_t x){uint16_t v=x;memcpy(m+a,&v,2);}
#define R32(a) RM32(m,(a))
#define R16(a) RM16(m,(a))
#define R8(a) ((uint32_t)m[(uint32_t)(a)])
#define W32(a,x) WM32(m,(a),(x))
#define W16(a,x) WM16(m,(a),(x))
#define W8(a,x) (m[(uint32_t)(a)]=(uint8_t)(x))
#else
#define R32(a) R32_(c,(a))
#define R16(a) R16_(c,(a))
#define R8(a) R8_(c,(a))
#define W32(a,x) W32_(c,(a),(x))
#define W16(a,x) W16_(c,(a),(x))
#define W8(a,x) W8_(c,(a),(x))
#endif
#define RS16(a) ((uint32_t)(int32_t)(int16_t)R16(a))
#define RS8(a) ((uint32_t)(int32_t)(int8_t)R8(a))
#define NZ(x) (fl.n=(uint32_t)(x)>>31,fl.z=(uint32_t)(x)==0)
static inline uint32_t addc(flags_t*p,uint32_t a,uint32_t b,uint32_t ci){uint64_t u=(uint64_t)a+b+ci;uint32_t r=(uint32_t)u;p->n=r>>31;p->z=r==0;p->cf=u>>32;p->v=((a^r)&(b^r))>>31;return r;}
static inline uint32_t lsl_(flags_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){if(s<32){p->cf=(a>>(32-s))&1;r=a<<s;}else{p->cf=s==32?a&1:0;r=0;}}p->n=r>>31;p->z=r==0;return r;}
static inline uint32_t lsr_(flags_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){if(s<32){p->cf=(a>>(s-1))&1;r=a>>s;}else{p->cf=s==32?a>>31:0;r=0;}}p->n=r>>31;p->z=r==0;return r;}
static inline uint32_t asr_(flags_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){if(s<32){p->cf=(a>>(s-1))&1;r=(uint32_t)((int32_t)a>>s);}else{p->cf=a>>31;r=(uint32_t)((int32_t)a>>31);}}p->n=r>>31;p->z=r==0;return r;}
static inline uint32_t ror_(flags_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){s&=31;if(s)r=(a>>s)|(a<<(32-s));p->cf=r>>31;}p->n=r>>31;p->z=r==0;return r;}
#ifdef TRACE_PC
#define TICK(a) (c->pc=(a))
#else
#define TICK(a) ((void)0)
#endif
