# cflat.py <calltree> <root fn hex> [skip fns comma]: flatten subtree; print ARGS / DrawString leaves with parent chain (last 3)
import sys,re
L=open(sys.argv[1],encoding='cp949',errors='replace').read().split('\n')
FN=1
root=sys.argv[2]; skip=set(sys.argv[3].split(',')) if len(sys.argv)>3 else set()
on=False; rd=0; st=[]
for l in L:
    if not l.strip(): continue
    d=(len(l)-len(l.lstrip()))//2; t=l.strip()
    if not on:
        if not t.startswith('*') and t.split()[0]==root: on=True; rd=d; st=[t.split()[0]]
        continue
    if d<=rd: break
    st=st[:d-rd]
    if t.startswith('*'):
        m=re.match(r'\* ARGS (\w+) (.*?) \(r1=',t)
        if m:
            if m.group(1) in skip: continue
            print('  '*(d-rd)+'%s %s'%(m.group(1),m.group(2)) + ('  '+t[t.find('*r1'):t.find(' (lr')] if '*r1' in t else ''))
        elif 'DrawString' in t: print('  '*(d-rd)+t[2:t.find(' (lr')]+'   <'+'>'.join(st[-3:]))
    else:
        st.append(t.split()[0])
        if FN and t.split()[0] not in skip: print("  "*(d-rd)+"["+t+"]")
