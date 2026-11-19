// Android: APK의 game.pak을 앱 내부 저장소(data/)에 풀고 game_dir을 data/game으로 정한다.
// 팩에는 font.gly, 1mgm.sf2, ui/, game/이 있어 main.c의 game_dir/../font.gly, ../ui, ../1mgm.sf2 경로가 그대로 맞는다.
// 세이브는 다른 폴더(SDL_GetPrefPath)라 건드리지 않는다. 팩 크기가 바뀐 때만 다시 푼다.
#ifdef __ANDROID__
#include "host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>

// stderr를 logcat으로 보낸다. 게스트 메모리 오류 같은 진단은 stderr로 나온다.
static void *stderr_pump(void *arg) {
    int fd = (int)(intptr_t)arg;
    char buf[1024];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf - 1)) > 0) {
        buf[n] = 0;
        __android_log_write(ANDROID_LOG_ERROR, "ecpe", buf);
    }
    return NULL;
}

// main.c가 부르지 않아도 라이브러리가 올라올 때 한 번 돈다(constructor). main.c를 건드리지 않으려는 선택이다.
__attribute__((constructor)) void android_log_stderr(void) {
    int fd[2];
    if (pipe(fd)) return;
    setvbuf(stderr, NULL, _IONBF, 0);
    dup2(fd[1], 2);
    close(fd[1]);
    pthread_t t;
    if (!pthread_create(&t, NULL, stderr_pump, (void *)(intptr_t)fd[0])) pthread_detach(t);
}

static uint32_t read_u32(SDL_IOStream *io) { Uint32 v = 0; SDL_ReadU32LE(io, &v); return v; }

static bool extract(SDL_IOStream *io, const char *root) {
    char magic[4];
    if (SDL_ReadIO(io, magic, 4) != 4 || memcmp(magic, "ECPK", 4)) return false;
    uint32_t count = read_u32(io);
    char name[512], path[1200];
    for (uint32_t i = 0; i < count; i++) {
        Uint16 len = 0;
        SDL_ReadU16LE(io, &len);
        if (len >= sizeof name || SDL_ReadIO(io, name, len) != len) return false;
        name[len] = 0;
        uint32_t size = read_u32(io);
        snprintf(path, sizeof path, "%s/%s", root, name);
        char *slash = strrchr(path, '/');
        *slash = 0;
        SDL_CreateDirectory(path);
        *slash = '/';
        void *data = malloc(size ? size : 1);
        if (SDL_ReadIO(io, data, size) != size) { free(data); return false; }
        FILE *f = fopen(path, "wb");
        if (!f) { free(data); return false; }
        fwrite(data, 1, size, f);
        fclose(f);
        free(data);
    }
    return true;
}

bool android_prepare_data(void) {
    char root[1024], marker[1100], want[32], have[32] = "";
    snprintf(root, sizeof root, "%s/data", SDL_GetAndroidInternalStoragePath());
    SDL_IOStream *io = SDL_IOFromFile("game.pak", "rb");
    if (!io) return false;
    snprintf(want, sizeof want, "%lld", (long long)SDL_GetIOSize(io));
    snprintf(marker, sizeof marker, "%s/.pak", root);
    FILE *f = fopen(marker, "r");
    if (f) { if (!fgets(have, sizeof have, f)) have[0] = 0; fclose(f); }
    bool ok = !strcmp(want, have);
    if (!ok) {
        SDL_CreateDirectory(root);
        remove(marker);
        ok = extract(io, root);
        if (ok && (f = fopen(marker, "w"))) { fputs(want, f); fclose(f); }
    }
    SDL_CloseIO(io);
    snprintf(game_dir, sizeof game_dir, "%s/game", root);
    return ok;
}
#endif
