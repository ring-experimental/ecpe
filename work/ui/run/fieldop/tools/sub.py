import sys,re
# sub.py file fn : print subtree under first line whose stripped text starts with fn
f,fn=sys.argv[1],sys.argv[2]
L=open(f,encoding='cp949',errors='replace').read().split('\n')
for i,l in enumerate(L):
    s=l.strip()
    if s.split(' ')[0]==fn or s.startswith('@ '+fn+'('):
        ind=len(l)-len(l.lstrip()); print(l)
        for m in L[i+1:]:
            if len(m)-len(m.lstrip())<=ind: break
            print(m)
        break
