/* GPL-3.0, see ../NOTICE. The game list's box art (boxart.h). */
#include "boxart.h"
#include "http.h"
#include "pocket_os.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO_FAILURE_STRINGS
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#include "../third_party/stb/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define MAX_IMAGE (8u << 20)
#define MAX_JSON (1u << 20)
#define ART_MAX 512 // textures are scaled down to this on their long side

static char root[1024];
static SDL_mutex *lock;
static SDL_Thread *worker;
static SDL_atomic_t quit, generation, finding;
static struct boxart_game *queue, findGame;
static int queueCount, queueNext;
static bool findWaiting;
static int findResult = -1;

static const char *const LIBRETRO_SYSTEMS[] = {"Nintendo - Game Boy Advance", "Nintendo - Game Boy Color",
                                               "Nintendo - Game Boy", "Nintendo - Nintendo DS"};

static int working; // the game being looked for now (1) or none (0), for the progress (under lock)
static bool forceAll; // SYNC BOX ART > ALL AGAIN: this queue looks again even where art was found (under lock)

/* GBA game codes (3 letters at 0xAC), DS game codes (at 0x0C) and Game Boy titles (at 0x134): the libretro name. */
static const struct {
  const char *key, *name;
  int system;
} BASES[] = {
    {"AXV", "Pokemon - Ruby Version (USA, Europe)", BOXART_GBA},
    {"AXP", "Pokemon - Sapphire Version (USA, Europe)", BOXART_GBA},
    {"BPE", "Pokemon - Emerald Version (USA, Europe)", BOXART_GBA},
    {"BPR", "Pokemon - FireRed Version (USA, Europe)", BOXART_GBA},
    {"BPG", "Pokemon - LeafGreen Version (USA, Europe)", BOXART_GBA},
    {"CPU", "Pokemon - Platinum Version (USA)", BOXART_NDS},
    {"IPK", "Pokemon - HeartGold Version (USA)", BOXART_NDS},
    {"IPG", "Pokemon - SoulSilver Version (USA)", BOXART_NDS},
    {"IRB", "Pokemon - Black Version (USA, Europe) (NDSi Enhanced)", BOXART_NDS},
    {"IRA", "Pokemon - White Version (USA, Europe) (NDSi Enhanced)", BOXART_NDS},
    {"IRE", "Pokemon - Black Version 2 (USA, Europe) (NDSi Enhanced)", BOXART_NDS},
    {"IRD", "Pokemon - White Version 2 (USA, Europe) (NDSi Enhanced)", BOXART_NDS},
    {"POKEMON RED", "Pokemon - Red Version (USA, Europe) (SGB Enhanced)", BOXART_GB},
    {"POKEMON BLUE", "Pokemon - Blue Version (USA, Europe) (SGB Enhanced)", BOXART_GB},
    {"POKEMON YELLOW", "Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced)", BOXART_GB},
    {"POKEMON_GLD", "Pokemon - Gold Version (USA, Europe) (SGB Enhanced) (GB Compatible)", BOXART_GBC},
    {"POKEMON_SLV", "Pokemon - Silver Version (USA, Europe) (SGB Enhanced) (GB Compatible)", BOXART_GBC},
    {"PM_CRYSTAL", "Pokemon - Crystal Version (USA, Europe) (Rev 1)", BOXART_GBC},
};

bool boxart_base_of_rom(const char *path, char *name, size_t size, int *system) {
  name[0] = 0;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  unsigned char head[0x150];
  size_t n = fread(head, 1, sizeof head, f);
  fclose(f);
  if (n < sizeof head)
    return false;
  char gba[4] = {(char)head[0xAC], (char)head[0xAD], (char)head[0xAE], 0};
  char nds[4] = {(char)head[0x0C], (char)head[0x0D], (char)head[0x0E], 0};
  char gb[17];
  memcpy(gb, head + 0x134, 16);
  gb[16] = 0;
  for (int i = 0; i < 16; i++)
    if ((unsigned char)gb[i] < 0x20 || (unsigned char)gb[i] > 0x7E)
      gb[i] = 0;
  for (size_t i = 0; i < sizeof BASES / sizeof *BASES; i++) {
    const char *key = BASES[i].key;
    bool hit = BASES[i].system == BOXART_GBA   ? !strcmp(gba, key)
               : BASES[i].system == BOXART_NDS ? !strcmp(nds, key)
                                                : !strncmp(gb, key, strlen(key));
    if (hit) {
      snprintf(name, size, "%s", BASES[i].name);
      *system = BASES[i].system;
      return true;
    }
  }
  return false;
}

static void path_of(char *out, size_t size, const char *stem, const char *kind) {
  if (!strcmp(kind, "custom"))
    snprintf(out, size, "%s/boxart/custom/%s.img", root, stem);
  else
    snprintf(out, size, "%s/boxart/%s.%s", root, stem, kind);
}

void boxart_libretro_name(const char *stem, char *out, size_t size) {
  snprintf(out, size, "%s", stem);
  for (char *p = out; *p; p++)
    if (strchr("&*/:`<>?\\|\"", *p))
      *p = '_';
}

static bool write_file(const char *path, const void *data, size_t len) {
  char tmp[1500];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return false;
  bool ok = fwrite(data, 1, len, f) == len;
  ok = fclose(f) == 0 && ok;
  if (ok)
    ok = pocket_replace(tmp, path);
  if (!ok)
    pocket_remove(tmp);
  return ok;
}

static bool is_image(const void *data, size_t len) {
  int w, h, n;
  return len > 0 && len < MAX_IMAGE && stbi_info_from_memory(data, (int)len, &w, &h, &n) && w > 0 && h > 0 &&
         w <= 8192 && h <= 8192;
}

/* The string value after "key": in json from `from`, with \/ unescaped. */
static bool json_value(const char *from, const char *key, char *out, size_t size) {
  char pat[64];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char *p = strstr(from, pat);
  if (!p)
    return false;
  p += strlen(pat);
  while (*p == ' ' || *p == ':')
    p++;
  size_t n = 0;
  if (*p == '"') {
    for (p++; *p && *p != '"' && n + 1 < size; p++) {
      if (*p == '\\' && p[1])
        p++;
      out[n++] = *p;
    }
  } else {
    while ((isdigit((unsigned char)*p)) && n + 1 < size)
      out[n++] = *p++;
  }
  out[n] = 0;
  return n > 0;
}

static char *fetch_image(const char *url, const char *header, size_t *len) {
  char *data = http_get(url, header, MAX_IMAGE, len);
  if (data && !is_image(data, *len)) {
    free(data);
    data = NULL;
  }
  return data;
}

static char *libretro_named(const char *name, int system, size_t *len) {
  char sys[128], enc[800], url[1024];
  http_encode(LIBRETRO_SYSTEMS[system >= 0 && system <= BOXART_NDS ? system : 0], sys, sizeof sys);
  char safe[256];
  boxart_libretro_name(name, safe, sizeof safe);
  http_encode(safe, enc, sizeof enc);
  snprintf(url, sizeof url, "https://thumbnails.libretro.com/%s/Named_Boxarts/%s.png", sys, enc);
  return fetch_image(url, NULL, len);
}

/* The libretro thumbnails: the file name as it is, then without [tags]. */
static char *from_libretro(const struct boxart_game *g, size_t *len) {
  char names[2][256];
  boxart_libretro_name(g->stem, names[0], sizeof names[0]);
  // Without [b] / [T+Eng] and the like, trimmed.
  size_t n = 0;
  int depth = 0;
  for (const char *p = names[0]; *p && n + 1 < sizeof names[1]; p++) {
    if (*p == '[')
      depth++;
    else if (*p == ']' && depth)
      depth--;
    else if (!depth)
      names[1][n++] = *p;
  }
  while (n && names[1][n - 1] == ' ')
    n--;
  names[1][n] = 0;
  const char *system = LIBRETRO_SYSTEMS[g->system >= 0 && g->system <= BOXART_NDS ? g->system : 0];
  char sys[128];
  http_encode(system, sys, sizeof sys);
  for (int i = 0; i < 2; i++) {
    if (i && !strcmp(names[0], names[1]))
      break;
    char name[800], url[1024];
    http_encode(names[i], name, sizeof name);
    snprintf(url, sizeof url, "https://thumbnails.libretro.com/%s/Named_Boxarts/%s.png", sys, name);
    char *data = fetch_image(url, NULL, len);
    if (data)
      return data;
  }
  return NULL;
}

/* SteamGridDB: the title searched, the first game's tallest grid (600x900, else any) as a PNG or JPEG. */
static char *from_steamgriddb(const struct boxart_game *g, const char *key, size_t *len) {
  if (!*key)
    return NULL;
  char auth[160], term[512], url[1024], id[32];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
  // Plain ASCII for the search ("Pokémon" -> "Pokemon").
  char ascii[256];
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char *)g->title; *p && n + 1 < sizeof ascii; p++) {
    if (*p == 0xC3 && (p[1] == 0xA9 || p[1] == 0x89)) {
      ascii[n++] = p[1] == 0xA9 ? 'e' : 'E';
      p++;
    } else if (*p < 0x80)
      ascii[n++] = (char)*p;
  }
  ascii[n] = 0;
  http_encode(ascii, term, sizeof term);
  snprintf(url, sizeof url, "https://www.steamgriddb.com/api/v2/search/autocomplete/%s", term);
  size_t jlen;
  char *json = http_get(url, auth, MAX_JSON, &jlen);
  const char *data = json ? strstr(json, "\"data\"") : NULL;
  bool found = data && json_value(data, "id", id, sizeof id);
  free(json);
  if (!found)
    return NULL;
  static const char *const QUERIES[] = {"?dimensions=600x900&mimes=image/png,image/jpeg",
                                        "?mimes=image/png,image/jpeg"};
  for (int q = 0; q < 2; q++) {
    snprintf(url, sizeof url, "https://www.steamgriddb.com/api/v2/grids/game/%s%s", id, QUERIES[q]);
    json = http_get(url, auth, MAX_JSON, &jlen);
    char art[1024];
    data = json ? strstr(json, "\"data\"") : NULL;
    bool got = data && json_value(data, "url", art, sizeof art);
    free(json);
    if (got) {
      char *img = fetch_image(art, NULL, len);
      if (img)
        return img;
    }
  }
  return NULL;
}

static bool look(const struct boxart_game *g) {
  char key[80];
  boxart_key(key, sizeof key);
  size_t len = 0;
  bool base = false;
  char *img = from_libretro(g, &len);
  if (!img && !g->hack && g->base[0]) // an original under a name of the player's own
    img = libretro_named(g->base, g->baseSystem, &len);
  if (!img)
    img = from_steamgriddb(g, key, &len);
  char path[1400], had[1400];
  path_of(had, sizeof had, g->stem, "img");
  path_of(path, sizeof path, g->stem, "base");
  bool ownArt = pocket_exists(had) && !pocket_exists(path); // art of its own from before: never swapped for the base
  if (!img && g->hack && g->base[0] && !ownArt) // a hack nobody has art for: the game it was made from, with a banner
    base = (img = libretro_named(g->base, g->baseSystem, &len)) != NULL;
  if (!img) {
    path_of(path, sizeof path, g->stem, "img");
    if (!pocket_exists(path)) { // looked again and found nothing: what it had stays
      path_of(path, sizeof path, g->stem, "none");
      write_file(path, "", 0);
    }
    return false;
  }
  path_of(path, sizeof path, g->stem, "img");
  bool ok = write_file(path, img, len);
  free(img);
  if (ok) {
    path_of(path, sizeof path, g->stem, "none");
    pocket_remove(path);
    path_of(path, sizeof path, g->stem, "base");
    if (base)
      write_file(path, "", 0);
    else
      pocket_remove(path);
    SDL_AtomicAdd(&generation, 1);
  }
  return ok;
}

int boxart_generation(void) { return SDL_AtomicGet(&generation); }

bool boxart_is_base(const char *stem) {
  char path[1400];
  path_of(path, sizeof path, stem, "custom");
  if (pocket_exists(path))
    return false;
  path_of(path, sizeof path, stem, "base");
  return pocket_exists(path);
}

static bool tried(const char *stem) {
  char path[1400];
  path_of(path, sizeof path, stem, "img");
  if (pocket_exists(path))
    return true;
  path_of(path, sizeof path, stem, "custom");
  if (pocket_exists(path))
    return true;
  path_of(path, sizeof path, stem, "none");
  return pocket_exists(path);
}

static int work(void *unused) {
  (void)unused;
  while (!SDL_AtomicGet(&quit)) {
    struct boxart_game g;
    bool have = false, isFind = false;
    SDL_LockMutex(lock);
    if (findWaiting) {
      g = findGame;
      findWaiting = false;
      have = isFind = true;
    } else
      while (queueNext < queueCount && !have) {
        g = queue[queueNext++];
        have = true;
      }
    working = have && !isFind;
    bool force = forceAll;
    if (queueNext >= queueCount)
      forceAll = false;
    SDL_UnlockMutex(lock);
    if (!have) {
      SDL_Delay(200);
      continue;
    }
    if (isFind) {
      bool ok = look(&g);
      SDL_LockMutex(lock);
      if (!findWaiting) {
        findResult = ok ? 1 : 0;
        SDL_AtomicSet(&finding, 0);
      }
      SDL_UnlockMutex(lock);
    } else if (force ? !boxart_has_custom(g.stem) : !tried(g.stem))
      look(&g); // ALL AGAIN: the old art stays until a new one replaces it
    SDL_LockMutex(lock);
    working = 0;
    SDL_UnlockMutex(lock);
  }
  return 0;
}

void boxart_sync(const struct boxart_game *games, int count, bool all) {
  char path[1400];
  for (int i = 0; i < count; i++) {
    path_of(path, sizeof path, games[i].stem, "none"); // tried before and none found: try again
    pocket_remove(path);
  }
  SDL_AtomicAdd(&generation, 1);
  boxart_queue(games, count);
  SDL_LockMutex(lock);
  forceAll = all && count > 0;
  SDL_UnlockMutex(lock);
}

bool boxart_progress(int *done, int *total) {
  SDL_LockMutex(lock);
  *total = queueCount;
  *done = queueNext - working;
  bool busy = queueNext < queueCount || working;
  SDL_UnlockMutex(lock);
  return busy;
}

void boxart_init(const char *data) {
  snprintf(root, sizeof root, "%s", data);
  char dir[1100];
  snprintf(dir, sizeof dir, "%s/boxart", root);
  pocket_mkdir(dir);
  snprintf(dir, sizeof dir, "%s/boxart/custom", root);
  pocket_mkdir(dir);
  if (!lock)
    lock = SDL_CreateMutex();
  if (!worker && lock) {
    SDL_AtomicSet(&quit, 0);
    worker = SDL_CreateThread(work, "boxart", NULL);
  }
}

void boxart_quit(void) {
  if (worker) {
    SDL_AtomicSet(&quit, 1);
    SDL_WaitThread(worker, NULL); // a fetch in flight ends by its timeouts
    worker = NULL;
  }
  boxart_textures_free();
  SDL_LockMutex(lock);
  free(queue);
  queue = NULL;
  queueCount = queueNext = 0;
  SDL_UnlockMutex(lock);
}

void boxart_key(char *out, size_t size) {
  char path[1100];
  snprintf(path, sizeof path, "%s/steamgriddb-key.txt", root);
  out[0] = 0;
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  if (fgets(out, (int)size, f)) {
    size_t n = strlen(out);
    while (n && isspace((unsigned char)out[n - 1]))
      out[--n] = 0;
    for (size_t i = 0; i < n; i++)
      if (!isalnum((unsigned char)out[i])) {
        out[0] = 0;
        break;
      }
  }
  fclose(f);
}

bool boxart_key_save(const char *key) {
  char k[80];
  size_t n = 0;
  for (const char *p = key; *p && n + 1 < sizeof k; p++)
    if (!isspace((unsigned char)*p)) {
      if (!isalnum((unsigned char)*p))
        return false;
      k[n++] = *p;
    }
  k[n] = 0;
  if (n && (n < 16 || n > 64))
    return false;
  char path[1100];
  snprintf(path, sizeof path, "%s/steamgriddb-key.txt", root);
  if (!n)
    return pocket_remove(path) || !pocket_exists(path);
  char line[96];
  int len = snprintf(line, sizeof line, "%s\n", k);
  return write_file(path, line, (size_t)len);
}

void boxart_queue(const struct boxart_game *games, int count) {
  struct boxart_game *copy = count > 0 ? malloc(sizeof *copy * (size_t)count) : NULL;
  if (copy)
    memcpy(copy, games, sizeof *copy * (size_t)count);
  SDL_LockMutex(lock);
  free(queue);
  queue = copy;
  queueCount = copy ? count : 0;
  queueNext = 0;
  forceAll = false;
  SDL_UnlockMutex(lock);
}

void boxart_find(const struct boxart_game *game) {
  SDL_LockMutex(lock);
  findGame = *game;
  findWaiting = true;
  findResult = -1;
  SDL_AtomicSet(&finding, 1);
  SDL_UnlockMutex(lock);
}

int boxart_find_result(void) {
  SDL_LockMutex(lock);
  int r = findResult;
  findResult = -1;
  SDL_UnlockMutex(lock);
  return r;
}

bool boxart_finding(void) { return SDL_AtomicGet(&finding) != 0; }

bool boxart_set_custom(const char *stem, const char *source, char *err, size_t errSize) {
  char src[1100];
  snprintf(src, sizeof src, "%s", source ? source : "");
  // Trim spaces, quotes and a file:/// prefix (a path copied from a file manager).
  char *s = src;
  while (*s == ' ' || *s == '"' || *s == '\'')
    s++;
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '"' || s[n - 1] == '\'' || s[n - 1] == '\n' || s[n - 1] == '\r'))
    s[--n] = 0;
  if (!strncmp(s, "file:///", 8))
    s += 7 + (s[9] == ':'); // file:///C:/x -> C:/x, file:///home/x -> /home/x
  if (!*s) {
    snprintf(err, errSize, "Copy an image's path or link first");
    return false;
  }
  char *data = NULL;
  size_t len = 0;
  if (!strncmp(s, "https://", 8)) {
    char url[1024];
    // Spaces in a pasted link.
    size_t k = 0;
    for (const char *p = s; *p && k + 4 < sizeof url; p++) {
      if (*p == ' ') {
        memcpy(url + k, "%20", 3);
        k += 3;
      } else
        url[k++] = *p;
    }
    url[k] = 0;
    if (!http_url_ok(url)) {
      snprintf(err, errSize, "That link can't be used");
      return false;
    }
    data = fetch_image(url, NULL, &len);
    if (!data) {
      snprintf(err, errSize, "No PNG or JPEG at that link");
      return false;
    }
  } else {
    FILE *f = fopen(s, "rb");
    if (!f) {
      snprintf(err, errSize, "Can't open that file");
      return false;
    }
    data = malloc(MAX_IMAGE);
    len = data ? fread(data, 1, MAX_IMAGE, f) : 0;
    fclose(f);
    if (!data || !is_image(data, len)) {
      free(data);
      snprintf(err, errSize, "That isn't a PNG or JPEG");
      return false;
    }
  }
  char path[1400];
  path_of(path, sizeof path, stem, "custom");
  bool ok = write_file(path, data, len);
  free(data);
  if (!ok)
    snprintf(err, errSize, "Couldn't save the box art");
  else
    SDL_AtomicAdd(&generation, 1);
  return ok;
}

bool boxart_clear_custom(const char *stem) {
  char path[1400];
  path_of(path, sizeof path, stem, "custom");
  bool ok = pocket_remove(path);
  SDL_AtomicAdd(&generation, 1);
  return ok;
}

bool boxart_has_custom(const char *stem) {
  char path[1400];
  path_of(path, sizeof path, stem, "custom");
  return pocket_exists(path);
}

/* --- textures --- */

#define CACHE 96
static struct {
  char stem[256];
  SDL_Texture *tex;
  int w, h, gen;
  uint32_t used;
} cache[CACHE];
static uint32_t useTick;

static SDL_Texture *load(SDL_Renderer *r, const char *stem, int *w, int *h) {
  char path[1400];
  path_of(path, sizeof path, stem, "custom");
  if (!pocket_exists(path))
    path_of(path, sizeof path, stem, "img");
  if (!pocket_exists(path))
    return NULL;
  int iw, ih, n;
  unsigned char *px = stbi_load(path, &iw, &ih, &n, 4);
  if (!px)
    return NULL;
  // Scaled down by a box filter to ART_MAX on the long side: smaller textures, no shimmer when drawn small.
  int step = 1;
  while ((iw > ih ? iw : ih) / step > ART_MAX)
    step++;
  int ow = iw / step, oh = ih / step;
  unsigned char *out = px;
  if (step > 1) {
    out = malloc((size_t)ow * oh * 4);
    if (!out) {
      stbi_image_free(px);
      return NULL;
    }
    for (int y = 0; y < oh; y++)
      for (int x = 0; x < ow; x++)
        for (int c = 0; c < 4; c++) {
          unsigned sum = 0;
          for (int dy = 0; dy < step; dy++)
            for (int dx = 0; dx < step; dx++)
              sum += px[((size_t)(y * step + dy) * iw + (x * step + dx)) * 4 + c];
          out[((size_t)y * ow + x) * 4 + c] = (unsigned char)(sum / (unsigned)(step * step));
        }
  }
  SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, ow, oh);
  if (t) {
    SDL_UpdateTexture(t, NULL, out, ow * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
#endif
  }
  if (out != px)
    free(out);
  stbi_image_free(px);
  *w = ow;
  *h = oh;
  return t;
}

SDL_Texture *boxart_texture(SDL_Renderer *r, const char *stem, int *w, int *h) {
  int gen = SDL_AtomicGet(&generation);
  useTick++;
  int slot = -1, oldest = 0;
  for (int i = 0; i < CACHE; i++) {
    if (cache[i].stem[0] && !strcmp(cache[i].stem, stem)) {
      slot = i;
      break;
    }
    if (cache[i].used < cache[oldest].used)
      oldest = i;
  }
  if (slot >= 0 && cache[slot].gen == gen) {
    cache[slot].used = useTick;
    *w = cache[slot].w;
    *h = cache[slot].h;
    return cache[slot].tex;
  }
  if (slot < 0)
    slot = oldest;
  if (cache[slot].tex)
    SDL_DestroyTexture(cache[slot].tex);
  snprintf(cache[slot].stem, sizeof cache[slot].stem, "%s", stem);
  cache[slot].tex = load(r, stem, &cache[slot].w, &cache[slot].h);
  cache[slot].gen = gen;
  cache[slot].used = useTick;
  *w = cache[slot].w;
  *h = cache[slot].h;
  return cache[slot].tex;
}

void boxart_textures_free(void) {
  for (int i = 0; i < CACHE; i++) {
    if (cache[i].tex)
      SDL_DestroyTexture(cache[i].tex);
    cache[i].tex = NULL;
    cache[i].stem[0] = 0;
  }
}
