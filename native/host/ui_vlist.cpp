// 세로 목록 부품 <vlist>. 문서(.rml)를 읽을 때 파일 층에서 보통 RML로 펼친다(RmlUi에는 문서 안에서 쓰는 부품이 없다).
// 모든 세로 목록이 같은 줄 모양·선택 표시·스크롤·빈 목록 글을 갖게 하려는 것이다. 쓰는 법과 속성은 ui/README.md '세로 목록 <vlist>'.
//
//   <vlist for="it : d.items" nav="items" idx="it.i" sel="it.sel" icon="it.icon" label="it.name" value="'×' + it.count"
//          scroll="ilist" empty="d.total == 0" empty-text="-"/>
//
// icon을 주면 줄 왼쪽에 아이콘 칸이 생기고, 안 주면 글만 나온다(도움말 목차 등). 속성 값은 data 식 그대로다(따옴표 안 글은 '...').
// <vlist ...>안쪽</vlist>로 쓰면 안쪽 내용이 이름과 값 사이에 들어간다(줄마다 더 보일 것이 있을 때).
#include <RmlUi/Core.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>

namespace {

using Attrs = std::map<std::string, std::string>;

// <vlist 뒤의 속성들을 읽는다. 값은 XML에 적힌 그대로(&amp; 등 그대로) 둔다. 펼친 결과도 XML로 다시 읽히기 때문이다.
size_t parse_attrs(const std::string &s, size_t p, Attrs &out, bool &self_close) {
    self_close = false;
    while (p < s.size()) {
        while (p < s.size() && isspace((unsigned char)s[p])) p++;
        if (p >= s.size()) break;
        if (s[p] == '/' && p + 1 < s.size() && s[p + 1] == '>') { self_close = true; return p + 2; }
        if (s[p] == '>') return p + 1;
        size_t k = p;
        while (p < s.size() && s[p] != '=' && !isspace((unsigned char)s[p]) && s[p] != '>' && s[p] != '/') p++;
        std::string key = s.substr(k, p - k);
        while (p < s.size() && isspace((unsigned char)s[p])) p++;
        if (p < s.size() && s[p] == '=') {
            p++;
            while (p < s.size() && isspace((unsigned char)s[p])) p++;
            char q = s[p];
            if (q != '"' && q != '\'') { out[key] = ""; continue; }
            size_t v = ++p;
            while (p < s.size() && s[p] != q) p++;
            out[key] = s.substr(v, p - v);
            p++;
        } else out[key] = "";
    }
    return p;
}

std::string get(const Attrs &a, const char *k) {
    auto it = a.find(k);
    return it == a.end() ? std::string() : it->second;
}
bool has(const Attrs &a, const char *k) { return a.count(k) != 0; }

std::string expand_one(const Attrs &a, const std::string &inner) {
    std::string o;
    // 상자: 크기(s 작은 줄, m 보통, l 두 줄 카드), 아이콘 유무, 스크롤
    std::string cls = "vlist";
    std::string size = get(a, "size");
    cls += " v-" + (size.empty() ? std::string("m") : size);
    if (has(a, "icon")) cls += " v-icons";
    if (has(a, "class")) cls += " " + get(a, "class");
    o += "<div class=\"" + cls + "\"";
    if (has(a, "id")) o += " id=\"" + get(a, "id") + "\"";
    if (has(a, "scroll")) o += " scroll=\"" + get(a, "scroll") + "\"";
    if (has(a, "if")) o += " data-if=\"" + get(a, "if") + "\"";
    if (has(a, "box-class")) o += " " + get(a, "box-class"); // data-class-... 같은 상자 속성을 그대로
    o += ">";
    // 줄
    o += "<div class=\"vrow\" data-for=\"" + get(a, "for") + "\"";
    if (has(a, "nav")) o += " nav=\"" + get(a, "nav") + "\"";
    if (has(a, "pick")) o += " pick=\"" + get(a, "pick") + "\"";
    if (has(a, "idx")) o += " data-attr-idx=\"" + get(a, "idx") + "\"";
    if (has(a, "key")) o += " data-attr-key=\"" + get(a, "key") + "\"";
    if (has(a, "sel")) o += " data-class-sel=\"" + get(a, "sel") + "\"";
    if (has(a, "idle")) o += " data-class-idle=\"" + get(a, "idle") + "\""; // 포커스가 다른 창에 있을 때의 선택 줄
    if (has(a, "dis")) o += " data-class-dis=\"" + get(a, "dis") + "\"";     // 고를 수는 있지만 쓸 수 없는 줄(흐리게)
    if (has(a, "off")) o += " data-class-off=\"" + get(a, "off") + "\"";     // 감추는 줄(빈 칸)
    if (has(a, "row-class")) o += " " + get(a, "row-class");                 // 화면만의 data-class-...
    o += ">";
    if (has(a, "icon")) {
        std::string ic = get(a, "icon");
        o += "<span class=\"vic\" data-if=\"" + ic + " != ''\"><img data-attr-src=\"" + ic + "\"/></span>"; // 이 줄에 아이콘 값이 없으면 칸도 없다(글만)
    }
    if (has(a, "lead")) // 이름 앞의 짧은 표(번호, 단축키)
        o += "<span class=\"vlead\">{{" + get(a, "lead") + "}}</span>";
    o += "<span class=\"vtx\"><span class=\"vlabel\">{{" + get(a, "label") + "}}</span>";
    if (has(a, "sub")) o += "<span class=\"vsub\">{{" + get(a, "sub") + "}}</span>";
    o += "</span>";
    if (has(a, "badge")) {
        std::string b = get(a, "badge");
        o += "<span class=\"vbadge\" data-if=\"" + b + " != ''\">{{" + b + "}}</span>";
    }
    o += inner;
    if (has(a, "value")) {
        o += "<span class=\"vval";
        if (has(a, "value-class")) o += " " + get(a, "value-class");
        o += "\">{{" + get(a, "value") + "}}</span>";
    }
    o += "</div>";
    if (has(a, "empty"))
        o += "<div class=\"vempty\" data-if=\"" + get(a, "empty") + "\">" + get(a, "empty-text") + "</div>";
    o += "</div>";
    return o;
}

} // namespace

// 문서 글에서 <vlist>를 모두 펼친다. 하나라도 있으면 <head> 바로 뒤에 vlist.rcss를 건다.
std::string ui_expand_vlist(const std::string &src, const std::string &path) {
    if (src.find("<vlist") == std::string::npos) return src;
    std::string out;
    size_t p = 0;
    while (true) {
        size_t s = src.find("<vlist", p);
        // 주석과 <style> 안의 글자 "<vlist"는 펼치지 않고 그대로 옮긴다(설명 주석에 부품 이름을 적으면 문서가 깨졌다).
        size_t cm = src.find("<!--", p), st = src.find("<style", p);
        size_t skip = std::min(cm, st);
        if (skip != std::string::npos && (s == std::string::npos || skip < s)) {
            size_t end = skip == cm ? src.find("-->", skip) : src.find("</style>", skip);
            end = end == std::string::npos ? src.size() : end + (skip == cm ? 3 : 8);
            out += src.substr(p, end - p);
            p = end;
            continue;
        }
        if (s == std::string::npos || (s + 6 < src.size() && !isspace((unsigned char)src[s + 6]) && src[s + 6] != '/' && src[s + 6] != '>')) {
            if (s == std::string::npos) { out += src.substr(p); break; }
            out += src.substr(p, s + 6 - p); p = s + 6; continue;
        }
        out += src.substr(p, s - p);
        Attrs a;
        bool self_close;
        size_t e = parse_attrs(src, s + 6, a, self_close);
        std::string inner;
        if (!self_close) {
            size_t c = src.find("</vlist>", e);
            if (c == std::string::npos) { fprintf(stderr, "ui: %s: </vlist>가 없다\n", path.c_str()); c = src.size(); inner = src.substr(e); e = c; }
            else { inner = src.substr(e, c - e); e = c + 8; }
        }
        if (!has(a, "for") || !has(a, "label")) fprintf(stderr, "ui: %s: <vlist>에 for와 label이 있어야 한다\n", path.c_str());
        out += expand_one(a, inner);
        p = e;
    }
    size_t h = out.find("<head>");
    if (h != std::string::npos) out.insert(h + 6, "<link type=\"text/rcss\" href=\"vlist.rcss\"/>");
    if (getenv("ECPE_VLIST_DUMP")) fprintf(stderr, "---- %s\n%s\n", path.c_str(), out.c_str());
    return out;
}

// 필드 메뉴 고정 버튼 띠 <fmbar/>(ui/README.md '필드 메뉴 틀'). 화면마다 같은 자리·크기의 띠를 같은 마크업으로 두려고 펼친다.
// 값은 restored/fm_shell.c의 fm_bar_*가 d.bar에 채운다: ex(보조 버튼 고정 칸 수, t 이름·on 쓸 수 있음·kl 키 이름표), ok, back.
// 오른쪽 끝 「이전」(back)과 그 왼쪽 「결정」(ok)은 늘 같은 자리·크기이고, 쓸 수 없으면 사라지지 않고 흐려진다(.off, 누를 수 없음).
// 누르면 pick "fmbar"(보조 0~, 결정 8, 이전 9)로 fm_shell.c가 받는다.
namespace {
std::string fmbar_markup();
std::string fmbar_btn(const std::string &v, const std::string &idx) {
    std::string lab = "<span class=\"fbt\">{{" + v + ".t}}</span><span class=\"fbk\" data-if=\"" + v + ".kl != ''\">{{" + v + ".kl | key}}</span>";
    return "<div class=\"fb\" data-if=\"" + v + ".on\" pick=\"fmbar\" " + idx + " data-attr-keyhint=\"" + v + ".kl\">" + lab + "</div>"
           "<div class=\"fb off\" data-if=\"!" + v + ".on\">" + lab + "</div>";
}
std::string ui_expand_fmbar(const std::string &src) {
    if (src.find("<fmbar/>") == std::string::npos) return src;
    // 주석 안의 "<fmbar/>" 글자(설명)는 그대로 둔다
    std::string out;
    size_t p = 0;
    while (true) {
        size_t s = src.find("<fmbar/>", p), cm = src.find("<!--", p);
        if (cm != std::string::npos && (s == std::string::npos || cm < s)) {
            size_t e = src.find("-->", cm);
            e = e == std::string::npos ? src.size() : e + 3;
            out += src.substr(p, e - p);
            p = e;
            continue;
        }
        if (s == std::string::npos) { out += src.substr(p); return out; }
        out += src.substr(p, s - p) + fmbar_markup();
        p = s + 8;
    }
}
std::string fmbar_markup() {
    return "<div class=\"fmbar\"><div class=\"fbex\"><div class=\"fbw\" data-for=\"b : d.bar.ex\" data-class-hid=\"b.t == ''\">" +
                    fmbar_btn("b", "data-attr-idx=\"it_index\"") + "</div><span class=\"fbh\" data-if=\"d.bar.hint != ''\">{{d.bar.hint}}</span></div>"
                    "<div class=\"fbw fok\">" + fmbar_btn("d.bar.ok", "idx=\"8\"") + "</div>"
                    "<div class=\"fbw fback\">" + fmbar_btn("d.bar.back", "idx=\"9\"") + "</div></div>";
}
} // namespace

// 파일 층: .rml만 펼쳐 메모리에서 읽히고, 나머지는 그대로 파일에서 읽는다.
namespace {
struct Mem { std::string data; size_t pos = 0; FILE *f = nullptr; };
struct Files final : Rml::FileInterface {
    Rml::FileHandle Open(const Rml::String &path) override {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) return 0;
        auto *m = new Mem;
        bool rml = path.size() > 4 && path.compare(path.size() - 4, 4, ".rml") == 0;
        if (!rml) { m->f = f; return (Rml::FileHandle)m; }
        char buf[16384];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) m->data.append(buf, n);
        fclose(f);
        m->data = ui_expand_vlist(ui_expand_fmbar(m->data), path);
        return (Rml::FileHandle)m;
    }
    void Close(Rml::FileHandle h) override {
        auto *m = (Mem *)h;
        if (m->f) fclose(m->f);
        delete m;
    }
    size_t Read(void *buf, size_t size, Rml::FileHandle h) override {
        auto *m = (Mem *)h;
        if (m->f) return fread(buf, 1, size, m->f);
        size_t n = std::min(size, m->data.size() - m->pos);
        memcpy(buf, m->data.data() + m->pos, n);
        m->pos += n;
        return n;
    }
    bool Seek(Rml::FileHandle h, long off, int origin) override {
        auto *m = (Mem *)h;
        if (m->f) return fseek(m->f, off, origin) == 0;
        long base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? (long)m->pos : (long)m->data.size();
        long np = base + off;
        if (np < 0 || np > (long)m->data.size()) return false;
        m->pos = (size_t)np;
        return true;
    }
    size_t Tell(Rml::FileHandle h) override {
        auto *m = (Mem *)h;
        return m->f ? (size_t)ftell(m->f) : m->pos;
    }
};
} // namespace

Rml::FileInterface *ui_file_interface() {
    static Files f;
    return &f;
}
