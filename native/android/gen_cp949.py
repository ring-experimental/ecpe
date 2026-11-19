# gen_cp949.py <출력 .c>: host/cp949.c의 표를 만든다. macOS의 iconv(CP949)를 기준으로 삼는다.
# 왜: Android NDK에는 iconv가 없고, 맥에서 보던 글과 한 글자도 다르면 안 된다.
# 파이썬 codec 'cp949'는 0xA2E8(㉾)을 비워 두어 맥 결과와 한 글자 달라서 쓰지 않는다.
# 표는 소스로 저장소에 둔다(공개 인코딩이라 원본 파생물이 아니다). 맥에서만 다시 만들 수 있다.
import os, subprocess, sys, tempfile
C = r'''
#include <iconv.h>
#include <stdio.h>
int main(void) {
    iconv_t cd = iconv_open("UTF-8", "CP949");
    for (int s = 0x81; s <= 0xFE; s++) for (int t = 0x41; t <= 0xFE; t++) {
        unsigned char in[2] = {s, t}; char *ip = (char *)in; size_t il = 2; char out[16], *op = out;
        size_t ol2 = 16; size_t r = iconv(cd, &ip, &il, &op, &ol2);
        if (r == (size_t)-1 || il) { iconv(cd, 0, 0, 0, 0); continue; }
        printf("%d %d", s, t);
        for (char *p = out; p < op; p++) printf(" %02x", (unsigned char)*p);
        printf("\n");
    }
    return 0;
}
'''
out = sys.argv[1]
with tempfile.TemporaryDirectory() as d:
    open(f'{d}/d.c', 'w').write(C)
    subprocess.run(['cc', f'{d}/d.c', '-o', f'{d}/d', '-liconv'], check=True)
    lines = subprocess.run([f'{d}/d'], capture_output=True, text=True, check=True).stdout.splitlines()
tab = {}
for l in lines:
    a = l.split()
    ch = bytes(int(x, 16) for x in a[2:]).decode()
    assert len(ch) == 1 and ord(ch) < 0x10000, l
    tab[(int(a[0]), int(a[1]))] = ord(ch)
with open(out, 'w') as w:
    w.write('// 자동 생성: android/gen_cp949.py (macOS iconv CP949). 고치지 말 것.\n')
    w.write('// 앞바이트 0x81~0xFE, 뒷바이트 0x41~0xFE 순. 0은 없는 글자.\n')
    w.write('#include <stdint.h>\nconst uint16_t cp949_tab[126 * 190] = {\n')
    for s in range(0x81, 0xFF):
        w.write(','.join(str(tab.get((s, t), 0)) for t in range(0x41, 0xFF)) + ',\n')
    w.write('};\n')
print(len(tab), 'entries')
