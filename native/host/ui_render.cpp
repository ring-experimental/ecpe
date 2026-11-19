// RmlUi를 SDL_Renderer로 그린다(크로노스 윙 ui_render.cpp에서 가져와 게임 그림 텍스처와 자리 옮김을 더했다).
// 게임 화면 위에 얹으므로 화면을 지우지 않는다. 그림은 화소 그림이라 모두 최근접으로 늘린다.
#include "ui_render.h"

std::unordered_map<std::string, UiSprite> ui_sprites;

UiRender::UiRender(SDL_Renderer *r) : ren(r) {
    // RmlUi 색은 알파가 미리 곱해져 있다.
    blend = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                                       SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
}

Rml::CompiledGeometryHandle UiRender::CompileGeometry(Rml::Span<const Rml::Vertex> v, Rml::Span<const int> i) {
    return reinterpret_cast<Rml::CompiledGeometryHandle>(new Geometry{v, i});
}

void UiRender::ReleaseGeometry(Rml::CompiledGeometryHandle g) { delete reinterpret_cast<Geometry *>(g); }

void UiRender::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f t, Rml::TextureHandle texture) {
    auto *g = reinterpret_cast<Geometry *>(handle);
    std::vector<SDL_Vertex> out(g->vertices.size());
    for (size_t i = 0; i < out.size(); i++) {
        const Rml::Vertex &v = g->vertices[i];
        Rml::Vector2f q = xform({v.position.x + t.x, v.position.y + t.y});
        out[i].position = {q.x + off_x, q.y + off_y};
        out[i].color = {v.colour.red / 255.f, v.colour.green / 255.f, v.colour.blue / 255.f, v.colour.alpha / 255.f};
        out[i].tex_coord = {v.tex_coord.x, v.tex_coord.y};
    }
    auto *tex = reinterpret_cast<SDL_Texture *>(texture);
    SDL_SetRenderDrawBlendMode(ren, blend);
    if (tex) SDL_SetTextureBlendMode(tex, blend);
    SDL_RenderGeometry(ren, tex, out.data(), (int)out.size(), g->indices.data(), (int)g->indices.size());
}

Rml::TextureHandle UiRender::LoadTexture(Rml::Vector2i &size, const Rml::String &source) {
    auto it = ui_sprites.find(source);
    if (it != ui_sprites.end()) {
        const UiSprite &sp = it->second;
        bool hd = sp.tw > 0;
        Rml::TextureHandle t = GenerateTexture({sp.rgba.data(), sp.rgba.size()}, hd ? Rml::Vector2i{sp.tw, sp.th} : Rml::Vector2i{sp.w, sp.h});
        size = {sp.w, sp.h}; // 문서에서의 크기(1px = 1dp)는 원래 그림과 같게
        if (t) SDL_SetTextureScaleMode(reinterpret_cast<SDL_Texture *>(t), hd ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        return t;
    }
    SDL_Surface *png = SDL_LoadPNG(source.c_str());
    if (!png) return 0;
    SDL_Surface *s = SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(png);
    if (!s) return 0;
    for (int y = 0; y < s->h; y++) {
        Uint8 *p = (Uint8 *)s->pixels + y * s->pitch;
        for (int x = 0; x < s->w; x++, p += 4) {
            int a = p[3];
            p[0] = (Uint8)(p[0] * a / 255); p[1] = (Uint8)(p[1] * a / 255); p[2] = (Uint8)(p[2] * a / 255);
        }
    }
    size = {s->w, s->h};
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    return reinterpret_cast<Rml::TextureHandle>(t);
}

Rml::TextureHandle UiRender::GenerateTexture(Rml::Span<const Rml::byte> src, Rml::Vector2i size) {
    SDL_Surface *s = SDL_CreateSurfaceFrom(size.x, size.y, SDL_PIXELFORMAT_RGBA32, (void *)src.data(), size.x * 4);
    if (!s) return 0;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    return reinterpret_cast<Rml::TextureHandle>(t);
}

void UiRender::ReleaseTexture(Rml::TextureHandle t) { SDL_DestroyTexture(reinterpret_cast<SDL_Texture *>(t)); }

Rml::Vector2f UiRender::xform(Rml::Vector2f p) const {
    if (!has_transform) return p;
    Rml::Vector4f r = transform * Rml::Vector4f(p.x, p.y, 0, 1);
    if (r.w != 0 && r.w != 1) return {r.x / r.w, r.y / r.w};
    return {r.x, r.y};
}

void UiRender::SetTransform(const Rml::Matrix4f *m) {
    has_transform = m != nullptr;
    if (m) transform = *m;
}

void UiRender::apply_scissor() {
    // scissor와 클립 마스크(외곽 사각형)의 겹친 곳만 그린다.
    SDL_FRect r;
    bool any = false;
    if (scissor) { r = {(float)rect.x, (float)rect.y, (float)rect.w, (float)rect.h}; any = true; }
    if (mask_on) {
        SDL_FRect m = mask_valid ? mask : SDL_FRect{0, 0, 0, 0};
        if (any) { SDL_FRect o; if (!SDL_GetRectIntersectionFloat(&r, &m, &o)) o = {0, 0, 0, 0}; r = o; }
        else { r = m; any = true; }
    }
    if (!any) { SDL_SetRenderClipRect(ren, nullptr); return; }
    // 화소 경계로 바깥쪽에 맞춘다(반 화소가 잘리지 않게)
    int x0 = (int)SDL_floorf(r.x), y0 = (int)SDL_floorf(r.y);
    int x1 = (int)SDL_ceilf(r.x + r.w), y1 = (int)SDL_ceilf(r.y + r.h);
    SDL_Rect c = {x0 + (int)off_x, y0 + (int)off_y, SDL_max(0, x1 - x0), SDL_max(0, y1 - y0)};
    SDL_SetRenderClipRect(ren, &c);
}

void UiRender::EnableScissorRegion(bool enable) { scissor = enable; apply_scissor(); }

void UiRender::SetScissorRegion(Rml::Rectanglei r) {
    rect = {r.Left(), r.Top(), r.Width(), r.Height()};
    apply_scissor();
}

void UiRender::EnableClipMask(bool enable) {
    mask_on = enable;
    if (!enable) mask_valid = false;
    apply_scissor();
}

void UiRender::RenderToClipMask(Rml::ClipMaskOperation op, Rml::CompiledGeometryHandle handle, Rml::Vector2f t) {
    auto *g = reinterpret_cast<Geometry *>(handle);
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const Rml::Vertex &v : g->vertices) {
        Rml::Vector2f q = xform({v.position.x + t.x, v.position.y + t.y});
        x0 = SDL_min(x0, q.x); y0 = SDL_min(y0, q.y); x1 = SDL_max(x1, q.x); y1 = SDL_max(y1, q.y);
    }
    SDL_FRect b = x1 > x0 && y1 > y0 ? SDL_FRect{x0, y0, x1 - x0, y1 - y0} : SDL_FRect{0, 0, 0, 0};
    switch (op) {
    case Rml::ClipMaskOperation::Set: mask = b; mask_valid = true; break;
    case Rml::ClipMaskOperation::Intersect:
        if (!mask_valid) { mask = b; mask_valid = true; }
        else { SDL_FRect o; if (!SDL_GetRectIntersectionFloat(&mask, &b, &o)) o = {0, 0, 0, 0}; mask = o; }
        break;
    case Rml::ClipMaskOperation::SetInverse: mask_valid = false; mask_on = false; break; // 사각형으로는 못 낸다(쓰지 않는다)
    }
    apply_scissor();
}
