// WIPI C 표 설치와 분배. 표 번호와 슬롯은 work/wipi-api.md 참고.
#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

// 초기화 함수 0x3d638(크로노스 윙 0x855e8과 같은 런타임 코드)이 각 표 포인터를 저장하는 전역(이미지 주소)과 표 번호.
static const struct { uint32_t global; uint32_t table; } TABLES[] = {
    {0x4dec0, 0}, {0x4de40, 1}, {0x4dda0, 2}, {0x4dce0, 3}, {0x4dea0, 4}, {0x4df00, 5}, {0x4dc80, 6}, {0x4dd80, 7}, {0x4dd60, 8},
    {0x4de60, 9}, {0x4dca0, 10}, {0x4dd20, 11}, {0x4de20, 12}, {0x4dd40, 13}, {0x4ddc0, 14}, {0x4dc60, 15}, {0x4dee0, 16}, {0x4dc20, 17},
};
#define IMAGE_BASE 0x101254u

void api_install(void) {
    cpu_t *c = &cpu;
    for (size_t t = 0; t < sizeof TABLES / sizeof TABLES[0]; t++) {
        uint32_t table = HOST_TABLES + (uint32_t)t * 0x400;
        for (uint32_t i = 0; i < 256; i++) W32(table + i * 4, API_BASE | TABLES[t].table << 16 | i * 2 | 1);
        W32(VD(IMAGE_BASE + TABLES[t].global), table);
    }
}

// sprintf: %d %u %x %s %c, 플래그 0, 폭, l/ll.
static uint32_t knl_format(cpu_t *c, uint32_t dest, const char *f, int argi, char *hostbuf, size_t hostcap) {
    char out[4096];
    size_t n = 0;
    for (; *f && n < sizeof out - 64; f++) {
        if (*f != '%') { out[n++] = *f; continue; }
        const char *s = f + 1;
        bool zero = false;
        int width = 0, longs = 0;
        if (*s == '0') { zero = true; s++; }
        while (*s >= '0' && *s <= '9') width = width * 10 + (*s++ - '0');
        if (width > 256) width = 256;
        while (*s == 'l') { longs++; s++; }
        char spec[16], tmp[300];
        switch (*s) {
        case '%': out[n++] = '%'; break;
        case 'd': case 'u': case 'x': {
            uint64_t v = arg(c, argi++);
            if (longs >= 2) v |= (uint64_t)arg(c, argi++) << 32;
            snprintf(spec, sizeof spec, zero ? "%%0%dll%c" : "%%%dll%c", width, *s);
            if (*s == 'd') { int64_t sv = longs >= 2 ? (int64_t)v : (int64_t)(int32_t)v; snprintf(tmp, sizeof tmp, spec, sv); }
            else snprintf(tmp, sizeof tmp, spec, v);
            size_t l = strlen(tmp);
            memcpy(out + n, tmp, l);
            n += l;
            break;
        }
        case 's': {
            uint32_t p = arg(c, argi++);
            const char *str = p ? gstr(p) : "(null)";
            size_t l = strlen(str);
            if (l > sizeof out - n - 64) l = sizeof out - n - 64;
            memcpy(out + n, str, l);
            n += l;
            break;
        }
        case 'c': out[n++] = (char)arg(c, argi++); break;
        case 0: f = s - 1; continue;
        default: out[n++] = '%'; out[n++] = *s; break;
        }
        f = s;
    }
    out[n] = 0;
    if (dest) memcpy(c->m + dest, out, n + 1);
    if (hostbuf) { strncpy(hostbuf, out, hostcap - 1); hostbuf[hostcap - 1] = 0; }
    return (uint32_t)n;
}

static void resource_path(char *out, size_t cap, const char *name) {
    while (*name == '/') name++;
    snprintf(out, cap, "%s/%s", game_dir, name);
}

// GetResourceID의 값은 경로 문자열의 게스트 주소다. 돌려받는 API가 없어 같은 경로에는 같은 주소를 준다.
#define RESOURCE_BUCKETS 4096
static struct resource_entry { uint32_t id; struct resource_entry *next; } *buckets[RESOURCE_BUCKETS];

static uint32_t resource_id(cpu_t *c, const char *name) {
    uint32_t hash = 2166136261u;
    for (const char *p = name; *p; p++) hash = (hash ^ (uint8_t)*p) * 16777619u;
    struct resource_entry **head = &buckets[hash % RESOURCE_BUCKETS];
    for (struct resource_entry *e = *head; e; e = e->next)
        if (!strcmp((char *)c->m + e->id, name)) return e->id;
    size_t l = strlen(name);
    uint32_t h = heap_alloc((uint32_t)l + 1);
    if (!h) return 0;
    memcpy(c->m + h, name, l + 1);
    struct resource_entry *e = malloc(sizeof *e);
    if (e) { e->id = h; e->next = *head; *head = e; }
    return h;
}

static void api_kernel(cpu_t *c, uint32_t i) {
    uint32_t *r = c->r;
    switch (i) {
    case 0: { char b[1024]; knl_format(c, 0, gstr(r[0]), 1, b, sizeof b); fprintf(stderr, "[printk] %s\n", b); break; }
    case 1: r[0] = knl_format(c, r[0], gstr(r[1]), 2, NULL, 0); break;
    case 7: // 게임이 스스로 끝낸다. 어디서 불렀는지 남긴다.
        fprintf(stderr, "게임 Exit 호출: LR %#x, 마지막 간접 호출 %#x, 스택", c->LR, host_last_call);
        for (uint32_t i = 0, sp = c->SP; i < 48; i++) {
            uint32_t v = R32(sp + i * 4);
            if ((v & 1) && v >= 0x101254 && v < 0x147878) fprintf(stderr, " %#x", v);
        }
        fprintf(stderr, "\n");
        request_exit();
        break;
    case 9: r[0] = 1; break;
    case 20: r[0] = handle_alloc(r[0]); break;
    case 21: r[0] = handle_alloc(r[0]); break;
    case 22: if (r[0]) handle_free(r[0]); break;
    // GetTotalMemory, GetFreeMemory. startClet(0x14308)은 이 값(최대 0x164000)만큼 게임 힙을 잡는다. WIE의 1MB로는 맵을 나갈 때 할당이 실패해 죽는다.
    case 23: case 24: r[0] = 0x400000; break;
    case 25: timer_def(r[0], r[1]); break;
    case 26: r[0] = (uint32_t)timer_set(r[0], (uint64_t)r[2] << 32 | r[1], r[3]); break;
    case 27: timer_unset(r[0]); break;
    case 28: { // CHRONOS_FAKE_TIME: 대조용. 호출마다 5씩 늘어 실행 간 시각 차이를 없앤다.
        static uint64_t fake;
        static int fake_on = -1;
        if (fake_on < 0) fake_on = getenv("CHRONOS_FAKE_TIME") != NULL;
        uint64_t t = fake_on ? (fake += 5) : game_time_ms();
        r[0] = (uint32_t)t; r[1] = (uint32_t)(t >> 32); break; }
    case 29: { // GetSystemProperty(id,out,size). 값은 WIE와 같게 둔다.
        static const char *const kv[][2] = {{"RSSILEVEL","30"},{"BATTERYLEVEL","100"},{"PHONEMODEL","Emulator"},{"PHONENUMBER",""},{"MIN","01000000000"},
            {"ANNUN_CALL","0"},{"ANNUN_SMS","0"},{"ANNUN_SILENT","0"},{"ANNUN_ALARM","0"},{"ANNUN_SECURITY","0"},{"CURRENTCH","0"},{"AIRPLANE_MODE","0"},{"ROAMING_AREA","0"},{"DS_LOCK","0"}};
        const char *id = gstr(r[0]);
        int res = -9;
        for (size_t k = 0; k < sizeof kv / sizeof kv[0]; k++)
            if (!strcmp(id, kv[k][0])) {
                size_t l = strlen(kv[k][1]);
                if (l + 1 > r[2]) { res = -18; break; }
                memcpy(c->m + r[1], kv[k][1], l + 1);
                res = 0;
                break;
            }
        r[0] = (uint32_t)res;
        break;
    }
    case 31: { // GetResourceID(name,&size)
        char path[2048];
        resource_path(path, sizeof path, gstr(r[0]));
        struct stat st;
        if (getenv("ECPE_RESLOG")) fprintf(stderr, "GetResourceID %s LR %#x\n", gstr(r[0]), c->LR);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            if (r[1]) W32(r[1], 0);
            r[0] = (uint32_t)-12;
            break;
        }
        uint32_t h = resource_id(c, gstr(r[0]));
        W32(r[1], (uint32_t)st.st_size);
        r[0] = h;
        break;
    }
    case 32: { // GetResource(id,buf,size)
        if ((int32_t)r[0] < 0) { r[0] = (uint32_t)-9; break; }
        char path[2048];
        resource_path(path, sizeof path, gstr(r[0]));
        FILE *f = fopen(path, "rb");
        if (!f) { r[0] = (uint32_t)-12; break; }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if ((uint32_t)len > r[2]) { fclose(f); r[0] = (uint32_t)-1; break; }
        size_t got = fread(c->m + handle_data(r[1]), 1, (size_t)len, f);
        (void)got;
        fclose(f);
        r[0] = 0;
        break;
    }
    default: fprintf(stderr, "커널 API %u 미구현\n", i); r[0] = 0;
    }
}

// 저장 파일. 핸들은 작은 양수다.
static struct { FILE *f; int mode; } files[64];

// 게임을 다시 시작할 때 게스트 주소를 가리키는 상태와 열린 파일을 비운다.
void api_reset(void) {
    for (int i = 0; i < RESOURCE_BUCKETS; i++)
        while (buckets[i]) { struct resource_entry *n = buckets[i]->next; free(buckets[i]); buckets[i] = n; }
    for (int i = 0; i < 64; i++) if (files[i].f) { fclose(files[i].f); files[i].f = NULL; }
}

static bool save_path(char *out, size_t cap, const char *name) {
    if (!*name || strstr(name, "..") || strlen(name) >= 128) return false;
    while (*name == '/') name++;
    snprintf(out, cap, "%s/%s", save_dir, name);
    for (char *p = out + strlen(save_dir) + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(out, 0755); *p = '/'; }
    return true;
}

static void api_fs(cpu_t *c, uint32_t i) {
    uint32_t *r = c->r;
    int fd = (int)r[0] - 1;
    bool bad = (i >= 1 && i <= 4 || i == 15) && (fd < 0 || fd >= 64 || !files[fd].f);
    if (bad) { r[0] = (uint32_t)-2; return; }
    char path[2048];
    switch (i) {
    case 0: { // Open(name,mode,access). 1 읽기, 2 쓰기, 4 쓰기+비움.
        int mode = (int)r[1];
        if (!save_path(path, sizeof path, gstr(r[0]))) { r[0] = (uint32_t)-3; break; }
        struct stat st;
        bool exists = stat(path, &st) == 0;
        if (!exists && mode == 1) { r[0] = (uint32_t)-12; break; }
        const char *m = mode == 1 ? "rb" : (mode == 4 || !exists) ? "w+b" : "r+b";
        FILE *f = fopen(path, m);
        if (!f) { r[0] = (uint32_t)-1; break; }
        int slot = 0;
        while (slot < 64 && files[slot].f) slot++;
        if (slot == 64) { fclose(f); r[0] = (uint32_t)-1; break; }
        files[slot].f = f;
        files[slot].mode = mode;
        r[0] = (uint32_t)slot + 1;
        break;
    }
    case 1: { // Read
        if (files[fd].mode == 2 || files[fd].mode == 4) { r[0] = (uint32_t)-24; break; }
        CK(r[1], r[2]);
        fseek(files[fd].f, 0, SEEK_CUR);
        size_t n = fread(c->m + r[1], 1, r[2], files[fd].f);
        r[0] = r[2] && !n ? (uint32_t)-23 : (uint32_t)n;
        break;
    }
    case 2: { // Write
        if (files[fd].mode == 1) { r[0] = (uint32_t)-24; break; }
        CK(r[1], r[2]);
        fseek(files[fd].f, 0, SEEK_CUR);
        r[0] = (uint32_t)fwrite(c->m + r[1], 1, r[2], files[fd].f);
        fflush(files[fd].f);
        break;
    }
    case 3: fclose(files[fd].f); files[fd].f = NULL; r[0] = 0; break;
    case 4: r[0] = fseek(files[fd].f, (int32_t)r[1], r[2] == 0 ? SEEK_SET : r[2] == 1 ? SEEK_CUR : SEEK_END) ? (uint32_t)-1 : (uint32_t)ftell(files[fd].f); break;
    case 5: { // FileAttribute(name,out,access)
        struct stat st;
        if (!save_path(path, sizeof path, gstr(r[0]))) { r[0] = (uint32_t)-3; break; }
        if (stat(path, &st)) { r[0] = (uint32_t)-12; break; }
        W32(r[1], 0); W32(r[1] + 4, 0); W32(r[1] + 8, (uint32_t)st.st_size);
        r[0] = 0;
        break;
    }
    case 6:
        if (!save_path(path, sizeof path, gstr(r[0]))) { r[0] = (uint32_t)-3; break; }
        r[0] = remove(path) ? (uint32_t)-12 : 0;
        break;
    case 12: r[0] = 0x1000000; break; // Available: 남은 공간. 넉넉한 값을 준다.
    case 15: r[0] = (uint32_t)ftell(files[fd].f); break;
    case 16: {
        struct stat st;
        if (!save_path(path, sizeof path, gstr(r[0]))) { r[0] = (uint32_t)-3; break; }
        r[0] = stat(path, &st) ? (uint32_t)-12 : 0;
        break;
    }
    default: fprintf(stderr, "파일 API %u 미구현\n", i); r[0] = (uint32_t)-1;
    }
}

// MC_MdaClip은 게임이 들여다보지 않으므로 앞 4바이트에 오디오 핸들만 둔다.
static void api_media(cpu_t *c, uint32_t i) {
    uint32_t *r = c->r;
    switch (i) {
    case 0: r[0] = heap_alloc(0x100); break;
    case 3:
        if (r[0]) { audio_free(R32(r[0])); heap_free(r[0]); }
        r[0] = 0;
        break;
    case 4: { // ClipPutData(clip,buf,size)
        if (!r[0]) { r[0] = (uint32_t)-1; break; }
        CK(r[1], r[2]);
        W32(r[0], audio_load(c->m + r[1], r[2]));
        r[0] = r[2];
        break;
    }
    case 8: if (r[0]) audio_play(R32(r[0]), r[1] != 0); r[0] = 0; break;
    case 11: if (r[0]) audio_stop(R32(r[0])); r[0] = 0; break;
    case 7: case 14: case 15: case 16: case 17: case 18: case 25: case 26: r[0] = 0; break; // 볼륨, 진동 등
    default: fprintf(stderr, "미디어 API %u 미구현\n", i); r[0] = 0;
    }
}

// 네트워크는 서버가 없으므로 모두 실패를 돌려준다.
static void api_net(cpu_t *c, uint32_t i) {
    fprintf(stderr, "네트워크 API %u: 서버 없음\n", i);
    c->r[0] = (uint32_t)-1;
}

void api_dispatch(cpu_t *c, uint32_t t, uint32_t i) {
    uint32_t sp = c->SP, *r = c->r;
    switch (t) {
    case 0: api_kernel(c, i); break;
    case 1:
        if (i == 0) r[0] = __builtin_bswap32(r[0]);
        else if (i == 1) r[0] = __builtin_bswap16((uint16_t)r[0]);
        else { fprintf(stderr, "유틸 API %u 미구현\n", i); r[0] = 0; }
        break;
    case 3: gfx_api(c, i); break;
    case 7: api_fs(c, i); break;
    case 10: api_media(c, i); break;
    case 11: api_net(c, i); break;
    case 2: r[0] = 0; break; // backlight
    default: fprintf(stderr, "표 %u API %u 미구현\n", t, i); r[0] = 0;
    }
    c->SP = sp;
}
