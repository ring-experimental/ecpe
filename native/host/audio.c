// 소리. 게임이 넣는 MMF를 smafev(smaf-player, MIT)로 이벤트로 바꿔 재생한다. 같은 내용은 해시로 한 번만 바꾼다.
// MIDI는 TinySoundFont(MIT)로 합성한다. 크로노스 윙 audio.c에서 빌드 때 변환을 실행 중 변환으로 바꿨다.
// 곡 하나와 효과음마다 SDL_AudioStream을 두어 한 장치에 묶고, 섞기와 샘플링 변환은 SDL에 맡긴다.
#include "host.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#define TSF_IMPLEMENTATION
#include "../third_party/tsf.h"

#define RATE 44100
typedef struct { uint32_t time; uint8_t kind, ch; uint32_t rate, len; const uint8_t *data; } ev_t;
typedef struct { uint64_t hash; uint32_t n; ev_t *ev; uint32_t duration; bool has_wave; } seq_t;
typedef struct { bool on, repeat; seq_t *s; uint32_t next; uint64_t pos; uint32_t handle; } play_t;
typedef struct { bool on, repeat; const int16_t *s; uint32_t n, pos, rate; int fade; uint32_t handle; uint64_t order; SDL_AudioStream *st; } wave_t;

static seq_t **seqs;
static uint32_t nseq;
#ifdef __EMSCRIPTEN__
// 브라우저: 변환기는 따로 받은 smafev.wasm이다(wasm32-unknown-unknown). 페이지가 시작 전에 Module.smafev로 열어 둔다(web/app.js).
// 결과를 이쪽 힙에 복사해 돌려준다. 실패하면 NULL.
#include <emscripten.h>
EM_JS(uint8_t *, smafev_convert, (const uint8_t *data, size_t len, size_t *out_len), {
    const ev = Module.smafev;
    if (!ev) return 0;
    const at = ev.smafev_alloc(len), lenAt = ev.smafev_alloc(4);
    new Uint8Array(ev.memory.buffer, at, len).set(HEAPU8.subarray(data, data + len));
    const rec = ev.smafev_convert(at, len, lenAt);
    ev.smafev_dealloc(at, len);
    const n = new Uint32Array(ev.memory.buffer, lenAt, 1)[0];
    ev.smafev_dealloc(lenAt, 4);
    if (!rec) return 0;
    const out = _malloc(n);
    HEAPU8.set(new Uint8Array(ev.memory.buffer, rec, n), out);
    ev.smafev_free(rec, n);
    HEAPU32[out_len >> 2] = n;
    return out;
});
#else
uint8_t *smafev_convert(const uint8_t *data, size_t len, size_t *out_len);
#endif
static tsf *synth;
static SDL_Mutex *lock;
static SDL_AudioDeviceID device;
static SDL_AudioStream *stream;
static play_t plays[16];
#define WAVE_STREAMS 24
#define WAVE_ACTIVE 16 // 이보다 많이 겹치면 가장 오래된 효과음을 줄여 끈다.
static wave_t waves[WAVE_STREAMS];
static seq_t **handles;
static uint32_t nhandles;
float audio_peak;

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

// 이벤트 레코드 하나를 읽고 다음 위치를 돌려준다. 형식은 work/tools/smafev 참고.
static const uint8_t *parse_seq(const uint8_t *p, seq_t *s) {
    s->n = rd32(p); p += 4;
    s->ev = calloc(s->n ? s->n : 1, sizeof(ev_t));
    for (uint32_t k = 0; k < s->n; k++) {
        ev_t *e = &s->ev[k];
        e->time = rd32(p); p += 4;
        e->kind = *p++;
        if (e->time > s->duration) s->duration = e->time;
        if (e->kind == 0) { e->len = rd32(p); p += 4; e->data = p; p += e->len; }
        else { s->has_wave = true; e->ch = *p++; e->rate = rd32(p); p += 4; e->len = rd32(p); p += 4; e->data = p; p += (size_t)e->len * 2; }
    }
    return p;
}

static void midi(tsf *synth, const uint8_t *m, uint32_t n) {
    if (n < 2) return;
    int ch = m[0] & 15;
    switch (m[0] & 0xf0) {
    case 0x90: if (n >= 3) { if (m[2]) tsf_channel_note_on(synth, ch, m[1], m[2] / 127.0f); else tsf_channel_note_off(synth, ch, m[1]); } break;
    case 0x80: if (n >= 3) tsf_channel_note_off(synth, ch, m[1]); break;
    case 0xc0: tsf_channel_set_presetnumber(synth, ch, m[1], ch == 9); break;
    case 0xb0: if (n >= 3) tsf_channel_midi_control(synth, ch, m[1], m[2]); break;
    case 0xe0: if (n >= 3) tsf_channel_set_pitchwheel(synth, ch, m[1] | m[2] << 7); break;
    }
}

// 반복과 곡 끝. 페달을 풀고 음을 놓아 음이 쌓이지 않게 한다.
// CC121은 울리고 있는 음의 게인과 팬까지 바꿔 음이 튀므로 여기서 쓰지 않는다.
static void synth_reset(void) {
    if (!synth) return;
    for (int ch = 0; ch < 16; ch++) tsf_channel_midi_control(synth, ch, 64, 0);
    tsf_note_off_all(synth);
}

// 페이드가 끝나 들리지 않을 때 음과 채널 상태를 모두 지운다.
static void synth_clear(void) {
    static float discard[128];
    if (!synth) return;
    tsf_reset(synth);
    for (int c = 0; c < 16; c++) tsf_channel_set_presetnumber(synth, c, 0, c == 9);
    tsf_render_float(synth, discard, 64, 0); // 끝난 음을 비운다.
}

static void run_events(play_t *p) {
    uint64_t now_ms = p->pos * 1000 / RATE;
    while (p->next < p->s->n && p->s->ev[p->next].time <= now_ms) {
        ev_t *e = &p->s->ev[p->next++];
        if (e->kind == 0 && synth) midi(synth, e->data, e->len);
    }
    if (p->next >= p->s->n && p->pos * 1000 / RATE >= p->s->duration) {
        synth_reset();
        if (p->repeat && p->s->duration) { p->pos = 0; p->next = 0; }
        else p->on = false;
    }
}

// 원본은 소리를 하나만 내는 기기용이라 클립이 하나뿐이고, 효과음을 낼 때 그 클립에 있던 배경음을 끈다.
// 크로노스 윙처럼 게임의 효과음 처리 안(sfx_scope, restored/sound.c)에서 일어난 정지는 배경음에 적용하지 않고,
// 그 안에서 시작한 MIDI 소리는 효과음 합성기로 보낸다. 그 밖의 정지는 실제 정지다.
#define MUSIC_FADE_FRAMES (RATE / 25) // 40ms
static int music_fade;
static bool sfx_scope;
// solo는 크로노스 윙 플레이리스트용으로 남아 있다. 이 게임에서는 쓰지 않는다.
static bool solo;

// MIDI로 된 효과음(0x11 등)은 배경음 합성기와 채널 상태가 섞이지 않게 따로 합성한다. 한 번에 하나만 울린다.
static tsf *sfx_synth;
static play_t sfx_midi;

static void SDLCALL sfx_midi_cb(void *ud, SDL_AudioStream *st, int additional, int total) {
    static float buf[512];
    (void)ud; (void)total;
    int frames = additional / (int)(sizeof(float) * 2);
    while (frames > 0) {
        int n = frames > 256 ? 256 : frames;
        SDL_LockMutex(lock);
        play_t *p = &sfx_midi;
        if (p->on) {
            uint64_t now_ms = p->pos * 1000 / RATE;
            while (p->next < p->s->n && p->s->ev[p->next].time <= now_ms) {
                ev_t *e = &p->s->ev[p->next++];
                if (e->kind == 0) midi(sfx_synth, e->data, e->len);
            }
            p->pos += n;
            if (p->next >= p->s->n && p->pos * 1000 / RATE >= p->s->duration) {
                if (p->repeat && p->s->duration) { p->pos = 0; p->next = 0; }
                else { p->on = false; tsf_note_off_all(sfx_synth); }
            }
        }
        tsf_render_float(sfx_synth, buf, n, 0); // 놓은 음의 꼬리까지 그린다
        SDL_UnlockMutex(lock);
        SDL_PutAudioStreamData(st, buf, n * 2 * (int)sizeof(float));
        frames -= n;
    }
}

static void sfx_midi_start(seq_t *seq, uint32_t handle, bool repeat) {
    if (!sfx_synth) return;
    SDL_LockMutex(lock);
    tsf_reset(sfx_synth);
    for (int c = 0; c < 16; c++) tsf_channel_set_presetnumber(sfx_synth, c, 0, c == 9);
    sfx_midi = (play_t){true, repeat, seq, 0, 0, handle};
    SDL_UnlockMutex(lock);
}

static play_t *music_play(void) {
    for (int i = 0; i < 16; i++) if (plays[i].on) return &plays[i];
    return NULL;
}

// 곡을 끊지 않고 줄여서 끈다. 다음 곡은 페이드와 synth_clear 뒤에 시작한다.
static void music_stop_now(void) {
    play_t *m = music_play();
    if (m) m->on = false;
    if (!music_fade) music_fade = MUSIC_FADE_FRAMES;
}

static void SDLCALL audio_cb(void *ud, SDL_AudioStream *st, int additional, int total) {
    static float buf[8192];
    (void)ud; (void)total;
    int frames = additional / (int)(sizeof(float) * 2);
    while (frames > 0) {
        int n = frames > 256 ? 256 : frames; // 이벤트 시각 해상도 약 6ms
        SDL_LockMutex(lock);
        if (!music_fade) for (int i = 0; i < 16; i++) if (plays[i].on) { run_events(&plays[i]); plays[i].pos += n; }
        if (synth) tsf_render_float(synth, buf, n, 0);
        else memset(buf, 0, sizeof(float) * 2 * n);
        if (music_fade) {
            for (int k = 0; k < n; k++) {
                float g = music_fade > 0 ? (float)music_fade-- / MUSIC_FADE_FRAMES : 0;
                buf[k * 2] *= g;
                buf[k * 2 + 1] *= g;
            }
            if (!music_fade) synth_clear();
        }
        SDL_UnlockMutex(lock);
        SDL_PutAudioStreamData(st, buf, n * 2 * (int)sizeof(float));
        frames -= n;
    }
}

// 파형 끝이 0이 아니면 딸깍거리므로 양 끝과 중간 정지를 짧게 줄인다. 원본 샘플 기준이다.
#define WAVE_FADE_IN 8
#define WAVE_FADE_OUT 40 // 8kHz에서 5ms

static void SDLCALL wave_cb(void *ud, SDL_AudioStream *st, int additional, int total) {
    wave_t *v = ud;
    int16_t buf[256];
    (void)total;
    int frames = additional / (int)sizeof(int16_t);
    while (frames > 0) {
        int want = frames > 256 ? 256 : frames, n = 0;
        bool ended = false;
        SDL_LockMutex(lock);
        if (!v->on) { SDL_UnlockMutex(lock); return; }
        while (n < want) {
            if (v->pos >= v->n) {
                if (v->repeat) v->pos = 0;
                else { ended = true; break; }
            }
            int g = 256;
            if (!v->repeat) {
                if (v->pos < WAVE_FADE_IN) g = g * (int)v->pos / WAVE_FADE_IN;
                uint32_t left = v->n - v->pos;
                if (left < WAVE_FADE_OUT) g = g * (int)left / WAVE_FADE_OUT;
            }
            if (v->fade > 0) {
                g = g * v->fade / WAVE_FADE_OUT;
                if (--v->fade == 0) ended = true;
            }
            buf[n++] = (int16_t)(v->s[v->pos++] * g / 256);
            if (ended) break;
        }
        if (ended) v->on = false;
        SDL_UnlockMutex(lock);
        SDL_PutAudioStreamData(st, buf, n * (int)sizeof(int16_t));
        frames -= n;
        if (ended) {
            // 변환기에 남은 샘플이 다음 소리 앞에 붙지 않게 0으로 밀어낸다.
            static const int16_t zero[16];
            SDL_PutAudioStreamData(st, zero, sizeof zero);
            return;
        }
    }
}

static void wave_start(const ev_t *e, uint32_t handle, bool repeat) {
    static uint64_t order;
    wave_t *free_v = NULL, *oldest = NULL;
    int active = 0;
    SDL_LockMutex(lock);
    for (int w = 0; w < WAVE_STREAMS; w++) {
        wave_t *v = &waves[w];
        if (!v->st) continue;
        if (!v->on) { if (!free_v) free_v = v; continue; }
        if (v->fade) continue;
        active++;
        if (!oldest || v->order < oldest->order) oldest = v;
    }
    if (active >= WAVE_ACTIVE && oldest) oldest->fade = WAVE_FADE_OUT;
    SDL_UnlockMutex(lock);
    if (!free_v) return;
    // 콜백이 스트림 잠금을 쥔 채 lock을 잡으므로 스트림 함수는 lock 밖에서 부른다.
    if (free_v->rate != e->rate) {
        SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, (int)e->rate};
        SDL_SetAudioStreamFormat(free_v->st, &spec, NULL);
    }
    SDL_LockMutex(lock);
    SDL_AudioStream *st = free_v->st;
    *free_v = (wave_t){true, repeat, (const int16_t *)e->data, e->len, 0, e->rate, 0, handle, ++order, st};
    SDL_UnlockMutex(lock);
}

// 최종 믹스. 효과음이 많이 겹쳐 1을 넘으면 버퍼 단위로 게인을 천천히 내려 파형을 자르지 않는다. 일시 정지 전후의 페이드도 여기서 한다.
#define MASTER_FADE_S 0.012f
#define LIMIT_RELEASE_S 0.2f
static SDL_AtomicInt master_on;
static void SDLCALL postmix(void *ud, const SDL_AudioSpec *spec, float *buf, int buflen) {
    static float limit = 1, master = 0;
    (void)ud;
    int ch = spec->channels, frames = buflen / (int)sizeof(float) / ch;
    if (frames <= 0) return;
    float peak = 0;
    for (int i = 0; i < frames * ch; i++) {
        float a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > peak) peak = a;
    }
    if (peak > audio_peak) audio_peak = peak;
    // 진단: ECPE_SOUNDDUMP=<파일>이면 최종 믹스(게인 전, float 스테레오, 장치 주파수)를 이어 쓴다.
    static FILE *dump; static int dump_init;
    if (!dump_init) { dump_init = 1; const char *p = getenv("ECPE_SOUNDDUMP"); if (p) { dump = fopen(p, "wb"); fprintf(stderr, "소리 기록: %d Hz %d채널 float\n", spec->freq, ch); } }
    if (dump) fwrite(buf, sizeof(float), (size_t)frames * ch, dump);
    float target = peak > 0.98f ? 0.98f / peak : 1;
    float rise = limit + frames / (spec->freq * LIMIT_RELEASE_S);
    if (target > rise) target = rise;
    float limit_step = (target - limit) / frames;
    float master_step = 1 / (spec->freq * MASTER_FADE_S);
    float master_to = SDL_GetAtomicInt(&master_on) ? 1.0f : 0.0f;
    for (int f = 0; f < frames; f++) {
        limit += limit_step;
        if (master < master_to) { master += master_step; if (master > 1) master = 1; }
        else if (master > master_to) { master -= master_step; if (master < 0) master = 0; }
        float g = limit * master;
        for (int c = 0; c < ch; c++) {
            float v = buf[f * ch + c] * g;
            buf[f * ch + c] = v > 1 ? 1 : v < -1 ? -1 : v;
        }
    }
}

// 장치를 바로 멈추면 파형이 끊겨 소리가 튄다. 줄인 뒤에 멈춘다.
static Uint32 SDLCALL pause_later(void *ud, SDL_TimerID id, Uint32 interval) {
    (void)ud; (void)id; (void)interval;
    if (!SDL_GetAtomicInt(&master_on)) SDL_PauseAudioDevice(device);
    return 0;
}

void audio_pause(bool paused) {
    if (!device) return;
    SDL_SetAtomicInt(&master_on, !paused);
    if (paused) SDL_AddTimer(30, pause_later, NULL);
    else SDL_ResumeAudioDevice(device);
}

void audio_init(const char *soundfont) {
    char path[1200];
    snprintf(path, sizeof path, "%s/../1mgm.sf2", game_dir);
    synth = tsf_load_filename(soundfont ? soundfont : path);
    if (synth) {
        tsf_set_output(synth, TSF_STEREO_INTERLEAVED, RATE, -6.0f);
        for (int c = 0; c < 16; c++) tsf_channel_set_presetnumber(synth, c, 0, c == 9);
    } else {
        fprintf(stderr, "사운드폰트를 열지 못함: BGM 없이 진행\n");
    }
    lock = SDL_CreateMutex();
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, RATE};
    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!device) { fprintf(stderr, "오디오 장치 실패: %s\n", SDL_GetError()); return; }
    SDL_SetAtomicInt(&master_on, 1);
    SDL_SetAudioPostmixCallback(device, postmix, NULL);
    stream = SDL_CreateAudioStream(&spec, NULL);
    if (stream) {
        SDL_SetAudioStreamGetCallback(stream, audio_cb, NULL);
        SDL_BindAudioStream(device, stream);
    }
    if (synth && (sfx_synth = tsf_copy(synth))) {
        tsf_set_output(sfx_synth, TSF_STEREO_INTERLEAVED, RATE, -6.0f);
        SDL_AudioStream *ss = SDL_CreateAudioStream(&spec, NULL);
        if (ss) { SDL_SetAudioStreamGetCallback(ss, sfx_midi_cb, NULL); SDL_BindAudioStream(device, ss); }
    }
    SDL_AudioSpec wave_spec = {SDL_AUDIO_S16, 1, 8000};
    for (int w = 0; w < WAVE_STREAMS; w++) {
        wave_t *v = &waves[w];
        v->rate = 8000;
        v->st = SDL_CreateAudioStream(&wave_spec, NULL);
        if (!v->st) continue;
        SDL_SetAudioStreamGain(v->st, 0.5f); // 여러 개가 겹쳐도 넘치지 않도록 절반으로 섞는다.
        SDL_SetAudioStreamGetCallback(v->st, wave_cb, v);
        SDL_BindAudioStream(device, v->st);
    }
}

uint32_t audio_load(const uint8_t *data, uint32_t size) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < size; i++) h = (h ^ data[i]) * 0x100000001b3ull;
    seq_t *found = NULL;
    for (uint32_t i = 0; i < nseq && !found; i++)
        if (seqs[i]->hash == h) found = seqs[i];
    if (!found) {
        size_t len = 0;
        uint8_t *rec = smafev_convert(data, size, &len); // 레코드는 재생하는 동안 계속 가리키므로 풀지 않는다.
        if (!rec || len < 4) { fprintf(stderr, "소리 데이터 변환 실패: %u바이트\n", size); return 0; }
        found = calloc(1, sizeof *found);
        found->hash = h;
        parse_seq(rec, found);
        seqs = realloc(seqs, sizeof *seqs * (nseq + 1));
        seqs[nseq++] = found;
    }
    handles = realloc(handles, sizeof *handles * (nhandles + 1));
    handles[nhandles++] = found;
    return nhandles;
}

void audio_stop(uint32_t handle) {
    if (!handle || !lock) return;
    SDL_LockMutex(lock);
    // 반복 효과음은 정지 호출 말고는 끝날 방법이 없으므로 끈다. 한 번 내는 효과음은 끝까지 울린다.
    for (int w = 0; w < WAVE_STREAMS; w++)
        if (waves[w].on && waves[w].repeat && waves[w].handle == handle && !waves[w].fade) waves[w].fade = WAVE_FADE_OUT;
    if (sfx_midi.on && sfx_midi.handle == handle && sfx_midi.repeat) { sfx_midi.on = false; if (sfx_synth) tsf_note_off_all(sfx_synth); }
    play_t *m = music_play();
    if (m && m->handle == handle && !sfx_scope) music_stop_now();
    SDL_UnlockMutex(lock);
}

void audio_play(uint32_t handle, bool repeat) {
    if (!handle || handle > nhandles) return;
    seq_t *seq = handles[handle - 1];
    if (seq->has_wave) {
        for (uint32_t i = 0; i < seq->n; i++)
            if (seq->ev[i].kind != 0) wave_start(&seq->ev[i], handle, repeat);
        return;
    }
    if (sfx_scope) { sfx_midi_start(seq, handle, repeat); return; }
    SDL_LockMutex(lock);
    play_t *m = music_play();
    if (solo) {
        SDL_UnlockMutex(lock);
        return;
    }
    if (m && m->s == seq) {
        // 효과음이 끝나 같은 곡을 다시 부르면 처음으로 돌리지 않고 핸들만 넘긴다.
        m->handle = handle;
        m->repeat = repeat;
    } else {
        music_stop_now();
        for (int i = 0; i < 16; i++) if (!plays[i].on) {
            plays[i] = (play_t){true, repeat, seq, 0, 0, handle};
            break;
        }
    }
    SDL_UnlockMutex(lock);
}

void audio_sfx_scope(bool in_sfx) { sfx_scope = in_sfx; }

void audio_music_stop(void) {
    if (!lock) return;
    SDL_LockMutex(lock);
    if (!solo) music_stop_now();
    SDL_UnlockMutex(lock);
}

void audio_free(uint32_t handle) { audio_stop(handle); }

// 게임을 다시 시작할 때 모든 소리를 끄고 핸들을 비운다. 바꿔 둔 곡은 남겨 다시 쓴다.
void audio_reset(void) {
    if (!lock) return;
    SDL_LockMutex(lock);
    music_stop_now();
    for (int w = 0; w < WAVE_STREAMS; w++) if (waves[w].on && !waves[w].fade) waves[w].fade = WAVE_FADE_OUT;
    if (sfx_midi.on) { sfx_midi.on = false; if (sfx_synth) tsf_note_off_all(sfx_synth); }
    nhandles = 0;
    SDL_UnlockMutex(lock);
}

// 곡을 바꿀 때 앞 곡은 페이드 뒤 synth_clear로 잔향까지 지워진다. 새 곡은 페이드가 끝나야 시작한다.
void audio_solo_play(uint32_t handle) {
    if (!handle || handle > nhandles || !lock) return;
    SDL_LockMutex(lock);
    solo = true;
    music_stop_now();
    for (int i = 0; i < 16; i++) if (!plays[i].on) {
        plays[i] = (play_t){true, false, handles[handle - 1], 0, 0, handle};
        break;
    }
    SDL_UnlockMutex(lock);
}

void audio_solo_stop(void) {
    if (!lock) return;
    SDL_LockMutex(lock);
    if (solo) music_stop_now();
    SDL_UnlockMutex(lock);
}

void audio_solo_end(void) {
    if (!lock) return;
    SDL_LockMutex(lock);
    if (solo) music_stop_now();
    solo = false;
    SDL_UnlockMutex(lock);
}

bool audio_solo_active(void) { return solo; }

bool audio_solo_playing(void) {
    if (!lock) return false;
    SDL_LockMutex(lock);
    bool on = solo && music_play();
    SDL_UnlockMutex(lock);
    return on;
}
