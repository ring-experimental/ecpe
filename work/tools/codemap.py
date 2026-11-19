#!/usr/bin/env python3
"""client.bin 재귀 하강 분석. 코드/리터럴/점프표/데이터 구역을 나눈다.

주소는 파일 오프셋이다. Ghidra 주소 = 오프셋 + 0x100000, 이미지 기준 오프셋 = 파일 오프셋 - BASE.
GCC Thumb PIC 코드라 sl = GOT 시작이고, 점프표는 sl 상대, 항목은 표 시작 상대다.
후보는 시험 해석을 통과해야만 확정한다. 게임은 ARMv4T라 BL 외 4바이트 명령과 v5 이상 명령은 무효다.
"""
import bisect, json, struct, sys, collections
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_ARM

data = open(sys.argv[1], 'rb').read()
out = sys.argv[2]
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
RELOC_N = u32(0x70)
BASE = 0x74 + RELOC_N * 4
GOT0, GOT1 = BASE + u32(BASE), BASE + u32(BASE + 4)
relocs = [BASE + u32(0x74 + i * 4) for i in range(RELOC_N)]
got = [u32(o) for o in range(GOT0, GOT1, 4)]
assert BASE + u32(BASE + 8) == len(data)

th = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
ar = Cs(CS_ARCH_ARM, CS_MODE_ARM)
THUMB1 = {'adc','adcs','add','adds','and','ands','asr','asrs','b','bic','bics','bl','bx','cmn','cmp','eor','eors',
          'ldm','ldmia','ldr','ldrb','ldrh','ldrsb','ldrsh','lsl','lsls','lsr','lsrs','mov','movs','mul','muls','mvn','mvns',
          'neg','negs','rsb','rsbs','orr','orrs','pop','push','ror','rors','sbc','sbcs','stm','stmia','str','strb','strh',
          'sub','subs','svc','tst'}
COND = {'eq','ne','hs','lo','mi','pl','vs','vc','hi','ls','ge','lt','gt','le','cs','cc'}
K_T, K_A, K_L, K_J = 1, 2, 3, 4
kind = bytearray(len(data))
# 텍스트 끝: 이 뒤로는 문자열과 데이터만 나온다. 에픽크로니클PE는 0x46878(소수 상수 뒤 서버 주소 문자열부터 데이터).
TEXT_MAX = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x46878

far_jumps = set()
FAR = {}

def decode(pc, thumb):
    ins = next((th if thumb else ar).disasm(data[pc:pc + 4], pc, 1), None)
    return ins

def plausible_head(o):
    """push lr 뒤 몇 명령이 Thumb1로 풀려야 함수 머리로 본다. 점프표·리터럴 안의 0xb5 바이트를 거른다."""
    pc = o
    for _ in range(6):
        ins = decode(pc, True)
        if ins is None or (ins.size == 4 and ins.mnemonic != 'bl'):
            return False
        b = ins.mnemonic.split('.')[0]
        if b not in THUMB1 and not (b[0] == 'b' and b[1:] in COND):
            return False
        pc += ins.size
    return True

PROLOGUES = sorted(o for o in range(BASE, GOT0, 2) if data[o + 1] == 0xb5 and plausible_head(o))

def prologue_window(pc):
    i = bisect.bisect_right(PROLOGUES, pc)
    return (PROLOGUES[i - 1] if i else BASE, PROLOGUES[i] if i < len(PROLOGUES) else TEXT_MAX)

def trial(entry, thumb, strict=True):
    """entry에서 닿는 CFG를 해석한다. 실패 사유 또는 (명령, 리터럴, 점프표, BL, 포인터, 미해결)을 돌려준다."""
    insns, lits, tables, bls, ptrs, tails = {}, set(), {}, set(), set(), []
    local_far = set()
    FAR[entry] = local_far
    work = [entry]
    while work:
        pc = work.pop()
        hist = []
        while True:
            if not (BASE <= pc < TEXT_MAX or 0x0c <= pc < 0x70) or (thumb and pc & 1) or (not thumb and pc & 3):
                return f'range {pc:#x}'
            if pc in insns:
                break
            # 호출이 확인된 진입점은 다른 함수의 리터럴 추정보다 우선한다.
            if (strict and kind[pc] in (K_L, K_J)) or (kind[pc] in (K_T, K_A) and kind[pc] != (K_T if thumb else K_A)):
                return f'overlap {pc:#x}'
            if pc in lits:
                return f'selfoverlap {pc:#x}'
            ins = decode(pc, thumb)
            if ins is None:
                return f'undecodable {pc:#x}'
            m, ops = ins.mnemonic, ins.op_str
            base = m.split('.')[0]
            if thumb:
                condbr = base[0] == 'b' and base[1:] in COND
                if base not in THUMB1 and not condbr:
                    return f'insn {pc:#x} {m}'
                if ins.size == 4 and base != 'bl':
                    return f'wide {pc:#x} {m}'
            insns[pc] = ins.size
            hist.append((pc, base, ops))
            nxt = pc + ins.size
            if 'pc, #' in ops and base == 'ldr':
                imm = int(ops.split('#')[1].rstrip(']'), 0)
                lit = ((pc + (4 if thumb else 8)) & ~3) + imm
                if lit + 4 > TEXT_MAX or lit in insns or lit + 2 in insns:
                    return f'litrange {pc:#x}'
                lits.add(lit)
                v = u32(lit)
                sv = v - (1 << 32) if v >> 31 else v
                t = GOT0 + sv
                if v & 1 and BASE <= t & ~1 < TEXT_MAX:
                    ptrs.add(t & ~1)
            if base == 'bl':
                t = int(ops.split('#')[1], 0)
                if not (BASE <= t < TEXT_MAX) and t != 0xc:
                    return f'bl range {pc:#x}'
                if t & 1:
                    return f'bl odd {pc:#x}'
                # GCC Thumb는 B(±2KB)가 닿지 않을 때만 먼 분기에 BL을 쓴다. 같은 함수 창 안의 먼 대상이면 호출이 아니라 점프다(컴파일러 길이 추정이 보수적이라 1980 거리도 관측됨).
                w0, w1 = prologue_window(pc)
                # 창 안에 들지 않아도, BL 뒤가 리터럴(위 절반 0)이면 돌아올 곳이 없으니 점프다.
                nh = struct.unpack_from('<H', data, pc + 4)[0]
                ends = nh & 0xff00 == 0xbd00 or nh & 0xff87 == 0x4700 or nh & 0xf800 == 0xe000
                after_is_data = not ends and data[pc + 6:pc + 8] == b'\0\0' and data[t + 1] != 0xb5
                if after_is_data or (abs(t - pc) >= 1500 and w0 < t < w1):
                    far_jumps.add(pc)
                    local_far.add(pc)
                    work.append(t)
                    break
                bls.add(t)
            elif base == 'b':
                work.append(int(ops.split('#')[1], 0)); break
            elif base[0] == 'b' and base[1:] in COND:
                work.append(int(ops.split('#')[1], 0))
            elif base == 'bx':
                if ops == 'pc':
                    return ('veneer', (pc + 4) & ~3, insns, lits, tables, bls, ptrs, tails)
                if ops != 'lr':
                    tails.append((pc, f'bx {ops}'))
                break
            elif base == 'pop' and 'pc' in ops:
                break
            elif (base == 'mov' or base == 'add') and ops.startswith('pc,'):
                t = jumptable(hist)
                if t:
                    tab, n = t
                    tables[tab] = n
                    for i in range(n):
                        if tab + i * 4 + 4 > GOT0:
                            return f'table range {pc:#x}'
                        v = u32(tab + i * 4)
                        work.append((tab + (v - (1 << 32) if v >> 31 else v)) & 0xffffffff)
                else:
                    tails.append((pc, f'{base} {ops}'))
                break
            elif not thumb and base == 'ldr' and ops.startswith('pc,'):
                tails.append((pc, f'{base} {ops}')); break
            pc = nxt
    return (None, None, insns, lits, tables, bls, ptrs, tails)

def jumptable(hist):
    """... cmp rX,#n; bhi; ldr rY,=off; ...; ldr r,[a,b]; adds r,r,x; mov pc,r 형태만 인식.
    cmp와 리터럴이 앞 블록에 있을 수 있어 실행 경로가 아니라 직전 바이트를 거슬러 찾는다."""
    pc = hist[-1][0]
    rev = []
    for o in range(pc - 2, max(pc - 80, BASE), -2):
        i = decode(o, True)
        rev.append((o, i.mnemonic, i.op_str) if i else (o, '', ''))
    if not any(m == 'ldr' and '[r' in o and '#' not in o and 'pc' not in o for _, m, o in rev[:5]):
        return None
    off = n = None
    for o, m, ops in rev:
        if off is None and m == 'ldr' and 'pc, #' in ops:
            off = u32(((o + 4) & ~3) + int(ops.split('#')[1].rstrip(']'), 0))
        if n is None and m == 'cmp' and '#' in ops:
            n = int(ops.split('#')[1], 0) + 1
        if off is not None and n is not None:
            return (GOT0 + (off - (1 << 32) if off >> 31 else off), n)
    return None

roots = {}
rejected = collections.Counter()
rejected_list = {}
tails_all, tables_all, bl_edges = [], {}, collections.Counter()
missing_bl = []

def accept(entry, thumb, why, r):
    _, _, insns, lits, tables, bls, ptrs, tails = r
    k = K_T if thumb else K_A
    for pc, sz in insns.items():
        for i in range(sz):
            kind[pc + i] = k
    for l in lits:
        for i in range(4):
            kind[l + i] = K_L
    for t, n in tables.items():
        tables_all[t] = n
        for i in range(n * 4):
            kind[t + i] = K_J
    roots[entry] = {'thumb': thumb, 'why': why, 'insns': len(insns), 'end': max(insns) + 2,
                    'addrs': sorted(insns), 'tables': sorted(tables.items()), 'calls': sorted(bls), 'far': sorted(FAR.get(entry, ()))}
    tails_all.extend(tails)
    return bls, ptrs

def try_root(entry, thumb, why, queue):
    if entry in roots or entry in rejected_list:
        return False
    # 다른 함수 본문 안의 진입점도 자기 본문을 따로 갖는다. 번역본은 같은 코드를 함수마다 복제한다.
    r = trial(entry, thumb, why in ('push', 'gap'))
    if isinstance(r, str):
        rejected[why] += 1
        rejected_list[entry] = (why, r)
        return False
    bls, ptrs = accept(entry, thumb, why, r)
    if r[0] == 'veneer':
        queue.append((r[1], False, 'veneer'))
    for t in bls:
        bl_edges[t] += 1
        queue.append((t, thumb, 'bl'))
    for t in ptrs:
        queue.append((t, True, 'ptr'))
    return True

def drain(queue):
    while queue:
        try_root(*queue.pop(), queue)

q = [(0x0c, True, 'entry')]
for o in relocs:
    v = u32(o)
    if v & 1 and BASE + (v & ~1) < TEXT_MAX:
        q.append((BASE + (v & ~1), True, 'reloc'))
for v in got:
    if v & 1 and BASE + (v & ~1) < TEXT_MAX:
        q.append((BASE + (v & ~1), True, 'got'))
drain(q)
n_static = len(roots)

# 함수 머리: push {.., lr}. 가변 인자 함수의 push {r1-r3}; push {lr}도 같은 규칙으로 잡는다.
for o in range(BASE, TEXT_MAX, 2):
    if o < TEXT_MAX and kind[o] == 0 and data[o + 1] == 0xb5:
        start = o - 2 if struct.unpack_from('<H', data, o - 2)[0] & 0xff00 == 0xb400 and kind[o - 2] == 0 else o
        q.append((start, True, 'push'))
        drain(q)
n_push = len(roots)

# 남은 틈: 앞 함수의 끝 바로 뒤에서 머리 없는 리프 함수를 찾는다. 텍스트 안(마지막 확정 코드 앞)만.
changed = True
while changed:
    changed = False
    last = TEXT_MAX
    o = BASE
    while o < last:
        # 0 패딩은 건너뛰고, 위 절반이 0인 워드는 작은 정수나 오프셋 데이터로 본다. bx pc 베니어만 예외다.
        hw = data[o:o + 2]
        prev_ok = kind[o - 1] != 0 or data[o - 2:o] == b'\0\0'
        if kind[o] == 0 and o % 2 == 0 and prev_ok and o + 4 <= TEXT_MAX and hw != b'\0\0' and (data[o + 2:o + 4] != b'\0\0' or hw == b'\x78\x47'):
            n = len(roots)
            q.append((o, True, 'gap'))
            drain(q)
            if len(roots) > n:
                changed = True
        o += 2

last_code = TEXT_MAX
# 틈 분류
def classify(a, b):
    s = data[a:b]
    if all(x == 0 for x in s):
        return 'zero'
    if b - a <= 3:
        return 'pad'
    txt = sum(1 for x in s if 0x20 <= x < 0x7f or x in (0, 9, 10, 13) or x >= 0xa1)
    if txt / len(s) > 0.9:
        return 'text'
    return 'unknown'
gaps = []
i = BASE
while i < GOT0:
    if kind[i] == 0:
        j = i
        while j < GOT0 and kind[j] == 0:
            j += 1
        gaps.append((i, j, classify(i, j)))
        i = j
    else:
        i += 1
inside = [g for g in gaps if g[1] <= last_code]
after = [g for g in gaps if g[0] > last_code]
c = collections.Counter(kind[BASE:GOT0])
c_in = collections.Counter(kind[BASE:last_code])
gs = collections.Counter()
for a, b, k in inside:
    gs[k] += b - a
text_span = last_code - BASE
summary = {
    'file_size': len(data), 'startup': [0, 0x70], 'reloc_table': [0x70, BASE], 'RELOC_N': RELOC_N,
    'BASE': BASE, 'GOT': [GOT0, GOT1], 'GOT_n': (GOT1 - GOT0) // 4,
    'bss_image': [u32(BASE + 8), u32(BASE + 12)], 'bss_size': u32(BASE + 12) - u32(BASE + 8),
    'last_code': last_code, 'text_span': text_span,
    'in_text': {'thumb': c_in[K_T], 'arm': c_in[K_A], 'literal': c_in[K_L], 'jumptable': c_in[K_J], 'gap': c_in[0]},
    'gap_kinds_in_text': gs, 'after_text': sum(b - a for a, b, _ in after),
    'functions': len(roots), 'function_why': collections.Counter(r['why'] for r in roots.values()),
    'far_jumps': len(far_jumps), 'jump_tables': len(tables_all), 'rejected': rejected, 'indirect_tails': len(tails_all),
    'tails_kinds': collections.Counter(m for _, m in tails_all),
    'static_functions': n_static, 'with_push': n_push,
}
print(json.dumps(summary, indent=1, default=lambda x: dict(x)))
json.dump({'summary': summary, 'roots': {hex(k): v for k, v in sorted(roots.items())},
           'tables': {hex(k): v for k, v in sorted(tables_all.items())},
           'gaps': [(a, b, k) for a, b, k in gaps], 'rejected': {hex(k): v for k, v in rejected_list.items()}},
          open(out, 'w'), default=lambda x: dict(x))
open(out + '.kind', 'wb').write(bytes(kind))
print('largest unknown gaps in text:')
for a, b, k in sorted((g for g in inside if g[2] == 'unknown'), key=lambda g: g[0] - g[1])[:25]:
    print(f'  {a:#x}-{b:#x} {b-a} {data[a:a+16].hex()}')
print('largest text gaps in text:')
for a, b, k in sorted((g for g in inside if g[2] == 'text'), key=lambda g: g[0] - g[1])[:5]:
    print(f'  {a:#x}-{b:#x} {b-a}')
