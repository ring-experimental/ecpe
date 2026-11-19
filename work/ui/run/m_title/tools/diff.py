import sys; sys.path.insert(0,__import__('os').path.join(__import__('os').path.normpath(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '../../../../..')), 'work/ui/run/m_title/tools'))
from m import *
a=M(sys.argv[1]); b=M(sys.argv[2]); base=int(sys.argv[3],16); n=int(sys.argv[4],16)
for o in range(0,n,4):
    x=a.w(base+o); y=b.w(base+o)
    if x!=y: print(f'+{o:#x}: {x:#x} -> {y:#x}')
