#!/usr/bin/env python3
"""UI 그리기 함수 정적 전수 조사. sa.json(sa.py 출력)을 읽어 프리미티브에 3단계 안에 닿는 함수를 모으고 화면별로 묶는다."""
import json, struct, collections, sys
ROOT = __import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../..'))
J = json.load(open(f'{ROOT}/work/ui/run/prim/sa.json'))
F = {int(k, 16): v for k, v in J['funcs'].items()}
d = open(f'{ROOT}/native/build/game/client.bin', 'rb').read()
B = 0x1254
u32 = lambda o: struct.unpack_from('<I', d, o)[0]
N = u32(0x70); REL = sorted(B + u32(0x74 + 4 * i) for i in range(N)); RELS = set(REL)

def cstr(o):
    if not (B <= o < len(d)): return None
    e = d.find(b'\0', o)
    if e < 0 or e - o < 1 or e - o > 300: return None
    try: s = d[o:e].decode('cp949')
    except Exception: return None
    if not all(c.isprintable() or c in '\n\r' for c in s): return None
    if len(s) < 2 and not any('가' <= c <= '힣' for c in s): return None
    return s

# 문자열 참조: 직접(refs) + 포인터 배열(재배치 워드가 문자열을 가리키는 연속 배열)을 참조하는 경우
def strs_of(f):
    out = []
    for r in F[f]['refs']:
        s = cstr(r)
        if s: out.append((r, s)); continue
        # 포인터 배열
        a = r; k = 0
        while a in RELS and k < 64:
            t = B + u32(a); s2 = cstr(t)
            if s2: out.append((t, s2))
            else: break
            a += 4; k += 1
    return out

TEXT = {0x3a1c: 'drawText(xy)', 0x3b48: 'marqueeText', 0x3c0c: 'textBox', 0x3bb0: 'textBox(char*)', 0x3bf0: 'textBox(x,y,w,h)',
        0x14240: 'textBoxMain', 0x3d68: 'drawLine', 0x3ecc: 'measure', 0x37a78: 'drawNumber(bitmap)'}
CHROME = {0x2ff80: 'window', 0x1415c: 'fillRect', 0x14184: 'drawRect', 0x467c: 'fillRect(gfx)', 0x4714: 'drawRect(gfx)',
          0x4760: 'drawLine(gfx)', 0x46c8: 'slot170', 0x4934: 'slot17c', 0x48fc: 'slot180', 0x4c9c: 'slot184', 0x4c74: 'slot174',
          0x37a24: 'gaugeBox'}
SPRITE = {0x4798: 'blit', 0x47e8: 'blitV', 0x4838: 'blitH', 0x4888: 'blitHV', 0x48d8: 'blitXY', 0x49a8: 'blitFx',
          0x56ec: 'sprComposite', 0x5810: 'sprFrame', 0x60dc: 'slotFrame', 0x58dc: 'sprFrameFx', 0x6104: 'slotFrameFx',
          0x5ae8: 'sprAnimFx', 0x5f84: 'slotAnim', 0x5fdc: 'slotAnim2', 0x5ffc: 'slotAnim3', 0x6038: 'slotAnim4', 0x5fac: 'slotComposite'}
PRIM = {**{k: 'T:' + v for k, v in TEXT.items()}, **{k: 'C:' + v for k, v in CHROME.items()}, **{k: 'S:' + v for k, v in SPRITE.items()}}
# 프리미티브 내부(이것들은 목록에서 뺀다)
LOW = set(range(0x1f00, 0x7500)) | {0x144b0, 0x3298}

calls = {f: sorted(set(v['calls'])) for f, v in F.items()}
callers = collections.defaultdict(set)
for f, cs in calls.items():
    for t in cs: callers[t].add(f)

# 3단계 안에 닿는 프리미티브
def reach(f, depth=3):
    got = collections.Counter(); seen = {f}; fr = [f]
    for dd in range(depth):
        nx = []
        for x in fr:
            for t in calls.get(x, []):
                if t in PRIM: got[PRIM[t]] += 1
                if t not in seen and t not in LOW and t in F: seen.add(t); nx.append(t)
        fr = nx
    return got

# vtable: 생성자가 저장하는 GOT 포인터 → 슬롯
VSTARTS = sorted(set(r for v in F.values() for r in v['vstores'] if 0x4d000 <= r < 0x4dc10))
def vt_slots(start):
    out = []; a = start
    import bisect
    i = bisect.bisect_right(VSTARTS, start); end = VSTARTS[i] if i < len(VSTARTS) else start + 160
    while a in RELS and len(out) < 40 and a < end:
        v = u32(a); t = B + (v & ~1)
        if v & 1 and t in F: out.append(t)
        else: break
        a += 4
    return out
classes = {}
for f, v in F.items():
    for r in v['vstores']:
        if 0x4d000 <= r < 0x4dc10:
            sl = vt_slots(r)
            if sl: classes.setdefault(r, {'ctor': set(), 'slots': sl})['ctor'].add(f)
method_of = collections.defaultdict(set)
for vt, c in classes.items():
    for i, m in enumerate(c['slots']): method_of[m].add((vt, i))
# 함수 포인터 참조(장면 표 등)
fptr_owner = collections.defaultdict(set)
for f, v in F.items():
    for t in v['fptrs']: fptr_owner[t].add(f)

# 기준점(이름 붙인 뿌리)
ANCH = {}
SCENES = [0xe588, 0xe5e4, 0xe85c, 0x10ac8, 0xebc4, 0x10458, 0x104e0, 0x1054c, 0x10674, 0x106c8, 0x10704, 0x10740, 0x108f4, 0x109f0, 0x109b8, 0x10af0, 0x10b0c, 0, 0x108b8, 0x10a0c, 0x10a70, 0x10b8c, 0x10ef0, 0x1048c]
for i, s in enumerate(SCENES):
    if s: ANCH[s] = f'scene{i}'
EXTRA = json.load(open(f'{ROOT}/work/ui/run/prim/anchors.json')) if len(sys.argv) > 1 else {}
for k, v in EXTRA.items(): ANCH[int(k, 16)] = v

def up(f, maxd=8):
    """부모 쪽으로 BFS. 직접 호출자, vtable 생성자, 함수 포인터 소유자를 따라 가장 가까운 기준점들."""
    seen = {f: 0}; fr = [f]; found = []
    for dd in range(1, maxd + 1):
        nx = []
        for x in fr:
            ps = set(callers.get(x, ()))
            for vt, i in method_of.get(x, ()): ps |= classes[vt]['ctor']
            ps |= fptr_owner.get(x, set())
            for p in ps:
                if p in seen: continue
                seen[p] = dd; nx.append(p)
                if p in ANCH: found.append((dd, ANCH[p], p))
        if found: return found
        fr = nx
    return found

rows = []
for f in sorted(F):
    if f in LOW or f in PRIM: continue
    r = reach(f)
    if not r: continue
    direct = collections.Counter(PRIM[t] for t in F[f]['calls'] if t in PRIM)
    anc = [(0, ANCH[f], f)] if f in ANCH else up(f)
    st = strs_of(f)
    meth = [f'vt{vt:x}[{i}]' for vt, i in sorted(method_of.get(f, ()))]
    rows.append(dict(f=f, size=F[f]['size'], direct=dict(direct), reach=dict(r), anc=anc, strs=st, meth=meth,
                     callers=sorted(callers.get(f, ())), ic=len(F[f]['icalls'])))
json.dump([{**r, 'anc': [[a, b, f'{c:x}'] for a, b, c in r['anc']], 'strs': [[f'{a:x}', s] for a, s in r['strs']],
            'callers': [f'{x:x}' for x in r['callers']]} for r in rows], open(f'{ROOT}/work/ui/run/prim/sweep.json', 'w'), ensure_ascii=False)
for r in rows:
    an = ','.join(sorted(set(b for a, b, c in r['anc'])))
    print(f"{r['f']:x}\t{r['size']}\t{an or '?'}\t{' '.join(r['meth'])}\tD:{','.join(f'{k}{v}' for k,v in r['direct'].items())}\tS:{' | '.join(s for a,s in r['strs'])[:100]}")
print(len(rows), 'ui funcs', file=sys.stderr)
print({f'{k:x}': {'ctor': [f'{x:x}' for x in v['ctor']], 'slots': [f'{x:x}' for x in v['slots']]} for k, v in classes.items()}, file=open(f'{ROOT}/work/ui/run/prim/classes.txt', 'w'))
