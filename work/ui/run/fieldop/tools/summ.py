import sys,re
# print path (function chain) for DrawString / frame calls, skipping field map
for f in sys.argv[1:]:
    print('=====',f.split('/')[-1])
    L=open(f,encoding='cp949',errors='replace').read().split('\n')
    stack=[]
    for l in L:
        if not l.strip(): continue
        ind=(len(l)-len(l.lstrip()))//2; s=l.strip()
        stack=stack[:ind]; 
        name=s.split('(')[0].replace('@ ','').split(' ')[0]
        stack.append(name)
        if 'DrawString' in s or re.match(r'@ (2ff80|56ec|5810|5fac)\(',s):
            if '2ee2c' in stack or '2f170' in stack or 'c66c' in stack: continue
            s2=re.sub(r'\[a[4-7]:[^]]*\]','',s)
            print('>'.join(stack[-7:-1]),'|',s2[:170])
