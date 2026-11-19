#pragma once
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

// 게임에서 떼어 낸 그림. 이름은 "sprite:<해시>_<폭>x<높이>"이고 화소는 RGBA(알파를 미리 곱함)다.
// 리마스터 일러스트("illust:…")는 크기(w, h, dp)는 원래 그림과 같고 화소는 tw×th(더 촘촘함)이며 부드럽게 늘린다.
struct UiSprite { int w, h; std::vector<Rml::byte> rgba; int tw = 0, th = 0; };
extern std::unordered_map<std::string, UiSprite> ui_sprites;

class UiRender : public Rml::RenderInterface {
public:
    explicit UiRender(SDL_Renderer *r);
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i &dimensions, const Rml::String &source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    // transform이 걸린 요소(.pop 등장 효과, 대화 ▼ 흔들림) 안에서는 RmlUi가 scissor 대신 클립 마스크로 자른다.
    // SDL_Renderer에는 스텐실이 없어 마스크 도형의 외곽 사각형으로 자른다(이동·확대만 쓰므로 사각형 상자는 정확하다).
    void SetTransform(const Rml::Matrix4f *transform) override;
    void EnableClipMask(bool enable) override;
    void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override;
    float off_x = 0, off_y = 0; // 게임 화면이 놓인 자리

private:
    struct Geometry {
        Rml::Span<const Rml::Vertex> vertices;
        Rml::Span<const int> indices;
    };
    void apply_scissor();
    Rml::Vector2f xform(Rml::Vector2f p) const;
    SDL_Renderer *ren;
    SDL_BlendMode blend;
    SDL_Rect rect{};
    bool scissor = false;
    bool has_transform = false;
    Rml::Matrix4f transform;
    bool mask_on = false, mask_valid = false;
    SDL_FRect mask{};   // 출력 화소(off 더하기 전) 기준
};
