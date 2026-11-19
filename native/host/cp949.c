// CP949 -> UTF-8 표 변환. ui.cpp의 to_utf8이 쓴다.
// 왜: Android NDK에는 iconv가 없다. 맥에서도 같은 표를 쓰게 해서 두 플랫폼 결과를 같게 한다(표는 cp949_table.c).
// 못 바꾸는 바이트는 한 바이트씩 '?'로 바꾼다(맥의 iconv 루프와 같은 동작).
#include <stddef.h>
#include <stdint.h>

extern const uint16_t cp949_tab[126 * 190];

// out은 len*3+4 바이트 이상. 쓴 바이트 수를 돌려준다.
size_t cp949_to_utf8(const unsigned char *s, int len, char *out) {
    char *o = out;
    for (int i = 0; i < len;) {
        unsigned c = s[i];
        unsigned u = 0;
        if (c < 0x80) { *o++ = (char)c; i++; continue; }
        if (c >= 0x81 && c <= 0xFE && i + 1 < len && s[i + 1] >= 0x41 && s[i + 1] <= 0xFE) {
            u = cp949_tab[(c - 0x81) * 190 + (s[i + 1] - 0x41)];
            if (u) i += 2;
        }
        if (!u) { *o++ = '?'; i++; continue; }
        if (u < 0x800) { *o++ = (char)(0xC0 | u >> 6); *o++ = (char)(0x80 | (u & 63)); }
        else { *o++ = (char)(0xE0 | u >> 12); *o++ = (char)(0x80 | (u >> 6 & 63)); *o++ = (char)(0x80 | (u & 63)); }
    }
    return (size_t)(o - out);
}
