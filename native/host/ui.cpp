// 원작 메뉴를 대신 그리는 RmlUi 층(ui.h). 문서마다 데이터 모델 하나를 두고, 게임 코드가 매 틱 채운 값 나무를 `d`로 묶는다.
#include "ui.h"
#include "ui_render.h"
#include <RmlUi/Core.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <deque>
#include <cmath>

extern "C" {
#include "host.h"
#include "gamepad.h"
}

bool ui_capturing;
Rml::FileInterface *ui_file_interface(); // ui_vlist.cpp: 문서를 읽을 때 <vlist>를 펼친다

namespace {

// 값 나무. 종류마다 VariableDefinition 하나가 맡는다.
struct Node {
    enum Kind { Scalar, Array, Struct } kind = Scalar;
    Rml::Variant value;
    std::vector<std::string> keys; // Struct: children과 같은 순서
    std::vector<Node> children;
};

struct ScalarDef final : Rml::VariableDefinition {
    ScalarDef() : VariableDefinition(Rml::DataVariableType::Scalar) {}
    bool Get(void *p, Rml::Variant &v) override { v = static_cast<Node *>(p)->value; return true; }
};
Rml::VariableDefinition *def_of(Node *n);
Node empty_node;
struct ArrayDef final : Rml::VariableDefinition {
    ArrayDef() : VariableDefinition(Rml::DataVariableType::Array) {}
    int Size(void *p) override { return (int)static_cast<Node *>(p)->children.size(); }
    Rml::DataVariable Child(void *p, const Rml::DataAddressEntry &a) override {
        Node *n = static_cast<Node *>(p);
        if (a.index >= 0 && a.index < (int)n->children.size()) return {def_of(&n->children[a.index]), &n->children[a.index]};
        if (a.name == "size") return Rml::MakeLiteralIntVariable((int)n->children.size());
        // 목록이 줄어든 틱에 RmlUi가 없어질 줄의 it.키를 한 번 더 읽는다. 빈 구조체를 주면 경고 없이 빈 값이 된다.
        static Node empty_struct = [] { Node e; e.kind = Node::Struct; return e; }();
        return {def_of(&empty_struct), &empty_struct};
    }
};
struct StructDef final : Rml::VariableDefinition {
    StructDef() : VariableDefinition(Rml::DataVariableType::Struct) {}
    Rml::DataVariable Child(void *p, const Rml::DataAddressEntry &a) override {
        Node *n = static_cast<Node *>(p);
        for (size_t i = 0; i < n->keys.size(); i++)
            if (n->keys[i] == a.name) return {def_of(&n->children[i]), &n->children[i]};
        return {def_of(&empty_node), &empty_node}; // 없는 키는 빈 값
    }
    // 뿌리가 바뀌는 구조라 이름 목록은 비워 둔다.
};
ScalarDef scalar_def; ArrayDef array_def; StructDef struct_def;
Rml::VariableDefinition *def_of(Node *n) {
    return n->kind == Node::Array ? (Rml::VariableDefinition *)&array_def : n->kind == Node::Struct ? (Rml::VariableDefinition *)&struct_def : &scalar_def;
}

// 터치로 고를 수 있는 목록(ui_nav). 래퍼가 틱마다 알린다.
struct Nav { std::string name; int at, count, prev, next, flags; }; // at: 커서(매크로 cur와 겹치지 않게)

struct Doc {
    std::string name;
    std::vector<Nav> navs, navs_pending; // 지금 화면의 것, 이번 틱에 채우는 것
    int pad = UI_PAD_AUTO, pad_pending = UI_PAD_AUTO;
    bool interactive = false;           // 보이는 요소 중 누를 것(key/nav/action)이 있다
    bool rescroll = false;              // 문서를 새로 읽었다(장치 전환·uireload): 다음 틱에 모든 목록의 커서 줄이 보이게 넘긴다
    Node root;           // 문서가 읽는 값
    std::string sig;     // root를 만든 값들의 줄글. 바뀌었을 때만 다시 묶는다.
    Rml::ElementDocument *el = nullptr;
    Rml::DataModelHandle handle;
    bool touched = false, visible = false, failed = false;
    bool force = false;  // ui_force: 새 UI가 꺼져 있어도 보인다
    bool below = false;  // ui_below: 다른 문서보다 늘 아래에 깐다(채운 차례와 상관없이)
    int order = 0;
};
std::vector<std::unique_ptr<Doc>> docs;
// 터치·패드 쪽(파일 끝). ui_present와 ui_render가 부른다.
void input_present();
void input_frame();
bool input_visible();
void doc_loaded(Doc *d);
bool hold_goal_view();

SDL_Renderer *renderer;
UiRender *render_if;
Rml::Context *ctx;
std::string ui_dir;
bool modern = true;
int touch_seq;
std::vector<int> shown_order; // 지금 화면에 쌓인 순서
// 키 별칭(from → to). 틱 동안 문서가 적은 것(pending)이 ui_present에서 지금 화면의 것(live)이 된다.
std::vector<std::pair<int, int>> alias_pending, alias_live;

// 채우는 중인 문서. 한 문서를 채우는 도중에 원본 그리기가 다른 영역의 래퍼를 부를 수 있어(상점 → 아이템 화면) 겹쳐 쌓는다.
struct Builder { Doc *cur = nullptr; Node root; std::vector<Node *> stack; std::string sig; };
std::vector<std::unique_ptr<Builder>> builders;
Builder idle;
Builder *bld = &idle;
#define cur (bld->cur)
#define build_root (bld->root)
#define stack (bld->stack)
#define build_sig (bld->sig)

// 텍스처 이름은 문서 경로와 합치지 않는다.
struct System final : Rml::SystemInterface {
    void JoinPath(Rml::String &out, const Rml::String &doc, const Rml::String &path) override {
        if (path.rfind("sprite:", 0) == 0) out = path;
        else Rml::SystemInterface::JoinPath(out, doc, path);
    }
    bool LogMessage(Rml::Log::Type type, const Rml::String &msg) override {
        fprintf(stderr, "[rml %d] %s\n", (int)type, msg.c_str());
        return true;
    }
    double GetElapsedTime() override { return SDL_GetTicks() / 1000.0; }
} system_if;

// iconv 대신 표 변환(host/cp949.c). Android NDK에 iconv가 없어서다.
extern "C" size_t cp949_to_utf8(const unsigned char *s, int len, char *out);
std::string to_utf8(const char *s, int len) {
    std::string out;
    if (!s || len <= 0) return out;
    out.resize((size_t)len * 3 + 4);
    out.resize(cp949_to_utf8((const unsigned char *)s, len, &out[0]));
    return out;
}

Node *add(const char *key, Node::Kind kind) {
    if (!cur || stack.empty()) return &empty_node;
    Node *p = stack.back();
    build_sig += kind == Node::Array ? '[' : kind == Node::Struct ? '{' : ';';
    if (p->kind == Node::Struct) {
        build_sig += key ? key : "";
        build_sig += '=';
        p->keys.emplace_back(key ? key : "");
    }
    p->children.emplace_back();
    Node *n = &p->children.back();
    n->kind = kind;
    return n;
}

void set(const char *key, Rml::Variant v) {
    Node *n = add(key, Node::Scalar);
    build_sig += v.Get<Rml::String>();
    n->value = std::move(v);
}

Doc *find_doc(const char *name) {
    for (auto &d : docs) if (d->name == name) return d.get();
    docs.emplace_back(new Doc);
    Doc *d = docs.back().get();
    d->name = name;
    d->root.kind = Node::Struct;
    return d;
}

// 키 이름표. 문서는 `{{ 'ok' | key }}`로 쓰고, 조작 장치가 바뀌면 모든 문서를 다시 그린다.
std::string key_label(const std::string &name);
Rml::Variant key_transform(const Rml::VariantList &args) {
    return Rml::Variant(key_label(args.empty() ? std::string() : args[0].Get<Rml::String>()));
}
// 게임패드 동작 이름표: `{{ 'menu' | padkey }}`(동작 id는 host/gamepad.c ACTIONS, 어깨 버튼 자리는 lb rb lt rt). 패드로 조작 중일 때만 그 동작에 묶인 버튼 이름,
// 아니면 빈 글. 원본 키로는 가릴 수 없는 버튼(START = 메뉴, BACK = 조작 설정)과 padhot 버튼의 안내에 쓴다.
Rml::Variant padkey_transform(const Rml::VariantList &args) {
    if (args.empty() || gamepad_source() != SOURCE_PAD) return Rml::Variant(std::string());
    Rml::String id = args[0].Get<Rml::String>();
    // 어깨 버튼 자리(메뉴의 탭·캐릭터 넘김은 맵핑이 아니라 자리로 정한다: gamepad.c input_set)
    static const char *const POS[] = {"lb", "rb", "lt", "rt"};
    for (int i = 0; i < 4; i++) if (id == POS[i]) return Rml::Variant(std::string(gamepad_fixed_label(2 + i)));
    const char *l = gamepad_action_label(id.c_str());
    return Rml::Variant(std::string(l ? l : ""));
}

void load_doc(Doc *d) {
    if (d->el || d->failed || !ctx) return;
    if (!d->handle) {
        Rml::DataModelConstructor c = ctx->CreateDataModel(d->name);
        if (!c) { d->failed = true; return; }
        c.BindCustomDataVariable("d", Rml::DataVariable(&struct_def, &d->root));
        static bool key_func; // 변환 함수는 문맥 전체에 한 번만 둔다
        if (!key_func) { c.RegisterTransformFunc("key", key_transform); c.RegisterTransformFunc("padkey", padkey_transform); key_func = true; }
        d->handle = c.GetModelHandle();
    }
    d->el = ctx->LoadDocument(ui_dir + "/" + d->name + ".rml");
    if (!d->el) { d->failed = true; fprintf(stderr, "ui: %s.rml을 열지 못함\n", d->name.c_str()); }
    else { doc_loaded(d); d->rescroll = true; }
}

} // namespace

namespace {
// 목록 커서가 바뀌면 그 줄을 보이게(scroll 상자 안일 때만). ui_present가 쌓고 ui_render가 배치 뒤에 처리한다.
struct ScrollReq { std::string doc, nav; int idx, from; }; // from: 옮기기 전 커서(-1이면 모름)
std::vector<ScrollReq> scroll_reqs;
bool finger_scrolling();
Doc *visible_doc(const std::string &name);
bool apply_scroll_reqs() {
    bool any = false;
    if (!finger_scrolling())
        for (auto &r : scroll_reqs) {
            Doc *d = visible_doc(r.doc);
            if (!d || !d->el) continue;
            Rml::Element *it = d->el->QuerySelector("[nav=\"" + r.nav + "\"][idx=\"" + std::to_string(r.idx) + "\"]");
            if (!it) continue;
            Rml::Element *sc = it->GetParentNode();
            while (sc && sc != d->el && !sc->HasAttribute("scroll")) sc = sc->GetParentNode();
            if (!sc || sc == d->el) continue;
            float top = sc->GetScrollTop();
            // 한 줄 앞을 미리 보인다: 커서가 보이는 마지막 줄에 닿기 전에 넘겨 다음 항목이 늘 보이게(콘솔 RPG 목록의 관례,
            // Xbox UWP 지침의 "포커스 줄을 화면 끝에 붙이지 않는다"). 옮긴 방향의 이웃 줄을 먼저 보이고, 커서 줄을 다시 보인다(목록이 아주 낮을 때 커서가 우선).
            if (r.from >= 0 && r.from != r.idx) {
                int nb = r.idx + (r.idx > r.from ? 1 : -1);
                if (Rml::Element *e = d->el->QuerySelector("[nav=\"" + r.nav + "\"][idx=\"" + std::to_string(nb) + "\"]"))
                    e->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest));
            }
            it->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest));
            any |= sc->GetScrollTop() != top;
        }
    scroll_reqs.clear();
    return any;
}

} // namespace

extern "C" {

bool ui_modern(void) { return modern && ctx; }
void ui_set_modern(bool on) { modern = on; }
int ui_screen_w(void) { return screen_w; }

void ui_key_alias(int from, int to) {
    for (auto &a : alias_pending) if (a.first == from) { a.second = to; return; }
    alias_pending.push_back({from, to});
}
void ui_key_alias_v2h(void) {
    ui_key_alias(UI_KEY_UP, UI_KEY_LEFT); ui_key_alias(UI_KEY_DOWN, UI_KEY_RIGHT);
    ui_key_alias('2', '4'); ui_key_alias('8', '6');
}
void ui_key_alias_h2v(void) {
    ui_key_alias(UI_KEY_LEFT, UI_KEY_UP); ui_key_alias(UI_KEY_RIGHT, UI_KEY_DOWN);
    ui_key_alias('4', '2'); ui_key_alias('6', '8');
}
int ui_key_map(int code) {
    if (!ui_modern()) return code;
    for (auto &a : alias_live) if (a.first == code) return a.second;
    return code;
}

static bool force_next, below_next;
void ui_force(void) { force_next = true; }
void ui_below(void) { below_next = true; }
void ui_begin(const char *doc) {
    force_next = false;
    below_next = false;
    builders.emplace_back(new Builder);
    bld = builders.back().get();
    cur = find_doc(doc);
    cur->navs_pending.clear();
    cur->pad_pending = UI_PAD_AUTO;
    build_root = Node();
    build_root.kind = Node::Struct;
    stack.assign(1, &build_root);
    build_sig.clear();
}

void ui_end(void) {
    if (!cur) return;
    Doc *d = cur;
    d->touched = true;
    d->force = force_next;
    d->below = below_next;
    d->order = ++touch_seq;
    if (build_sig != d->sig) {
        d->sig.swap(build_sig);
        d->root = std::move(build_root);
        if (d->handle) d->handle.DirtyAllVariables();
    }
    builders.pop_back();
    bld = builders.empty() ? &idle : builders.back().get();
}

// 이번 틱에 이미 채운 문서를 감춘다(필드 메뉴 틀 안의 자체 창이 아래 화면 문서를 가린다. 반투명 막 아래로 비치거나 눌리지 않게).
void ui_hide(const char *name) {
    for (auto &d : docs) if (d->name == name && d->touched) d->touched = false;
}

void ui_nav(const char *name, int cursor, int count, int prev_key, int next_key, int flags) {
    if (!cur) return;
    for (auto &n : cur->navs_pending) if (n.name == name) { n = {name, cursor, count, prev_key, next_key, flags}; return; }
    cur->navs_pending.push_back({name, cursor, count, prev_key, next_key, flags});
}
void ui_pad(int mode) { if (cur) cur->pad_pending = mode; }

void ui_int(const char *key, int v) { set(key, Rml::Variant(v)); }
void ui_bool(const char *key, bool v) { set(key, Rml::Variant(v)); }
void ui_text(const char *key, const char *utf8) { set(key, Rml::Variant(Rml::String(utf8 ? utf8 : ""))); }
void ui_strn(const char *key, const char *s, int len) { set(key, Rml::Variant(to_utf8(s, len))); }
void ui_str(const char *key, const char *s) { ui_strn(key, s, s ? (int)strlen(s) : 0); }
void ui_fmt(const char *key, const char *fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    ui_text(key, b);
}
// 자식을 더하면 부모의 children이 다시 잡힐 수 있어, 스택에는 열린 가지의 마지막 자식만 둔다(열린 동안 형제를 더하지 않는다).
void ui_list(const char *key) { stack.push_back(add(key, Node::Array)); }
void ui_obj(const char *key) { stack.push_back(add(key, Node::Struct)); }
void ui_item(void) { stack.push_back(add(nullptr, Node::Struct)); }
void ui_close(void) {
    if (stack.size() > 1) { stack.pop_back(); build_sig += ')'; }
}

// ---- 게임 그림 떼어 내기 ----
static std::vector<uint16_t> saved;
static int scratch_depth;
enum { KEY = 0xf81f };
struct Cap { int x, y; unsigned color; std::string s; };
static std::vector<Cap> caps;

static bool screen(uint16_t **px, int *w, int *h, int *stride) {
    cpu_t *c = &cpu;
    uint32_t hnd = gfx_screen_handle();
    if (!hnd) return false;
    uint32_t f = handle_data(hnd);
    *w = (int)R32_(c, f); *h = (int)R32_(c, f + 4); *stride = (int)R32_(c, f + 8) / 2;
    *px = (uint16_t *)(c->m + handle_data(R32_(c, f + 16)));
    return true;
}

static void illust_forget(void);
void ui_scratch_clear(void) {
    illust_forget();
    uint16_t *px; int w, h, st;
    if (!screen(&px, &w, &h, &st)) return;
    for (int i = 0; i < st * h; i++) px[i] = KEY;
}

void ui_scratch_begin(void) {
    uint16_t *px; int w, h, st;
    if (!screen(&px, &w, &h, &st)) return;
    // 겹친 호출(다른 영역의 래퍼 안에서 다시 불림)은 바깥의 화면과 글 기록을 그대로 둔다.
    if (scratch_depth++) return;
    saved.assign(px, px + st * h);
    caps.clear();
    ui_capturing = true;
    ui_scratch_clear();
}

void ui_scratch_end(void) {
    uint16_t *px; int w, h, st;
    if (!scratch_depth || --scratch_depth) return;
    ui_capturing = false;
    if (screen(&px, &w, &h, &st) && (int)saved.size() == st * h) memcpy(px, saved.data(), saved.size() * 2);
}

void ui_cap_add(int x, int y, unsigned color, const char *s, int len) {
    if (caps.size() < 4096) caps.push_back({x, y, color, std::string(s, (size_t)len)});
}
int ui_cap_count(void) { return (int)caps.size(); }
const char *ui_cap_text(int i, int *x, int *y, unsigned *color) {
    if (i < 0 || i >= (int)caps.size()) return "";
    if (x) *x = caps[i].x;
    if (y) *y = caps[i].y;
    if (color) *color = caps[i].color;
    return caps[i].s.c_str();
}

// ---- 리마스터 일러스트 ----
// 원본 초상화(UI 시트 프레임 60~66)를 scratch에 그릴 때 restored/illust.c가 그 자리(ui_illust_mark)를 알린다. 이 자리에 걸친
// 그림을 떼면(ui_sprite) 원래 화소 대신 같은 자리에 맞춘 고해상도 그림(ui/illust_<이름>.png)으로 바꾼다. 그림 파일은 원본 프레임의
// 테두리(뒤집지 않은 것)와 꼭 맞게 만들어 둔다(work/illust/README.md). 자른 칸(메뉴의 40×40 얼굴 등)도 같은 자리를 잘라 쓴다.
static bool illust_on = true;                 // 환경설정 '일러스트'(ui.ini illust). 끄면 원작 도트 그대로
// 화면 모드(데스크톱): 환경설정의 「화면」 줄, F11, Alt+Enter. ui.ini에 마지막으로 고른 모드(fullscreen)와 창 크기(window)를 남긴다.
// disp_full은 지금 창의 상태, cfg_full은 저장할 값이다(--windowed로 띄운 실행은 저장된 값을 바꾸지 않는다).
static bool disp_full = true, cfg_full = true;
static int win_w, win_h;                      // 마지막 창 모드 크기(창 좌표). 0이면 기본 크기
static const char *const ILLUST_NAMES[UI_ILLUST_TOTAL] = {"arc", "nihat", "jeremi", "giant", "io", "razpassa", "wysiwyg", "nihat_young", "kirien",
                                                           "gennad", "moad", "arcane", "teller", "hamelin"};
constexpr int ILLUST_NPC_SCALE = 8; // NPC 그림 화소 / 문서 dp
struct IllustMark { int id, x, y, w, h; bool flip; int cx0, cy0, cx1, cy1; }; // 프레임 테두리와 그릴 때의 클립(오른쪽·아래 끝은 안 듦)
static std::vector<IllustMark> illust_marks;
struct IllustArt { bool tried = false; int w = 0, h = 0; std::vector<Rml::byte> rgba; }; // 알파를 미리 곱함
static IllustArt illust_art[UI_ILLUST_TOTAL];
static std::vector<uint16_t> probe_saved;

static void illust_forget(void) { illust_marks.clear(); }

static const IllustArt *illust_load(int id) {
    if (id < 0 || id >= UI_ILLUST_TOTAL) return nullptr;
    IllustArt &a = illust_art[id];
    if (!a.tried) {
        a.tried = true;
        std::string path = ui_dir + "/illust_" + ILLUST_NAMES[id] + ".png";
        if (SDL_Surface *png = SDL_LoadPNG(path.c_str())) {
            if (SDL_Surface *c = SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32)) {
                a.w = c->w; a.h = c->h;
                a.rgba.resize((size_t)a.w * a.h * 4);
                for (int y = 0; y < a.h; y++) {
                    const Rml::byte *i = (const Rml::byte *)c->pixels + y * c->pitch;
                    Rml::byte *o = a.rgba.data() + (size_t)y * a.w * 4;
                    for (int x = 0; x < a.w; x++, i += 4, o += 4) {
                        int al = i[3];
                        o[0] = (Rml::byte)(i[0] * al / 255); o[1] = (Rml::byte)(i[1] * al / 255); o[2] = (Rml::byte)(i[2] * al / 255); o[3] = (Rml::byte)al;
                    }
                }
                SDL_DestroySurface(c);
            }
            SDL_DestroySurface(png);
        }
    }
    return a.w > 0 ? &a : nullptr;
}

extern "C" bool ui_illust(void) { return illust_on; }
extern "C" bool ui_illust_has(int id) { return illust_load(id) != nullptr; }
extern "C" bool ui_illust_size(int id, int *w, int *h) {
    const IllustArt *a = illust_load(id);
    if (!a) return false;
    *w = (a->w + ILLUST_NPC_SCALE / 2) / ILLUST_NPC_SCALE; *h = (a->h + ILLUST_NPC_SCALE / 2) / ILLUST_NPC_SCALE;
    return true;
}
extern "C" bool ui_illust_image(const char *key, int id, int flip) {
    std::string k = key;
    int w = 0, h = 0;
    const IllustArt *a = illust_load(id);
    if (!a || !ui_illust_size(id, &w, &h)) { ui_text(key, ""); ui_int((k + "_w").c_str(), 0); ui_int((k + "_h").c_str(), 0); return false; }
    char b[48];
    snprintf(b, sizeof b, "illust:npc%d_%d", id, flip != 0);
    if (!ui_sprites.count(b)) {
        UiSprite sp{w, h, a->rgba, a->w, a->h};
        if (flip)
            for (int y = 0; y < a->h; y++)
                for (int x = 0; x < a->w / 2; x++)
                    std::swap_ranges(&sp.rgba[((size_t)y * a->w + x) * 4], &sp.rgba[((size_t)y * a->w + x) * 4 + 4], &sp.rgba[((size_t)y * a->w + a->w - 1 - x) * 4]);
        ui_sprites.emplace(b, std::move(sp));
    }
    ui_text(key, b);
    ui_int((k + "_w").c_str(), w);
    ui_int((k + "_h").c_str(), h);
    return true;
}
extern "C" void ui_illust_mark(int id, int x, int y, int w, int h, int flip, int cx, int cy, int cw, int ch) {
    int x0 = std::max(x, cx), y0 = std::max(y, cy), x1 = std::min(x + w, cx + cw), y1 = std::min(y + h, cy + ch);
    if (x1 > x0 && y1 > y0 && illust_marks.size() < 64) illust_marks.push_back({id, x, y, w, h, flip != 0, x0, y0, x1, y1});
}

// 시험 그리기: 화면을 보관하고 투명색으로 채운다. ui_probe_end가 그린 것의 테두리를 재고 화면을 되돌린다.
extern "C" void ui_probe_begin(void) {
    uint16_t *px; int w, h, st;
    if (!screen(&px, &w, &h, &st)) return;
    probe_saved.assign(px, px + st * h);
    for (int i = 0; i < st * h; i++) px[i] = KEY;
}
extern "C" bool ui_probe_end(int *bx, int *by, int *bw, int *bh) {
    uint16_t *px; int w, h, st;
    if (!screen(&px, &w, &h, &st) || (int)probe_saved.size() != st * h) return false;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (px[j * st + i] != KEY) { if (i < x0) x0 = i; if (i > x1) x1 = i; if (j < y0) y0 = j; if (j > y1) y1 = j; }
    memcpy(px, probe_saved.data(), probe_saved.size() * 2);
    if (x1 < 0) return false;
    *bx = x0; *by = y0; *bw = x1 - x0 + 1; *bh = y1 - y0 + 1;
    return true;
}

// 떼는 칸(x, y, w, h)에 걸친 초상화 자리가 있으면 그 칸을 고해상도로 만든 그림 이름을 돌려준다(없으면 빈 글).
// 칸 안에서 초상화 자리 밖은 원래 화소를 늘려 쓴다.
static std::string illust_sprite(const uint16_t *px, int st, int x, int y, int w, int h) {
    if (!illust_on || illust_marks.empty()) return {};
    const IllustMark *best = nullptr;
    int best_a = 0;
    for (auto &m : illust_marks) {
        int ix = std::min(x + w, m.cx1) - std::max(x, m.cx0), iy = std::min(y + h, m.cy1) - std::max(y, m.cy0);
        if (ix > 0 && iy > 0 && ix * iy > best_a) { best_a = ix * iy; best = &m; }
    }
    if (!best) return {};
    const IllustArt *art = illust_load(best->id);
    if (!art) return {};
    const IllustMark m = *best;
    // 원래 화소도 이름에 넣는다(자리 밖에 다른 것이 그려졌을 수 있다)
    uint32_t hash = 0x811c9dc5u;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) hash = (hash ^ px[(y + j) * st + x + i]) * 16777619u;
    char b[96];
    snprintf(b, sizeof b, "illust:%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%08x", m.id, m.flip, x - m.x, y - m.y, w, h, m.w, m.h,
             m.cx0 - m.x, m.cy0 - m.y, m.cx1 - m.x, m.cy1 - m.y, hash);
    std::string name = b;
    if (ui_sprites.count(name)) return name;
    float sx = (float)art->w / m.w, sy = (float)art->h / m.h; // 원래 화소 하나에 든 그림 화소
    float s = std::min(std::max(sx, sy), 2048.f / std::max(w, h));
    int tw = std::max(1, (int)std::lround(w * s)), th = std::max(1, (int)std::lround(h * s));
    UiSprite sp{w, h, std::vector<Rml::byte>((size_t)tw * th * 4), tw, th};
    Rml::byte *o = sp.rgba.data();
    for (int j = 0; j < th; j++) {
        float gy = y + (j + 0.5f) / s;
        for (int i = 0; i < tw; i++, o += 4) {
            float gx = x + (i + 0.5f) / s;
            if (gx >= m.cx0 && gx < m.cx1 && gy >= m.cy0 && gy < m.cy1) {
                float u = (gx - m.x) / m.w, v = (gy - m.y) / m.h;
                if (m.flip) u = 1.f - u;
                int ax = std::clamp((int)(u * art->w), 0, art->w - 1), ay = std::clamp((int)(v * art->h), 0, art->h - 1);
                memcpy(o, &art->rgba[((size_t)ay * art->w + ax) * 4], 4);
                continue;
            }
            uint16_t p = px[std::clamp((int)gy, y, y + h - 1) * st + std::clamp((int)gx, x, x + w - 1)];
            if (p == KEY) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
            o[0] = (Rml::byte)(((p >> 11 & 31) * 255 + 15) / 31);
            o[1] = (Rml::byte)(((p >> 5 & 63) * 255 + 31) / 63);
            o[2] = (Rml::byte)(((p & 31) * 255 + 15) / 31);
            o[3] = 255;
        }
    }
    ui_sprites.emplace(name, std::move(sp));
    return name;
}

static void sprite(const char *key, int x, int y, int w, int h, bool trim) {
    uint16_t *px; int sw, sh, st;
    std::string name;
    if (screen(&px, &sw, &sh, &st)) {
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > sw) w = sw - x;
        if (y + h > sh) h = sh - y;
        if (trim && w > 0 && h > 0) {
            int x0 = w, y0 = h, x1 = -1, y1 = -1;
            for (int j = 0; j < h; j++)
                for (int i = 0; i < w; i++)
                    if (px[(y + j) * st + x + i] != KEY) { if (i < x0) x0 = i; if (i > x1) x1 = i; if (j < y0) y0 = j; if (j > y1) y1 = j; }
            if (x1 < 0) w = h = 0;
            else { x += x0; y += y0; w = x1 - x0 + 1; h = y1 - y0 + 1; }
        }
        if (w > 0 && h > 0) name = illust_sprite(px, st, x, y, w, h);
        if (w > 0 && h > 0 && name.empty()) {
            uint32_t hash = 0x811c9dc5u;
            bool any = false;
            for (int j = 0; j < h; j++)
                for (int i = 0; i < w; i++) { uint16_t p = px[(y + j) * st + x + i]; hash = (hash ^ p) * 16777619u; any |= p != KEY; }
            if (any) {
                char b[64];
                snprintf(b, sizeof b, "sprite:%08x_%dx%d", hash, w, h);
                name = b;
                if (!ui_sprites.count(name)) {
                    UiSprite s{w, h, std::vector<Rml::byte>((size_t)w * h * 4)};
                    Rml::byte *o = s.rgba.data();
                    for (int j = 0; j < h; j++)
                        for (int i = 0; i < w; i++, o += 4) {
                            uint16_t p = px[(y + j) * st + x + i];
                            if (p == KEY) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
                            o[0] = (Rml::byte)(((p >> 11 & 31) * 255 + 15) / 31);
                            o[1] = (Rml::byte)(((p >> 5 & 63) * 255 + 31) / 63);
                            o[2] = (Rml::byte)(((p & 31) * 255 + 15) / 31);
                            o[3] = 255;
                        }
                    ui_sprites.emplace(name, std::move(s));
                }
            }
        }
    }
    if (name.empty()) w = h = 0;
    ui_text(key, name.c_str());
    std::string k = key;
    ui_int((k + "_w").c_str(), w);
    ui_int((k + "_h").c_str(), h);
}
void ui_sprite(const char *key, int x, int y, int w, int h) { sprite(key, x, y, w, h, false); }
// 진단: scratch 화면의 투명색이 아닌 테두리를 PPM(투명은 자홍)으로 남긴다. 그린 것이 없으면 false.
bool ui_scratch_dump(const char *path) {
    uint16_t *px; int w, h, st;
    if (!screen(&px, &w, &h, &st)) return false;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (px[j * st + i] != KEY) { if (i < x0) x0 = i; if (i > x1) x1 = i; if (j < y0) y0 = j; if (j > y1) y1 = j; }
    if (x1 < 0) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", x1 - x0 + 1, y1 - y0 + 1);
    for (int j = y0; j <= y1; j++)
        for (int i = x0; i <= x1; i++) {
            uint16_t p = px[j * st + i];
            unsigned char o[3] = {(unsigned char)((p >> 11 & 31) * 255 / 31), (unsigned char)((p >> 5 & 63) * 255 / 63), (unsigned char)((p & 31) * 255 / 31)};
            fwrite(o, 1, 3, f);
        }
    fclose(f);
    return true;
}

void ui_sprite_trim(const char *key, int x, int y, int w, int h) { sprite(key, x, y, w, h, true); }

// ---- 호스트 ----
bool ui_init(SDL_Renderer *ren, const char *dir) {
    renderer = ren;
    ui_dir = dir;
    render_if = new UiRender(ren);
    Rml::SetRenderInterface(render_if);
    Rml::SetSystemInterface(&system_if);
    Rml::SetFileInterface(ui_file_interface());
    if (!Rml::Initialise()) return false;
    // 글꼴: 게임 글자와 같은 Neo둥근모, 없는 글자는 갈무리9.
    // 소스 폴더(native/ui)를 바로 가리키면(ECPE_UI_DIR) 글꼴은 옆의 third_party에서 읽는다.
    std::string fonts = ui_dir;
    if (FILE *f = fopen((fonts + "/neodgm.ttf").c_str(), "rb")) fclose(f);
    else fonts = ui_dir + "/../third_party";
    if (!Rml::LoadFontFace(fonts + "/neodgm.ttf", false)) return false;
    Rml::LoadFontFace(fonts + "/Galmuri9.ttf", true);
    ctx = Rml::CreateContext("main", {640, 320});
    return ctx != nullptr;
}

void ui_reload(void) {
    if (!ctx) return;
    for (auto &d : docs) {
        if (d->el) d->el->Close();
        d->el = nullptr; d->failed = false; d->visible = false;
    }
    ctx->Update();
    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();
    shown_order.clear();
}

void ui_present(void) {
    if (!ctx) return;
    // 개발용: ECPE_UI_DEMO=<문서>면 그 문서를 값 없이 늘 띄운다(스타일 확인).
    static const char *demo = getenv("ECPE_UI_DEMO");
    if (demo) { ui_begin(demo); ui_end(); }
    alias_live.swap(alias_pending);
    alias_pending.clear();
    for (auto &d : docs) {
        bool want = d->touched && (modern || d->force || d->name == "toast"); // 토스트(restored/items_ext.c)는 원작 그리기에서도 보인다
        if (d->touched) {
            // 커서가 바뀐 목록(키·패드로 옮김)은 그 줄이 scroll 상자 안에서 보이게 넘긴다(ui_render에서 배치 뒤에). 끄는 중이면 두지 않는다.
            for (auto &n : d->navs_pending) {
                const Nav *o = nullptr;
                for (auto &m : d->navs) if (m.name == n.name) o = &m;
                if (n.at >= 0 && (!o || o->at != n.at || d->rescroll)) scroll_reqs.push_back({d->name, n.name, n.at, o && !d->rescroll ? o->at : -1});
            }
            d->navs.swap(d->navs_pending); d->navs_pending.clear(); d->pad = d->pad_pending;
            d->rescroll = false;
        }
        else if (!want) { d->navs.clear(); d->pad = UI_PAD_AUTO; }
        d->touched = false;
        if (want) load_doc(d.get());
        if (!d->el) { d->visible = false; continue; }
        if (want && !d->visible) { d->el->Show(Rml::ModalFlag::None, Rml::FocusFlag::None); d->el->SetClass("open", true); }
        if (!want && d->visible) { d->el->Hide(); d->el->SetClass("open", false); }
        d->visible = want;
    }
    // 나중에 채운 문서가 위에 오게 쌓는다. 순서가 바뀔 때만 손댄다.
    std::vector<std::pair<int, int>> ord;
    for (size_t i = 0; i < docs.size(); i++) if (docs[i]->visible) ord.push_back({docs[i]->order - (docs[i]->below ? 1 << 30 : 0), (int)i});
    std::sort(ord.begin(), ord.end());
    std::vector<int> now;
    for (auto &o : ord) now.push_back(o.second);
    if (now != shown_order) {
        for (int i : now) docs[i]->el->PullToFront();
        shown_order.swap(now);
    }
    input_present();
}

bool ui_active(void) {
    for (auto &d : docs) if (d->visible) return true;
    return input_visible();
}

// 게임 화면이 놓인 자리(출력 화소). 터치 좌표를 문맥 좌표로 바꿀 때 쓴다.
static float view_x, view_y, view_w, view_h;

void ui_render(float x, float y, float w, float h) {
    if (!ctx) return;
    view_x = (float)(int)x; view_y = (float)(int)y; view_w = w; view_h = h;
    Rml::Vector2i dim((int)w, (int)h);
    if (ctx->GetDimensions() != dim) ctx->SetDimensions(dim);
    float dp = h / 320.f;
    if (ctx->GetDensityIndependentPixelRatio() != dp) ctx->SetDensityIndependentPixelRatio(dp);
    input_frame();
    if (!ui_active()) return;
    render_if->off_x = view_x;
    render_if->off_y = view_y;
    ctx->Update();
    if (apply_scroll_reqs()) ctx->Update();
    if (hold_goal_view()) ctx->Update(); // 바꾼 선택 표시를 이번 그림에 반영한다
    ctx->Render();
    SDL_SetRenderClipRect(renderer, nullptr);
}

} // extern "C"

// ======================================================================================================
// 터치, 터치 패드, 조작 설정. 메뉴는 문서 요소를 직접 누르고(key/nav/action 속성, ui.h), 필드에서는 화면 패드를 쓴다.
// 입력은 모두 원본 키로 바꿔 넘긴다. 원본은 틱마다 "이번 틱에 누른 키" 하나만 읽으므로(입력 객체 +0, 0x61a0/0x61d4,
// 틱 끝 0x613c가 지운다) 터치로 만든 키는 대기열에 넣어 틱마다 하나씩 보낸다. 누르고 바로 떼도 그 틱에는 눌린 키로 읽힌다.
// 목록 항목을 누르면 목표 번호를 두고, 틱마다 래퍼가 알린 커서(ui_nav)를 보며 한 칸씩 움직인다. 키가 씹히거나
// 넘쳐도 다음 틱에 다시 맞춘다.
// ======================================================================================================
extern "C" void host_key(int type, int code); // main.c: 별칭을 거쳐 누르고 뗀다(키보드와 같다)
extern "C" void host_key_tap(int code);       // main.c: 별칭 없이 눌렀다 뗀다
extern "C" uint64_t frames;
extern "C" void ext_toast(const char *utf8); // restored/items_ext.c

namespace {

int key_code(const std::string &n) {
    static const struct { const char *name; int code; } K[] = {
        {"up", -1}, {"down", -2}, {"left", -3}, {"right", -4}, {"ok", -5}, {"lsk", -6}, {"rsk", -7}, {"clear", -16}, {"star", '*'}, {"hash", '#'}};
    for (auto &k : K) if (n == k.name) return k.code;
    if (n.size() == 1 && n[0] >= '0' && n[0] <= '9') return n[0];
    return 0;
}

std::string key_label(const std::string &name) {
    if (name == "auto" || name == "skip" || name == "bauto" || name == "bfast") { // 호스트 기능(대사 자동·빨리 넘기기, 자동 전투·전투 2배속)
        int src = gamepad_source();
        if (src == SOURCE_PAD) {
            // 키 설정에서 따로 붙인 버튼이 있으면 그것, 없으면 문서가 padhot으로 빌려 쓰는 버튼(대사 중 X·Y, 전투 중 Y·START)
            const char *l = gamepad_action_label(name.c_str());
            if (!l) l = gamepad_action_label(name == "auto" ? "star" : name == "skip" ? "hash" : name == "bauto" ? "hash" : "menu");
            return l ? l : "";
        }
        if (src == SOURCE_KEYBOARD) return name == "auto" ? "Tab" : name == "skip" ? "Ctrl" : name == "bauto" ? "R" : "F";
        return "";
    }
    int code = key_code(name);
    if (!code) return name;
    if (code >= -4 && code <= -1) { static const char *A[] = {"↑", "↓", "←", "→"}; return A[-code - 1]; }
    int src = gamepad_source();
    if (src == SOURCE_PAD) {
        const char *l = gamepad_code_label(code);
        return l ? l : "-";
    }
    if (src == SOURCE_KEYBOARD) {
        // main.c key_from_scancode의 배치(123/QWE/ASD/ZXC가 전화 키패드 자리)
        switch (code) {
        case -5: return "Enter";
        case -16: return "Esc";
        case -6: return "Shift";
        case '*': return "Z";
        case '#': return "C";
        case '0': return "X";
        case '4': return "Q"; case '5': return "W"; case '6': return "E";
        case '7': return "A"; case '8': return "S"; case '9': return "D";
        default: break;
        }
    }
    switch (code) { // 원작 폰 키 이름(터치에서는 base.rcss가 이름표를 감추고 버튼으로 보인다)
    case -5: return "OK";
    case -16: return "CLR";
    case -6: return "LSK";
    case -7: return "RSK";
    default: return std::string(1, (char)code);
    }
}

// ---- 대기열과 목록 목표 ----
std::deque<int> keyq;
uint64_t keyq_frame = ~0ull;
struct Goal { bool on = false; std::string doc, nav; int target = 0, steps = 0; bool ok = false; } goal; // ok: 닿으면 OK(누르면 바로 결정)

Doc *visible_doc(const std::string &name) {
    for (auto &d : docs) if (d->visible && d->name == name) return d.get();
    return nullptr;
}
Nav *find_nav(Doc *d, const std::string &name) {
    if (!d) return nullptr;
    for (auto &n : d->navs) if (n.name == name) return &n;
    return nullptr;
}
uint64_t human_ms;           // 사람이 마지막으로 게임 키를 넣은 때(ui_human_ms). 래퍼의 자동 입력(ui_key_tap)은 세지 않는다.
bool auto_queue;
void queue_key(int code) {
    if (!code || keyq.size() >= 32) return;
    keyq.push_back(code);
    if (!auto_queue) human_ms = now_ms();
}

// 목표까지 한 칸. 끝을 넘어 도는 목록은 가까운 쪽으로 돈다.
int step_key(const Nav &n, int target) {
    if (n.at < 0) return n.next;
    if (n.flags & UI_NAV_WRAP && n.count > 0) {
        int fwd = ((target - n.at) % n.count + n.count) % n.count;
        return fwd <= n.count - fwd ? n.next : n.prev;
    }
    return target > n.at ? n.next : n.prev;
}

// 터치로 목록의 먼 줄을 누르면 원본 키를 틱마다 하나씩 보내 커서를 옮긴다(goal_step). 그동안 문서는 원본 커서를 그대로 따라
// 선택 띠가 중간 줄들을 한 칸씩 훑고 지나갔다. 손가락은 넷째 줄을 눌렀는데 첫째~셋째 줄이 차례로 밝아져, 누름(active)과 선택(hover)이
// 꼬인 것처럼 보였다(폰, 도우미 선택창). 문서마다 선택 표시를 묶는 방식(d.cursor 비교, C가 만든 it.sel 등)이 달라서, 데이터를 반영한
// 뒤 그 목록의 `sel` 표시만 목표 줄 하나에 둔다. 닿으면 데이터와 같은 상태가 되고, 도중에 끊기면 그 문서의 값을 다시 읽혀 되돌린다.
std::string held_doc;
bool hold_goal_view() {
    if (goal.on) {
        Doc *d = visible_doc(goal.doc);
        if (!d || !d->el) return false;
        bool changed = false;
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, "[nav]");
        for (Rml::Element *e : l) {
            if (e->GetAttribute<Rml::String>("nav", "") != goal.nav) continue;
            bool want = e->GetAttribute<int>("idx", -1) == goal.target;
            if (e->IsClassSet("sel") != want) { e->SetClass("sel", want); changed = true; }
        }
        held_doc = goal.doc;
        return changed;
    }
    if (held_doc.empty()) return false;
    Doc *d = visible_doc(held_doc);
    held_doc.clear();
    if (!d || !d->handle) return false;
    d->handle.DirtyAllVariables(); // data-class-sel이 실제 클래스와 견줘 다시 맞춘다
    return true;
}

// 시험용: ECPE_NAVLOG가 있으면 목표 이동의 매 단계(알린 커서, 목표, 보낸 키)를 로그에 남긴다(지나침·되돌아옴 찾기).
static bool navlog() { static int on = -1; if (on < 0) on = getenv("ECPE_NAVLOG") != nullptr; return on; }
void goal_step() {
    if (!goal.on) return;
    Nav *n = find_nav(visible_doc(goal.doc), goal.nav);
    if (!n || goal.target >= n->count) { goal.on = false; return; }
    if (n->at == goal.target) {
        goal.on = false;
        if (navlog()) fprintf(stderr, "NAV %s/%s at=%d target=%d -> %s\n", goal.doc.c_str(), goal.nav.c_str(), n->at, goal.target, goal.ok ? "OK" : "done");
        if (goal.ok && keyq.empty()) queue_key(UI_KEY_OK);
        return;
    }
    if (!keyq.empty()) return;
    if (--goal.steps < 0) { goal.on = false; return; }
    int k = step_key(*n, goal.target);
    if (navlog()) fprintf(stderr, "NAV %s/%s at=%d target=%d key=%s\n", goal.doc.c_str(), goal.nav.c_str(), n->at, goal.target, k == n->next ? "next" : "prev");
    queue_key(k);
}

// ---- 패드 묶음. 위치는 화면 비율(중심), 크기는 em. 문서(ui/pad.rml)는 g[번호]로 묶으므로 순서를 맞춘다. ----
struct Group {
    const char *id;
    float def_x, def_y, w_em, h_em;
    float x, y, scale;
    Rml::String left, top, font;
};
Group groups[] = {
    {"dpad", 0.13f, 0.72f, 3.f, 3.f},
    {"ok", 0.90f, 0.76f, 1.5f, 1.5f},
    {"menu", 0.93f, 0.52f, 0.9f, 0.9f}, // 전투 대상 고르기의 취소도 이것이라 아래 띠에 가리지 않는 높이에 둔다
    {"map", 0.79f, 0.58f, 0.9f, 0.9f},
    {"zero", 0.77f, 0.84f, 0.9f, 0.9f},
    {"sys", 0.04f, 0.07f, 0.7f, 0.7f},
};
constexpr float BASE_DP = 34.f;
constexpr Uint64 CAPTURE_TIMEOUT_MS = 5000; // 게임패드 키 설정이 버튼을 기다리는 시간

// 기타 설정의 배율 칸(%). 칸 번호를 ui.ini에 적는 대신 %를 적어 칸이 바뀌어도 가까운 칸으로 읽는다.
constexpr int RATE_STEPS[] = {50, 100, 150, 200, 300, 500, 1000};
constexpr int RATE_ONE = 1;
constexpr const char *RATE_KEYS[3] = {"rate_exp", "rate_gold", "rate_drop"};

struct Model {
    bool pad_on = false, edit = false, gamepad = false, airship = false, touch = false;
    bool play = false;            // 플레이 장면(필드·스크립트·미니게임·전투)인가. 타이틀 쪽 장면에서는 패드를 숨긴다(pad.rml)
    bool auto_on = false, skip_on = false; // 대사 자동 넘김, 빨리 넘기기(restored/ui_dialog.c가 읽는다)
    bool bauto = false, bfast = false;     // 자동 전투, 전투 2배속(restored/autobattle.c가 읽는다). ui.ini에 남는다
    int mode = 0;                 // 0 숨김, UI_PAD_DIR, UI_PAD_FIELD
    int size = 100, alpha = 70, selected = -1, capture = -1;
    int rate[3] = {RATE_ONE, RATE_ONE, RATE_ONE}; // 기타 설정: 경험치, 골드, 드랍률 배율(RATE_STEPS 칸 번호)
} model;
struct Bind { Rml::String name, key; };
std::vector<Bind> binds;
int pad_generation = -1, label_generation = -1;
Uint64 capture_until;

SDL_Window *window;
Rml::ElementDocument *pad_doc, *settings_doc, *editbar_doc, *binds_doc, *etc_doc, *savedata_doc;
Rml::ElementDocument *focus_pending;
Rml::DataModelHandle model_h;
std::string cfg_path;
float laid_w, laid_h; // 배치를 계산한 문맥 크기(dp)

enum { F_NONE, F_PAD, F_TAP, F_EDIT, F_HOLD, F_SCROLL };
// 요소는 ObserverPtr로 든다. 문서의 data-for 줄은 값이 바뀌면 없어지거나 다른 항목에 다시 쓰이므로, 손가락을 든 사이에
// 지워진 요소를 건드리지 않게. 누른 줄의 nav 이름은 누를 때 따로 적어 둔다.
struct Finger {
    SDL_FingerID id; bool used; int kind;
    Rml::ObserverPtr<Rml::Element> el; Doc *doc; int group;
    float x0, y0, grab_x, grab_y, acc; bool moved;
    std::string nav; float step;
};
Finger fingers[10];
bool finger_scrolling() { for (auto &f : fingers) if (f.kind == F_SCROLL) return true; return false; }

void save_config() {
    if (cfg_path.empty()) return;
    FILE *f = fopen(cfg_path.c_str(), "w");
    if (!f) return;
    fprintf(f, "pad %d\nsize %d\nopacity %d\nautoplay %d\nautobattle %d\nfastbattle %d\n", model.pad_on, model.size, model.alpha, model.auto_on, model.bauto, model.bfast);
    for (int k = 0; k < 3; k++) fprintf(f, "%s %d\n", RATE_KEYS[k], RATE_STEPS[model.rate[k]]);
    fprintf(f, "illust %d\n", illust_on);
    fprintf(f, "fullscreen %d\n", cfg_full);
    if (win_w > 0 && win_h > 0) fprintf(f, "window %d %d\n", win_w, win_h);
    for (auto &g : groups) fprintf(f, "%s %.4f %.4f %.3f\n", g.id, g.x, g.y, g.scale);
    for (int i = 0; i < gamepad_action_count(); i++) fprintf(f, "bind %s %s\n", gamepad_config_id(i), gamepad_config_value(i));
    fclose(f);
}

void reset_groups() { for (auto &g : groups) { g.x = g.def_x; g.y = g.def_y; g.scale = 1.f; } }

void load_config(bool default_pad) {
    reset_groups();
    model.pad_on = default_pad;
    FILE *f = fopen(cfg_path.c_str(), "r");
    if (!f) return;
    char name[32], line[128], value[32];
    float a, b, c;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "bind %31s %31s", name, value) == 2) { gamepad_config_read(name, value); continue; }
        int n = sscanf(line, "%31s %f %f %f", name, &a, &b, &c);
        if (n == 2 && !strcmp(name, "pad")) model.pad_on = a != 0;
        else if (n == 2 && !strcmp(name, "size")) model.size = (int)std::clamp(a, 50.f, 200.f);
        else if (n == 2 && !strcmp(name, "opacity")) model.alpha = (int)std::clamp(a, 10.f, 100.f);
        else if (n == 2 && !strcmp(name, "autoplay")) model.auto_on = a != 0;
        else if (n == 2 && !strcmp(name, "autobattle")) model.bauto = a != 0;
        else if (n == 2 && !strcmp(name, "fastbattle")) model.bfast = a != 0;
        else if (n == 2 && !strcmp(name, "illust")) illust_on = a != 0;
        else if (n == 2 && !strcmp(name, "fullscreen")) cfg_full = a != 0;
        else if (n == 3 && !strcmp(name, "window")) { win_w = (int)a; win_h = (int)b; }
        else if (n == 2 && !strncmp(name, "rate_", 5)) {
            for (int k = 0; k < 3; k++) if (!strcmp(name, RATE_KEYS[k])) {
                int best = RATE_ONE;
                for (int i = 0; i < (int)std::size(RATE_STEPS); i++) if (std::fabs(RATE_STEPS[i] - a) < std::fabs(RATE_STEPS[best] - a)) best = i;
                model.rate[k] = best;
            }
        }
        else if (n == 4) for (auto &g : groups) if (!strcmp(g.id, name)) { g.x = std::clamp(a, 0.f, 1.f); g.y = std::clamp(b, 0.f, 1.f); g.scale = std::clamp(c, 0.5f, 2.5f); }
    }
    fclose(f);
}

// 묶음 중심(화면 비율)과 크기로 스타일 값을 만든다. 화면 밖으로 나가지 않게 막는다.
void update_layout() {
    float dp = ctx->GetDensityIndependentPixelRatio();
    Rml::Vector2i d = ctx->GetDimensions();
    float sw = d.x / dp, sh = d.y / dp;
    laid_w = sw; laid_h = sh;
    for (auto &g : groups) {
        float font = BASE_DP * model.size / 100.f * g.scale;
        float w = g.w_em * font, h = g.h_em * font;
        float left = std::clamp(g.x * sw - w / 2, 0.f, std::max(0.f, sw - w));
        float top = std::clamp(g.y * sh - h / 2, 0.f, std::max(0.f, sh - h));
        g.font = Rml::ToString(font) + "dp";
        g.left = Rml::ToString(left) + "dp";
        g.top = Rml::ToString(top) + "dp";
    }
    model_h.DirtyVariable("g");
}

void refresh_binds() {
    binds.resize(gamepad_action_count());
    for (int i = 0; i < (int)binds.size(); i++) binds[i] = {gamepad_action_name(i), gamepad_action_key(i)};
    model_h.DirtyVariable("binds");
}

void set_capture(int row) {
    model.capture = row;
    capture_until = SDL_GetTicks() + CAPTURE_TIMEOUT_MS;
    model_h.DirtyVariable("capture");
}

// 기타 설정(환경설정의 '기타' 버튼, F3). 전투 보상 배율은 restored/rates.c가 ui_rate로 읽는다.
void show_etc() { etc_doc->Show(); etc_doc->PullToFront(); focus_pending = etc_doc; }
void hide_etc() { etc_doc->Hide(); save_config(); }
std::string rate_text(int k) {
    int v = RATE_STEPS[model.rate[k]];
    char b[16];
    if (v % 100) snprintf(b, sizeof b, "×%d.%d", v / 100, v % 100 / 10);
    else snprintf(b, sizeof b, "×%d", v / 100);
    return b;
}

// 세이브 데이터 창(환경설정의 '세이브 데이터' 버튼, ui/savedata.rml). 파일 다루기는 host/savefile.c.
// 단계: 0 고르기, 1 덮어쓰기 경고, 2 결과 알림. 단계가 바뀌면 그 단계의 기본 버튼(경고는 「취소」)에 초점을 둔다.
struct { int stage = 0, have = 0, file = 0; Rml::String msg; bool focus = false; } sd;
void sd_set(int stage) {
    sd.stage = stage;
    sd.focus = true;
    for (const char *n : {"sd_stage", "sd_have", "sd_file", "sd_msg"}) model_h.DirtyVariable(n);
}
void show_savedata() { sd.have = savefile_slot_count(); sd_set(0); savedata_doc->Show(); savedata_doc->PullToFront(); }
void hide_savedata() { if (sd.stage == 1) savefile_import_answer(false); savedata_doc->Hide(); }

void show_settings() { settings_doc->Show(); settings_doc->PullToFront(); focus_pending = settings_doc; }

void close_binds() {
    set_capture(-1);
    binds_doc->Hide();
    save_config();
    show_settings();
}

void set_edit(bool on) {
    model.edit = on;
    model.selected = -1;
    model_h.DirtyVariable("edit");
    model_h.DirtyVariable("selected");
    if (on) { settings_doc->Hide(); editbar_doc->Show(); editbar_doc->PullToFront(); }
    else { editbar_doc->Hide(); save_config(); }
}

void bind_model() {
    Rml::DataModelConstructor m = ctx->CreateDataModel("ui");
    if (auto s = m.RegisterStruct<Group>()) {
        s.RegisterMember("left", &Group::left);
        s.RegisterMember("top", &Group::top);
        s.RegisterMember("font", &Group::font);
    }
    static std::vector<Group *> refs;
    for (auto &g : groups) refs.push_back(&g);
    m.RegisterArray<std::vector<Group *>>();
    m.Bind("g", &refs);
    m.Bind("mode", &model.mode);
    m.Bind("airship", &model.airship);
    m.Bind("play", &model.play);
    m.Bind("edit", &model.edit);
    m.Bind("selected", &model.selected);
    m.Bind("pad_on", &model.pad_on);
    m.Bind("alpha", &model.alpha);
    m.Bind("gamepad", &model.gamepad);
    m.Bind("touch", &model.touch);
    // 설정 창 아래 키 안내(창 조작은 고정 배치다): src 0 키보드, 1 터치, 2 패드. 패드 버튼 이름은 패드 종류를 따른다
    m.BindFunc("src", [](Rml::Variant &v) { v = gamepad_source(); });
    m.BindFunc("pad_ok", [](Rml::Variant &v) { v = Rml::String(gamepad_fixed_label(0)); });
    m.BindFunc("pad_back", [](Rml::Variant &v) { v = Rml::String(gamepad_fixed_label(1)); });
    m.Bind("capture", &model.capture);
    if (auto s = m.RegisterStruct<Bind>()) {
        s.RegisterMember("name", &Bind::name);
        s.RegisterMember("key", &Bind::key);
    }
    m.RegisterArray<std::vector<Bind>>();
    m.Bind("binds", &binds);
    m.BindEventCallback("open_binds", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) {
        settings_doc->Hide();
        binds_doc->Show();
        binds_doc->PullToFront();
        focus_pending = binds_doc;
    });
    m.BindEventCallback("close_binds", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { close_binds(); });
    m.BindEventCallback("bind_start", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args) {
        if (!args.empty()) set_capture(args[0].Get<int>());
    });
    m.BindEventCallback("bind_reset", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { gamepad_bind_reset(); save_config(); });
    m.BindFunc("pad_size", [](Rml::Variant &v) { v = model.size; }, [](const Rml::Variant &v) { model.size = v.Get<int>(); update_layout(); save_config(); });
    for (int k = 0; k < 3; k++) {
        static const char *idx[3] = {"rate_exp", "rate_gold", "rate_drop"}, *txt[3] = {"rate_exp_t", "rate_gold_t", "rate_drop_t"};
        m.BindFunc(idx[k], [k](Rml::Variant &v) { v = model.rate[k]; }, [k](const Rml::Variant &v) {
            model.rate[k] = std::clamp(v.Get<int>(), 0, (int)std::size(RATE_STEPS) - 1);
            model_h.DirtyVariable(txt[k]);
            save_config();
        });
        m.BindFunc(txt[k], [k](Rml::Variant &v) { v = Rml::String(rate_text(k)); });
    }
    m.BindEventCallback("rate_reset", [](Rml::DataModelHandle h, Rml::Event &, const Rml::VariantList &) {
        for (int k = 0; k < 3; k++) model.rate[k] = RATE_ONE;
        for (const char *n : {"rate_exp", "rate_gold", "rate_drop", "rate_exp_t", "rate_gold_t", "rate_drop_t"}) h.DirtyVariable(n);
        save_config();
    });
    m.BindFunc("illust", [](Rml::Variant &v) { v = illust_on; }, [](const Rml::Variant &v) { illust_on = v.Get<bool>(); save_config(); });
    m.BindFunc("sd_stage", [](Rml::Variant &v) { v = sd.stage; });
    m.BindFunc("sd_have", [](Rml::Variant &v) { v = sd.have; });
    m.BindFunc("sd_file", [](Rml::Variant &v) { v = sd.file; });
    m.BindFunc("sd_msg", [](Rml::Variant &v) { v = sd.msg; });
    m.BindEventCallback("sd_export", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { savefile_choose(false); });
    m.BindEventCallback("sd_import", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { savefile_choose(true); });
    m.BindEventCallback("sd_yes", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { savefile_import_answer(true); });
    m.BindEventCallback("sd_no", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { savefile_import_answer(false); sd_set(0); });
    m.BindEventCallback("sd_ok", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { sd.have = savefile_slot_count(); sd_set(0); });
    m.BindEventCallback("sd_close", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { hide_savedata(); });
    m.BindEventCallback("close_etc", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { hide_etc(); });
    m.BindEventCallback("save", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { save_config(); });
    m.BindEventCallback("open_edit", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { set_edit(true); });
    m.BindEventCallback("close", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { settings_doc->Hide(); save_config(); });
    m.BindEventCallback("done", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { set_edit(false); show_settings(); });
    m.BindEventCallback("reset", [](Rml::DataModelHandle h, Rml::Event &, const Rml::VariantList &) {
        reset_groups();
        model.size = 100;
        model.alpha = 70;
        h.DirtyVariable("pad_size");
        h.DirtyVariable("alpha");
        update_layout();
        save_config();
    });
    m.BindEventCallback("resize", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args) {
        if (model.selected < 0 || args.empty()) return;
        auto &g = groups[model.selected];
        g.scale = std::clamp(g.scale + args[0].Get<float>(), 0.5f, 2.5f);
        update_layout();
        save_config();
    });
    model_h = m.GetModelHandle();
}

int group_of(Rml::Element *e) {
    for (; e; e = e->GetParentNode())
        for (int i = 0; i < (int)std::size(groups); i++) if (e->GetId() == groups[i].id) return i;
    return -1;
}

bool pad_shown() { return pad_doc && model.pad_on && model.mode && !model.edit; }

// 방향 패드 묶음 상자(문맥 좌표). 누르기 시작은 이 상자 안에서만.
bool dpad_box(Rml::Vector2f &c, float &r) {
    Rml::Element *g = pad_doc ? pad_doc->GetElementById("dpad") : nullptr;
    if (!g || !g->IsVisible(true)) return false;
    Rml::Vector2f o = g->GetAbsoluteOffset(Rml::BoxArea::Border), sz = g->GetBox().GetSize(Rml::BoxArea::Border);
    c = o + sz * 0.5f;
    r = std::min(sz.x, sz.y) * 0.5f;
    return r > 0;
}
bool in_dpad(float x, float y) {
    if (!pad_shown()) return false;
    Rml::Vector2f c; float r;
    if (!dpad_box(c, r)) return false;
    float dx = x - c.x, dy = y - c.y;
    return dx * dx + dy * dy <= r * r * 1.1f;
}
// 가운데에서 본 각도로 네 방향 중 하나. 가운데 작은 원(반지름의 15%)에서는 없음.
Rml::Element *dpad_arrow(float x, float y) {
    Rml::Vector2f c; float r;
    if (!dpad_box(c, r)) return nullptr;
    float dx = x - c.x, dy = y - c.y;
    if (dx * dx + dy * dy < r * r * 0.0225f) return nullptr;
    const char *code = std::fabs(dx) > std::fabs(dy) ? (dx < 0 ? "-3" : "-4") : (dy < 0 ? "-1" : "-2");
    return pad_doc->QuerySelector(std::string("#dpad [code=\"") + code + "\"]");
}

Rml::Element *pad_button_at(float x, float y) {
    if (!pad_shown()) return nullptr;
    Rml::Element *e = ctx->GetElementAtPoint({x, y}, nullptr, pad_doc);
    for (; e && e != pad_doc; e = e->GetParentNode()) if (e->HasAttribute("code") || e->HasAttribute("action")) return e;
    return nullptr;
}

bool is_target(Rml::Element *e) { return e->HasAttribute("key") || e->HasAttribute("nav") || e->HasAttribute("action") || e->HasAttribute("hold") || e->HasAttribute("pick"); }

// pick="<이름>" idx="<번호>": 누르면 번호를 남겨 두고 래퍼가 ui_pick(이름)으로 가져가 원본 키를 직접 보낸다
// (여러 단계를 거치는 이동처럼 nav 하나로 안 되는 것). nav와 함께 달면 그 nav가 지금 알려져 있을 때는 nav가 먼저다.
std::vector<std::pair<std::string, int>> picks;

// scroll="<이름>" 요소(overflow-y: auto인 글 상자): 손가락으로 끌기, 휠, 게임패드 오른쪽 스틱, 래퍼의 ui_scroll로 넘긴다.
Rml::Element *scroll_at(float x, float y) {
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        for (Rml::Element *e = ctx->GetElementAtPoint({x, y}, nullptr, d->el); e && e != d->el; e = e->GetParentNode())
            if (e->HasAttribute("scroll")) return e;
    }
    return nullptr;
}
Rml::Element *scroll_named(const char *name) {
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, "[scroll]");
        for (auto *e : l) if (e->IsVisible(true) && (!name || e->GetAttribute<Rml::String>("scroll", "") == name)) return e;
    }
    return nullptr;
}

// 위에 쌓인 문서부터 누를 것(key/nav/action)을 찾는다.
Rml::Element *target_at(float x, float y, Doc **out) {
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        Rml::Element *e = ctx->GetElementAtPoint({x, y}, nullptr, d->el);
        for (; e; e = e->GetParentNode()) {
            if (is_target(e)) {
                // 빈 key(data-attr-key가 ''인 칸)는 누를 수 없다.
                if (e->HasAttribute("key") && !key_code(e->GetAttribute<Rml::String>("key", ""))) break;
                if (e->HasAttribute("hold") && !key_code(e->GetAttribute<Rml::String>("hold", ""))) break;
                *out = d;
                return e;
            }
            if (e == d->el) break;
        }
        // 누른 자리에 누를 것이 없으면 그 문서의 "아무 데나 누르기"(.tapall)로 받는다. tapall은 문서 맨 앞(맨 아래)에 깔려
        // 대사창 판처럼 위에 놓인 요소를 누르면 판이 먼저 걸려 터치가 씹혔다(폰에서 대사창 위를 누를 때).
        if (Rml::Element *t = d->el->QuerySelector(".tapall"); t && t->IsVisible(true) && t->HasAttribute("key")) {
            Rml::Element *hit = ctx->GetElementAtPoint({x, y}, nullptr, d->el);
            if (hit) { *out = d; return t; }
        }
    }
    return nullptr;
}

void press(Rml::Element *b) {
    if (!b) return;
    if (b->HasAttribute("action")) return;
    int n = b->GetAttribute<int>("presses", 0);
    b->SetAttribute("presses", n + 1);
    b->SetClass("on", true);
    if (n == 0) host_key(2, b->GetAttribute<int>("code", 0));
}

void do_action(const std::string &a) {
    if (a == "settings") ui_toggle_settings();
    else if (a == "etc") ui_toggle_etc();
    else if (a == "savedata") ui_toggle_savedata();
    else if (a == "illust") ui_toggle_illust();
    else if (a == "auto") ui_toggle_auto();
    else if (a == "skip") ui_toggle_skip();
    else if (a == "bauto") ui_toggle_battle_auto();
    else if (a == "bfast") ui_toggle_battle_fast();
}

void release(Rml::Element *b) {
    if (!b) return;
    if (b->HasAttribute("action")) { do_action(b->GetAttribute<Rml::String>("action", "")); return; }
    int n = b->GetAttribute<int>("presses", 0);
    if (n <= 0) return;
    b->SetAttribute("presses", n - 1);
    if (n == 1) { b->SetClass("on", false); host_key(3, b->GetAttribute<int>("code", 0)); }
}

// 문서 요소를 눌렀다(손가락을 뗐다).
void activate(Doc *d, Rml::Element *e) {
    if (e->HasAttribute("action")) { do_action(e->GetAttribute<Rml::String>("action", "")); return; }
    if (e->HasAttribute("key")) {
        goal.on = false;
        queue_key(key_code(e->GetAttribute<Rml::String>("key", "")));
        return;
    }
    std::string nav = e->GetAttribute<Rml::String>("nav", "");
    int idx = e->GetAttribute<int>("idx", -1);
    Nav *n = nav.empty() ? nullptr : find_nav(d, nav);
    if (!n && e->HasAttribute("pick")) {
        std::string name = e->GetAttribute<Rml::String>("pick", "");
        goal.on = false;
        for (auto &p : picks) if (p.first == name) { p.second = idx; return; }
        picks.push_back({name, idx});
        return;
    }
    if (!n || idx < 0 || idx >= n->count) return;
    // UI_NAV_DIRECT: 래퍼가 원본 커서를 직접 옮긴다(ui_pick(목록 이름) → 번호). 키를 한 칸씩 흉내 내지 않는다.
    if (n->flags & UI_NAV_DIRECT) {
        goal.on = false;
        for (auto &p : picks) if (p.first == nav) { p.second = idx; return; }
        picks.push_back({nav, idx});
        return;
    }
    // 누르면 그 항목으로 가서 바로 결정한다(UI_NAV_ACT인 목록. 탭처럼 고르기만 하는 줄은 옮기기만). 사용자 결정(2026-11-10):
    // 처음에는 "고른 뒤 다시 누르면 결정"이었으나 터치에서는 바로 선택되는 편이 자연스럽다.
    bool ok = n->flags & UI_NAV_ACT;
    if (goal.on && goal.doc == d->name && goal.nav == nav) { goal.target = idx; goal.steps = n->count * 2 + 4; goal.ok = ok; return; }
    if (n->at == idx) {
        if (ok && keyq.empty()) queue_key(UI_KEY_OK);
        return;
    }
    keyq.clear();
    goal = {true, d->name, nav, idx, n->count * 2 + 4, ok};
    goal_step();
}

// ---- 세로 목록(<vlist>, ui_vlist.cpp) 공통 조작 ----
// 지금 고르는 세로 목록: 위 문서부터, 알린 목록(ui_nav) 중 커서 줄이 .vlist 안에 보이는 것.
constexpr int UI_KEY_UP_ = -1, UI_KEY_DOWN_ = -2;
bool active_vlist(Doc **od, Nav **on, Rml::Element **orow) {
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        for (auto &n : d->navs) {
            if (n.at < 0 || n.count <= 0) continue;
            Rml::Element *row = d->el->QuerySelector(".vlist [nav=\"" + n.name + "\"][idx=\"" + std::to_string(n.at) + "\"]");
            if (row && row->IsVisible(true)) { *od = d; *on = &n; *orow = row; return true; }
        }
    }
    return false;
}
// 커서만 옮긴다(결정하지 않는다). 원본 키를 틱마다 하나씩 보내는 목표 이동이라 UI_NAV_DIRECT 목록에도 그대로 맞는다
// (DIRECT의 pick은 "그 줄로 가서 결정"이라 여기서는 쓰지 않는다).
void move_cursor(Doc *d, Nav &n, int idx) {
    if (idx < 0 || idx >= n.count) return;
    bool same = goal.on && goal.doc == d->name && goal.nav == n.name;
    if (!same && idx == n.at) return;
    if (same) { goal.target = idx; goal.steps = n.count * 2 + 4; goal.ok = false; return; }
    keyq.clear();
    goal = {true, d->name, n.name, idx, n.count * 2 + 4, false};
    goal_step();
}
// 쪽 넘기기: PgUp/PgDn은 보이는 줄 수 - 1만큼(한 줄은 겹쳐 남겨 어디서 이어지는지 보이게), Home/End는 처음·끝.
// 근거: Microsoft "Gamepad and remote control interactions"의 가속 표(Page up/down = PgUp/PgDn, 패드는 LT/RT),
// Xbox 접근성 지침 112의 Sea of Thieves 예("LT/RT 쪽, LB/RB 탭"). 패드 LT/RT는 이 게임에서 원본 1·3번 키라 키보드만 둔다.
// 자체 화면 목록(pick 줄, ui.h): 다른 줄을 pick하면 커서만 옮기고(같은 줄을 다시 pick하면 실행) 래퍼가 상태를 직접 쓴다.
// 지금 고른 줄(.sel)이 보이는 위 문서의 .vlist를 찾는다.
Rml::Element *active_pick_row() {
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, ".vlist .vrow.sel[pick]");
        for (auto *e : l) if (e->IsVisible(true) && !e->HasAttribute("nav")) return e; // nav도 단 줄의 pick은 "들어가기"라 뺀다
    }
    return nullptr;
}
bool page_pick(SDL_Scancode s) {
    Rml::Element *row = active_pick_row();
    if (!row) return false;
    Rml::Element *box = row->GetParentNode();
    std::vector<Rml::Element *> rows;
    int at = -1;
    for (int i = 0; i < box->GetNumChildren(); i++) {
        Rml::Element *c = box->GetChild(i);
        if (!c->IsClassSet("vrow") || !c->HasAttribute("pick") || !c->IsVisible(true)) continue;
        if (c == row) at = (int)rows.size();
        rows.push_back(c);
    }
    if (at < 0) return false;
    float rh = row->GetBox().GetSize(Rml::BoxArea::Margin).y;
    int page = rh > 0 ? std::max(1, (int)(box->GetClientHeight() / rh) - 1) : 5, last = (int)rows.size() - 1;
    int t = s == SDL_SCANCODE_PAGEUP ? std::max(0, at - page) : s == SDL_SCANCODE_PAGEDOWN ? std::min(last, at + page) : s == SDL_SCANCODE_HOME ? 0 : last;
    if (t != at) ui_push_pick(rows[t]->GetAttribute<Rml::String>("pick", "").c_str(), rows[t]->GetAttribute<int>("idx", -1));
    return true;
}
bool page_key(SDL_Scancode s) {
    Doc *d; Nav *n; Rml::Element *row;
    if (!active_vlist(&d, &n, &row)) return page_pick(s);
    int from = goal.on && goal.doc == d->name && goal.nav == n->name ? goal.target : n->at;
    Rml::Element *box = row->GetParentNode();
    float rh = row->GetBox().GetSize(Rml::BoxArea::Margin).y;
    int rows = rh > 0 && box ? std::max(1, (int)(box->GetClientHeight() / rh) - 1) : 5;
    int t = from;
    if (s == SDL_SCANCODE_PAGEUP) t = std::max(0, from - rows);
    else if (s == SDL_SCANCODE_PAGEDOWN) t = std::min(n->count - 1, from + rows);
    else if (s == SDL_SCANCODE_HOME) t = 0;
    else if (s == SDL_SCANCODE_END) t = n->count - 1;
    move_cursor(d, *n, t);
    return true;
}
// 끝에서 돌기: 세로 목록의 마지막 줄에서 아래로 가면 첫 줄로, 첫 줄에서 위로 가면 마지막 줄로(Xbox 접근성 지침 112:
// "한 방향으로만 움직이는 목록은 끝에서 처음으로 돌아간다"). 원본이 이미 도는 목록(UI_NAV_WRAP)은 원본에 맡긴다.
bool wrap_key(int code) {
    if (!ctx) return false;
    Doc *d; Nav *n; Rml::Element *row;
    if (active_vlist(&d, &n, &row)) {
        if (n->flags & UI_NAV_WRAP || n->count < 2 || (goal.on && goal.nav == n->name)) return false;
        if (code == n->next && n->at == n->count - 1) { move_cursor(d, *n, 0); return true; }
        if (code == n->prev && n->at == 0) { move_cursor(d, *n, n->count - 1); return true; }
        return false;
    }
    Rml::Element *sel = active_pick_row();
    if (!sel || (code != UI_KEY_UP_ && code != UI_KEY_DOWN_)) return false;
    Rml::Element *box = sel->GetParentNode();
    std::vector<Rml::Element *> rows;
    for (int i = 0; i < box->GetNumChildren(); i++) {
        Rml::Element *c = box->GetChild(i);
        if (c->IsClassSet("vrow") && c->HasAttribute("pick") && c->IsVisible(true)) rows.push_back(c);
    }
    if (rows.size() < 2) return false;
    Rml::Element *to = code == UI_KEY_DOWN_ && sel == rows.back() ? rows.front() : code == UI_KEY_UP_ && sel == rows.front() ? rows.back() : nullptr;
    if (!to) return false;
    ui_push_pick(to->GetAttribute<Rml::String>("pick", "").c_str(), to->GetAttribute<int>("idx", -1));
    return true;
}
// 지금 보이는 문서가 그 원본 키를 쓰는가(키 안내·버튼의 key, 누르고 있기 hold). 쓰면 어깨 버튼은 원본 키 그대로 보낸다.
bool screen_uses_key(int code) {
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, "[key],[hold],[keyhint]"); // keyhint: pick 버튼처럼 key 없이 원본 키를 쓰는 곳(필드 메뉴 버튼 띠)
        for (auto *e : l) {
            std::string k = e->HasAttribute("key") ? e->GetAttribute<Rml::String>("key", "") : e->HasAttribute("hold") ? e->GetAttribute<Rml::String>("hold", "") : e->GetAttribute<Rml::String>("keyhint", "");
            if (key_code(k) == code && e->IsVisible(true)) return true;
        }
    }
    return false;
}
// 문서의 [속성=값] 요소 중 보이는 첫 것을 누른 것으로 친다(위 문서부터). 없으면 false.
bool activate_marked(const char *attr, const char *want) {
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, std::string("[") + attr + "]");
        for (auto *e : l)
            if (e->GetAttribute<Rml::String>(attr, "") == want && e->IsVisible(true) && is_target(e)) { activate(d, e); return true; }
    }
    return false;
}
bool pad_shortcut(int which, int code) {
    if (!ctx || ui_modal()) return false;
    // 필드 메뉴 틀(fm_shell 문서, restored/fm_shell.c)이 떠 있으면 틀이 이긴다(ui/README.md '필드 메뉴 틀'): LB/RB는 레일 탭 이전/다음,
    // LT/RT는 캐릭터 화면에서 캐릭터 이전/다음. 틀이 그 버튼을 쓰지 않는 상태(캐릭터 띠가 없거나 단축키 대기처럼 띠가 표시를 거둔 때)면
    // 아래 예전 규칙으로 간다. 그 때문에 패드에서 못 누르게 된 원본 키(장비 해제 0 등)는 화면의 버튼 띠가 다른 버튼으로 길을 남긴다.
    if (visible_doc("fm_shell")) {
        if (which < 2) return activate_marked("rail-shoulder", which == 0 ? "prev" : "next");
        if (activate_marked("char-shoulder", which == 2 ? "prev" : "next")) return true;
    }
    if (screen_uses_key(code)) return false;
    if (which >= 2) return page_key(which == 2 ? SDL_SCANCODE_PAGEUP : SDL_SCANCODE_PAGEDOWN);
    const char *want = which == 0 ? "prev" : "next";
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, "[shoulder]");
        for (auto *e : l)
            if (e->GetAttribute<Rml::String>("shoulder", "") == want && e->IsVisible(true)) {
                if (e->HasAttribute("key") && !key_code(e->GetAttribute<Rml::String>("key", ""))) continue; // 지금은 못 누르는 화살표(빈 key)
                activate(d, e);
                return true;
            }
    }
    return false;
}

// 마우스를 올리면 그 줄로 커서가 간다(누르면 결정은 터치와 같다). PC판 콘솔 RPG의 관례이고, Xbox 접근성 지침 112는 메뉴를
// 마우스·키보드·패드 모두로 다룰 수 있게 하라고 한다(Grounded PC 예). 손가락이 만든 마우스 이벤트와 버튼을 누른 채 끄는 것은 뺀다.
// 같은 줄 위에서 움직이는 동안은 다시 옮기지 않아, 키로 커서를 옮긴 뒤 마우스가 조금 흔들려도 되돌아가지 않는다.
Rml::ObserverPtr<Rml::Element> hover_row, hover_el;
void mouse_hover(float x, float y) {
    Doc *d = nullptr;
    Rml::Element *t = target_at(x, y, &d);
    // 마우스가 올라간 누를 것(버튼·탭·레일·칩·줄)에 hover 클래스를 단다(밝게 보이게. RmlUi의 :hover는 이 층에서 마우스를 넘기지 않아 쓰지 않는다).
    if (t != hover_el.get()) {
        if (hover_el) hover_el->SetClass("hover", false);
        hover_el = t && !t->IsClassSet("tapall") ? t->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
        if (hover_el) hover_el->SetClass("hover", true);
    }
    Rml::Element *row = t && (t->HasAttribute("nav") || t->HasAttribute("pick")) && t->IsClassSet("vrow") ? t : nullptr;
    if (row == hover_row.get()) return;
    hover_row = row ? row->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
    if (!row) return;
    // 자체 화면 pick 줄(nav 없음): 이미 고른 줄이 아니면 pick(커서만 옮김). 고른 줄을 pick하면 실행이므로 건드리지 않는다.
    // nav와 pick을 같이 단 줄(옵션 목록, 타이틀 하위 목록)의 pick은 "그 항목으로 들어가기"라 올려 두기로 보내면 안 된다
    // (저장 화면에서 옵션 목록에 마우스를 올리자 도움말이 열렸다). 그 nav가 지금 알려지지 않았으면 아무것도 하지 않는다.
    if (!row->HasAttribute("nav") || !find_nav(d, row->GetAttribute<Rml::String>("nav", ""))) {
        if (!row->HasAttribute("nav") && row->HasAttribute("pick") && !row->IsClassSet("sel"))
            ui_push_pick(row->GetAttribute<Rml::String>("pick", "").c_str(), row->GetAttribute<int>("idx", -1));
        return;
    }
    Nav *n = find_nav(d, row->GetAttribute<Rml::String>("nav", ""));
    if (n) move_cursor(d, *n, row->GetAttribute<int>("idx", -1));
}

// 손가락 좌표(창 비율) → 문맥 좌표(게임 화면 기준 출력 화소)
void finger_pos(const SDL_Event *e, float &x, float &y) {
    int ww = 1, wh = 1;
    SDL_GetWindowSize(window, &ww, &wh);
    float rx = e->tfinger.x * ww, ry = e->tfinger.y * wh;
    SDL_RenderCoordinatesFromWindow(renderer, rx, ry, &rx, &ry);
    x = rx - view_x; y = ry - view_y;
}

// 손가락을 먼저 비운 뒤 뗀다. 설정 버튼(action)을 떼면 ui_toggle_settings가 다시 모든 손가락을 떼므로, 비우지 않으면 끝없이 돈다.
void release_finger(Finger &f) {
    if (!f.used) return;
    f.used = false;
    if (f.kind == F_PAD && f.el) release(f.el.get());
    if ((f.kind == F_TAP || f.kind == F_HOLD) && f.el) f.el->SetClass("press", false);
    if (f.kind == F_HOLD) host_key(3, f.group);
}

void finger_event(const SDL_Event *e) {
    float x, y;
    finger_pos(e, x, y);
    float dp = ctx->GetDensityIndependentPixelRatio();
    Finger *f = nullptr;
    for (auto &c : fingers) if (c.used && c.id == e->tfinger.fingerID) f = &c;
    if (e->type == SDL_EVENT_FINGER_DOWN) {
        if (f) return;
        for (auto &c : fingers) if (!c.used) { f = &c; break; }
        if (!f) return;
        *f = Finger{};
        f->id = e->tfinger.fingerID; f->used = true; f->kind = F_NONE; f->group = -1; f->x0 = x; f->y0 = y;
        if (model.edit) {
            int g = group_of(ctx->GetElementAtPoint({x, y}, nullptr, pad_doc));
            if (g < 0) return;
            model.selected = g;
            model_h.DirtyVariable("selected");
            f->kind = F_EDIT;
            f->group = g;
            f->grab_x = groups[g].x - x / ctx->GetDimensions().x;
            f->grab_y = groups[g].y - y / ctx->GetDimensions().y;
            return;
        }
        if (in_dpad(x, y)) { f->kind = F_PAD; f->group = 0; Rml::Element *b = dpad_arrow(x, y); if (b) { f->el = b->GetObserverPtr(); press(b); } return; }
        if (Rml::Element *b = pad_button_at(x, y)) { f->kind = F_PAD; f->el = b->GetObserverPtr(); f->group = group_of(b); press(b); return; }
        Doc *d = nullptr;
        Rml::Element *t = target_at(x, y, &d);
        // 글 상자 안을 누르면 끌어 넘긴다(그 안의 누를 것이 아니면). tapall로 받은 것은 누를 것으로 치지 않는다.
        if (Rml::Element *sc = scroll_at(x, y); sc && (!t || t->IsClassSet("tapall"))) {
            f->kind = F_SCROLL; f->el = sc->GetObserverPtr(); f->grab_y = sc->GetScrollTop(); return;
        }
        if (t) {
            f->el = t->GetObserverPtr(); f->doc = d;
            if (t->HasAttribute("nav")) {
                // 목록 줄은 누름 색을 칠하지 않는다. 커서가 곧 그 줄로 가므로 선택 색과 누름 색이 두 줄에 섞여 보이던 문제.
                f->nav = t->GetAttribute<Rml::String>("nav", "");
                f->step = std::max(t->GetBox().GetSize(Rml::BoxArea::Border).y, 10 * dp);
            } else t->SetClass("press", true);
            // hold="7": 누르고 있는 동안 원본 키도 눌린 채로 둔다(전투 적 이름표처럼 누르는 동안만 뜻이 있는 키)
            if (t->HasAttribute("hold")) { f->kind = F_HOLD; f->group = key_code(t->GetAttribute<Rml::String>("hold", "")); host_key(2, f->group); }
            else f->kind = F_TAP;
        }
        return;
    }
    if (!f) return;
    if (e->type == SDL_EVENT_FINGER_MOTION) {
        if (f->kind == F_EDIT) {
            Rml::Vector2i d = ctx->GetDimensions();
            groups[f->group].x = std::clamp(x / d.x + f->grab_x, 0.f, 1.f);
            groups[f->group].y = std::clamp(y / d.y + f->grab_y, 0.f, 1.f);
            update_layout();
        } else if (f->kind == F_PAD && f->group == 0) {
            // 방향 패드: 손가락을 떼지 않고 옮겨 갈 수 있고, 방향은 패드 가운데에서 본 각도로 정한다(칸 사이 틈에서 끊기지 않게).
            // 먼저 떼고 누른다. 뗄 때 남은 방향은 main.c send_key가 이어 준다.
            Rml::Element *hit = dpad_arrow(x, y);
            if (hit != f->el.get()) {
                if (f->el) release(f->el.get());
                f->el = hit ? hit->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
                if (hit) press(hit);
            }
        } else if (f->kind == F_SCROLL) {
            if (f->el) f->el->SetScrollTop(f->grab_y - (y - f->y0));
        } else if (f->kind == F_TAP) {
            float dx = x - f->x0, dy = y - f->y0;
            if (!f->moved && (dx * dx + dy * dy) > (8 * dp) * (8 * dp)) { f->moved = true; if (f->el) f->el->SetClass("press", false); }
            // scroll 상자 안의 누를 것(pick·nav 줄)을 누른 채 끌면 누르기를 버리고 상자를 끌어 넘긴다.
            // scroll 상자 밖의 nav 목록(원본 창 그대로 보이는 목록)은 아래처럼 한 칸씩 넘긴다.
            if (f->moved && f->el) {
                Rml::Element *sc = f->el.get();
                while (sc && !sc->HasAttribute("scroll")) sc = sc->GetParentNode();
                if (sc) {
                    f->kind = F_SCROLL;
                    f->el = sc->GetObserverPtr();
                    f->grab_y = sc->GetScrollTop() + (y - f->y0); // 문턱만큼 튀지 않게 지금 자리에서 이어 끈다
                    return;
                }
            }
            // 목록을 끌면 한 칸씩 넘긴다(손가락을 올리면 다음 줄로). 칸 높이는 누른 줄의 크기다.
            Nav *n = f->moved && !f->nav.empty() ? find_nav(f->doc, f->nav) : nullptr;
            if (n && n->flags & (UI_NAV_VERT | UI_NAV_HORZ)) {
                bool vert = n->flags & UI_NAV_VERT;
                float step = f->step;
                float delta = vert ? y - f->y0 : x - f->x0;
                int want = (int)(-delta / step);
                int done = (int)f->acc;
                if (want != done && keyq.size() < 4) {
                    goal.on = false;
                    queue_key(want > done ? n->next : n->prev);
                    f->acc += want > done ? 1 : -1;
                }
            }
        }
        return;
    }
    // 뗌, 취소
    if (f->kind == F_EDIT) save_config();
    Doc *hit_doc = nullptr;
    Rml::Element *hit = f->kind == F_TAP && e->type == SDL_EVENT_FINGER_UP && !f->moved ? target_at(x, y, &hit_doc) : nullptr;
    bool tap = hit && hit == f->el.get() && hit_doc == f->doc;
    release_finger(*f);
    if (tap) activate(hit_doc, hit);
}

int mouse_button(int b) { return b == SDL_BUTTON_LEFT ? 0 : b == SDL_BUTTON_RIGHT ? 1 : 2; }

// 게임 장면 번호(게임 객체 +0xc: 7 필드, 10 스크립트 진행, 13 전투, 14 메뉴). work/ui/survey/field.md 1절.
enum { SCENE_SCRIPT = 10 };
int game_scene() {
    cpu_t *c = &cpu;
    uint32_t app = R32_(c, 0x14ee34);
    if (app < GUEST_LOW || app >= GUEST_MEM - 0x60) return -1;
    uint32_t g = R32_(c, app + 0x48);
    if (g < GUEST_LOW || g >= GUEST_MEM - 0x40) return -1;
    return (int)R32_(c, g + 0xc);
}

// 지금 필드에서 비행정을 타고 있다(이착륙 0번 키). 조건은 원본 HUD 0x12b18과 같다(work/ui/survey/field.md 2절).
bool on_airship() {
    cpu_t *c = &cpu;
    uint32_t app = R32_(c, 0x14ee34);
    if (app < GUEST_LOW || app >= GUEST_MEM - 0x60) return false;
    uint32_t g = R32_(c, app + 0x48);
    if (g < GUEST_LOW || g >= GUEST_MEM - 0x40) return false;
    return R32_(c, g + 0xc) == 7 && (R32_(c, g + 0x14) & 8) && (R32_(c, g + 0x20) & 0x20000);
}

void sync_bool(bool &v, bool now, const char *name) { if (v != now) { v = now; model_h.DirtyVariable(name); } }

// SKIP은 대사가 이어지는 동안만: 선택지가 뜨거나 대사창이 1.5초(틱 기준 약 45) 넘게 없으면 끈다.
// 말풍선(dialog_balloon)도 OK를 기다리는 연출이 있다(새 게임 첫 장면 "총사!"). SKIP이면 바로, AUTO면 1.5초 뒤 OK를 넣는다.
// 대사창(dialog)은 restored/ui_dialog.c가 쪽 단위로 처리한다.
int skip_idle;
uint64_t balloon_since;
void input_present() {
    goal_step();
    bool talk = visible_doc("dialog"), balloon = visible_doc("dialog_balloon") != nullptr;
    if (model.skip_on) {
        if (visible_doc("dialog_choice")) ui_toggle_skip();
        else if (talk || balloon || game_scene() == SCENE_SCRIPT) { skip_idle = 0; if (balloon && !talk) ui_key_tap(UI_KEY_OK); }
        else if (++skip_idle > 15) ui_toggle_skip(); // 스크립트가 끝나 필드(7)·전투·메뉴로 돌아왔다
    }
    if (!balloon || talk) balloon_since = 0;
    else if (model.auto_on) {
        uint64_t now = now_ms();
        if (!balloon_since) balloon_since = now;
        else if (now - balloon_since >= 1500 && ui_key_tap(UI_KEY_OK)) balloon_since = now;
    }
}

bool input_visible() { return pad_shown() || ui_modal() || model.edit; }

void doc_loaded(Doc *d) {
    int src = gamepad_source();
    d->el->SetClass("touch", src == SOURCE_TOUCH);
    d->el->SetClass("pad", src == SOURCE_PAD);
}

// 매 그리기 전에: 문서가 누를 것을 보이는지 살피고, 패드 모양과 장치별 키 이름표를 맞춘다.
void input_frame() {
    if (!pad_doc) return;
    Rml::Vector2i dim = ctx->GetDimensions();
    float dp = ctx->GetDensityIndependentPixelRatio();
    if (dim.x / dp != laid_w || dim.y / dp != laid_h) update_layout();
    if (gamepad_generation() != label_generation) {
        int src = gamepad_source();
        // `{{ 'ok' | key }}`처럼 값에 기대지 않는 식은 RmlUi가 다시 계산하지 않는다. 장치나 맵핑이 바뀌면(드물다)
        // 열린 문서를 다시 읽어 이름표를 새로 만든다. 문서는 다음 틱에 채워지며 다시 뜬다.
        if (label_generation >= 0) ui_reload();
        label_generation = gamepad_generation();
        for (auto &d : docs) if (d->el) doc_loaded(d.get());
        sync_bool(model.touch, src == SOURCE_TOUCH, "touch");
        model_h.DirtyVariable("src"); model_h.DirtyVariable("pad_ok"); model_h.DirtyVariable("pad_back");
    }
    // 패드 모양: 위 문서가 정한 것, 없으면 누를 것이 보이는 문서가 있는지로 정한다.
    int mode = -1;
    bool inter = false;
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        if (mode < 0 && d->pad != UI_PAD_AUTO) mode = d->pad;
        bool any = !d->navs.empty();
        if (!any) {
            Rml::ElementList l;
            d->el->QuerySelectorAll(l, "[key],[nav],[action]");
            for (auto *e : l) if (e->IsVisible(true)) { any = true; break; }
        }
        d->interactive = any;
        inter |= any;
    }
    // 플레이 장면: 타이틀 쪽(로고, 타이틀, 메인 메뉴, 새 게임·이어하기 준비, 첫 실행, 만든사람들 진입)과 필드 메뉴(14),
    // 게임 중 불러오기(18)는 아니다. 이 장면들은 문서를 직접 누르거나 아무 곳이나 누르므로 패드가 필요 없는데, 문서가 바뀌는
    // 빈 틱(「PRESS ANY KEY」가 깜빡여 꺼진 틱, 메뉴 화면 사이)마다 기본 모양(필드 패드)이 번쩍 떴다(폰에서 발견).
    int scene = game_scene();
    bool play = scene >= 7 && scene != 14 && scene != 18 && scene != 22 && scene != 23;
    sync_bool(model.play, play, "play");
    if (mode < 0) mode = inter || !play ? UI_PAD_NONE : UI_PAD_FIELD;
    // 패드로 조작 중이면 화면 패드를 숨긴다(터치하면 다시 나온다).
    int shown = mode == UI_PAD_NONE || gamepad_source() == SOURCE_PAD ? 0 : mode;
    // 새로 보이거나 모양이 바뀔 때는 같은 모양이 몇 프레임 이어진 뒤에 바꾼다(문서 사이 한 틱의 빈자리에 번쩍이지 않게).
    // 숨길 때는 바로 숨긴다(메뉴가 뜨자마자 패드가 비켜야 누를 것을 가리지 않는다).
    static int want_mode = -1, want_frames = 0;
    if (shown != want_mode) { want_mode = shown; want_frames = 0; }
    else if (want_frames < 1000) want_frames++;
    if (shown != 0 && shown != model.mode && want_frames < 4) shown = model.mode;
    if (shown != model.mode) {
        model.mode = shown;
        model_h.DirtyVariable("mode");
        for (auto &f : fingers) if (f.used && f.kind == F_PAD) release_finger(f);
    }
    sync_bool(model.airship, shown == UI_PAD_FIELD && on_airship(), "airship");
    if (gamepad_generation() != pad_generation) {
        pad_generation = gamepad_generation();
        refresh_binds();
        sync_bool(model.gamepad, gamepad_connected(), "gamepad");
        if (!model.gamepad && binds_doc->IsVisible()) close_binds();
    }
    if (ui_pad_capturing() && SDL_GetTicks() > capture_until) set_capture(-1);
    if (focus_pending) {
        ctx->Update();
        Rml::Element *first = focus_pending->QuerySelector("button.bind, #first");
        if (first) first->Focus(true);
        focus_pending = nullptr;
    }
    if (sd.focus && savedata_doc->IsVisible()) {
        ctx->Update();
        static const char *ID[] = {"sd0", "sd1", "sd2"};
        if (Rml::Element *b = savedata_doc->GetElementById(ID[sd.stage])) b->Focus(true);
        sd.focus = false;
    }
}

} // namespace

extern "C" {

bool ui_modal(void) { return ctx && settings_doc && (model.edit || settings_doc->IsVisible() || binds_doc->IsVisible() || etc_doc->IsVisible() || savedata_doc->IsVisible()); }

// 세이브 데이터 창(host/savefile.c가 결과·경고를 이리로 알린다)
void ui_toggle_savedata(void) {
    if (!ctx || !savedata_doc) return;
    if (ui_modal() && !savedata_doc->IsVisible()) return;
    gamepad_release_all();
    ui_release_all();
    if (savedata_doc->IsVisible()) hide_savedata();
    else show_savedata();
}
void ui_savedata_say(const char *msg) {
    if (!ctx || !savedata_doc) return;
    sd.msg = msg;
    sd_set(2);
    if (!savedata_doc->IsVisible()) { savedata_doc->Show(); savedata_doc->PullToFront(); }
}
void ui_savedata_ask(int in_file, int have) {
    if (!ctx || !savedata_doc) return;
    sd.file = in_file;
    sd.have = have;
    sd_set(1);
    if (!savedata_doc->IsVisible()) { savedata_doc->Show(); savedata_doc->PullToFront(); }
}

int ui_rate(int kind) { return kind >= 0 && kind < 3 ? RATE_STEPS[model.rate[kind]] : 100; }

void ui_toggle_etc(void) {
    if (!ctx || !etc_doc) return;
    if (ui_modal() && !etc_doc->IsVisible()) return; // 조작 설정이 떠 있으면 그쪽이 먼저
    gamepad_release_all();
    ui_release_all();
    if (etc_doc->IsVisible()) hide_etc();
    else show_etc();
}

bool ui_pad_capturing(void) { return ctx && model.capture >= 0; }

bool ui_pad_capture(int input) {
    if (!ui_pad_capturing()) return false;
    gamepad_bind(model.capture, input);
    set_capture(-1);
    save_config();
    return true;
}

void ui_pad_nav(int nav) {
    if (!ctx) return;
    if (nav == UI_NAV_BACK) { ui_toggle_settings(); return; }
    static const Rml::Input::KeyIdentifier KEYS[] = {Rml::Input::KI_UP, Rml::Input::KI_DOWN, Rml::Input::KI_LEFT, Rml::Input::KI_RIGHT, Rml::Input::KI_RETURN};
    ctx->ProcessKeyDown(KEYS[nav], 0);
    ctx->ProcessKeyUp(KEYS[nav], 0);
}

void ui_release_all(void) {
    if (!ctx) return;
    for (auto &c : fingers) if (c.used) release_finger(c);
}

void ui_toggle_settings(void) {
    if (!ctx || !settings_doc) return;
    gamepad_release_all();
    if (ui_pad_capturing()) { set_capture(-1); return; }
    if (binds_doc->IsVisible()) { close_binds(); return; }
    if (model.edit) { set_edit(false); return; }
    ui_release_all();
    if (etc_doc->IsVisible()) { hide_etc(); return; } // 기타 설정 창도 같은 키(ESC, BACK)로 닫는다
    if (savedata_doc->IsVisible()) { // 세이브 데이터 창: 경고·알림 단계에서는 고르기로, 고르기에서는 닫는다(파일 창이 떠 있으면 그대로)
        if (savefile_dialog_open()) return;
        if (sd.stage == 1) { savefile_import_answer(false); sd_set(0); }
        else if (sd.stage == 2) { sd.have = savefile_slot_count(); sd_set(0); }
        else hide_savedata();
        return;
    }
    if (settings_doc->IsVisible()) { settings_doc->Hide(); save_config(); }
    else show_settings();
}

// ---- 화면 모드 ----
// 창을 만들기 전에 main.c가 저장된 모드와 크기를 읽는다(전체화면으로 띄웠다가 창으로 바꾸는 깜빡임을 피한다).
bool ui_peek_display(const char *path, bool *full, int *w, int *h) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char line[128], name[32];
    float a, b;
    bool any = false;
    while (fgets(line, sizeof line, f)) {
        int n = sscanf(line, "%31s %f %f", name, &a, &b);
        if (n == 2 && !strcmp(name, "fullscreen")) { *full = a != 0; any = true; }
        else if (n == 3 && !strcmp(name, "window") && a >= 240 && b >= 160) { *w = (int)a; *h = (int)b; }
    }
    fclose(f);
    return any;
}
bool ui_display_supported(void) {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    return false; // 브라우저는 페이지가 전체화면을 맡는다(web/app.js, F11)
#else
    return window && !(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN); // 헤드리스 시험(숨긴 창)에서는 줄을 두지 않는다
#endif
}
bool ui_fullscreen(void) { return disp_full; }
void ui_set_fullscreen(bool on) {
    if (!ui_display_supported()) return;
    if (on && !disp_full && !(SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)) SDL_GetWindowSize(window, &win_w, &win_h);
    disp_full = cfg_full = on;
    SDL_SetWindowFullscreen(window, on);
    if (!on && win_w > 0 && win_h > 0) SDL_SetWindowSize(window, win_w, win_h);
    save_config();
}
void ui_toggle_fullscreen(void) { ui_set_fullscreen(!disp_full); }

void ui_toggle_illust(void) { illust_on = !illust_on; save_config(); if (model_h) model_h.DirtyVariable("illust"); }
bool ui_dialog_auto(void) { return model.auto_on; }
bool ui_dialog_skip(void) { return model.skip_on; }
void ui_toggle_auto(void) {
    model.auto_on = !model.auto_on;
    if (model.auto_on) model.skip_on = false;
    save_config();
}
void ui_toggle_skip(void) {
    model.skip_on = !model.skip_on;
    skip_idle = 0;
}
bool ui_scroll(const char *name, float dp) {
    if (!ctx) return false;
    Rml::Element *e = scroll_named(name);
    if (!e) return false;
    e->SetScrollTop(e->GetScrollTop() + dp * ctx->GetDensityIndependentPixelRatio());
    return true;
}

bool ui_scroll_show(const char *name, int idx) {
    if (!ctx) return false;
    Rml::Element *e = scroll_named(name);
    if (!e) return false;
    Rml::Element *it = e->QuerySelector("[idx=\"" + std::to_string(idx) + "\"]");
    if (!it) return false;
    // 한 줄 앞 미리 보이기(apply_scroll_reqs와 같다). 옮긴 방향은 상자마다 지난번 번호로 안다. 줄은 DOM 형제 순서로 본다.
    static std::vector<std::pair<std::string, int>> last;
    int from = -1;
    for (auto &l : last) if (l.first == name) { from = l.second; l.second = idx; }
    if (from < 0 && name) last.push_back({name, idx});
    if (from >= 0 && from != idx) {
        Rml::Element *nb = idx > from ? it->GetNextSibling() : it->GetPreviousSibling();
        if (nb && nb->IsVisible(true)) nb->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest));
    }
    it->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest));
    return true;
}

int ui_pick(const char *name) {
    for (size_t i = 0; i < picks.size(); i++)
        if (picks[i].first == name) { int v = picks[i].second; picks.erase(picks.begin() + (long)i); return v; }
    return -1;
}

void ui_push_pick(const char *name, int idx) { picks.push_back({name, idx}); }
bool ui_wrap_key(int code) { return wrap_key(code); }
bool ui_pad_shortcut(int which, int code) { return pad_shortcut(which, code); }
// 보이는 문서에 hotkey="<원본 키 이름>" 요소가 있으면 그 키를 누를 때 요소를 누른 것으로 친다(키보드·패드 공통, 원본에는 보내지 않는다).
bool ui_hot_key(int code) {
    if (!ctx || ui_modal() || !code) return false;
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, "[hotkey]");
        for (auto *e : l)
            if (key_code(e->GetAttribute<Rml::String>("hotkey", "")) == code && e->IsVisible(true) && is_target(e)) { activate(d, e); return true; }
    }
    return false;
}
// 보이는 문서에 padhot="<동작 id>" 요소가 있으면 그 요소를 누른 것으로 친다(게임패드 버튼을 그 화면에서만 빌려 쓴다).
bool ui_pad_hot(const char *action_id) {
    if (!ctx || ui_modal() || !action_id) return false;
    return activate_marked("padhot", action_id);
}

bool ui_key_tap(int code) {
    if (!keyq.empty()) return false;
    auto_queue = true;
    queue_key(code);
    auto_queue = false;
    return true;
}

// 자동 전투·전투 2배속(restored/autobattle.c). 켜고 끄면 알림을 띄우고 ui.ini에 남긴다.
bool ui_battle_auto(void) { return model.bauto; }
bool ui_battle_fast(void) { return model.bfast; }
void ui_toggle_battle_auto(void) {
    model.bauto = !model.bauto;
    ext_toast(model.bauto ? "자동 전투 켬" : "자동 전투 끔");
    save_config();
}
void ui_toggle_battle_fast(void) {
    model.bfast = !model.bfast;
    ext_toast(model.bfast ? "전투 2배속 켬" : "전투 2배속 끔");
    save_config();
}
void ui_note_human(void) { human_ms = now_ms(); }
uint64_t ui_human_ms(void) { return human_ms; }

void ui_init_input(SDL_Window *win, const char *config_path, bool force_pad) {
    if (!ctx) return;
    window = win;
    cfg_path = config_path;
    bind_model();
#ifdef __ANDROID__
    load_config(true);
#else
    load_config(false);
#endif
    if (force_pad) model.pad_on = true;
    disp_full = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
    update_layout(); // 문서가 처음 읽을 때 자리 값이 비어 있지 않게
    pad_doc = ctx->LoadDocument(ui_dir + "/pad.rml");
    settings_doc = ctx->LoadDocument(ui_dir + "/settings.rml");
    editbar_doc = ctx->LoadDocument(ui_dir + "/editbar.rml");
    binds_doc = ctx->LoadDocument(ui_dir + "/gamepad.rml");
    etc_doc = ctx->LoadDocument(ui_dir + "/etc.rml");
    savedata_doc = ctx->LoadDocument(ui_dir + "/savedata.rml");
    savefile_set_window(win);
    if (!pad_doc || !settings_doc || !editbar_doc || !binds_doc || !etc_doc || !savedata_doc) {
        fprintf(stderr, "ui: 패드·설정 문서를 열지 못함\n");
        pad_doc = nullptr;
        return;
    }
    pad_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}

// 래퍼(게스트 코드 실행 중)가 여는 호스트 창은 여기로 미룬다. 설정 창을 여는 길은 눌린 패드 버튼을 떼는데(gamepad_release_all →
// host_key → guest_call), 게스트 틱 안에서 그러면 게스트를 다시 들어가 cpu 상태가 깨져 게임이 죽었다(패드로 「열기」를 누를 때만:
// 그때만 눌린 버튼이 있다. 마우스는 이벤트 루프에서 열려 괜찮았다).
static std::vector<std::string> later_actions;
void ui_action_later(const char *action) { if (action) later_actions.push_back(action); }

void ui_input_tick(void) {
    if (!later_actions.empty()) {
        std::vector<std::string> l;
        l.swap(later_actions);
        for (auto &a : l) do_action(a);
    }
    // 틱마다 하나씩. 같은 틱 안에 두 번 누르면 원본은 마지막 것만 읽는다.
    if (!keyq.empty() && frames != keyq_frame) {
        keyq_frame = frames;
        int k = keyq.front();
        keyq.pop_front();
        host_key_tap(k);
    }
}

static bool shift_held, shift_used; // 틀이 떠 있는 동안 눌린 왼쪽 Shift(아래 ui_event)

int ui_input_source(void) { int s = gamepad_source(); return s == SOURCE_PAD ? UI_SRC_PAD : s == SOURCE_TOUCH ? UI_SRC_TOUCH : UI_SRC_KEYBOARD; }
bool ui_doc_visible(const char *name) { return ctx && visible_doc(name) != nullptr; }

bool ui_event(const SDL_Event *e) {
    if (!ctx || !pad_doc) return false;
    bool modal = ui_modal();
    switch (e->type) {
    case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_MOTION: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED:
        // 설정 창은 RmlUi가 마우스 이벤트로 다룬다(터치도 SDL이 마우스로 바꿔 준다). 편집 중에는 손가락으로 묶음을 끈다.
        if (model.edit || !modal) finger_event(e);
        return true;
    case SDL_EVENT_MOUSE_MOTION: case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!modal) { // 게임 쪽 누르기는 손가락 이벤트(마우스도 SDL이 손가락으로 바꿔 준다)로 받는다. 여기서는 올려 두기만.
            // 마우스 오른쪽 버튼 = 이전(CLR). PC 게임 메뉴의 관례다. 메뉴 장면(14: 필드 메뉴·상점·타이틀 옵션)에서만 쓴다.
            // 필드에서는 CLR이 메뉴를 열어 버리므로 받지 않는다. SDL은 왼쪽 버튼만 손가락으로 바꿔 주므로 여기서 직접 본다.
            if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_RIGHT && e->button.which != SDL_TOUCH_MOUSEID &&
                (visible_doc("fm_shell") || game_scene() == 14)) {
                goal.on = false;
                queue_key(UI_KEY_CLEAR);
                return true;
            }
            if (e->type == SDL_EVENT_MOUSE_MOTION && e->motion.which != SDL_TOUCH_MOUSEID && !e->motion.state) {
                float mx = e->motion.x, my = e->motion.y;
                SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
                mouse_hover(mx - view_x, my - view_y);
            }
            return true;
        }
        float mx = e->type == SDL_EVENT_MOUSE_MOTION ? e->motion.x : e->button.x;
        float my = e->type == SDL_EVENT_MOUSE_MOTION ? e->motion.y : e->button.y;
        SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
        ctx->ProcessMouseMove((int)(mx - view_x), (int)(my - view_y), 0);
        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) ctx->ProcessMouseButtonDown(mouse_button(e->button.button), 0);
        else if (e->type == SDL_EVENT_MOUSE_BUTTON_UP) ctx->ProcessMouseButtonUp(mouse_button(e->button.button), 0);
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        if (modal) ctx->ProcessMouseWheel({0, -e->wheel.y}, 0);
        else {
            float mx = e->wheel.mouse_x, my = e->wheel.mouse_y;
            SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
            Rml::Element *sc = scroll_at(mx - view_x, my - view_y);
            if (!sc) sc = scroll_named(nullptr);
            if (sc) sc->SetScrollTop(sc->GetScrollTop() - e->wheel.y * 24 * ctx->GetDensityIndependentPixelRatio());
        }
        return true;
    case SDL_EVENT_WINDOW_RESIZED:
        // 창 모드에서 바꾼 크기를 기억한다. 전체화면으로 가는 중의 크기는 disp_full이 먼저 켜져 있어 남기지 않는다.
        if (!disp_full && ui_display_supported() && !(SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
            win_w = e->window.data1; win_h = e->window.data2; save_config();
        }
        return false;
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN: case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: {
        // 창 단추(맥의 초록 단추 등)로 바뀐 것도 따라간다.
        bool on = e->type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
        if (on != disp_full && ui_display_supported()) { disp_full = cfg_full = on; save_config(); }
        return false;
    }
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP:
        // 전체화면/창: F11, Alt+Enter(Windows 관례). 설정 창이 떠 있어도 받는다. Alt+Enter의 Enter는 게임으로 보내지 않는다.
        if (e->key.scancode == SDL_SCANCODE_F11 || ((e->key.scancode == SDL_SCANCODE_RETURN || e->key.scancode == SDL_SCANCODE_KP_ENTER) && (e->key.mod & SDL_KMOD_ALT))) {
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) ui_toggle_fullscreen();
            return true;
        }
        if (e->key.scancode == SDL_SCANCODE_F1) {
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) ui_toggle_settings();
            return true;
        }
        if (e->key.scancode == SDL_SCANCODE_F4) { // 일러스트 원작/리마스터
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) ui_toggle_illust();
            return true;
        }
        if (e->key.scancode == SDL_SCANCODE_F3) {
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) ui_toggle_etc();
            return true;
        }
        // 필드 메뉴 틀이 떠 있으면 Tab / Shift+Tab은 레일 탭 다음/이전, [ ]는 캐릭터 이전/다음(restored/fm_shell.c가 pick으로 받는다).
        // Shift+Tab을 누르려고 왼쪽 Shift를 누르면 원본 LSK(메인에서는 메뉴 닫기)로 새어 나갔다. 틀이 떠 있는 동안 왼쪽 Shift는
        // 뗄 때 보내고, 그 사이 Tab을 눌렀으면 보내지 않는다.
        if (!modal && e->key.scancode == SDL_SCANCODE_LSHIFT && (shift_held || visible_doc("fm_shell"))) {
            if (e->type == SDL_EVENT_KEY_DOWN) { if (!e->key.repeat) { shift_held = true; shift_used = false; } }
            else { if (shift_held && !shift_used) queue_key(UI_KEY_LSK); shift_held = false; }
            return true;
        }
        if (!modal && e->key.scancode == SDL_SCANCODE_TAB && visible_doc("fm_shell")) {
            if (e->type == SDL_EVENT_KEY_DOWN) {
                bool back = shift_held || (e->key.mod & SDL_KMOD_SHIFT);
                shift_used = shift_held;
                ui_push_pick("railstep", back ? 0 : 1);
            }
            return true;
        }
        if (!modal && (e->key.scancode == SDL_SCANCODE_LEFTBRACKET || e->key.scancode == SDL_SCANCODE_RIGHTBRACKET) && visible_doc("fm_shell")) {
            if (e->type == SDL_EVENT_KEY_DOWN) ui_push_pick("charstep", e->key.scancode == SDL_SCANCODE_RIGHTBRACKET);
            return true;
        }
        if (!modal && (e->key.scancode == SDL_SCANCODE_TAB || e->key.scancode == SDL_SCANCODE_LCTRL || e->key.scancode == SDL_SCANCODE_RCTRL)) {
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) { if (e->key.scancode == SDL_SCANCODE_TAB) ui_toggle_auto(); else ui_toggle_skip(); }
            return true;
        }
        // R 자동 전투, F 전투 2배속(전투 밖에서도 미리 켜 둘 수 있다). 원본 키 배치(123 QWE ASD ZXC)에 없는 글쇠다.
        if (!modal && (e->key.scancode == SDL_SCANCODE_R || e->key.scancode == SDL_SCANCODE_F)) {
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) { if (e->key.scancode == SDL_SCANCODE_R) ui_toggle_battle_auto(); else ui_toggle_battle_fast(); }
            return true;
        }
        if (!modal && (e->key.scancode == SDL_SCANCODE_PAGEUP || e->key.scancode == SDL_SCANCODE_PAGEDOWN ||
                       e->key.scancode == SDL_SCANCODE_HOME || e->key.scancode == SDL_SCANCODE_END)) {
            if (e->type == SDL_EVENT_KEY_DOWN) page_key(e->key.scancode);
            return true;
        }
        if (modal) {
            if (e->type == SDL_EVENT_KEY_DOWN) {
                SDL_Scancode s = e->key.scancode;
                if (s == SDL_SCANCODE_ESCAPE || s == SDL_SCANCODE_AC_BACK || s == SDL_SCANCODE_BACKSPACE) { if (!e->key.repeat) ui_toggle_settings(); }
                else if (s == SDL_SCANCODE_UP) ui_pad_nav(UI_NAV_UP);
                else if (s == SDL_SCANCODE_DOWN) ui_pad_nav(UI_NAV_DOWN);
                else if (s == SDL_SCANCODE_LEFT) ui_pad_nav(UI_NAV_LEFT);
                else if (s == SDL_SCANCODE_RIGHT) ui_pad_nav(UI_NAV_RIGHT);
                else if ((s == SDL_SCANCODE_RETURN || s == SDL_SCANCODE_SPACE) && !e->key.repeat) ui_pad_nav(UI_NAV_OK);
            }
            // 떼는 것은 게임에도 넘긴다: 환경설정 줄에서 Enter로 이 창을 열면(restored/cfgx.c) 누름은 게임이 받았고 뗌은 창이 뜬 뒤에 온다.
            // 삼키면 원본 입력에 OK가 눌린 채로 남는다. 짝 없는 뗌은 무해하다.
            if (e->type == SDL_EVENT_KEY_UP && e->key.scancode != SDL_SCANCODE_LSHIFT) return false;
            return e->key.scancode != SDL_SCANCODE_F11;
        }
        return false;
    default:
        return false;
    }
}

// 시험용: 문서에서 누를 것을 찾아 그 가운데를 누른다. `tap <선택자>`(예: `tap [nav=items][idx=3]`), 없으면 false.
bool ui_tap_selector(const char *sel) {
    if (!ctx) return false;
    for (auto it = shown_order.rbegin(); it != shown_order.rend(); ++it) {
        Doc *d = docs[*it].get();
        if (!d->visible || !d->el) continue;
        // 숨은 블록에도 같은 속성이 있을 수 있어 보이는 첫 요소를 고른다
        Rml::ElementList l;
        d->el->QuerySelectorAll(l, sel);
        for (Rml::Element *e : l) {
            if (!e->IsVisible(true) || !is_target(e)) continue;
            if (e->HasAttribute("hold")) continue; // 누르고 있기는 finger로 시험한다
            activate(d, e);
            return true;
        }
    }
    return false;
}

} // extern "C"
