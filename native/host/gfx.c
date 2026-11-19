// MC_grp* 구현. 16bpp RGB565 프레임버퍼. 크로노스 윙 gfx.c에 ECPE가 쓰는 슬롯(GetContext, PutPixel, DrawString, 글꼴)을 더했다.
#include "host.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { int w, h, bpl; uint16_t *px; } fb_t;
static uint32_t screen_handle;
int screen_w = 240, screen_h = 320;

static fb_t fb_get(uint32_t handle) {
    cpu_t *c = &cpu;
    uint32_t f = handle_data(handle);
    fb_t b = { (int)R32(f), (int)R32(f + 4), (int)R32(f + 8), (uint16_t *)(c->m + handle_data(R32(f + 16))) };
    return b;
}

static uint32_t fb_new(int w, int h) {
    cpu_t *c = &cpu;
    uint32_t mem = handle_alloc(20), buf = handle_alloc((uint32_t)w * h * 2), f = handle_data(mem);
    W32(f, w); W32(f + 4, h); W32(f + 8, w * 2); W32(f + 12, 16); W32(f + 16, buf);
    return mem;
}

static inline void plot(fb_t *b, int x, int y, uint16_t col, int cx, int cy, int cw, int ch) {
    if (x < 0 || y < 0 || x >= b->w || y >= b->h) return;
    if (x < cx || x >= cx + cw || y < cy || y >= cy + ch) return;
    b->px[y * (b->bpl / 2) + x] = col;
}

static uint16_t fg(uint32_t gc) { return (uint16_t)R32_(&cpu, gc + 12); }

static void fill_rect(fb_t *b, int x, int y, int w, int h, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = (int64_t)x + w > b->w ? b->w : x + w, y1 = (int64_t)y + h > b->h ? b->h : y + h;
    for (int py = y0; py < y1; py++) {
        uint16_t *row = b->px + py * (b->bpl / 2);
        for (int px = x0; px < x1; px++) row[px] = col;
    }
}

static void draw_rect(fb_t *b, int x, int y, int w, int h, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    if (w == 1 || h == 1) { fill_rect(b, x, y, w, h, col); return; }
    fill_rect(b, x, y, w, 1, col);
    fill_rect(b, x, y + h - 1, w, 1, col);
    fill_rect(b, x, y + 1, 1, h - 2, col);
    fill_rect(b, x + w - 1, y + 1, 1, h - 2, col);
}

static void draw_line(fb_t *b, int x1, int y1, int x2, int y2, uint16_t col) {
    if (x1 < -4096 || y1 < -4096 || x2 < -4096 || y2 < -4096 || x1 > 8192 || y1 > 8192 || x2 > 8192 || y2 > 8192) return;
    int dx = abs(x2 - x1), dy = abs(y2 - y1), sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, err = dx - dy;
    for (;;) {
        plot(b, x1, y1, col, 0, 0, b->w, b->h);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

static bool in_sweep(float px, float py, float cx, float cy, float start, float sweep) {
    if (fabsf(sweep) >= 360.0f) return true;
    if (sweep == 0.0f) return false;
    float dx = px - cx, dy = py - cy;
    if (dx * dx + dy * dy < 1.0f) return true;
    float angle = atan2f(-dy, dx) * 180.0f / (float)M_PI;
    float s = sweep > 0 ? start : start + sweep, w = sweep > 0 ? sweep : -sweep;
    float r = fmodf(angle - s, 360.0f);
    if (r < 0) r += 360.0f;
    return r <= w;
}

static void fill_arc(fb_t *b, int x, int y, int w, int h, int start, int arc, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    float ra = (w - 1.0f) / 2, rb = (h - 1.0f) / 2, cx = x + ra, cy = y + rb;
    float da = ra > 0.5f ? ra : 0.5f, db = rb > 0.5f ? rb : 0.5f;
    for (int py = y < 0 ? 0 : y; py < y + h && py < b->h; py++)
        for (int px = x < 0 ? 0 : x; px < x + w && px < b->w; px++) {
            float nx = (px - cx) / da, ny = (py - cy) / db;
            if (nx * nx + ny * ny > 1.0f || !in_sweep((float)px, (float)py, cx, cy, (float)start, (float)arc)) continue;
            plot(b, px, py, col, x, y, w, h);
        }
}

static void draw_arc(fb_t *b, int x, int y, int w, int h, int start, int arc, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    float ra = (w - 1.0f) / 2, rb = (h - 1.0f) / 2, cx = x + ra, cy = y + rb, rad = fmaxf(fmaxf(ra, rb), 1.0f);
    float sw = arc * (float)M_PI / 180, st = start * (float)M_PI / 180;
    int64_t steps = (int64_t)ceilf(fabsf(sw) * rad) * 2, lim = 16 * (b->w + b->h);
    if (steps < 1) steps = 1;
    if (steps > lim) steps = lim;
    for (int64_t i = 0; i <= steps; i++) {
        float t = st + sw * i / steps;
        plot(b, (int)roundf(cx + ra * cosf(t)), (int)roundf(cy - rb * sinf(t)), col, x, y, w, h);
    }
}

// Canvas::draw와 같은 범위. 같은 버퍼 안의 복사는 겹침 방향을 고려한다.
static void blit(fb_t *d, int dx, int dy, int w, int h, fb_t *s, int sx, int sy) {
    if (w < 0 || h < 0) return;
    int64_t x0 = 0, y0 = 0, x1 = w, y1 = h;
    if (-(int64_t)dx > x0) x0 = -(int64_t)dx;
    if (-(int64_t)sx > x0) x0 = -(int64_t)sx;
    if (-(int64_t)dy > y0) y0 = -(int64_t)dy;
    if (-(int64_t)sy > y0) y0 = -(int64_t)sy;
    if ((int64_t)d->w - dx < x1) x1 = (int64_t)d->w - dx;
    if ((int64_t)s->w - sx < x1) x1 = (int64_t)s->w - sx;
    if ((int64_t)d->h - dy < y1) y1 = (int64_t)d->h - dy;
    if ((int64_t)s->h - sy < y1) y1 = (int64_t)s->h - sy;
    if (x0 >= x1 || y0 >= y1) return;
    int n = (int)(x1 - x0) * 2;
    int ystep = dy > sy ? -1 : 1;
    for (int64_t y = ystep < 0 ? y1 - 1 : y0; y >= y0 && y < y1; y += ystep)
        memmove(d->px + (dy + y) * (d->bpl / 2) + dx + x0, s->px + (sy + y) * (s->bpl / 2) + sx + x0, n);
}

void gfx_api(cpu_t *c, uint32_t i) {
    uint32_t *r = c->r;
    switch (i) {
    case 2: // GetScreenFrameBuffer
        if (!screen_handle) screen_handle = fb_new(screen_w, screen_h);
        r[0] = screen_handle;
        break;
    case 3: // DestroyOffscreenFramebuffer. fb_new가 만든 머리와 화소 버퍼를 함께 푼다.
        if (r[0]) {
            handle_free(R32(handle_data(r[0]) + 16));
            handle_free(r[0]);
        }
        break;
    case 4: r[0] = fb_new((int)r[0], (int)r[1]); break;
    case 5: memset(c->m + r[0], 0, 52); break;
    case 6: { // SetContext(ctx, op, value)
        uint32_t g = r[0], v = r[2];
        switch (r[1]) {
        case 0:
            if (!v) W32(g, R32(g) & ~1u);
            else {
                for (int k = 0; k < 4; k++) W16(g + 4 + k * 2, R32(v + k * 4));
                W32(g, R32(g) | 1);
            }
            break;
        case 1: W32(g + 12, v); break;
        case 2: W32(g + 16, v); break;
        case 3: W32(g + 20, v); break;
        case 4: W32(g + 24, v); break;
        case 5: W32(g + 32, v); break;
        case 6: W32(g + 36, v); break;
        case 7: W32(g + 44, v); break;
        case 8: W32(g + 48, v); break;
        case 10: W16(g + 28, R32(v)); W16(g + 30, R32(v + 4)); break;
        }
        break;
    }
    case 7: { // GetContext(ctx, op, out)
        uint32_t g = r[0], o = r[2];
        switch (r[1]) {
        case 0: for (int k = 0; k < 4; k++) W32(o + k * 4, (uint32_t)(int32_t)(int16_t)R16(g + 4 + k * 2)); break;
        case 1: W32(o, R32(g + 12)); break;
        case 2: W32(o, R32(g + 16)); break;
        case 3: W32(o, R32(g + 20)); break;
        case 4: W32(o, R32(g + 24)); break;
        case 5: W32(o, R32(g + 32)); break;
        case 6: W32(o, R32(g + 36)); break;
        case 7: W32(o, R32(g + 44)); break;
        case 8: W32(o, R32(g + 48)); break;
        case 10: W32(o, (uint32_t)(int32_t)(int16_t)R16(g + 28)); W32(o + 4, (uint32_t)(int32_t)(int16_t)R16(g + 30)); break;
        }
        break;
    }
    case 8: { fb_t b = fb_get(r[0]); plot(&b, (int)r[1], (int)r[2], fg(arg(c, 3)), 0, 0, b.w, b.h); break; } // PutPixel(fb,x,y,gc)
    case 9: { fb_t b = fb_get(r[0]); draw_line(&b, (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), fg(arg(c, 5))); break; }
    case 10: { fb_t b = fb_get(r[0]); draw_rect(&b, (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), fg(arg(c, 5))); break; }
    case 11: { fb_t b = fb_get(r[0]); fill_rect(&b, (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), fg(arg(c, 5))); break; }
    case 12: { // CopyFrameBuffer(dst,dx,dy,w,h,src,sx,sy,gc)
        fb_t d = fb_get(r[0]), s = fb_get(arg(c, 5));
        blit(&d, (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), &s, (int)arg(c, 6), (int)arg(c, 7));
        break;
    }
    case 14: { // CopyArea(dst,dx,dy,w,h,x,y,gc)
        fb_t d = fb_get(r[0]);
        blit(&d, (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), &d, (int)arg(c, 5), (int)arg(c, 6));
        break;
    }
    case 15: { fb_t b = fb_get(r[0]); draw_arc(&b, (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), (int)arg(c, 5), (int)arg(c, 6), fg(arg(c, 7))); break; }
    case 16: { fb_t b = fb_get(r[0]); fill_arc(&b, (int)r[1], (int)r[2], (int)r[3], (int)arg(c, 4), (int)arg(c, 5), (int)arg(c, 6), fg(arg(c, 7))); break; }
    case 17: { // DrawString(fb,x,y,str,len,gc). y는 글자 아래선이다. len이 -1이면 끝까지, 다른 음수면 그리지 않는다.
        fb_t b = fb_get(r[0]);
        int len = (int)arg(c, 4);
        const uint8_t *str = (const uint8_t *)gstr(r[3]);
        if (len == -1) len = (int)strlen((const char *)str);
        if (len < 0) break;
        if (ui_capturing) ui_cap_add((int)r[1], (int)r[2], fg(arg(c, 5)), (const char *)str, len);
        font_draw(b.px, b.bpl / 2, b.w, b.h, (int)r[1], (int)r[2], str, len, fg(arg(c, 5)));
        break;
    }
    case 21: video_present(r[1]); break;
    case 22: r[0] = ((r[0] & 0xff) >> 3) << 11 | ((r[1] & 0xff) >> 2) << 5 | (r[2] & 0xff) >> 3; break;
    case 23: { // GetRgbFromPixel(pixel,&r,&g,&b)
        uint32_t p = r[0] & 0xffff;
        W32(r[1], ((p >> 11 & 31) * 255 + 15) / 31);
        W32(r[2], ((p >> 5 & 63) * 255 + 31) / 63);
        W32(r[3], ((p & 31) * 255 + 15) / 31);
        break;
    }
    case 24: { // GetDisplayInfo(reserved,out)
        uint32_t o = r[1];
        W32(o, 16); W32(o + 4, 16); W32(o + 8, screen_w); W32(o + 12, screen_h); W32(o + 16, 2 * screen_w);
        W32(o + 20, 1); W32(o + 24, 0xf800); W32(o + 28, 0x7e0); W32(o + 32, 0x1f);
        r[0] = 1;
        break;
    }
    case 25: break; // Repaint. 게임은 FlushLcd로 그린다.
    case 26: r[0] = 0; break; // GetFont. 글꼴은 하나다(WIE와 같음).
    case 27: r[0] = 12; break; // GetFontHeight, Ascent, Descent는 WIE 값.
    case 28: r[0] = 10; break;
    case 29: r[0] = 2; break;
    case 30: { // GetStringWidth(font,str,len)
        int len = (int)r[2];
        const uint8_t *str = (const uint8_t *)gstr(r[1]);
        if (len == -1) len = (int)strlen((const char *)str);
        r[0] = len < 0 ? 0 : (uint32_t)font_string_width(str, len);
        break;
    }
    default:
        fprintf(stderr, "그래픽 API %u 미구현\n", i);
        r[0] = 0;
    }
}

uint32_t gfx_screen_handle(void) { return screen_handle; }
void gfx_reset(void) { screen_handle = 0; }
