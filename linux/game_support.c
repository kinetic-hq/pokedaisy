/* GPL-3.0, see ../NOTICE. The game grid's support tag (game_support.h). */
#include "game_support.h"
#include "pd_dex.h"
#include "pd_game.h"
#include "pd_guide.h"
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *game_support_word(int level) {
  return level == GAME_SUPPORT_FULL     ? "FULL"
         : level == GAME_SUPPORT_PARTLY ? "PARTLY"
         : level == GAME_SUPPORT_PLAY   ? "PLAY ONLY"
                                        : NULL;
}

static bool ends_with(const char *s, const char *tail) {
  size_t n = strlen(s), m = strlen(tail);
  if (n < m)
    return false;
  for (size_t i = 0; i < m; i++)
    if ((s[n - m + i] | 0x20) != (tail[i] | 0x20))
      return false;
  return true;
}

/* DS: the carts the desktop's DS companion reads (linux/dsperate/frontend.cpp): Platinum (USA, CPUE: Renegade
 * Platinum too) and HeartGold / SoulSilver (USA rev 0). Everything else plays without one. */
static int ds_support(const char *path, char *detail, size_t size) {
  FILE *f = fopen(path, "rb");
  unsigned char head[0x30] = {0}, mark[4] = {0}, originMark[4] = {0};
  size_t n = f ? fread(head, 1, sizeof head, f) : 0;
  // Renegade Platinum's mark (linux/dsperate/companion.cpp's ds_renegade_platinum): overlay 16's pointer to its type
  // chart. Origin HeartGold's (ds_origin_cart): SaveData_Get's pool word in its ARM9.
  bool renegade = f && !fseek(f, 0x221060, SEEK_SET) && fread(mark, 1, 4, f) == 4 &&
                  (mark[0] | mark[1] << 8 | mark[2] << 16 | (uint32_t)mark[3] << 24) == 0x021E9F81u;
  bool origin = f && !fseek(f, 0x4000 + 0x2773C, SEEK_SET) && fread(originMark, 1, 4, f) == 4 &&
                (originMark[0] | originMark[1] << 8 | originMark[2] << 16 | (uint32_t)originMark[3] << 24) == 0x021D11B0u;
  if (f)
    fclose(f);
  if (n < sizeof head) {
    snprintf(detail, size, "Couldn't read the cart");
    return GAME_SUPPORT_UNKNOWN;
  }
  if (!memcmp(head + 12, "CPUE", 4) && head[30] <= 1) {
    // Renegade Platinum reads its own tables (docs/manual/desktop-game-support.md); its trainers are its own: no GUIDE.
    if (head[30] == 1 && renegade) {
      snprintf(detail, size, "Every companion tab but GUIDE");
      return GAME_SUPPORT_PARTLY;
    }
    snprintf(detail, size, "Every companion tab");
    return GAME_SUPPORT_FULL;
  }
  if (!memcmp(head + 12, "IPKJ", 4) && !head[30] && origin) {
    snprintf(detail, size, "Every companion tab; GUIDE has its wild creatures only");
    return GAME_SUPPORT_PARTLY;
  }
  // HeartFelt Gold (ds_heartfelt_cart): HeartGold's header with its ARM9 grown to 0x111EF8 bytes.
  if (!memcmp(head + 12, "IPKE", 4) && !head[30] &&
      (head[0x2C] | head[0x2D] << 8 | head[0x2E] << 16 | (uint32_t)head[0x2F] << 24) == 0x111EF8u) {
    snprintf(detail, size, "PARTY, BAG and MAP: no BATTLE, DEX or GUIDE yet");
    return GAME_SUPPORT_PARTLY;
  }
  if ((!memcmp(head + 12, "IPKE", 4) || !memcmp(head + 12, "IPGE", 4)) && !head[30]) {
    // A retail HeartGold / SoulSilver is 128 MB; a bigger one is a hack read as the game its header names.
    struct stat st;
    if (!stat(path, &st) && st.st_size == (128l << 20)) {
      snprintf(detail, size, "Every companion tab");
      return GAME_SUPPORT_FULL;
    }
    snprintf(detail, size, "Read as %s by its header: a hack's changes aren't followed", head[15] == 'E' && head[14] == 'K' ? "HeartGold" : "SoulSilver");
    return GAME_SUPPORT_PARTLY;
  }
  snprintf(detail, size, "No DS companion for this cart (%.4s) yet: the game runs", (const char *)head + 12);
  return GAME_SUPPORT_PLAY;
}

int game_support_of_rom(const char *path, char *detail, size_t size) {
  detail[0] = 0;
  if (ends_with(path, ".nds"))
    return ds_support(path, detail, size);
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(detail, size, "Couldn't read the game");
    return GAME_SUPPORT_UNKNOWN;
  }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (len < 0x150 || len > (64l << 20)) {
    fclose(f);
    snprintf(detail, size, "Not a cart the companion knows");
    return GAME_SUPPORT_PLAY;
  }
  uint8_t *rom = malloc((size_t)len);
  bool ok = rom && fread(rom, 1, (size_t)len, f) == (size_t)len;
  fclose(f);
  if (!ok) {
    free(rom);
    snprintf(detail, size, "Couldn't read the game");
    return GAME_SUPPORT_UNKNOWN;
  }
  struct pd_game g;
  pd_game_detect(&g, rom, (size_t)len);
  g.rom = rom;
  g.romSize = (size_t)len;
  pd_game_identify(&g);
  int level;
  if (!g.cfg) {
    // (pd_game.c's reasons were written for the 3DS port; on the desktop a hack it doesn't know just runs.)
    const char *why = g.unsupported && !strstr(g.unsupported, "3DS") ? g.unsupported : NULL;
    snprintf(detail, size, "%s", why ? why : "A version the companion doesn't know yet: the game runs");
    level = GAME_SUPPORT_PLAY;
  } else {
    // The tabs the companion shows for it (pd_ui.c's ui_tab_available) and the battle's foe team.
    const struct pd_config *cfg = g.cfg;
    bool dex = pd_dex_tab(&g);
    bool guide = pd_guide_for(&g) != NULL;
    bool foes = cfg->enemyParty != 0;
    char missing[96] = "";
    if (!dex)
      strcat(missing, "DEX");
    if (!guide)
      strcat(strcat(missing, *missing ? ", " : ""), "GUIDE");
    if (!foes)
      strcat(strcat(missing, *missing ? ", " : ""), "FOE TEAM");
    if (*missing) {
      snprintf(detail, size, "No %s", missing);
      level = GAME_SUPPORT_PARTLY;
    } else {
      snprintf(detail, size, "Every companion tab");
      level = GAME_SUPPORT_FULL;
    }
  }
  free(rom);
  return level;
}

/* --- the library in the background, cached as DATA/support-cache.tsv: the stamp, then a line a game (size, time,
 * level, detail, path) --- */

struct entry {
  int level;
  char detail[96];
};
struct cached {
  long long size, time;
  int level;
  char detail[96], path[1024];
};
static SDL_Thread *thread;
static SDL_atomic_t stop, generation;
static SDL_mutex *lock;
static struct entry *entries;
static char **paths;
static int count;
static char cachePath[1100], stampText[64];

static void file_key(const char *path, long long *size, long long *time) {
  struct stat st;
  *size = *time = -1;
  if (!stat(path, &st)) {
    *size = (long long)st.st_size;
    *time = (long long)st.st_mtime;
  }
}

static struct cached *read_cache(int *n) {
  struct cached *old = NULL;
  int cap = 0;
  *n = 0;
  FILE *f = fopen(cachePath, "r");
  if (!f)
    return NULL;
  char line[1400];
  size_t stampLen = strlen(stampText);
  bool same = fgets(line, sizeof line, f) && !strncmp(line, stampText, stampLen) && line[stampLen] == '\n';
  while (same && fgets(line, sizeof line, f)) {
    if (*n == cap) {
      cap = cap ? cap * 2 : 64;
      void *grown = realloc(old, (size_t)cap * sizeof *old);
      if (!grown)
        break;
      old = grown;
    }
    struct cached *c = &old[*n];
    // (sscanf, not strtok: the grid's thread may be using strtok's state.)
    if (sscanf(line, "%lld\t%lld\t%d\t%95[^\t]\t%1023[^\n]", &c->size, &c->time, &c->level, c->detail, c->path) == 5)
      (*n)++;
  }
  fclose(f);
  return old;
}

static int work(void *unused) {
  (void)unused;
  int oldCount = 0;
  struct cached *old = read_cache(&oldCount);
  bool changed = false;
  for (int i = 0; i < count && !SDL_AtomicGet(&stop); i++) {
    long long size, time;
    file_key(paths[i], &size, &time);
    int level = -1;
    char detail[96] = "";
    for (int j = 0; j < oldCount; j++)
      if (old[j].size == size && old[j].time == time && !strcmp(old[j].path, paths[i])) {
        level = old[j].level;
        snprintf(detail, sizeof detail, "%s", strcmp(old[j].detail, "-") ? old[j].detail : "");
        break;
      }
    if (level < 0) {
      level = game_support_of_rom(paths[i], detail, sizeof detail);
      changed = true;
    }
    SDL_LockMutex(lock);
    entries[i].level = level;
    snprintf(entries[i].detail, sizeof entries[i].detail, "%s", detail);
    SDL_UnlockMutex(lock);
    SDL_AtomicAdd(&generation, 1);
  }
  free(old);
  FILE *f;
  if (changed && !SDL_AtomicGet(&stop) && (f = fopen(cachePath, "w"))) {
    fprintf(f, "%s\n", stampText);
    for (int i = 0; i < count; i++) {
      long long size, time;
      file_key(paths[i], &size, &time);
      if (entries[i].level != GAME_SUPPORT_UNKNOWN && !strchr(paths[i], '\t') && !strchr(paths[i], '\n'))
        fprintf(f, "%lld\t%lld\t%d\t%s\t%s\n", size, time, entries[i].level,
                entries[i].detail[0] ? entries[i].detail : "-", paths[i]);
    }
    fclose(f);
  }
  return 0;
}

void game_support_stop(void) {
  if (thread) {
    SDL_AtomicSet(&stop, 1);
    SDL_WaitThread(thread, NULL);
    thread = NULL;
  }
  if (lock)
    SDL_LockMutex(lock);
  for (int i = 0; i < count; i++)
    free(paths[i]);
  free(paths);
  free(entries);
  paths = NULL;
  entries = NULL;
  count = 0;
  if (lock)
    SDL_UnlockMutex(lock);
}

void game_support_start(const char *root, const char *const *list, int n, const char *stamp) {
  game_support_stop();
  if (!lock)
    lock = SDL_CreateMutex();
  snprintf(cachePath, sizeof cachePath, "%s/support-cache.tsv", root);
  // The rules' own revision too, so a build that changes them looks again within a version.
  snprintf(stampText, sizeof stampText, "pokedaisy %s rules 5", stamp ? stamp : "?");
  entries = calloc((size_t)n + 1, sizeof *entries);
  paths = calloc((size_t)n + 1, sizeof *paths);
  if (!entries || !paths || !lock)
    return;
  for (int i = 0; i < n; i++)
    paths[i] = strdup(list[i] ? list[i] : "");
  count = n;
  SDL_AtomicSet(&stop, 0);
  thread = SDL_CreateThread(work, "support", NULL);
}

int game_support_get(int i, char *detail, size_t size) {
  int level = GAME_SUPPORT_UNKNOWN;
  if (detail && size)
    detail[0] = 0;
  if (!lock)
    return level;
  SDL_LockMutex(lock);
  if (i >= 0 && i < count && entries) {
    level = entries[i].level;
    if (detail && size)
      snprintf(detail, size, "%s", entries[i].detail);
  }
  SDL_UnlockMutex(lock);
  return level;
}

int game_support_generation(void) { return SDL_AtomicGet(&generation); }
