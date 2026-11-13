/* FF MUSIC's STEADY on the desktop: see ff_music.h. The Android app's notes (FfMusicRenderer.kt and the files beside
 * it) say why each step is done the way it is. GPL-3.0, see ../NOTICE. */
#include "ff_music.h"

#include "pocket_os.h"
#include "../app/src/main/cpp/pk_call.h"

#include <SDL.h>
#include <dirent.h>
#include <mgba-util/vfs.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#define ROM_BASE 0x08000000u
#define MAX_SONGS 4096
#define ALT (1 << 16)            /* an alternate song (Heart and Soul's second table) */
#define CAPTURE_SECONDS 100      /* the longest recording: a song with no loop seen in it is kept whole */
#define WATCHDOG_FRAMES (60 * (CAPTURE_SECONDS + 5))
#define LOOP_TAIL_SECONDS 3      /* past the loop's end: the clip starts 1 s in, plus the splice search */
#define BOOT_FRAMES 300
#define MAX_BOOT_FRAMES 1800
#define START_FRAMES 30
#define SAFE_CALL_FRAMES 60
#define ATTEMPTS 2
#define IDLE_MS 30000
#define BUDGET_BYTES (1536ull << 20) /* every ROM's clips together */
#define MAX_KEYS 1024

struct ffm {
  uint8_t *rom;
  size_t len;
  char dir[1100], top[1024];
  SDL_Thread *thread;
  SDL_mutex *lock;
  SDL_cond *wake;
  bool stop, located, supported;
  uint32_t songNumStart, idle;
  uint32_t headerXor;
  int count;
  uint32_t header[2 * MAX_SONGS];
  int id[2 * MAX_SONGS];
  uint32_t queue[64];
  int queued;
  uint32_t asked[MAX_KEYS], ready[MAX_KEYS], never[MAX_KEYS];
  int nAsked, nReady, nNever;
  /* The render core's, on its thread only. */
  struct mCore *core;
  uint32_t *video;
  int16_t *buf;
  size_t bufLen;
  int frameStart[WATCHDOG_FRAMES + 1];
  int64_t history[WATCHDOG_FRAMES + 1];
};

static bool has(const uint32_t *list, int n, uint32_t key) {
  for (int i = 0; i < n; i++)
    if (list[i] == key)
      return true;
  return false;
}
static void add(uint32_t *list, int *n, uint32_t key) {
  if (!has(list, *n, key) && *n < MAX_KEYS)
    list[(*n)++] = key;
}
static void drop(uint32_t *list, int *n, uint32_t key) {
  for (int i = 0; i < *n; i++)
    if (list[i] == key) {
      list[i] = list[--*n];
      return;
    }
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static unsigned le16(const uint8_t *p) { return p[0] | p[1] << 8; }

/* ---- Where the sound engine keeps its songs (M4aSongs.kt) ---- */

struct sig {
  const char *pattern;
  int tableLdr, altLdr;
  uint32_t headerXor;
};
static const struct sig SIGS[] = {
    {"00b50004074a0849400b401883885900c918890089180a680168101c00f0a2fb", 6, -1, 0},
    {"00b5..4b0004400bc018828853009b18..4a9b000168d058........01bc0047", 2, -1, 0},
    {"00b5..4a03045b0b9b1899884a005218..4992008858..4a416b914202d11968........01bc0047", 2, -1, 0},
    {"000430b5000c002916d0..4d2b685a1c12d02a0000210c3206e00c3213000c3b1b6801315c1c07d08342f6d1"
     "4b005b189b000433eb1802e0..4bc000c31899884a005218..4992008858..4a416b914202d11968........30bc01bc0047",
     0x38, 0x0A, 0},
    {"00b50004..4a..49400b401883885900c918890089180a680168802000064140101c", 6, -1, 0x80000000u},
};

static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0; }

static long find_sig(const uint8_t *rom, size_t len, const char *pattern) {
  uint8_t value[160], mask[160];
  size_t n = strlen(pattern) / 2;
  for (size_t k = 0; k < n; k++) {
    char hi = pattern[k * 2], lo = pattern[k * 2 + 1];
    value[k] = (uint8_t)((hi == '.' ? 0 : hexval(hi) << 4) | (lo == '.' ? 0 : hexval(lo)));
    mask[k] = (uint8_t)((hi == '.' ? 0 : 0xF0) | (lo == '.' ? 0 : 0x0F));
  }
  for (size_t i = 0; i + n <= len; i += 2) {
    if ((rom[i] & mask[0]) != value[0])
      continue;
    size_t j = 1;
    while (j < n && (rom[i + j] & mask[j]) == value[j])
      j++;
    if (j == n)
      return (long)i;
  }
  return -1;
}

/* The word a Thumb `ldr rX, [pc, #imm]` at rom[fn + off] loads, or 0. */
static uint32_t literal(const uint8_t *rom, size_t len, size_t fn, int off) {
  unsigned ins = le16(rom + fn + off);
  if (ins >> 11 != 9)
    return 0;
  size_t addr = ((fn + off + 4) & ~(size_t)3) + (ins & 0xFF) * 4;
  return addr + 4 <= len ? le32(rom + addr) : 0;
}

static bool in_rom(uint32_t a, size_t len) { return a >= ROM_BASE && a < ROM_BASE + len; }

static void map_song(struct ffm *f, uint32_t headerOff, int id) {
  for (int i = 0; i < f->count; i++)
    if (f->header[i] == headerOff)
      return; /* the first id that plays a header keeps it */
  if (f->count < 2 * MAX_SONGS) {
    f->header[f->count] = headerOff;
    f->id[f->count++] = id;
  }
}

bool ffm_locate_table(const uint8_t *rom, size_t len, uint32_t *songNumStart, uint32_t *table) {
  for (size_t s = 0; s < sizeof SIGS / sizeof SIGS[0]; s++) {
    long at = find_sig(rom, len, SIGS[s].pattern);
    if (at < 0)
      continue;
    uint32_t t = literal(rom, len, (size_t)at, SIGS[s].tableLdr);
    if (!in_rom(t, len))
      return false;
    *songNumStart = ROM_BASE + (uint32_t)at + 1; /* Thumb */
    *table = t;
    return true;
  }
  return false;
}

static bool locate(struct ffm *f) {
  const uint8_t *rom = f->rom;
  size_t len = f->len;
  for (size_t s = 0; s < sizeof SIGS / sizeof SIGS[0]; s++) {
    long at = find_sig(rom, len, SIGS[s].pattern);
    if (at < 0)
      continue;
    uint32_t table = literal(rom, len, (size_t)at, SIGS[s].tableLdr);
    if (!in_rom(table, len))
      return false;
    size_t t = table - ROM_BASE;
    int n = 0;
    while (n < MAX_SONGS && t + n * 8 + 8 <= len) {
      uint32_t header = le32(rom + t + n * 8) ^ SIGS[s].headerXor;
      if (!in_rom(header, len))
        break;
      map_song(f, header - ROM_BASE, n);
      n++;
    }
    if (SIGS[s].altLdr >= 0) {
      uint32_t alt = literal(rom, len, (size_t)at, SIGS[s].altLdr);
      if (in_rom(alt, len))
        for (size_t e = alt - ROM_BASE, k = 0; e + 12 <= len && k < MAX_SONGS; e += 12, k++) {
          uint32_t num = le32(rom + e), header = le32(rom + e + 4);
          if (num == 0xFFFFFFFFu || num > 0xFFFF || !in_rom(header, len))
            break;
          map_song(f, header - ROM_BASE, (int)num | ALT);
        }
    }
    f->idle = 0;
    for (size_t i = 0; i + 2 <= len; i += 2)
      if (rom[i] == 0xFE && rom[i + 1] == 0xE7) {
        f->idle = ROM_BASE + (uint32_t)i;
        break;
      }
    f->songNumStart = ROM_BASE + (uint32_t)at + 1;
    f->headerXor = SIGS[s].headerXor;
    return n > 0;
  }
  return false;
}

static int song_id(const struct ffm *f, uint32_t key) {
  for (int i = 0; i < f->count; i++)
    if (f->header[i] == key)
      return f->id[i];
  return -1;
}

/* ---- The song a core is playing (FfMusicKey.kt) and where it loops (M4aLoop.kt) ---- */

static bool in_ram(uint32_t a) { return (a >= 0x02000000u && a < 0x02040000u) || (a >= 0x03000000u && a < 0x03008000u); }
static uint32_t r32(struct mCore *c, uint32_t a) { return c->rawRead32(c, a, -1); }
static unsigned r8(struct mCore *c, uint32_t a) { return c->rawRead8(c, a, -1); }

/* The background-music player's MusicPlayerInfo: the tail of SoundInfo's player chain. */
static uint32_t bgm_player(struct mCore *c) {
  uint32_t info = r32(c, 0x03007FF0);
  if (!in_ram(info))
    return 0;
  uint32_t p = r32(c, info + 0x24), bgm = 0;
  for (int n = 0; p; n++) {
    if (!in_ram(p) || n >= 12)
      return 0;
    bgm = p;
    p = r32(c, p + 0x3C);
  }
  return bgm;
}

uint32_t ffm_key(struct mCore *c) {
  uint32_t bgm = bgm_player(c);
  if (!bgm || !(r32(c, bgm + 4) & 0xFFFF))
    return 0;
  uint32_t header = r32(c, bgm);
  return header >= ROM_BASE && header < 0x0A000000u ? header - ROM_BASE : 0;
}

struct loop_watch {
  int64_t *history;
  int frames, track, firstGoto, startFrame, endFrame;
};

static int64_t main_pos(struct mCore *c, struct loop_watch *w) {
  uint32_t bgm = bgm_player(c);
  if (!bgm)
    return -1;
  uint32_t tracks = r32(c, bgm + 0x2C);
  if (!in_ram(tracks))
    return -1;
  if (w->track < 0) {
    unsigned count = r8(c, bgm + 0x08);
    if (count > 16)
      count = 16;
    for (unsigned i = 0; i < count && w->track < 0; i++)
      if (r8(c, tracks + i * 0x50) & 0x80)
        w->track = (int)i;
    if (w->track < 0)
      return -1;
  }
  uint32_t t = tracks + (uint32_t)w->track * 0x50;
  uint32_t pos = r8(c, t + 0x02) == 0 ? r32(c, t + 0x40) : r32(c, t + 0x44);
  return pos >= ROM_BASE && pos < 0x0A000000u ? (int64_t)pos : -1;
}

/* The target of a GOTO (0xB2 + pointer) just past [prev] that lands at or before [now], or -1. */
static int64_t goto_target(struct mCore *c, int64_t prev, int64_t now) {
  uint8_t b[128];
  for (int i = 0; i < 128; i++)
    b[i] = (uint8_t)r8(c, (uint32_t)prev + i);
  for (int i = 0; i + 5 <= 128; i++) {
    if (b[i] != 0xB2)
      continue;
    uint32_t ptr = le32(b + i + 1);
    if (ptr >= ROM_BASE && ptr < 0x0A000000u && (int64_t)ptr <= now && (int64_t)ptr < prev + i)
      return ptr;
  }
  return -1;
}

static void loop_frame(struct mCore *c, struct loop_watch *w) {
  int f = w->frames;
  int64_t now = main_pos(c, w);
  w->history[w->frames++] = now;
  if (w->endFrame >= 0 || f == 0)
    return;
  int64_t prev = w->history[f - 1];
  if (now < 0 || prev < 0 || now >= prev)
    return;
  int64_t target = goto_target(c, prev, now);
  if (target < 0)
    return;
  int crossed = -1;
  for (int i = 0; i < f; i++)
    if (w->history[i] > target) {
      crossed = i;
      break;
    }
  if (crossed > 0 && w->history[crossed - 1] >= 0 && w->history[crossed - 1] <= target) {
    w->startFrame = crossed;
    w->endFrame = f;
  } else if (w->firstGoto >= 0) {
    w->startFrame = w->firstGoto;
    w->endFrame = f;
  } else {
    w->firstGoto = f;
  }
}

bool ffm_splice(const int16_t *pcm, size_t len, size_t loopStart, size_t loopEnd, int rate, size_t *from,
                size_t *clipLen) {
  long total = (long)len / 2, start = (long)loopStart / 2, p0 = ((long)loopEnd - (long)loopStart) / 2;
  long w = rate / 10, r = rate / 60 * 3;
  if (p0 <= r + w)
    return false;
  long s = start + rate;
  if (s + p0 + r + w > total)
    s = total - p0 - r - w;
  if (s < start)
    return false;
  long best = p0;
  int64_t bestErr = INT64_MAX;
  for (long p = p0 - r; p <= p0 + r; p++) {
    int64_t err = 0;
    for (long i = 0; i < w * 2 && err < bestErr; i++) {
      int64_t d = (int64_t)pcm[s * 2 + i] - pcm[(s + p) * 2 + i];
      err += d * d;
    }
    if (err < bestErr) {
      bestErr = err;
      best = p;
    }
  }
  *from = (size_t)s * 2;
  *clipLen = (size_t)best * 2;
  return true;
}

/* ---- The cache: DATA/ffmusic/v1/<crc>/song_<offset>.wav and its .intro.wav ---- */

static void clip_path(const struct ffm *f, uint32_t key, bool intro, char *out, size_t size) {
  snprintf(out, size, "%s/song_%07x%s.wav", f->dir, (unsigned)key, intro ? ".intro" : "");
}

static bool write_wav(const char *path, const int16_t *pcm, size_t shorts) {
  char tmp[2400];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *o = fopen(tmp, "wb");
  if (!o)
    return false;
  uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0};
  uint32_t data = (uint32_t)(shorts * 2), riff = 36 + data, rate = FFM_RATE, bytes = FFM_RATE * 4;
  memcpy(h + 4, &riff, 4);
  memcpy(h + 24, &rate, 4);
  memcpy(h + 28, &bytes, 4);
  h[32] = 4;
  h[34] = 16;
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &data, 4);
  bool ok = fwrite(h, 1, 44, o) == 44 && fwrite(pcm, 2, shorts, o) == shorts;
  ok = fclose(o) == 0 && ok;
  remove(path);
  return ok && rename(tmp, path) == 0;
}

static int16_t *read_wav(const char *path, size_t *frames) {
  FILE *in = fopen(path, "rb");
  if (!in)
    return NULL;
  uint8_t h[44];
  int16_t *pcm = NULL;
  if (fread(h, 1, 44, in) == 44 && !memcmp(h, "RIFF", 4) && !memcmp(h + 36, "data", 4) && le32(h + 24) == FFM_RATE) {
    size_t n = le32(h + 40) / 4;
    pcm = malloc(n ? n * 4 : 4);
    if (pcm && fread(pcm, 4, n, in) != n) {
      free(pcm);
      pcm = NULL;
    }
    *frames = n;
  }
  fclose(in);
  return pcm;
}

/* Keeps every ROM's clips under BUDGET_BYTES: the songs played longest ago (a song's loop file's time) go first. */
static void trim(struct ffm *f, uint32_t keep) {
  struct song {
    char path[2400];
    uint64_t bytes;
    time_t when;
  };
  size_t n = 0, cap = 256;
  struct song *songs = malloc(cap * sizeof *songs);
  uint64_t total = 0;
  DIR *top = songs ? opendir(f->top) : NULL;
  struct dirent *d;
  while (top && (d = readdir(top))) {
    if (d->d_name[0] == '.')
      continue;
    char sub[2400];
    snprintf(sub, sizeof sub, "%s/%s", f->top, d->d_name);
    DIR *dir = opendir(sub);
    struct dirent *e;
    while (dir && (e = readdir(dir))) {
      size_t l = strlen(e->d_name);
      char path[2400];
      struct stat st;
      snprintf(path, sizeof path, "%s/%s", sub, e->d_name);
      if (stat(path, &st))
        continue;
      total += (uint64_t)st.st_size;
      if (l < 5 || strcmp(e->d_name + l - 4, ".wav") || (l > 10 && !strcmp(e->d_name + l - 10, ".intro.wav")))
        continue;
      if (n == cap) {
        struct song *more = realloc(songs, (cap *= 2) * sizeof *songs);
        if (!more)
          break;
        songs = more;
      }
      snprintf(songs[n].path, sizeof songs[n].path, "%s", path);
      songs[n].path[strlen(songs[n].path) - 4] = 0; /* without ".wav" */
      songs[n].bytes = (uint64_t)st.st_size;
      songs[n].when = st.st_mtime;
      n++;
    }
    if (dir)
      closedir(dir);
  }
  if (top)
    closedir(top);
  char kept[2400];
  clip_path(f, keep, false, kept, sizeof kept);
  kept[strlen(kept) - 4] = 0;
  while (total > BUDGET_BYTES) {
    size_t oldest = n;
    for (size_t i = 0; i < n; i++)
      if (strcmp(songs[i].path, kept) && songs[i].bytes && (oldest == n || songs[i].when < songs[oldest].when))
        oldest = i;
    if (oldest == n)
      break;
    char path[2410];
    struct stat st;
    snprintf(path, sizeof path, "%s.intro.wav", songs[oldest].path);
    if (!stat(path, &st))
      total -= (uint64_t)st.st_size;
    remove(path);
    snprintf(path, sizeof path, "%s.wav", songs[oldest].path);
    remove(path);
    total -= songs[oldest].bytes;
    songs[oldest].bytes = 0;
    unsigned k;
    const char *name = strrchr(songs[oldest].path, '/');
    if (name && sscanf(name, "/song_%x", &k) == 1 && !strncmp(songs[oldest].path, f->dir, strlen(f->dir))) {
      SDL_LockMutex(f->lock);
      drop(f->ready, &f->nReady, k);
      drop(f->asked, &f->nAsked, k);
      SDL_UnlockMutex(f->lock);
    }
  }
  free(songs);
}

/* ---- The render core (pokedaisy_jni.c's rg, FfMusicRenderer's RenderCore) ---- */

static int16_t scratch[4096 * 2];

static int drain(struct mCore *c) {
  blip_t *l = c->getAudioChannel(c, 0), *r = c->getAudioChannel(c, 1);
  int n = blip_samples_avail(l);
  if (n > 4096)
    n = 4096;
  if (n <= 0)
    return 0;
  blip_read_samples(l, scratch, n, 1);
  blip_read_samples(r, scratch + 1, n, 1);
  return n * 2;
}

static void run_frame(struct mCore *c) {
  c->runFrame(c);
  drain(c);
}

/* Parks the main loop on a `b .` once it runs with the VBlank interrupt on: the game can't change the music while a
 * song records, and the interrupt keeps the sound engine going. */
static bool park(struct mCore *c, uint32_t spin) {
  struct ARMCore *cpu = (struct ARMCore *)c->cpu;
  for (int i = 0; i < 5000000; i++) {
    if ((cpu->privilegeMode == MODE_SYSTEM || cpu->privilegeMode == MODE_USER) && !cpu->cpsr.i && !cpu->halted &&
        (c->busRead16(c, 0x04000208) & 1) && (c->busRead16(c, 0x04000200) & 1) && (c->busRead16(c, 0x04000004) & 8)) {
      cpu->gprs[ARM_PC] = (int32_t)(spin & ~1u);
      _ARMSetMode(cpu, MODE_THUMB);
      ThumbWritePC(cpu);
      return true;
    }
    c->step(c);
  }
  return false;
}

static void core_close(struct ffm *f) {
  if (f->core) {
    f->core->deinit(f->core);
    f->core = NULL;
  }
  free(f->video);
  f->video = NULL;
}

static bool core_open(struct ffm *f) {
  uint8_t *copy = malloc(f->len);
  if (!copy)
    return false;
  memcpy(copy, f->rom, f->len);
  struct VFile *vf = VFileFromMemory(copy, f->len);
  struct mCore *c = vf ? mCoreFindVF(vf) : NULL;
  if (!c || !c->init(c)) {
    if (vf)
      vf->close(vf);
    free(copy);
    return false;
  }
  mCoreInitConfig(c, NULL);
  unsigned w, h;
  c->desiredVideoDimensions(c, &w, &h);
  f->video = calloc((size_t)w * h, sizeof(uint32_t));
  c->setVideoBuffer(c, (void *)f->video, w);
  c->setAudioBufferSize(c, 2048);
  blip_set_rates(c->getAudioChannel(c, 0), c->frequency(c), FFM_RATE);
  blip_set_rates(c->getAudioChannel(c, 1), c->frequency(c), FFM_RATE);
  if (!c->loadROM(c, vf)) {
    vf->close(vf);
    free(copy);
    mCoreConfigDeinit(&c->config);
    c->deinit(c);
    return false;
  }
  c->reset(c); /* no save: this core never sees the player's */
  f->core = c;
  for (int i = 0; i < BOOT_FRAMES; i++)
    run_frame(c);
  for (int i = BOOT_FRAMES; i < MAX_BOOT_FRAMES && !ffm_key(c) && !f->stop; i++)
    run_frame(c);
  if (f->idle)
    park(c, f->idle);
  return true;
}

/* m4aSongNumStart(n, alt), a frame at a time until a frame lets it run. */
static bool force_song(struct mCore *c, uint32_t fn, uint32_t number, uint32_t alt) {
  const uint32_t args[4] = {number, alt, 0, 0};
  for (int i = 0; i < SAFE_CALL_FRAMES; i++) {
    if (pk_call(c, fn & ~1u, args, NULL))
      return true;
    run_frame(c);
  }
  return false;
}

enum outcome { CACHED, ENDED, FAILED, STOPPED };

static enum outcome record(struct ffm *f, int id) {
  struct mCore *c = f->core;
  size_t cap = (size_t)FFM_RATE * 2 * CAPTURE_SECONDS;
  if (!f->buf) {
    f->buf = malloc(cap * sizeof *f->buf);
    if (!f->buf)
      return FAILED;
  }
  for (int attempt = 0; attempt < ATTEMPTS; attempt++) {
    if (!force_song(c, f->songNumStart, (uint32_t)id & 0xFFFF, (id & ALT) ? 1 : 0))
      return ENDED;
    uint32_t key = 0;
    for (int i = 0; i < START_FRAMES && !key; i++) {
      run_frame(c);
      key = ffm_key(c);
    }
    if (!key)
      return ENDED;
    SDL_LockMutex(f->lock);
    bool done = has(f->ready, f->nReady, key);
    SDL_UnlockMutex(f->lock);
    if (done)
      return CACHED;
    struct loop_watch w = {f->history, 0, -1, -1, -1, -1};
    size_t pos = 0, stopAt = cap;
    int frames = 0;
    bool spoiled = false, ended = false;
    while (pos < stopAt && frames < WATCHDOG_FRAMES && !f->stop) {
      f->frameStart[frames++] = (int)pos;
      c->runFrame(c);
      int n = drain(c);
      uint32_t now = ffm_key(c);
      if (now != key) {
        ended = now == 0;
        spoiled = true;
        break;
      }
      size_t take = (size_t)n < cap - pos ? (size_t)n : cap - pos;
      memcpy(f->buf + pos, scratch, take * sizeof *scratch);
      pos += take;
      loop_frame(c, &w);
      if (w.endFrame >= 0 && stopAt == cap) {
        size_t tail = pos + (size_t)FFM_RATE * 2 * LOOP_TAIL_SECONDS;
        stopAt = tail < cap ? tail : cap;
      }
    }
    if (f->stop)
      return STOPPED;
    if (ended)
      return ENDED;
    if (spoiled)
      continue;
    size_t from = 0, len = pos;
    if (w.endFrame >= 0 &&
        !ffm_splice(f->buf, pos, (size_t)f->frameStart[w.startFrame], (size_t)f->frameStart[w.endFrame], FFM_RATE,
                    &from, &len))
      w.endFrame = -1;
    if (w.endFrame < 0 && pos < cap)
      continue; /* no loop, and not kept whole either */
    char intro[2400], loop[2400];
    clip_path(f, key, true, intro, sizeof intro);
    clip_path(f, key, false, loop, sizeof loop);
    if (!write_wav(intro, f->buf, from) || !write_wav(loop, f->buf + from, len))
      return FAILED;
    SDL_LockMutex(f->lock);
    add(f->ready, &f->nReady, key);
    SDL_UnlockMutex(f->lock);
    fprintf(stderr, "FF music: recorded song %d as song_%07x, a %.1f s loop\n", id & 0xFFFF, (unsigned)key,
            len / 2.0 / FFM_RATE);
    trim(f, key);
    return CACHED;
  }
  return FAILED;
}

static void scan_cache(struct ffm *f) {
  DIR *dir = opendir(f->dir);
  struct dirent *e;
  while (dir && (e = readdir(dir))) {
    unsigned k;
    char tail[16] = "";
    if (sscanf(e->d_name, "song_%x%15s", &k, tail) == 2 && !strcmp(tail, ".wav")) {
      char intro[2400];
      struct stat st;
      clip_path(f, k, true, intro, sizeof intro);
      if (!stat(intro, &st))
        add(f->ready, &f->nReady, k);
    }
  }
  if (dir)
    closedir(dir);
}

static int run(void *arg) {
  struct ffm *f = arg;
  bool ok = locate(f);
  scan_cache(f);
  SDL_LockMutex(f->lock);
  f->supported = ok;
  f->located = true;
  SDL_UnlockMutex(f->lock);
  if (!ok) {
    fprintf(stderr, "FF music: this ROM's sound engine isn't one STEADY knows\n");
    return 0;
  }
  while (true) {
    SDL_LockMutex(f->lock);
    while (!f->queued && !f->stop) {
      if (SDL_CondWaitTimeout(f->wake, f->lock, IDLE_MS) == SDL_MUTEX_TIMEDOUT && f->core) {
        SDL_UnlockMutex(f->lock);
        core_close(f); /* idle a while: its memory back until the next song */
        SDL_LockMutex(f->lock);
      }
    }
    if (f->stop) {
      SDL_UnlockMutex(f->lock);
      break;
    }
    uint32_t key = f->queue[0];
    memmove(f->queue, f->queue + 1, (size_t)--f->queued * sizeof f->queue[0]);
    bool done = has(f->ready, f->nReady, key);
    SDL_UnlockMutex(f->lock);
    if (done)
      continue;
    int id = song_id(f, key);
    enum outcome o = FAILED;
    if (id >= 0 && (f->core || core_open(f)))
      o = record(f, id);
    if (o == STOPPED)
      break;
    if (o != CACHED) {
      SDL_LockMutex(f->lock);
      add(f->never, &f->nNever, key);
      SDL_UnlockMutex(f->lock);
    }
  }
  core_close(f);
  return 0;
}

struct ffm *ffm_start(const uint8_t *rom, size_t len, uint32_t crc, const char *root) {
  struct ffm *f = calloc(1, sizeof *f);
  if (!f || !(f->rom = malloc(len))) {
    free(f);
    return NULL;
  }
  memcpy(f->rom, rom, len);
  f->len = len;
  char path[1024];
  snprintf(path, sizeof path, "%s/ffmusic", root);
  pocket_mkdir(path);
  snprintf(f->top, sizeof f->top, "%s/ffmusic/v1", root);
  pocket_mkdir(f->top);
  snprintf(f->dir, sizeof f->dir, "%s/%08x", f->top, (unsigned)crc);
  pocket_mkdir(f->dir);
  f->lock = SDL_CreateMutex();
  f->wake = SDL_CreateCond();
  f->thread = SDL_CreateThread(run, "ffmusic", f);
  if (!f->thread) {
    f->located = true; /* supported stays false: everything plays sped up */
  }
  return f;
}

void ffm_stop(struct ffm *f) {
  if (!f)
    return;
  SDL_LockMutex(f->lock);
  f->stop = true;
  SDL_CondSignal(f->wake);
  SDL_UnlockMutex(f->lock);
  if (f->thread)
    SDL_WaitThread(f->thread, NULL);
  SDL_DestroyCond(f->wake);
  SDL_DestroyMutex(f->lock);
  free(f->buf);
  free(f->rom);
  free(f);
}

void ffm_request(struct ffm *f, uint32_t key) {
  if (!f || !key)
    return;
  SDL_LockMutex(f->lock);
  if (has(f->ready, f->nReady, key)) {
    char path[2400];
    clip_path(f, key, false, path, sizeof path);
    utime(path, NULL); /* played now: kept over songs played longer ago */
  } else if (!has(f->asked, f->nAsked, key) && !has(f->never, f->nNever, key) && f->queued < 64) {
    add(f->asked, &f->nAsked, key);
    memmove(f->queue + 1, f->queue, (size_t)f->queued++ * sizeof f->queue[0]);
    f->queue[0] = key; /* the newest song first */
    SDL_CondSignal(f->wake);
  }
  SDL_UnlockMutex(f->lock);
}

enum ffm_state ffm_state(struct ffm *f, uint32_t key) {
  if (!f || !key)
    return FFM_NEVER;
  SDL_LockMutex(f->lock);
  enum ffm_state s = has(f->ready, f->nReady, key)                                 ? FFM_READY
                     : (f->located && !f->supported) || has(f->never, f->nNever, key) ? FFM_NEVER
                                                                                      : FFM_COMING;
  SDL_UnlockMutex(f->lock);
  return s;
}

bool ffm_load(struct ffm *f, uint32_t key, struct ffm_clip *out) {
  char intro[2400], loop[2400];
  memset(out, 0, sizeof *out);
  clip_path(f, key, true, intro, sizeof intro);
  clip_path(f, key, false, loop, sizeof loop);
  out->key = key;
  out->loop = read_wav(loop, &out->loopFrames);
  out->intro = read_wav(intro, &out->introFrames);
  if (!out->loop || !out->loopFrames) {
    ffm_clip_free(out);
    SDL_LockMutex(f->lock);
    drop(f->ready, &f->nReady, key); /* gone (made room for others): recorded again */
    drop(f->asked, &f->nAsked, key);
    SDL_UnlockMutex(f->lock);
    return false;
  }
  return true;
}

void ffm_clip_free(struct ffm_clip *clip) {
  free(clip->intro);
  free(clip->loop);
  memset(clip, 0, sizeof *clip);
}

void ffm_clip_fill(struct ffm_clip *clip, int16_t *out, size_t frames) {
  for (size_t i = 0; i < frames; i++, clip->at++) {
    const int16_t *s;
    if (clip->at < clip->introFrames)
      s = clip->intro + clip->at * 2;
    else
      s = clip->loop + ((clip->at - clip->introFrames) % clip->loopFrames) * 2;
    out[i * 2] = s[0];
    out[i * 2 + 1] = s[1];
  }
}
