#!/usr/bin/env python3
"""client.bin의 판을 가려 빌드에 넘길 값을 찍는다(파이썬 표준 라이브러리만, 원본 내용 없음).

verpick.py <client.bin> map    기준 판이 아니면 native/ver/<AID>.map 경로(기준 판이면 아무것도 찍지 않는다)
verpick.py <client.bin> text   codemap.py에 넘길 코드 끝(기준 판 0x46878)
판은 실행 이미지의 SHA-256으로 확인한다. 모르는 판이면 실패한다.
"""
import hashlib, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KNOWN = {
    'f3b1e2b8aab918d9e567d819c5048f9a36be0acb37847e0171ca790f6850ab2e': None,          # KTF 010100D5 01.00.00(기준)
    '484e9f3aa0669ba07e02933577e1274aa573f02cdf75df171d613aa61998628a': '010100D2',    # KTF 010100D2 01.00.02
}
h = hashlib.sha256(open(sys.argv[1], 'rb').read()).hexdigest()
if h not in KNOWN: sys.exit(f'지원하지 않는 client.bin(SHA-256 {h})')
aid = KNOWN[h]
path = os.path.join(ROOT, 'native', 'ver', f'{aid}.map') if aid else None
if sys.argv[2] == 'map':
    if path: print(path)
elif sys.argv[2] == 'text':
    text = '0x46878'
    if path:
        for l in open(path):
            if l.startswith('text '): text = '0x' + l.split()[1]
    print(text)
