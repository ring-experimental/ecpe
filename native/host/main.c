// 에픽크로니클PE 네이티브 진입점. SDL3 창, 입력(키보드, 터치, 게임패드), 시험용 제어 파일. 구조는 크로노스 윙 main.c와 같다.
// 사용법: ecpe [게임 데이터 폴더] [--windowed] [--touch] [--width <폭>] [--save <폴더>] [--ctrl <파일>] [--soundfont <sf2>]
#include "host.h"
#include "ui.h"
#include "gamepad.h"
void ext_toast_frame(void); // restored/items_ext.c
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

char game_dir[1024], save_dir[1024];
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static bool running = true;
static const char *ctrl_path;
uint64_t frames;
static bool frame_dirty;
static int width_arg;
static float scale_arg = 2; // 창 모드와 헤드리스의 배율(게임 화소 → 창 화소)
enum { ANDROID_INPUT_POLL_MS = 8 };

static bool game_exited;
unsigned game_generation; // 게임을 다시 시작할 때마다 는다. 래퍼가 지난 게임의 상태(퀵세이브 슬롯 등)를 버리는 데 쓴다
void request_exit(void) { running = false; game_exited = true; }

static bool save_exists(const char *name) {
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", save_dir, name);
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}

// Clet 네이티브 스텁(0x3fad4 등)이 부르는 C 함수. startClet(argc, argv), handleInput(종류, 키, 0).
// 종류 2=누름 3=뗌, 키는 위 -1, 아래 -2, 좌 -3, 우 -4, OK -5, 소프트 -6/-7, CLEAR -16, 숫자는 ASCII다.
// pauseClet 0x14414, resumeClet 0x14448, destroyClet 0x143b8, paintClet 0x144ac는 아직 부르지 않는다. 백그라운드에서는 타이머만 멈춘다.
static const uint32_t FN_START = 0x114308, FN_INPUT = 0x11447c;

// 높이 320을 유지하고 폭은 모니터 비율을 따른다(240~640). 화면은 늘 가로가 길다는 기준이라 창 모드도 같은 폭을 쓴다
// (예전에는 창 모드 기본이 원작 폭 240이어서 세로 화면이 되고 새 UI의 좁은 배치가 켜졌다). 240·320 같은 좁은 폭은 --width로만 준다(원작 대조 시험용).
// 게임은 GetDisplayInfo로 폭을 읽어 메뉴, 대사창, 필드를 스스로 넓게 그린다.
static int choose_width(void) {
    int w = width_arg;
#ifdef __EMSCRIPTEN__
    // 브라우저: 페이지 비율을 따른다. 폭은 시작할 때 한 번 정해지므로, 터치 기기를 세로로 든 채 시작해도
    // Android 빌드처럼 가로로 돌렸을 때의 비율(화면의 긴 변 / 짧은 변)로 잡는다(크로노스 윙 웹과 같다).
    if (!w) {
        int pw = EM_ASM_INT({ return window.innerWidth; }), ph = EM_ASM_INT({ return window.innerHeight; });
        int lng = EM_ASM_INT({ return Math.max(screen.width, screen.height); }), shrt = EM_ASM_INT({ return Math.min(screen.width, screen.height); });
        bool touch = EM_ASM_INT({ return navigator.maxTouchPoints > 0; });
        if (pw > ph && ph > 0) w = 320 * pw / ph;
        else if (shrt > 0) w = 320 * lng / shrt;
        else w = 568;
        (void)touch;
    }
#endif
    if (!w) {
        const SDL_DisplayMode *m = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
#ifdef __ANDROID__
        // 시작 순간에는 세로로 보고될 수 있어 긴 변을 폭으로 쓴다.
        w = m && m->h ? 320 * SDL_max(m->w, m->h) / SDL_min(m->w, m->h) : 568;
#else
        w = m && m->h ? 320 * m->w / m->h : 568; // 모니터를 모르면 16:9
#endif
    }
    if (w < 240) w = 240;
    if (w > 640) w = 640;
    return w & ~1;
}

// 대조용. ECPE_FRAMELOG=<폴더>: 표시한 프레임마다 log.txt에 `순번 해시 폭 높이`, 처음 본 프레임은 <해시>.raw(RGB565).
// ctrl 파일의 `framelog <폴더>`/`framelog off`로 도중에 켜고 끈다.
static char framelog_dir[1024];
static void frame_log(const uint8_t *px, int w, int h, int bpl) {
    static const char *dir;
    static int init;
    static uint32_t seen[65536];
    if (!init) { init = 1; dir = getenv("ECPE_FRAMELOG"); }
    static char cur[1024];
    if (framelog_dir[0]) {
        snprintf(cur, sizeof cur, "%s", framelog_dir);
        dir = strcmp(cur, "off") ? cur : NULL;
        framelog_dir[0] = 0;
    }
    if (!dir) return;
    uint32_t hash = 0x811c9dc5u;
    for (int y = 0; y < h; y++)
        for (int i = 0; i < w * 2; i++) hash = (hash ^ px[y * bpl + i]) * 16777619u;
    char path[1100];
    snprintf(path, sizeof path, "%s/log.txt", dir);
    FILE *o = fopen(path, "a");
    if (o) { fprintf(o, "%llu %08x %d %d\n", (unsigned long long)frames, hash, w, h); fclose(o); }
    uint32_t slot = hash & 0xffff;
    while (seen[slot] && seen[slot] != hash) slot = (slot + 1) & 0xffff;
    if (seen[slot]) return;
    seen[slot] = hash;
    snprintf(path, sizeof path, "%s/%08x.raw", dir, hash);
    o = fopen(path, "wb");
    if (!o) return;
    for (int y = 0; y < h; y++) fwrite(px + y * bpl, 2, w, o);
    fclose(o);
}

bool android_prepare_data(void);

static char wshot_path[1024];
// 게임 화면을 비율대로 맞춘다. 남는 곳은 검게 둔다.
static void render_frame(void) {
    int ow, oh;
    SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (tex) {
        float tw, th;
        SDL_GetTextureSize(tex, &tw, &th);
        float scale = SDL_min(ow / tw, oh / th);
        SDL_FRect dst = {(ow - tw * scale) / 2, (oh - th * scale) / 2, tw * scale, th * scale};
        SDL_RenderTexture(ren, tex, NULL, &dst);
        ui_render(dst.x, dst.y, dst.w, dst.h);
    }
    if (wshot_path[0]) { // 합친 화면(게임 + UI)을 그대로 남긴다
        SDL_Surface *s = SDL_RenderReadPixels(ren, NULL);
        if (s) { SDL_SavePNG(s, wshot_path); SDL_DestroySurface(s); }
        wshot_path[0] = 0;
    }
    SDL_RenderPresent(ren);
    frame_dirty = false;
}

void video_present(uint32_t fb) {
    cpu_t *c = &cpu;
    uint32_t f = handle_data(fb);
    int w = (int)R32(f), h = (int)R32(f + 4), bpl = (int)R32(f + 8);
    uint8_t *px = c->m + handle_data(R32(f + 16));
    frame_log(px, w, h, bpl);
    ts_present();
    ext_toast_frame(); // restored/items_ext.c: 토스트 문서(새 UI가 꺼져도 보인다)
    ui_present();
    frames++;
    if (!tex) {
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    }
    SDL_UpdateTexture(tex, NULL, px, bpl);
    frame_dirty = true;
}

static void write_shot(const char *path) {
    cpu_t *c = &cpu;
    uint32_t h = gfx_screen_handle();
    if (!h) return;
    uint32_t f = handle_data(h);
    int w = (int)R32(f), ht = (int)R32(f + 4), bpl = (int)R32(f + 8);
    uint8_t *px = c->m + handle_data(R32(f + 16));
    FILE *o = fopen(path, "wb");
    if (!o) return;
    fprintf(o, "P6\n%d %d\n255\n", w, ht);
    for (int y = 0; y < ht; y++)
        for (int x = 0; x < w; x++) {
            uint16_t p;
            memcpy(&p, px + y * bpl + x * 2, 2);
            uint8_t rgb[3] = {(uint8_t)(((p >> 11 & 31) * 255 + 15) / 31), (uint8_t)(((p >> 5 & 63) * 255 + 31) / 63), (uint8_t)(((p & 31) * 255 + 15) / 31)};
            fwrite(rgb, 1, 3, o);
        }
    fclose(o);
}

static void game_key(int type, int code) {
    uint32_t a[3] = {(uint32_t)type, (uint32_t)code, 0};
    guest_call(VF(FN_INPUT), a, 3);
}

// 새 UI의 키 별칭(ui_key_map)은 누를 때 정하고, 뗄 때는 누를 때 보낸 키를 뗀다(그 사이 화면이 바뀌어도 짝이 맞게).
// 방향 겹침: 원본 입력 객체는 누를 때와 뗄 때 모두 +8에 그 키를 적는다(0x61a0). 위를 누른 채 왼쪽을 누르고 위를 떼면
// +8이 "뗀 위"가 되어 계속 누르고 있는 왼쪽으로 걷지 않고 멈춘다(화면 패드에서 위→왼쪽으로 끌 때 끊기던 원인).
// 방향을 뗄 때 아직 눌린 다른 방향이 있으면 가장 나중에 누른 것을 다시 눌러 이어 준다(크로노스 윙 send_key와 같은 생각).
static bool is_dir(int k) { return k >= -4 && k <= -1; }
static void send_key(int type, int code) {
    static struct { int from, to; unsigned seq; } held[16];
    static unsigned seq;
    if (type == 2) {
        ui_note_human(); // 자동 전투가 사람 입력을 알아본다(restored/autobattle.c)
        int to = ui_key_map(code);
        if (ui_hot_key(to)) return;  // 문서의 hotkey 요소(ui.h)
        if (ui_wrap_key(to)) return; // 세로 목록 끝에서 돌기(ui.cpp). 원본에는 보내지 않는다(떼는 것도 짝 없이 무해하다)
        for (int i = 0; i < 16; i++) if (!held[i].from || held[i].from == code) { held[i].from = code; held[i].to = to; held[i].seq = ++seq; break; }
        game_key(type, to);
        return;
    }
    int to = code;
    bool found = false;
    for (int i = 0; i < 16; i++) if (held[i].from == code) { held[i].from = 0; to = held[i].to; found = true; break; }
    game_key(type, to);
    if (!found || !is_dir(to)) return;
    int last = -1;
    for (int i = 0; i < 16; i++) if (held[i].from && is_dir(held[i].to) && (last < 0 || held[i].seq > held[last].seq)) last = i;
    if (last >= 0) game_key(2, held[last].to);
}

// 터치 패드와 게임패드가 부른다(키보드와 같은 길).
void host_key(int type, int code) { if (code) send_key(type, code); }
// 문서를 눌러 만든 키(ui.cpp 대기열). 원본 키를 그대로 눌렀다 뗀다. 원본은 이 틱에 누른 키로 읽는다.
void host_key_tap(int code) { game_key(2, code); game_key(3, code); }

static int key_from_scancode(SDL_Scancode s) {
    switch (s) {
    case SDL_SCANCODE_AC_BACK: return -16;
    case SDL_SCANCODE_UP: return -1;
    case SDL_SCANCODE_DOWN: return -2;
    case SDL_SCANCODE_LEFT: return -3;
    case SDL_SCANCODE_RIGHT: return -4;
    case SDL_SCANCODE_SPACE: case SDL_SCANCODE_RETURN: return -5;
    case SDL_SCANCODE_LSHIFT: return -6;
    case SDL_SCANCODE_RSHIFT: return -7;
    case SDL_SCANCODE_BACKSPACE: case SDL_SCANCODE_ESCAPE: return -16;
    case SDL_SCANCODE_1: return '1';
    case SDL_SCANCODE_2: return '2';
    case SDL_SCANCODE_3: return '3';
    case SDL_SCANCODE_Q: return '4';
    case SDL_SCANCODE_W: return '5';
    case SDL_SCANCODE_E: return '6';
    case SDL_SCANCODE_A: return '7';
    case SDL_SCANCODE_S: return '8';
    case SDL_SCANCODE_D: return '9';
    case SDL_SCANCODE_Z: return '*';
    case SDL_SCANCODE_X: return '0';
    case SDL_SCANCODE_C: return '#';
    // 숫자 줄·숫자 키패드의 4~9, 0도 원작 숫자 키로 받는다(저들의영웅처럼 숫자를 치는 화면). 1~3은 위에서 이미 숫자다.
    case SDL_SCANCODE_4: case SDL_SCANCODE_KP_4: return '4';
    case SDL_SCANCODE_5: case SDL_SCANCODE_KP_5: return '5';
    case SDL_SCANCODE_6: case SDL_SCANCODE_KP_6: return '6';
    case SDL_SCANCODE_7: case SDL_SCANCODE_KP_7: return '7';
    case SDL_SCANCODE_8: case SDL_SCANCODE_KP_8: return '8';
    case SDL_SCANCODE_9: case SDL_SCANCODE_KP_9: return '9';
    case SDL_SCANCODE_0: case SDL_SCANCODE_KP_0: return '0';
    case SDL_SCANCODE_KP_1: return '1';
    case SDL_SCANCODE_KP_2: return '2';
    case SDL_SCANCODE_KP_3: return '3';
    case SDL_SCANCODE_KP_ENTER: return -5;
    default: return 0;
    }
}

static int key_from_name(const char *n) {
    static const struct { const char *name; int code; } K[] = {{"up",-1},{"down",-2},{"left",-3},{"right",-4},{"ok",-5},{"lsk",-6},{"rsk",-7},{"clear",-16},{"star",'*'},{"hash",'#'}};
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++) if (!strcmp(n, K[i].name)) return K[i].code;
    if (n[0] >= '0' && n[0] <= '9' && !n[1]) return n[0];
    return 0;
}

// ECPE_SCRIPT=<파일>: `<프레임> key|up|quit [이름]` 줄을 그만큼 표시한 뒤 실행한다.
static void script_poll(void) {
    static FILE *f;
    static int init, have;
    static unsigned long long at;
    static char cmd[16], name[16];
    if (!init) { init = 1; const char *p = getenv("ECPE_SCRIPT"); if (p) f = fopen(p, "r"); }
    while (f) {
        if (!have) {
            char line[64];
            if (!fgets(line, sizeof line, f)) { fclose(f); f = NULL; return; }
            name[0] = 0;
            if (sscanf(line, "%llu %15s %15s", &at, cmd, name) < 2) continue;
            have = 1;
        }
        if (at > frames) return;
        have = 0;
        int k = key_from_name(name);
        if (!strcmp(cmd, "key") && k) send_key(2, k);
        else if (!strcmp(cmd, "up") && k) send_key(3, k);
        else if (!strcmp(cmd, "quit")) running = false;
    }
}

// 진단: 화면 버퍼를 자홍색으로 덮는다. 그 뒤에도 자홍색이 남는 곳은 게임이 다시 그리지 않는 곳이다(넓은 화면 잔상 찾기).
static void poison_screen(void) {
    cpu_t *c = &cpu;
    uint32_t h = gfx_screen_handle();
    if (!h) return;
    uint32_t f = handle_data(h);
    int n = (int)R32(f + 4) * (int)R32(f + 8) / 2;
    uint16_t *px = (uint16_t *)(c->m + handle_data(R32(f + 16)));
    for (int i = 0; i < n; i++) px[i] = 0xf81f;
}

// 시험용. `finger down|move|up <id> <x> <y>`, `mouse down|move|up <x> <y>`. 좌표는 창 기준 0~1 비율이다.
static void inject_pointer(const char *line) {
    char what[16] = "", act[16] = "";
    int id = 0;
    float x = 0, y = 0;
    SDL_Event e;
    SDL_zero(e);
    if (sscanf(line, "%15s %15s", what, act) != 2) return;
    if (!strcmp(what, "finger") || !strcmp(what, "mfinger")) {
        if (sscanf(line, "%*s %*s %d %f %f", &id, &x, &y) != 3) return;
        e.type = !strcmp(act, "down") ? SDL_EVENT_FINGER_DOWN : !strcmp(act, "up") ? SDL_EVENT_FINGER_UP : SDL_EVENT_FINGER_MOTION;
        // mfinger: SDL이 마우스 왼쪽 버튼에서 만들어 주는 손가락(마우스 클릭 시험. 조작 장치를 터치로 바꾸지 않는다)
        e.tfinger.touchID = !strcmp(what, "mfinger") ? SDL_MOUSE_TOUCHID : 1;
        e.tfinger.fingerID = id + 1;
        e.tfinger.x = x;
        e.tfinger.y = y;
    } else {
        if (sscanf(line, "%*s %*s %f %f", &x, &y) != 2) return;
        int ww = 1, wh = 1;
        SDL_GetWindowSize(win, &ww, &wh);
        e.type = !strcmp(act, "down") ? SDL_EVENT_MOUSE_BUTTON_DOWN : !strcmp(act, "up") ? SDL_EVENT_MOUSE_BUTTON_UP : SDL_EVENT_MOUSE_MOTION;
        if (!strcmp(act, "rdown") || !strcmp(act, "rup")) { // 오른쪽 버튼
            e.type = act[1] == 'd' ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            e.button.button = SDL_BUTTON_RIGHT; e.button.x = x * ww; e.button.y = y * wh; e.button.down = act[1] == 'd';
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION) { e.motion.x = x * ww; e.motion.y = y * wh; }
        else { e.button.button = SDL_BUTTON_LEFT; e.button.x = x * ww; e.button.y = y * wh; e.button.down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN; }
    }
    SDL_PushEvent(&e);
}

// WIE_CTRL과 같은 형식: key <이름>, shot <ppm>, poison, quit. 줄 단위로 읽고 지운다.
static int release_code;
static uint64_t release_at;

static void ctrl_poll(void) {
    script_poll();
    if (release_code && now_ms() >= release_at) { send_key(3, release_code); release_code = 0; }
    if (!ctrl_path) return;
    // 읽는 사이에 덧붙는 줄을 잃지 않게 먼저 이름을 바꿔 떼어 낸 뒤 읽는다.
    char taken[1100];
    snprintf(taken, sizeof taken, "%s.taken", ctrl_path);
    if (rename(ctrl_path, taken)) return;
    FILE *f = fopen(taken, "r");
    if (!f) return;
    char line[1100];
    char lines[32][1100];
    int n = 0;
    while (n < 32 && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        strcpy(lines[n++], line);
    }
    fclose(f);
    remove(taken);
    for (int i = 0; i < n; i++) {
        if (!strncmp(lines[i], "key ", 4)) { // key <이름> [누르고 있을 ms]
            char nm[16] = ""; unsigned ms = 150;
            sscanf(lines[i] + 4, "%15s %u", nm, &ms);
            int k = key_from_name(nm);
            if (k) { if (release_code) send_key(3, release_code); send_key(2, k); release_code = k; release_at = now_ms() + ms; }
        } else if (!strncmp(lines[i], "shot ", 5)) write_shot(lines[i] + 5);
        else if (!strncmp(lines[i], "framelog ", 9)) snprintf(framelog_dir, sizeof framelog_dir, "%s", lines[i] + 9);
        else if (!strcmp(lines[i], "poison")) poison_screen();
        else if (!strncmp(lines[i], "wshot ", 6)) { snprintf(wshot_path, sizeof wshot_path, "%s", lines[i] + 6); frame_dirty = true; }
        else if (!strcmp(lines[i], "uireload")) { ui_reload(); frame_dirty = true; }
        else if (!strcmp(lines[i], "ui classic") || !strcmp(lines[i], "ui modern")) { ui_set_modern(lines[i][3] == 'm'); frame_dirty = true; }
        else if (!strncmp(lines[i], "calltree ", 9)) ts_request(lines[i] + 9);
        else if (!strncmp(lines[i], "memdump ", 8)) { // 진단: 게스트 메모리(0x100000~)를 파일로 남긴다
            FILE *o = fopen(lines[i] + 8, "wb");
            if (o) { fwrite(cpu.m + GUEST_LOW, 1, GUEST_MEM - GUEST_LOW, o); fclose(o); }
        }
        else if (!strncmp(lines[i], "loadbin ", 8)) { // 시험용: loadbin <게스트 주소(hex)> <파일> — 파일을 게스트 메모리에 그대로 쓴다(이벤트 스크립트 주입 등)
            unsigned addr = 0; char path[1024] = "";
            if (sscanf(lines[i] + 8, "%x %1023[^\n]", &addr, path) == 2 && addr >= GUEST_LOW && addr < GUEST_MEM) {
                FILE *o = fopen(path, "rb");
                if (o) { size_t got = fread(cpu.m + addr, 1, GUEST_MEM - addr, o); fclose(o); fprintf(stderr, "loadbin %#x %zu\n", addr, got); }
            }
        }
        else if (!strncmp(lines[i], "poke", 4)) { // 시험용: poke8|poke16|poke32 <게스트 주소(hex)> <값>
            unsigned bits = 0, addr = 0; long long v = 0;
            if (sscanf(lines[i], "poke%u %x %lli", &bits, &addr, &v) == 3 && addr >= GUEST_LOW && addr + 4 <= GUEST_MEM) {
                if (bits == 8) cpu.m[addr] = (uint8_t)v;
                else if (bits == 16) { uint16_t x = (uint16_t)v; memcpy(cpu.m + addr, &x, 2); }
                else if (bits == 32) { uint32_t x = (uint32_t)v; memcpy(cpu.m + addr, &x, 4); }
            }
        }
        else if (!strncmp(lines[i], "finger ", 7) || !strncmp(lines[i], "mfinger ", 8) || !strncmp(lines[i], "mouse ", 6)) inject_pointer(lines[i]);
        else if (!strncmp(lines[i], "padhot ", 7)) { // padhot <동작 id>: 그 동작의 패드 버튼을 누른 것으로 친다(문서의 padhot 요소 시험)
            if (!ui_pad_hot(lines[i] + 7)) fprintf(stderr, "padhot %s: 빌려 쓰는 요소 없음(원본 키로 감)\n", lines[i] + 7);
        }
        else if (!strncmp(lines[i], "shoulder ", 9)) { // shoulder 0|1|2|3: LB/RB/LT/RT를 누른 것으로 친다(기본 묶음 원본 키 0,7,1,3). 가상 패드 버튼이 안 들어와서
            int w = atoi(lines[i] + 9);
            static const int code[4] = {'0', '7', '1', '3'};
            if (w >= 0 && w < 4 && !ui_pad_shortcut(w, code[w])) fprintf(stderr, "shoulder %d: 원본 키로 보냄\n", w);
        }
        else if (!strncmp(lines[i], "scan ", 5)) { // scan [shift+]<SDL 글쇠 이름>: 키보드 글쇠를 그대로 누르고 뗀다(PageDown 등 원본 키가 아닌 글쇠 시험)
            const char *nm = lines[i] + 5;
            bool shift = !strncmp(nm, "shift+", 6);
            if (shift) nm += 6;
            SDL_Scancode sc = SDL_GetScancodeFromName(nm);
            if (sc == SDL_SCANCODE_UNKNOWN) fprintf(stderr, "scan: 모름 %s\n", lines[i] + 5);
            else for (int d = 1; d >= 0; d--) {
                SDL_Event e;
                SDL_zero(e);
                e.type = d ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                e.key.scancode = sc;
                e.key.down = d;
                if (shift) e.key.mod = SDL_KMOD_LSHIFT;
                SDL_PushEvent(&e);
            }
        }
        else if (!strncmp(lines[i], "tap ", 4)) { if (!ui_tap_selector(lines[i] + 4)) fprintf(stderr, "tap: 없음 %s\n", lines[i] + 4); }
        else if (!strncmp(lines[i], "source ", 7)) { // source keyboard|touch|pad: 키 이름표와 패드 표시를 그 장치로
            const char *v = lines[i] + 7;
            gamepad_set_source(!strcmp(v, "touch") ? SOURCE_TOUCH : !strcmp(v, "pad") ? SOURCE_PAD : SOURCE_KEYBOARD);
            frame_dirty = true;
        }
        else if (!strcmp(lines[i], "settings")) { ui_toggle_settings(); frame_dirty = true; }
        else if (!strcmp(lines[i], "savedata")) { ui_toggle_savedata(); frame_dirty = true; } // 세이브 데이터 창(파일 창은 ECPE_SAVEFILE_TEST로 건너뛴다)
        else if (!strncmp(lines[i], "gcall ", 6)) { // 시험용: gcall <파일 오프셋 hex> [인자 hex...] — 틱 사이에 게스트 함수를 부른다(상점 열기 등)
            uint32_t args[8] = {0}, fn = 0;
            char *p = lines[i] + 6, *end;
            fn = (uint32_t)strtoul(p, &end, 16);
            int n = 0;
            for (p = end; n < 8; p = end) { uint32_t v = (uint32_t)strtoul(p, &end, 16); if (end == p) break; args[n++] = v; }
            if (fn) { uint32_t r = guest_call(VF(0x100000 + fn), args, n); fprintf(stderr, "gcall %#x -> %#x\n", fn, r); }
        }
        else if (gamepad_ctrl(lines[i])) {}
        else if (!strcmp(lines[i], "quit")) running = false;
    }
}

static void load_image(void) {
    cpu_t *c = &cpu;
    char path[1100];
    snprintf(path, sizeof path, "%s/client.bin", game_dir);
    FILE *f = fopen(path, "rb");
    if (!f) { SDL_Log("%s 없음", path); exit(1); }
    size_t n = fread(c->m + IMAGE_LOAD, 1, 0x100000, f);
    fclose(f);
    // 판을 고른다(host/ver.h). restored/는 기준 판 주소로 쓰여 있고 다른 판에서는 대응표로 바꾼다.
    if (!ver_select(c->m + IMAGE_LOAD, (uint32_t)n)) { SDL_Log("모르는 판의 client.bin(%zu바이트)", n); exit(1); }
    fprintf(stderr, "판 %s\n", ver_name());
    // 원본 시작 코드(0xc)가 하는 재배치와 같다. BSS는 새로 받은 페이지라 이미 0이다.
    uint32_t relocs = R32(IMAGE_LOAD + 0x70), base = IMAGE_LOAD + 0x74 + relocs * 4;
    for (uint32_t i = 0; i < relocs; i++) {
        uint32_t o = base + R32(IMAGE_LOAD + 0x74 + i * 4);
        W32(o, R32(o) + base);
    }
    uint32_t got0 = R32(base), got1 = R32(base + 4); // 머리 값도 위에서 재배치됐다.
    for (uint32_t o = got0; o < got1; o += 4) W32(o, R32(o) + base);
    fprintf(stderr, "이미지 %zu바이트 적재, 기준 %#x, GOT %#x-%#x\n", n, base, got0, got1);
}

static void start_game(void) {
    cpu.SP = STACK_TOP - 16;
    load_image();
    heap_init();
    api_install();
    uint32_t start[2] = {0, 0};
    guest_call(VF(FN_START), start, 2);
}

// 게스트 메모리와 호스트 쪽 게임 상태를 비우고 startClet부터 다시 부른다.
static void restart_game(void) {
    game_generation++;
    audio_reset();
    timers_reset();
    api_reset();
    gfx_reset();
    guest_mem_clear();
    memset(cpu.r, 0, sizeof cpu.r);
    game_exited = false;
    start_game();
}

// Android의 SDL_WaitEventTimeout은 잠들지 않고 시간이 될 때까지 돌아 코어 하나를 다 쓴다. 직접 재우고 입력은 짧은 간격으로 본다.
static bool wait_event(SDL_Event *e, int ms) {
#ifdef __ANDROID__
    if (SDL_PollEvent(e)) return true;
    if (ms > ANDROID_INPUT_POLL_MS) ms = ANDROID_INPUT_POLL_MS;
    if (ms > 0) SDL_Delay(ms);
    return SDL_PollEvent(e);
#else
    return SDL_WaitEventTimeout(e, ms);
#endif
}

static bool paused, first_run;

// 메인 루프 한 바퀴. 데스크톱은 입력이나 다음 타이머까지 자고, 브라우저는 화면 주기마다 한 번 부른다(web_tick).
static void tick(void) {
    SDL_Event e;
    int64_t wait = timers_next_ms();
    // 시험용 제어 파일만 짧게 폴링한다. 평소에는 입력이나 다음 타이머까지 잔다.
    int64_t max_wait = ctrl_path ? 10 : 100;
    if (ui_active()) { max_wait = 16; frame_dirty = true; } // UI 애니메이션
    if (wait > max_wait || wait < 0) wait = max_wait;
#ifdef __EMSCRIPTEN__
    wait = 0; // 브라우저에서는 기다리지 않고 돌아간다.
#endif
    if (wait_event(&e, (int)wait)) {
        do {
            if (e.type == SDL_EVENT_QUIT) running = false;
            else if (e.type == SDL_EVENT_WILL_ENTER_BACKGROUND || e.type == SDL_EVENT_DID_ENTER_FOREGROUND) {
                bool bg = e.type == SDL_EVENT_WILL_ENTER_BACKGROUND;
                if (bg != paused) { paused = bg; audio_pause(paused); }
                if (bg) { ui_release_all(); gamepad_release_all(); }
            } else if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST) frame_dirty = true;
            if (gamepad_event(&e)) { frame_dirty = true; continue; }
            if (ui_event(&e)) { frame_dirty = true; continue; }
            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !e.key.repeat) {
                if (e.key.scancode == SDL_SCANCODE_F2 && e.type == SDL_EVENT_KEY_DOWN) { // 원작 그리기와 바꿔 보기
                    ui_set_modern(!ui_modern());
                    continue;
                }
                if (e.key.scancode == SDL_SCANCODE_F5 && e.type == SDL_EVENT_KEY_DOWN) { // 퀵세이브(필드 HUD, restored/ui_dialog.c)
                    ui_push_pick("qsave", 0);
                    continue;
                }
                int k = key_from_scancode(e.key.scancode);
                if (k) send_key(e.type == SDL_EVENT_KEY_DOWN ? 2 : 3, k);
            }
        } while (SDL_PollEvent(&e));
    }
    ctrl_poll();
    if (savefile_poll()) { // 세이브 불러오기가 끝났다: 슬롯 머리를 다시 읽게 타이틀부터 다시 시작한다
        fprintf(stderr, "세이브 불러오기 끝: 게임을 다시 시작\n");
        restart_game();
        running = true;
    }
    gamepad_tick();
    ui_input_tick();
    if (!paused) timers_run();
    if (frame_dirty) render_frame();
    if (!running && game_exited && first_run && save_exists("gopt.sav")) {
        fprintf(stderr, "첫 실행 최적화 끝: 게임을 다시 시작\n");
        first_run = false;
        restart_game();
        running = true;
    }
}

#ifdef __EMSCRIPTEN__
// 저장 폴더(/persist, IndexedDB)를 내려 쓴다. 바뀐 파일만 쓰이므로 주기적으로 부른다. now면 주기를 기다리지 않는다.
static void web_save_sync(bool now) {
    static uint64_t last;
    uint64_t t = SDL_GetTicks();
    if (!now && t - last < 2000) return;
    last = t;
    EM_ASM({ if (Module.saveSync) Module.saveSync(); });
}

// 게임이 종료를 고르면 저장을 내려 쓴 뒤 페이지에 알린다(web/app.js의 Module.onGameExit).
static void web_tick(void) {
    tick();
    web_save_sync(false);
    if (running) return;
    emscripten_cancel_main_loop();
    web_save_sync(true);
    EM_ASM({ if (Module.onGameExit) Module.onGameExit(); });
}
#endif

int main(int argc, char **argv) {
    bool windowed = false, force_pad = false;
    const char *soundfont = NULL, *save = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--touch")) force_pad = true;
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--width") && i + 1 < argc) width_arg = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale_arg = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--classic")) ui_set_modern(false);
        else if (!strcmp(argv[i], "--ctrl") && i + 1 < argc) ctrl_path = argv[++i];
        else if (!strcmp(argv[i], "--soundfont") && i + 1 < argc) soundfont = argv[++i];
        else snprintf(game_dir, sizeof game_dir, "%s", argv[i]);
    }
    extern FILE *api_trace;
    if (getenv("ECPE_TRACE")) api_trace = fopen(getenv("ECPE_TRACE"), "w");
#ifdef __ANDROID__
    windowed = false;
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
    // ECPE_HEADLESS=1: 창을 숨기고 소리 없이 돈다(자동 시험용). 그리기는 평소와 같은 렌더러라 shot/wshot이 그대로 나온다.
    bool headless = getenv("ECPE_HEADLESS") != NULL;
    if (headless) { SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"); windowed = true; }
    // 마우스 누르기도 손가락 이벤트로 받는다(메뉴 누르기를 데스크톱에서도 쓴다). 터치는 SDL이 마우스로도 바꿔 준다(설정 창).
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
#ifdef __EMSCRIPTEN__
    // 키는 캔버스에 포커스가 있을 때만 받는다. 페이지의 입력 칸에 치는 키가 게임으로 새지 않게 한다.
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }
#ifdef __ANDROID__
    if (!android_prepare_data()) { SDL_Log("게임 데이터 풀기 실패: %s", SDL_GetError()); return 1; }
#endif
    // 인자가 없으면 실행 파일 옆의 game 폴더를 쓴다.
    if (!game_dir[0]) snprintf(game_dir, sizeof game_dir, "%sgame", SDL_GetBasePath());
    screen_w = choose_width();
    if (save) snprintf(save_dir, sizeof save_dir, "%s", save);
    else { char *p = SDL_GetPrefPath("ecpe", "save"); snprintf(save_dir, sizeof save_dir, "%s", p); SDL_free(p); }
    size_t l = strlen(save_dir);
    if (l && (save_dir[l - 1] == '/' || save_dir[l - 1] == '\\')) save_dir[l - 1] = 0; // Windows 경로는 \로 끝난다
    SDL_CreateDirectory(save_dir);
    char ui_cfg[1100];
    snprintf(ui_cfg, sizeof ui_cfg, "%s/ui.ini", save_dir);

    // 화면 모드: --windowed가 없으면 ui.ini에 남은 마지막 모드(F11, Alt+Enter, 환경설정 「화면」)와 창 크기를 쓴다.
    int ww = (int)(screen_w * scale_arg), wh = (int)(320 * scale_arg);
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    if (!headless) {
        bool full = !windowed;
        int sw = 0, sh = 0;
        if (ui_peek_display(ui_cfg, &full, &sw, &sh) && !windowed) windowed = !full;
        if (sw && sh) { ww = sw; wh = sh; }
    }
#endif
#ifdef __EMSCRIPTEN__
    // 페이지를 채우고, 캔버스를 화면 픽셀로 만든다(없으면 작은 캔버스를 브라우저가 한 번 더 늘려 도트가 고르지 않게 뭉개진다).
    // 전체화면은 페이지가 맡는다(web/app.js).
    win = SDL_CreateWindow("에픽크로니클 PE", ww, wh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_FILL_DOCUMENT | SDL_WINDOW_HIGH_PIXEL_DENSITY);
#else
    win = SDL_CreateWindow("에픽크로니클 PE", ww, wh,
                           headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE | (windowed ? 0 : SDL_WINDOW_FULLSCREEN));
#endif
    ren = win ? SDL_CreateRenderer(win, NULL) : NULL;
    if (!ren) {
        SDL_Log("창 생성 실패: %s", SDL_GetError());
        return 1;
    }
    if (!headless) SDL_SetRenderVSync(ren, 1);
    char font_path[1100];
    snprintf(font_path, sizeof font_path, "%s/../font.gly", game_dir);
#ifdef __EMSCRIPTEN__
    snprintf(font_path, sizeof font_path, "/font.gly"); // 배포 꾸러미(ecpe.data)
#endif
    if (!font_load(font_path)) SDL_Log("글꼴 표를 열지 못함: 글자 없이 진행");

    // 메뉴를 대신 그리는 UI. 문서는 ECPE_UI_DIR(개발용) 또는 게임 폴더 옆 ui/에서 읽는다.
    char ui_path[1100];
    if (getenv("ECPE_UI_DIR")) snprintf(ui_path, sizeof ui_path, "%s", getenv("ECPE_UI_DIR"));
    else snprintf(ui_path, sizeof ui_path, "%s/../ui", game_dir);
#ifdef __EMSCRIPTEN__
    snprintf(ui_path, sizeof ui_path, "/ui");
#endif
    if (!ui_init(ren, ui_path)) SDL_Log("UI를 열지 못함(%s): 원작 그리기로 진행", ui_path);
    // 터치 패드와 조작 설정. 설정은 저장 폴더의 ui.ini(패드 크기·위치·투명도, 게임패드 맵핑).
    gamepad_init();
    ui_init_input(win, ui_cfg, force_pad);

    guest_mem_alloc();
    audio_init(soundfont);
    // 첫 실행은 원작대로 "속도최적화" 뒤 gopt.sav를 남기고 재시작을 요청하며 끝난다. 그 종료만 다시 띄워 이어 간다.
    first_run = !save_exists("gopt.sav");
    start_game();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(web_tick, 0, true);
#endif
    while (running) tick();
    extern float audio_peak;
    fprintf(stderr, "종료, 프레임 %llu, 소리 최대값 %.3f\n", (unsigned long long)frames, audio_peak);
    SDL_Quit();
    return 0;
}
