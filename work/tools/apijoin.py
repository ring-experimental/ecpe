#!/usr/bin/env python3
"""apiscan 결과와 WIE 호출 로그를 합쳐 슬롯별 WIPI API 표를 만든다.
apijoin.py <apiscan.json> <calls.txt> <wie 저장소> <out.tsv>"""
import collections, json, re, sys
scan = json.load(open(sys.argv[1]))
wie = sys.argv[3]
src = open(wie + '/wie-ktf/src/runtime/svc_ids.rs').read()
mt = open(wie + '/wie-ktf/src/runtime/wipi_c/method_table.rs').read()

def enum_names(name):
    body = re.search(r'pub enum ' + name + r' \{(.*?)\n\}', src, re.S).group(1)
    out, i = {}, 0
    for line in body.splitlines():
        m = re.match(r'\s*(\w+)(?:\s*=\s*(\d+))?,', line)
        if m:
            if m.group(2):
                i = int(m.group(2))
            out[i] = m.group(1); i += 1
    return out

def table_list(fn):
    m = re.search(r'pub fn ' + fn + r'\(\)[^{]*\{(.*?)\n\}', mt, re.S)
    if not m:
        return {}
    names = re.findall(r'^\s*(?:(\w+(?:::\w+)*)\.into_body\(\)|gen_stub\(\s*\d+,\s*"([^"]+)"\))', m.group(1), re.M)
    return {i: (a.split('::')[-1] or b) or b for i, (a, b) in enumerate(names)}
NAMES = {0: enum_names('WIPICKernelMethodId'), 3: enum_names('WIPICGraphicsMethodId'), 7: enum_names('WIPICFsMethodId'),
         1: table_list('get_util_method_table'), 2: table_list('get_misc_method_table'), 4: table_list('get_im_method_table'),
         9: table_list('get_uic_method_table'), 10: table_list('get_media_method_table'), 11: table_list('get_net_method_table'),
         13: table_list('get_unk12_method_table')}
TABLES = {0:'Kernel',1:'Util',2:'Misc',3:'Graphics',4:'Im',5:'If4',6:'If5',7:'Fs',8:'If7',9:'Uic',10:'Media',11:'Net',12:'If11',13:'If12',14:'If13',15:'If14',16:'If15',17:'If16'}
calls = collections.Counter(); by_lr = {}
for line in open(sys.argv[2]):
    n, i, lr = line.split()
    i, lr = int(i, 16), int(lr, 16)
    calls[i] += int(n)
    by_lr[(lr & ~1) - 0x100000] = i
slot_site = {}
for a in scan['api']:
    for s in a['sites']:
        slot_site[s + 4] = (a['global'], a['offset'])
# 전역 -> 표: 관측된 호출지로 정한다.
votes = collections.defaultdict(collections.Counter)
matched = 0
for lr, i in by_lr.items():
    if lr in slot_site:
        g, off = slot_site[lr]
        matched += 1
        votes[g][(i >> 16, (i & 0xffff) * 4 - off)] += 1
print('observed call sites', len(by_lr), 'matched to static slots', matched)
g2t = {}
for g, c in votes.items():
    (t, delta), n = c.most_common(1)[0]
    g2t[g] = (t, delta)
    print(f'global {g:#x} -> {TABLES[t]} delta {delta} votes {dict(c)}')
# 초기화 함수 0x855e8이 WIPICInterface 필드를 이 순서로 전역에 저장한다. 관측 표와 어긋나면 중단한다.
INIT = {0x110b00: 0, 0x110a80: 1, 0x1109e0: 2, 0x110920: 3, 0x110ae0: 4, 0x110b40: 5, 0x1108c0: 6, 0x1109c0: 7, 0x1109a0: 8,
        0x110aa0: 9, 0x1108e0: 10, 0x110960: 11, 0x110a60: 12, 0x110980: 13, 0x110a00: 14, 0x1108a0: 15, 0x110b20: 16, 0x110860: 17}
for g, (t, d) in g2t.items():
    assert INIT.get(g) == t and d == 0, hex(g)
g2t = {g: (t, 0) for g, t in INIT.items()}
rows = {}
def row(i):
    return rows.setdefault(i, {'sites': 0, 'funcs': set(), 'n': 0})
for a in scan['api']:
    if a['global'] in g2t:
        t, d = g2t[a['global']]
        r = row((t << 16) | ((a['offset'] + d) // 4))
        r['sites'] += len(a['sites']); r['funcs'] |= set(a['funcs'])
for i, n in calls.items():
    row(i)['n'] = n
un = [a for a in scan['api'] if a['global'] not in g2t]
print('slots on unmapped globals', [(hex(a['global']), a['offset'], len(a['sites'])) for a in un])
with open(sys.argv[4], 'w') as f:
    f.write('table\tindex\tname\tstatic_sites\truntime_calls\twie\n')
    for i in sorted(rows):
        t, x = i >> 16, i & 0xffff
        nm = NAMES.get(t, {}).get(x, '?')
        stub = 'stub' if re.search(r'^(MC|OEMC)_', nm) or t in (5, 6, 8, 12, 14, 15, 16, 17) else ''
        f.write(f"{TABLES.get(t,t)}\t{x}\t{nm}\t{rows[i]['sites']}\t{rows[i]['n']}\t{stub}\n")
print(open(sys.argv[4]).read())
print('runtime calls not at matched static slots:', sorted({hex(l) for l in by_lr if l not in slot_site})[:40])
