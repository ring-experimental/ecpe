// 세이브 데이터 내보내기/불러오기(사용자 요청, 원작에 없다). 환경설정의 「세이브 데이터」 버튼이 여는 호스트 창(ui/savedata.rml)에서 쓴다.
// 구조는 크로노스 윙 native/host/savefile.c와 같다: 파일 고르기만 운영체제 창을 쓰고 안내·경고는 게임 UI(host/ui.cpp)가 띄운다.
//   내보내기: 저장 폴더의 슬롯 파일(slot<n>.sav)과 슬롯별 확장 저장(ext<n>.ini)을 파일 하나(.ecsave)로 묶어 고른 자리에 쓴다.
//   불러오기: 묶음을 검증한 뒤 경고 창에서 확인을 받으면 지금 슬롯을 전부 지우고(저장 폴더 backup/으로 옮긴다) 묶음의 슬롯으로 바꾼다.
//             묶음에 없는 슬롯은 빈 슬롯이 된다. 게임이 슬롯 머리를 메모리에 들고 있으므로 끝나면 게임을 타이틀부터 다시 시작한다.
// 환경설정(gopt.sav)과 조작·기타 설정(ui.ini)은 기기 설정이라 옮기지 않는다.
// 묶음 형식: 식별값 8바이트, 파일 수 u32, (이름 16바이트, 크기 u32, 내용)*, 앞 전체의 FNV-1a 64비트. 정수는 리틀 엔디언.
// 시험: ECPE_SAVEFILE_TEST=<파일>이면 파일 창 없이 그 경로를 쓴다.
#include "host.h"
#include "ui.h"
#include <SDL3/SDL.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char MAGIC[8] = "ECPESAV1";
#define NAME_LEN 16
#define MAX_FILE (256 * 1024)
#define MAX_ENTRIES 20 // slot0~9.sav + ext0~9.ini

// 슬롯에 딸린 파일 이름인가: slot<숫자>.sav(kind 0) 또는 ext<숫자>.ini(kind 1).
static int slot_file_kind(const char *name) {
    static const struct { const char *pre, *suf; } K[] = {{"slot", ".sav"}, {"ext", ".ini"}};
    for (int k = 0; k < 2; k++) {
        size_t p = strlen(K[k].pre);
        if (!strncmp(name, K[k].pre, p) && isdigit((unsigned char)name[p]) && !strcmp(name + p + 1, K[k].suf)) return k;
    }
    return -1;
}

static uint64_t fnv(const uint8_t *p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

static void path_of(char *out, size_t cap, const char *dir, const char *name) { snprintf(out, cap, "%s%s/%s", save_dir, dir, name); }

// 저장 폴더의 슬롯 파일 이름들(이름순). 돌려준 수만큼 names[i]에 담는다.
static int list_slot_files(char names[MAX_ENTRIES][NAME_LEN]) {
    int n = 0, count = 0;
    char **all = SDL_GlobDirectory(save_dir, NULL, 0, &count);
    for (int i = 0; all && i < count && n < MAX_ENTRIES; i++)
        if (slot_file_kind(all[i]) >= 0) snprintf(names[n++], NAME_LEN, "%s", all[i]);
    SDL_free(all);
    qsort(names, (size_t)n, NAME_LEN, (int (*)(const void *, const void *))strcmp);
    return n;
}

// SDL_SaveFile은 열기에 실패해도, 닫으며 버퍼를 못 써도 성공을 돌려줄 수 있다. 직접 열고 닫는 결과까지 본다.
static bool write_all(const char *path, const void *data, size_t len) {
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) return false;
    bool ok = SDL_WriteIO(io, data, len) == len;
    if (!SDL_CloseIO(io)) ok = false;
    return ok;
}

static void date_name(char *out, size_t cap, const char *dir, bool with_time) {
    SDL_Time now = 0;
    SDL_DateTime t = {0};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &t, true);
    if (with_time) snprintf(out, cap, "%secpe-save-%04d-%02d-%02d-%02d%02d%02d.ecsave", dir, t.year, t.month, t.day, t.hour, t.minute, t.second);
    else snprintf(out, cap, "%secpe-save-%04d-%02d-%02d.ecsave", dir, t.year, t.month, t.day);
}

// ---- 내보내기 ----
int savefile_slot_count(void) {
    char names[MAX_ENTRIES][NAME_LEN];
    int n = list_slot_files(names), slots = 0;
    for (int i = 0; i < n; i++) if (slot_file_kind(names[i]) == 0) slots++;
    return slots;
}

static void export_to(const char *path) {
    char names[MAX_ENTRIES][NAME_LEN];
    int n = list_slot_files(names), slots = 0;
    size_t len = 12;
    uint8_t *out = malloc(len + 8);
    uint32_t count = 0;
    memcpy(out, MAGIC, 8);
    for (int i = 0; i < n; i++) {
        char p[1200];
        path_of(p, sizeof p, "", names[i]);
        size_t sz;
        void *d = SDL_LoadFile(p, &sz);
        if (!d) continue;
        if (sz <= MAX_FILE) {
            out = realloc(out, len + NAME_LEN + 4 + sz + 8);
            memset(out + len, 0, NAME_LEN);
            memcpy(out + len, names[i], strlen(names[i]));
            uint32_t size = (uint32_t)sz;
            memcpy(out + len + NAME_LEN, &size, 4);
            memcpy(out + len + NAME_LEN + 4, d, sz);
            len += NAME_LEN + 4 + sz;
            count++;
            if (slot_file_kind(names[i]) == 0) slots++;
        }
        SDL_free(d);
    }
    if (!slots) { free(out); ui_savedata_say("내보낼 세이브가 없습니다."); return; }
    memcpy(out + 8, &count, 4);
    uint64_t h = fnv(out, len);
    memcpy(out + len, &h, 8);
    len += 8;
#ifdef __ANDROID__
    // 파일 창이 준 주소에 못 써도 꺼낼 수 있게 앱 전용 외부 폴더에도 남긴다(Android/data/<패키지>/files).
    const char *ext = SDL_GetAndroidExternalStoragePath();
    if (ext) {
        char dir[1100], copy[1200];
        snprintf(dir, sizeof dir, "%s/", ext);
        date_name(copy, sizeof copy, dir, true);
        if (write_all(copy, out, len)) SDL_Log("내보내기 사본 %s", copy);
        else SDL_Log("내보내기 사본 실패 %s: %s", copy, SDL_GetError());
    }
#endif
    bool ok = write_all(path, out, len);
    if (!ok) SDL_Log("내보내기 실패 %s: %s", path, SDL_GetError());
    free(out);
    char msg[160];
    snprintf(msg, sizeof msg, "슬롯 %d개를 내보냈습니다.", slots);
    ui_savedata_say(ok ? msg : "세이브를 내보내지 못했습니다.");
}

// ---- 불러오기 ----
typedef struct { char name[NAME_LEN + 1]; const uint8_t *data; uint32_t size; } entry_t;
static uint8_t *loaded;
static entry_t entries[MAX_ENTRIES];
static int nentries;

static bool parse_bundle(const uint8_t *d, size_t n) {
    nentries = 0;
    if (n < 20 || memcmp(d, MAGIC, 8)) return false;
    uint64_t want;
    memcpy(&want, d + n - 8, 8);
    if (fnv(d, n - 8) != want) return false;
    uint32_t count;
    memcpy(&count, d + 8, 4);
    if (count == 0 || count > MAX_ENTRIES) return false;
    size_t p = 12, end = n - 8;
    int slots = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (end - p < NAME_LEN + 4) return false;
        entry_t *e = &entries[nentries];
        memset(e->name, 0, sizeof e->name);
        memcpy(e->name, d + p, NAME_LEN);
        int kind = slot_file_kind(e->name); // 이름은 slot<n>.sav / ext<n>.ini만: 저장 폴더 밖이나 다른 파일을 건드리지 못한다
        if (kind < 0) return false;
        for (int k = 0; k < nentries; k++) if (!strcmp(entries[k].name, e->name)) return false;
        memcpy(&e->size, d + p + NAME_LEN, 4);
        p += NAME_LEN + 4;
        if (e->size > MAX_FILE || e->size > end - p) return false;
        if (kind == 0) { if (!e->size) return false; slots++; }
        e->data = d + p;
        p += e->size;
        nentries++;
    }
    return p == end && slots > 0;
}

static void import_from(const char *path) {
    size_t n;
    SDL_free(loaded);
    loaded = SDL_LoadFile(path, &n);
    if (!loaded || !parse_bundle(loaded, n)) {
        if (!loaded) SDL_Log("불러오기: %s을(를) 읽지 못함: %s", path, SDL_GetError());
        SDL_free(loaded);
        loaded = NULL;
        ui_savedata_say("올바른 세이브 파일이 아닙니다.");
        return;
    }
    int in_file = 0;
    for (int i = 0; i < nentries; i++) if (slot_file_kind(entries[i].name) == 0) in_file++;
    ui_savedata_ask(in_file, savefile_slot_count());
}

// 새 파일을 모두 쓴 뒤에야 지금 슬롯을 backup/으로 옮긴다. 중간에 실패하면 되돌린다.
static bool apply_import(void) {
    char a[1200], b[1200], dir[1100];
    snprintf(dir, sizeof dir, "%s/import", save_dir);
    SDL_CreateDirectory(dir);
    for (int i = 0; i < nentries; i++) {
        path_of(a, sizeof a, "/import", entries[i].name);
        if (!write_all(a, entries[i].data, entries[i].size)) return false;
    }
    // 지난 백업은 지우고 지금 슬롯을 통째로 옮긴다(묶음에 없는 슬롯도 비워야 한다)
    snprintf(dir, sizeof dir, "%s/backup", save_dir);
    SDL_CreateDirectory(dir);
    int count = 0;
    char **old = SDL_GlobDirectory(dir, NULL, 0, &count);
    for (int i = 0; old && i < count; i++)
        if (slot_file_kind(old[i]) >= 0) { path_of(b, sizeof b, "/backup", old[i]); SDL_RemovePath(b); }
    SDL_free(old);
    char names[MAX_ENTRIES][NAME_LEN];
    int n = list_slot_files(names);
    bool moved[MAX_ENTRIES] = {0}, ok = true;
    for (int i = 0; i < n; i++) {
        path_of(a, sizeof a, "", names[i]);
        path_of(b, sizeof b, "/backup", names[i]);
        if (!(moved[i] = SDL_RenamePath(a, b))) ok = false;
    }
    int placed = 0;
    for (; ok && placed < nentries; placed++) {
        path_of(a, sizeof a, "/import", entries[placed].name);
        path_of(b, sizeof b, "", entries[placed].name);
        if (!SDL_RenamePath(a, b)) { ok = false; break; }
    }
    if (ok) {
        snprintf(dir, sizeof dir, "%s/import", save_dir);
        SDL_RemovePath(dir);
        return true;
    }
    for (int i = 0; i < placed; i++) { path_of(b, sizeof b, "", entries[i].name); SDL_RemovePath(b); }
    for (int i = 0; i < n; i++) if (moved[i]) {
        path_of(a, sizeof a, "/backup", names[i]);
        path_of(b, sizeof b, "", names[i]);
        SDL_RenamePath(a, b);
    }
    return false;
}

static bool restart_wanted;

void savefile_import_answer(bool yes) {
    if (yes && loaded) {
        if (apply_import()) {
            restart_wanted = true; // 메인 루프(savefile_poll)에서 다시 시작한다
            ui_savedata_say("세이브를 불러왔습니다. 타이틀 화면에서 이어하기를 고르세요.");
        } else {
            SDL_Log("불러오기 적용 실패: %s", SDL_GetError());
            ui_savedata_say("세이브를 불러오지 못했습니다. 기존 세이브는 그대로입니다.");
        }
    }
    SDL_free(loaded);
    loaded = NULL;
    nentries = 0;
}

// ---- 파일 창 ---- 응답은 다른 스레드에서 올 수 있어 메인 루프에서 처리한다.
static SDL_AtomicInt dialog_state; // 0 없음, 1 열림, 2 응답 도착
static bool dialog_import;
static char picked[2048];
static struct SDL_Window *dialog_window;

static void SDLCALL on_pick(void *ud, const char *const *files, int filter) {
    (void)ud; (void)filter;
    picked[0] = 0;
    if (files && files[0]) snprintf(picked, sizeof picked, "%s", files[0]);
    else if (!files) SDL_Log("파일 창 오류: %s", SDL_GetError());
    SDL_SetAtomicInt(&dialog_state, 2);
}

bool savefile_dialog_open(void) { return SDL_GetAtomicInt(&dialog_state) != 0; }

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// 브라우저에는 파일 창 대신 페이지가 고르기 화면을 띄우고(web/app.js의 Module.pickSave), 고른 파일을 임시 폴더에 써서 알려 준다. 취소면 빈 문자열이다.
EMSCRIPTEN_KEEPALIVE void web_save_picked(const char *path) {
    const char *files[] = {path, NULL};
    on_pick(NULL, path && path[0] ? files : NULL, 0);
}
#endif
void savefile_set_window(struct SDL_Window *w) { dialog_window = w; }

void savefile_choose(bool import) {
    static const SDL_DialogFileFilter filter[] = {{"Epic Chronicle PE save", "ecsave"}};
    if (savefile_dialog_open()) return;
    if (!import && !savefile_slot_count()) { ui_savedata_say("내보낼 세이브가 없습니다."); return; }
    dialog_import = import;
    SDL_SetAtomicInt(&dialog_state, 1);
    const char *test = getenv("ECPE_SAVEFILE_TEST"); // 자동 시험용
    if (test) { const char *f[] = {test, NULL}; on_pick(NULL, f, 0); return; }
#ifdef __EMSCRIPTEN__
    // 내보내기는 임시 폴더에 만든 뒤 내려받게 한다(savefile_poll).
    if (import) { EM_ASM({ Module.pickSave(); }); return; }
    char tmp[1200];
    date_name(tmp, sizeof tmp, "/tmp/", false);
    const char *made[] = {tmp, NULL};
    on_pick(NULL, made, 0);
    return;
#endif
    if (import) SDL_ShowOpenFileDialog(on_pick, NULL, dialog_window, filter, 1, NULL, false);
    else {
        // 기본값: 문서 폴더의 ecpe-save-YYYY-MM-DD.ecsave. 문서 폴더를 모르면 이름만 넘긴다.
        char path[1200];
        const char *dir = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
        date_name(path, sizeof path, dir ? dir : "", false);
        SDL_ShowSaveFileDialog(on_pick, NULL, dialog_window, filter, 1, path);
    }
}

// 메인 루프마다. 불러오기가 끝나 게임을 다시 시작해야 하면 true(한 번).
bool savefile_poll(void) {
    if (SDL_GetAtomicInt(&dialog_state) == 2) {
        SDL_SetAtomicInt(&dialog_state, 0);
        if (picked[0]) { if (dialog_import) import_from(picked); else export_to(picked); }
#ifdef __EMSCRIPTEN__
        if (picked[0] && !dialog_import) EM_ASM({ Module.downloadFile(UTF8ToString($0)); }, picked);
#endif
    }
    if (restart_wanted) { restart_wanted = false; return true; }
    return false;
}
