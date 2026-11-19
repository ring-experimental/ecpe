// 플랫폼 글꼴. 게임은 글자를 MC_grpDrawString/GetStringWidth로 그린다(크로노스 윙과 달리 게임 글꼴이 없다).
// 글리프는 빌드 때 work/tools/glyphdump가 WIE와 같은 방식(neodgm.ttf, ab_glyph 10pt)으로 만든 표를 쓴다.
// 문자열은 CP949이고 해석은 WHATWG EUC-KR 디코더(encoding_rs)와 같다.
#include "host.h"
#include <stdio.h>
#include <stdlib.h>

enum { FONT_SIZE = 10, BUCKETS = 32768 };
typedef struct glyph { uint16_t code; uint32_t ch; float adv, minx, miny; uint16_t w, h; const uint8_t *cov; struct glyph *next; } glyph_t;

static uint8_t *blob;
static glyph_t *glyphs, *buckets[BUCKETS], *replacement;

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static float rdf(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }
static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }

bool font_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "글꼴 표 없음: %s\n", path); return false; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    blob = malloc((size_t)len);
    if (!blob || fread(blob, 1, (size_t)len, f) != (size_t)len || memcmp(blob, "GLY1", 4)) { fclose(f); return false; }
    fclose(f);
    uint32_t n = rd32(blob + 4);
    glyphs = calloc(n, sizeof *glyphs);
    const uint8_t *p = blob + 8;
    for (uint32_t i = 0; i < n; i++) {
        glyph_t *g = &glyphs[i];
        g->code = rd16(p); g->ch = rd32(p + 2); g->adv = rdf(p + 6); g->minx = rdf(p + 10); g->miny = rdf(p + 14);
        g->w = rd16(p + 18); g->h = rd16(p + 20); g->cov = p + 22;
        p += 22 + (size_t)g->w * g->h;
        if (g->code == 0xfffd) replacement = g;
        g->next = buckets[g->code % BUCKETS];
        buckets[g->code % BUCKETS] = g;
    }
    return true;
}

static glyph_t *lookup(uint16_t code) {
    for (glyph_t *g = buckets[code % BUCKETS]; g; g = g->next)
        if (g->code == code) return g;
    return NULL;
}

// 다음 글자를 돌려주고 쓴 바이트 수를 *used에 남긴다. 표에 없는 두 바이트는 U+FFFD이고, 둘째 바이트가 ASCII면 그 바이트는 다시 읽는다.
static glyph_t *next_glyph(const uint8_t *s, int left, int *used) {
    uint8_t b = s[0];
    *used = 1;
    if (b < 0x80) return lookup(b);
    if (b < 0x81 || b == 0xff || left < 2) return replacement;
    glyph_t *g = lookup((uint16_t)(b << 8 | s[1]));
    if (g) { *used = 2; return g; }
    if (s[1] >= 0x80) *used = 2;
    return replacement;
}

static bool is_control(uint32_t ch) { return ch < 0x20 || (ch >= 0x7f && ch < 0xa0); }

int font_string_width(const uint8_t *s, int len) {
    float w = 0;
    for (int i = 0, used; i < len; i += used) {
        glyph_t *g = next_glyph(s + i, len - i, &used);
        if (g) w += g->adv;
    }
    return (int)w;
}

// WIE canvas.draw_text와 같은 위치 계산과 섞기. 색은 RGB565를 8비트로 늘려 섞고 다시 줄인다.
void font_draw(uint16_t *px, int bpl_px, int w, int h, int x, int baseline, const uint8_t *s, int len, uint16_t color) {
    int cr = ((color >> 11 & 31) * 255 + 15) / 31, cg = ((color >> 5 & 63) * 255 + 31) / 63, cb = ((color & 31) * 255 + 15) / 31;
    int top = baseline - FONT_SIZE;
    float pos = 0;
    for (int i = 0, used; i < len; i += used) {
        glyph_t *g = next_glyph(s + i, len - i, &used);
        if (!g || is_control(g->ch)) continue;
        for (int gy = 0; gy < g->h; gy++)
            for (int gx = 0; gx < g->w; gx++) {
                uint8_t a = g->cov[gy * g->w + gx];
                if (!a) continue;
                int X = x + (int)((float)gx + g->minx + pos), Y = top + (int)((float)gy + g->miny + (float)FONT_SIZE);
                if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
                uint16_t *d = &px[Y * bpl_px + X];
                if (a == 255) { *d = (uint16_t)((cr >> 3) << 11 | (cg >> 2) << 5 | cb >> 3); continue; }
                uint16_t bg = *d;
                int br = ((bg >> 11 & 31) * 255 + 15) / 31, bgg = ((bg >> 5 & 63) * 255 + 31) / 63, bb = ((bg & 31) * 255 + 15) / 31;
                float f = a / 255.0f;
                uint8_t r = (uint8_t)(cr * f + br * (1.0f - f)), gg = (uint8_t)(cg * f + bgg * (1.0f - f)), b = (uint8_t)(cb * f + bb * (1.0f - f));
                *d = (uint16_t)((r >> 3) << 11 | (gg >> 2) << 5 | b >> 3);
            }
        pos += g->adv;
    }
}
