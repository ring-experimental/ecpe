import re,sys
b=open(__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'native/build/game/client.bin'),'rb').read()
G=0x4e624
def cstr(o):
    if not (0<o<len(b)): return None
    e=b.find(b'\0',o)
    if e<0 or e-o<2 or e-o>200: return None
    try: s=b[o:e].decode('cp949')
    except: return None
    return s if all(ch.isprintable() or ch=='\n' for ch in s) and any(ord(c)>0x7f or c.isalpha() for c in s) else None
lo,hi=int(sys.argv[1],16),int(sys.argv[2],16)
cur=None
for l in open(sys.argv[0].rsplit('/',1)[0]+'/all.dis'):
    if l.startswith('; ----'): cur=int(l.split()[2][2:],16); continue
    if cur is None or not (lo<=cur<hi): continue
    m=re.search(r'; =(0x[0-9a-f]+)',l)
    if m:
        v=int(m.group(1),16); o=(G+v)&0xffffffff
        s=cstr(o)
        if s: print('%x %s G+%x=%x %r'%(cur,l.split(':')[0].strip(),v,o,s))
