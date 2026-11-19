import sys,re
sys.path.insert(0,__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'work/ui/run/m_title/tools'))
from m import M
mem=M(sys.argv[2]) if len(sys.argv)>2 and sys.argv[2].endswith('.bin') else None
skipbg='--all' not in sys.argv
BG={'ec0c','ec1e','ecd2','ece4','ecf8','ed0c','eda4','f504','f518','f52e','f546'}
def S(a):
    if not mem or not (0x100000<=a<0x4000000): return ''
    try: return '"'+mem.str(a,60).replace('\n','⏎')+'"'
    except Exception: return ''
out=[];seen={}
for l in open(sys.argv[1]):
    p=l.split()
    if not p or p[0].startswith('--'): continue
    f=p[0]; d={k:v for k,v in (x.split('=') for x in p[1:])}
    lr=d['lr']
    if skipbg and lr in BG: continue
    r0=int(d['r0'],16); r1,r2,r3=int(d['r1']),int(d['r2']),int(d['r3']); s=[int(d[f's{i}']) for i in range(5)]
    c=lambda v:'#%02x%02x%02x'%((v>>8)&0xff,(v>>16)&0xff,(v>>24)&0xff)
    if f=='1415c': t=f'FILL x={r0} y={r1} w={r2} h={r3} c={c(s[0])}'
    elif f=='14184': t=f'RECT x={r0} y={r1} w={r2} h={r3} c={c(s[0])}'
    elif f=='37a24': t=f'BEVEL x={r0} y={r1} w={r2} h={r3} out={c(s[0])} in={c(s[1])}'
    elif f=='56ec': t=f'SPR sheet={r0:x} fr={r1} x={r2} y={r3} s0={s[0]} s1={s[1]} flip={s[2]}'
    elif f=='14240': t=f'TEXTBOX x={r0} y={r1} w={r2} h={r3} str={s[0]:x}{S(s[0])} flags={s[1]:x}'
    elif f=='3a1c': t=f'TEXT x={r1} y={r2} str={r3:x}{S(r3)}'
    elif f=='3bf0': t=f'TEXTRECT x={r1} y={r2} w={r3} h={s[0]} str={s[1]:x}{S(s[1])} flags={s[2]:x}'
    elif f=='261d4': t=f'GRAD r0={r0:x} c1={c(r1)} c2={c(r2)}'
    elif f in ('54f0','54dc','467c','3298','4714','4798','47e8','4838','4888','3b48','5ae8'): continue
    else: t=l.strip()
    key=(t,lr)
    if key in seen: seen[key]+=1; continue
    seen[key]=1; out.append(key)
for t,lr in out: print(f'{t}  (lr {lr})' + (f' x{seen[(t,lr)]}' if seen[(t,lr)]>1 else ''))
