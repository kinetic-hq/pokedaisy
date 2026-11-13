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
  char tmp[1600];
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

/* A whole image file (NULL if it isn't a PNG or JPEG or can't be read). */
static char *read_image(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  char *data = malloc(MAX_IMAGE);
  *len = data ? fread(data, 1, MAX_IMAGE, f) : 0;
  fclose(f);
  if (data && !is_image(data, *len)) {
    free(data);
    data = NULL;
  }
  return data;
}

/* --- the art folders (BOX ART LOCATION) --------------------------------------------------------------------------- */

static int artWhere = BOXART_BESIDE_GAMES; // under lock
static char artFolder[1024];
struct art_game {
  char stem[256], name[256], dir[1024];
  int system;
  bool shared;
};
static struct art_game *artGames; // under lock
static int artCount;
static const char *const ART_SUBFOLDER[] = {"gba", "gbc", "gb", "nds"};
static const char *const ART_EXT[] = {".png", ".jpg", ".jpeg"};

static void location_load(void) {
  char path[1100], line[1024];
  snprintf(path, sizeof path, "%s/boxart-location.txt", root);
  artWhere = BOXART_BESIDE_GAMES;
  artFolder[0] = 0;
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  if (fgets(line, sizeof line, f) && !strncmp(line, "folder", 6) && fgets(line, sizeof line, f)) {
    line[strcspn(line, "\r\n")] = 0;
    if (*line) {
      artWhere = BOXART_ONE_FOLDER;
      snprintf(artFolder, sizeof artFolder, "%s", line);
    }
  }
  fclose(f);
}

void boxart_location(int *where, char *folder, size_t size) {
  SDL_LockMutex(lock);
  *where = artWhere;
  snprintf(folder, size, "%s", artFolder);
  SDL_UnlockMutex(lock);
}

bool boxart_location_set(int where, const char *folder) {
  char text[1200], path[1100];
  int n = where == BOXART_ONE_FOLDER && folder && *folder ? snprintf(text, sizeof text, "folder\n%s\n", folder)
                                                         : snprintf(text, sizeof text, "beside\n");
  if (n < 0 || (size_t)n >= sizeof text)
    return false;
  snprintf(path, sizeof path, "%s/boxart-location.txt", root);
  if (!write_file(path, text, (size_t)n))
    return false;
  SDL_LockMutex(lock);
  location_load();
  SDL_UnlockMutex(lock);
  SDL_AtomicAdd(&generation, 1);
  return true;
}

/* A name as art files are matched by it: lower case, "é" as "e", what a file name can't hold as "_", no spaces or
 * dots at its end (Windows drops them). */
static void art_key(const char *s, char *out, size_t size) {
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p && n + 1 < size; p++) {
    unsigned c = *p;
    if (c == 0xC3 && (p[1] == 0xA9 || p[1] == 0x89)) {
      c = 'e';
      p++;
    } else if (c < 0x20 || strchr("\\/:*?\"<>|", (int)c))
      c = '_';
    else if (c < 0x80)
      c = (unsigned)tolower((int)c);
    out[n++] = (char)c;
  }
  while (n && (out[n - 1] == ' ' || out[n - 1] == '.'))
    n--;
  out[n] = 0;
}

/* The name a new picture gets: the game's own, with what a file name can't hold as "_". */
static void art_file_base(const char *s, char *out, size_t size) {
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p && n + 1 < size; p++)
    out[n++] = *p < 0x20 || strchr("\\/:*?\"<>|", (int)*p) ? '_' : (char)*p;
  while (n && (out[n - 1] == ' ' || out[n - 1] == '.'))
    n--;
  out[n] = 0;
}

bool boxart_art_name_matches(const char *file, const char *name, int *num) {
  const char *dot = strrchr(file, '.');
  if (!dot)
    return false;
  bool image = false;
  for (size_t i = 0; i < sizeof ART_EXT / sizeof *ART_EXT; i++)
    image = image || !strcasecmp(dot, ART_EXT[i]);
  char base[300], kb[300], kn[300];
  size_t bl = (size_t)(dot - file);
  if (!image || bl >= sizeof base)
    return false;
  memcpy(base, file, bl);
  base[bl] = 0;
  art_key(base, kb, sizeof kb);
  art_key(name, kn, sizeof kn);
  size_t ln = strlen(kn);
  // The tile's own name too: the list name less "Pokemon " (game_grid shows the rest).
  if (ln && strncmp(kb, kn, ln) && !strncmp(kn, "pokemon ", 8) && kn[8]) {
    memmove(kn, kn + 8, ln - 7);
    ln -= 8;
  }
  if (!ln || strncmp(kb, kn, ln))
    return false;
  if (!kb[ln]) {
    *num = 0;
    return true;
  }
  const char *d = kb + ln + 1;
  if (kb[ln] != ' ' || !*d || strlen(d) > 4)
    return false;
  for (const char *q = d; *q; q++)
    if (!isdigit((unsigned char)*q))
      return false;
  *num = atoi(d);
  return *num > 0;
}

struct art_file {
  int num;
  char name[300];
};
struct art_list {
  const char *name;
  struct art_file *files;
  int count, max;
};
static void art_each(const char *file, void *user) {
  struct art_list *l = user;
  int num;
  if (l->count < l->max && boxart_art_name_matches(file, l->name, &num)) {
    l->files[l->count].num = num;
    snprintf(l->files[l->count].name, sizeof l->files[l->count].name, "%s", file);
    l->count++;
  }
}
/* NAME's pictures in DIR: its box art first, then the alternates by number. */
static int art_files(const char *dir, const char *name, struct art_file *files, int max) {
  struct art_list l = {name, files, 0, max};
  pocket_list_dir(dir, art_each, &l);
  for (int i = 1; i < l.count; i++)
    for (int k = i; k > 0 && files[k].num < files[k - 1].num; k--) {
      struct art_file t = files[k];
      files[k] = files[k - 1];
      files[k - 1] = t;
    }
  return l.count;
}

static bool art_game_of(const char *stem, struct art_game *out) {
  bool found = false;
  SDL_LockMutex(lock);
  for (int i = 0; i < artCount && !found; i++)
    if (!strcmp(artGames[i].stem, stem)) {
      *out = artGames[i];
      found = true;
    }
  SDL_UnlockMutex(lock);
  return found;
}

/* The folder a game's pictures are in. ONE FOLDER: its console's folder, or the folder's top level when its
 * pictures are there and not in its console's (unless another game on another console has its name). */
static bool art_home(const struct art_game *g, char *out, size_t size) {
  int where;
  char folder[1024];
  boxart_location(&where, folder, sizeof folder);
  if (where != BOXART_ONE_FOLDER || !*folder) {
    if (!*g->dir)
      return false;
    snprintf(out, size, "%s/boxart", g->dir);
    return true;
  }
  char sub[1100];
  snprintf(sub, sizeof sub, "%s/%s", folder, ART_SUBFOLDER[g->system >= 0 && g->system <= BOXART_NDS ? g->system : 0]);
  struct art_file one[1];
  if (!g->shared && !art_files(sub, g->name, one, 1) && art_files(folder, g->name, one, 1))
    snprintf(out, size, "%s", folder);
  else
    snprintf(out, size, "%s", sub);
  return true;
}

bool boxart_art_folder(const char *stem, char *out, size_t size) {
  struct art_game g;
  return art_game_of(stem, &g) && art_home(&g, out, size);
}

bool boxart_name_shared(const char *stem) {
  struct art_game g;
  int where;
  char folder[1024];
  boxart_location(&where, folder, sizeof folder);
  return where == BOXART_ONE_FOLDER && art_game_of(stem, &g) && g.shared;
}

/* The game's box art in its folder. */
static bool art_main_path(const char *stem, char *out, size_t size) {
  struct art_game g;
  char home[1100];
  struct art_file f[8];
  if (!art_game_of(stem, &g) || !art_home(&g, home, sizeof home))
    return false;
  int n = art_files(home, g.name, f, 8);
  if (!n || f[0].num)
    return false;
  snprintf(out, size, "%s/%s", home, f[0].name);
  return true;
}

static const char *image_ext(const char *data, size_t len) {
  return len >= 4 && !memcmp(data, "\x89PNG", 4) ? ".png" : ".jpg";
}

/* The box art in HOME (F: its N pictures) renamed to the next free alternate number. */
static bool art_shift_main(const char *home, const char *base, struct art_file *f, int n) {
  char from[1500], to[1500];
  for (int i = 0; i < n; i++) {
    if (f[i].num)
      continue;
    int next = 1;
    for (bool used = true; used; next += used) {
      used = false;
      for (int k = 0; k < n && !used; k++)
        used = f[k].num == next;
    }
    snprintf(from, sizeof from, "%s/%s", home, f[i].name);
    snprintf(to, sizeof to, "%s/%s %d%s", home, base, next, strrchr(f[i].name, '.'));
    if (pocket_exists(to) || !pocket_replace(from, to))
      return false;
    f[i].num = next;
  }
  return true;
}

/* A new picture as the game's box art, in its folder (made now if it isn't there yet): the box art it had becomes
 * the next free alternate first. Nothing there is ever overwritten. */
static bool art_save_main(const char *stem, const char *data, size_t len) {
  struct art_game g;
  char home[1100], base[300];
  if (!art_game_of(stem, &g) || !art_home(&g, home, sizeof home))
    return false;
  art_file_base(g.name, base, sizeof base);
  if (!*base || (!pocket_is_dir(home) && !pocket_mkdir(home)))
    return false;
  struct art_file *f = malloc(256 * sizeof *f);
  if (!f)
    return false;
  int n = art_files(home, g.name, f, 256);
  bool ok = art_shift_main(home, base, f, n);
  free(f);
  char to[1500];
  if (!ok)
    return false;
  snprintf(to, sizeof to, "%s/%s%s", home, base, image_ext(data, len));
  if (pocket_exists(to) || !write_file(to, data, len))
    return false;
  SDL_AtomicAdd(&generation, 1);
  return true;
}

bool boxart_art_use(const char *stem, int num) {
  struct art_game g;
  char home[1100], base[300];
  if (num <= 0 || !art_game_of(stem, &g) || !art_home(&g, home, sizeof home))
    return false;
  art_file_base(g.name, base, sizeof base);
  struct art_file f[256];
  int n = art_files(home, g.name, f, 256), main = -1, alt = -1;
  for (int i = 0; i < n; i++) {
    if (!f[i].num && main < 0)
      main = i;
    if (f[i].num == num && alt < 0)
      alt = i;
  }
  if (alt < 0 || !*base)
    return false;
  const char *altExt = strrchr(f[alt].name, '.');
  char altPath[1500], tmp[1500], to[1500];
  snprintf(altPath, sizeof altPath, "%s/%s", home, f[alt].name);
  snprintf(to, sizeof to, "%s/%s%s", home, base, altExt);
  if (main < 0) { // no box art: the alternate simply takes its place
    if (pocket_exists(to) || !pocket_replace(altPath, to))
      return false;
  } else {
    char mainPath[1500], mainTo[1500];
    snprintf(mainPath, sizeof mainPath, "%s/%s", home, f[main].name);
    snprintf(mainTo, sizeof mainTo, "%s/%s %d%s", home, base, num, strrchr(f[main].name, '.'));
    snprintf(tmp, sizeof tmp, "%s/%s swap%s", home, base, altExt);
    if (pocket_exists(tmp) || !pocket_replace(altPath, tmp))
      return false;
    if ((pocket_exists(mainTo) || !pocket_replace(mainPath, mainTo))) {
      pocket_replace(tmp, altPath); // put it back
      return false;
    }
    if (pocket_exists(to) || !pocket_replace(tmp, to)) {
      pocket_replace(mainTo, mainPath);
      pocket_replace(tmp, altPath);
      return false;
    }
  }
  SDL_AtomicAdd(&generation, 1);
  return true;
}

/* Appends code point c to out as UTF-8 (BMP only), if it fits. */
static size_t put_utf8(char *out, size_t n, size_t size, unsigned c) {
  if (c < 0x80 && n + 1 < size)
    out[n++] = (char)c;
  else if (c >= 0x80 && c < 0x800 && n + 2 < size) {
    out[n++] = (char)(0xC0 | c >> 6);
    out[n++] = (char)(0x80 | (c & 0x3F));
  } else if (c >= 0x800 && n + 3 < size) {
    out[n++] = (char)(0xE0 | c >> 12);
    out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
    out[n++] = (char)(0x80 | (c & 0x3F));
  }
  return n;
}

/* The string value after "key": in json from `from`, unescaped (\/, \" and \uXXXX: SteamGridDB sends
 * "Pokémon"). */
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
      if (*p == '\\' && p[1] == 'u' && isxdigit((unsigned char)p[2]) && isxdigit((unsigned char)p[3]) &&
          isxdigit((unsigned char)p[4]) && isxdigit((unsigned char)p[5])) {
        char hex[5] = {p[2], p[3], p[4], p[5], 0};
        n = put_utf8(out, n, size, (unsigned)strtoul(hex, NULL, 16));
        p += 5;
        continue;
      }
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

bool boxart_json_string(const char *json, const char *key, char *out, size_t size) {
  return json_value(json, key, out, size);
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

/* A title's words for matching: lower case ASCII ("Pokémon" -> "pokemon"), letters and digits, split at anything
 * else, without the words every entry has ("pokemon", "version", "the") and "and" ("Heart & Soul" is "Heart and
 * Soul"). Returns how many (at most 16). */
static int title_words(const char *s, char words[16][32]) {
  int n = 0;
  size_t len = 0;
  char w[32];
  for (const unsigned char *p = (const unsigned char *)s;; p++) {
    int ch = *p;
    if (ch == 0xC3 && (p[1] == 0xA9 || p[1] == 0x89)) {
      ch = 'e';
      p++;
    }
    if (ch && ch < 0x80 && isalnum(ch)) {
      if (len + 1 < sizeof w)
        w[len++] = (char)tolower(ch);
      continue;
    }
    if (len) {
      w[len] = 0;
      len = 0;
      if (strcmp(w, "pokemon") && strcmp(w, "version") && strcmp(w, "the") && strcmp(w, "and") && n < 16)
        snprintf(words[n++], 32, "%s", w);
    }
    if (!ch)
      break;
  }
  return n;
}

/* SteamGridDB's entry is this game: the same words, none missing and none added, or the same letters once the
 * spaces go ("Ash Gray" is "AshGray"). "Pokemon Black" isn't "Pokemon Black 2", and a hack's name never matches
 * Pinball or another spin-off. */
bool boxart_title_matches(const char *want, const char *got) {
  char a[16][32], b[16][32];
  int na = title_words(want, a), nb = title_words(got, b);
  if (!na || !nb)
    return false;
  char ja[600] = "", jb[600] = "";
  for (int i = 0; i < na; i++)
    strncat(ja, a[i], sizeof ja - strlen(ja) - 1);
  for (int i = 0; i < nb; i++)
    strncat(jb, b[i], sizeof jb - strlen(jb) - 1);
  if (!strcmp(ja, jb))
    return true;
  if (na != nb)
    return false;
  for (int i = 0; i < na; i++) {
    bool found = false;
    for (int j = 0; j < nb && !found; j++)
      found = !strcmp(a[i], b[j]);
    if (!found)
      return false;
  }
  return true;
}

/* SteamGridDB: the title searched, the first entry with the same name (boxart_title_matches), its first English
 * grid (600x900, else any) as a PNG or JPEG. None that match: nothing, so a hack falls back to its base game's box. */
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
  bool found = false;
  for (const char *p = data; p && !found && (p = strstr(p, "\"id\"")); p += 4) {
    char name[256];
    if (json_value(p, "id", id, sizeof id) && json_value(p, "name", name, sizeof name) &&
        boxart_title_matches(ascii, name))
      found = true;
  }
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
    // Each grid is an object from its "id" on, its "language" before its "url": the first in English.
    bool got = false;
    for (const char *p = data; p && !got && (p = strstr(p, "{\"id\"")); p += 5) {
      const char *next = strstr(p + 5, "{\"id\"");
      const char *lang = strstr(p, "\"language\"");
      if (!lang || (next && lang > next))
        continue;
      char code[8];
      if (json_value(lang, "language", code, sizeof code) && !strcmp(code, "en") && json_value(p, "url", art, sizeof art))
        got = true;
    }
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
  char folderArt[1400];
  if (!base && !art_main_path(g->stem, folderArt, sizeof folderArt) && art_save_main(g->stem, img, len)) {
    free(img); // in the game's art folder: the marks of the search go
    path_of(path, sizeof path, g->stem, "none");
    pocket_remove(path);
    path_of(path, sizeof path, g->stem, "base");
    pocket_remove(path);
    return true;
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
  if (art_main_path(stem, path, sizeof path))
    return false;
  path_of(path, sizeof path, stem, "custom");
  if (pocket_exists(path))
    return false;
  path_of(path, sizeof path, stem, "base");
  return pocket_exists(path);
}

static bool tried(const char *stem) {
  char path[1400];
  if (art_main_path(stem, path, sizeof path))
    return true;
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

/* Art found before matching was strict (any SteamGridDB hit, in any language) is looked for again, once: everything
 * but the player's own (boxart/custom) goes. */
static void forget_found(const char *name, void *dir) {
  size_t n = strlen(name);
  if ((n > 4 && !strcmp(name + n - 4, ".img")) || (n > 5 && (!strcmp(name + n - 5, ".none") || !strcmp(name + n - 5, ".base")))) {
    char path[1400];
    snprintf(path, sizeof path, "%s/%s", (const char *)dir, name);
    pocket_remove(path);
  }
}

/* A base game's box (its .base mark and the art) or a "none found" mark: looked for again. */
static void forget_base(const char *name, void *dir) {
  size_t n = strlen(name);
  char path[1400];
  if (n > 5 && !strcmp(name + n - 5, ".none")) {
    snprintf(path, sizeof path, "%s/%s", (const char *)dir, name);
    pocket_remove(path);
  } else if (n > 5 && !strcmp(name + n - 5, ".base")) {
    snprintf(path, sizeof path, "%s/%s", (const char *)dir, name);
    pocket_remove(path);
    snprintf(path, sizeof path, "%s/%.*s.img", (const char *)dir, (int)(n - 5), name);
    pocket_remove(path);
  }
}

void boxart_init(const char *data) {
  snprintf(root, sizeof root, "%s", data);
  char dir[1100];
  snprintf(dir, sizeof dir, "%s/boxart", root);
  pocket_mkdir(dir);
  char mark[1200];
  snprintf(mark, sizeof mark, "%s/matching-v2", dir);
  if (!pocket_exists(mark)) {
    pocket_list_dir(dir, forget_found, dir);
    write_file(mark, "", 0);
  }
  // 1.16.0 read SteamGridDB's "Pokémon" as other words, so a hack got its base game's box or none: those are
  // looked for again, once. A hack's own art and the player's own stay.
  snprintf(mark, sizeof mark, "%s/matching-v3", dir);
  if (!pocket_exists(mark)) {
    pocket_list_dir(dir, forget_base, dir);
    write_file(mark, "", 0);
  }
  snprintf(dir, sizeof dir, "%s/boxart/custom", root);
  pocket_mkdir(dir);
  if (!lock)
    lock = SDL_CreateMutex();
  SDL_LockMutex(lock);
  location_load();
  SDL_UnlockMutex(lock);
  if (!worker && lock) {
    SDL_AtomicSet(&quit, 0);
    worker = SDL_CreateThread(work, "boxart", NULL);
  }
}

void boxart_quit(void) {
  boxart_pick_close();
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

void boxart_games(const struct boxart_game *games, int count) {
  struct art_game *list = count > 0 ? calloc((size_t)count, sizeof *list) : NULL;
  if (count > 0 && !list)
    return;
  for (int i = 0; i < count; i++) {
    snprintf(list[i].stem, sizeof list[i].stem, "%s", games[i].stem);
    snprintf(list[i].name, sizeof list[i].name, "%s", games[i].title);
    snprintf(list[i].dir, sizeof list[i].dir, "%s", games[i].dir);
    list[i].system = games[i].system;
  }
  // ONE FOLDER's top level: a name two consoles' games share is theirs in their console's folder only.
  for (int i = 0; i < count; i++) {
    char a[300], b[300];
    art_key(list[i].name, a, sizeof a);
    for (int k = 0; k < count && !list[i].shared; k++) {
      if (k == i || ART_SUBFOLDER[list[k].system & 3] == ART_SUBFOLDER[list[i].system & 3])
        continue;
      art_key(list[k].name, b, sizeof b);
      list[i].shared = !strcmp(a, b);
    }
  }
  SDL_LockMutex(lock);
  free(artGames);
  artGames = list;
  artCount = list ? count : 0;
  SDL_UnlockMutex(lock);
  // 1.18.1 and older kept the art in DATA/boxart (the player's own pick in custom): copied once into the game's
  // folder as its box art, where it has none. The old files stay.
  for (int i = 0; i < count; i++) {
    char mark[1400], src[1400], have[1400];
    path_of(mark, sizeof mark, games[i].stem, "moved");
    if (pocket_exists(mark))
      continue;
    path_of(src, sizeof src, games[i].stem, "custom");
    if (!pocket_exists(src)) {
      path_of(have, sizeof have, games[i].stem, "base");
      path_of(src, sizeof src, games[i].stem, "img");
      if (pocket_exists(have) || !pocket_exists(src))
        continue; // a hack's base-game box isn't its own art
    }
    size_t len = 0;
    char *img = read_image(src, &len);
    bool moved = img && (art_main_path(games[i].stem, have, sizeof have) || art_save_main(games[i].stem, img, len));
    free(img);
    if (moved)
      write_file(mark, "", 0);
  }
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
    if (!pocket_exists(s)) {
      snprintf(err, errSize, "Can't open that file");
      return false;
    }
    data = read_image(s, &len);
    if (!data) {
      snprintf(err, errSize, "That isn't a PNG or JPEG");
      return false;
    }
  }
  char path[1400];
  struct art_game g;
  bool inFolder = art_game_of(stem, &g), ok;
  if (inFolder) // the game's art folder: the box art it had becomes an alternate
    ok = art_save_main(stem, data, len);
  else {
    path_of(path, sizeof path, stem, "custom");
    ok = write_file(path, data, len);
  }
  free(data);
  if (!ok)
    snprintf(err, errSize, inFolder ? "Couldn't save the box art in its folder" : "Couldn't save the box art");
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
  if (art_main_path(stem, path, sizeof path))
    return true;
  path_of(path, sizeof path, stem, "custom");
  return pocket_exists(path);
}

/* --- BOX ART's picker (boxart.h): every cover SteamGridDB has for a game, plus libretro's, lazily ---------------- */

#define PICK_MAX 400   // candidates kept (SteamGridDB pages of 50)
#define PICK_ENTRIES 20

struct pick_cand {
  char url[512], thumb[512];
  int source, w, h, local;
  bool ready, failed;
};
static SDL_Thread *pickThread;
static SDL_atomic_t pickQuit;
// Under lock:
static struct pick_cand *pick;
static int pickCount, pickFocus, pickPage, pickGameIndex = -1, pickJob, pickSerial;
static bool pickPagesDone, pickNoKey, pickBusy;
static char pickTerm[128], pickEntryNames[PICK_ENTRIES][128], pickEntryIds[PICK_ENTRIES][32];
static int pickEntryCount = -1;
static struct boxart_game pickGame;
enum { JOB_NONE, JOB_OPEN, JOB_SEARCH, JOB_ENTRY };

void boxart_pick_stem(int i, char *out, size_t size) { snprintf(out, size, "~pick-%d", i); }

static void pick_file(int i, char *path, size_t size) {
  char stem[32];
  boxart_pick_stem(i, stem, sizeof stem);
  path_of(path, size, stem, "img");
}

/* Adds a candidate (under lock); data, when given, is its picture already (libretro's), written as its thumbnail. */
static void pick_add(int source, const char *url, const char *thumb, int w, int h, const char *data, size_t len) {
  if (pickCount >= PICK_MAX)
    return;
  struct pick_cand *c = &pick[pickCount];
  memset(c, 0, sizeof *c);
  c->source = source;
  c->local = -1;
  c->w = w;
  c->h = h;
  snprintf(c->url, sizeof c->url, "%s", url ? url : "");
  snprintf(c->thumb, sizeof c->thumb, "%s", thumb && *thumb ? thumb : url ? url : "");
  if (data) {
    char path[1400];
    pick_file(pickCount, path, sizeof path);
    c->ready = write_file(path, data, len);
    c->failed = !c->ready;
  }
  pickCount++;
  SDL_AtomicAdd(&generation, 1);
}

/* The image's size from its header (PNG / JPEG), for libretro's covers. */
static void image_size(const unsigned char *d, size_t len, int *w, int *h) {
  int n;
  if (!stbi_info_from_memory(d, (int)len, w, h, &n))
    *w = *h = 0;
}

/* SteamGridDB's games for a term: names and ids (entries), -1 without a key. */
static int sgdb_search(const char *term, char names[][128], char ids[][32], int max) {
  char key[80];
  boxart_key(key, sizeof key);
  if (!*key)
    return -1;
  char auth[160], enc[512], url[1024], ascii[128];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
  size_t an = 0;
  for (const unsigned char *p = (const unsigned char *)term; *p && an + 1 < sizeof ascii; p++) {
    if (*p == 0xC3 && (p[1] == 0xA9 || p[1] == 0x89)) { // "Pokémon" -> "Pokemon"
      ascii[an++] = p[1] == 0xA9 ? 'e' : 'E';
      p++;
    } else if (*p < 0x80)
      ascii[an++] = (char)*p;
  }
  ascii[an] = 0;
  http_encode(ascii, enc, sizeof enc);
  snprintf(url, sizeof url, "https://www.steamgriddb.com/api/v2/search/autocomplete/%s", enc);
  size_t jlen;
  char *json = http_get(url, auth, MAX_JSON, &jlen);
  const char *data = json ? strstr(json, "\"data\"") : NULL;
  int n = 0;
  for (const char *p = data; p && n < max && (p = strstr(p, "\"id\"")); p += 4) {
    if (json_value(p, "id", ids[n], 32) && json_value(p, "name", names[n], 128))
      n++;
  }
  free(json);
  return n;
}

int boxart_parse_grids(const char *json, struct boxart_grid *out, int max) {
  const char *data = json ? strstr(json, "\"data\"") : NULL;
  int n = 0;
  // Each grid is an object from its "{"id"" to the next one's: its own fields read from that slice alone.
  for (const char *p = data; p && n < max && (p = strstr(p, "{\"id\"")); ) {
    const char *next = strstr(p + 5, "{\"id\"");
    size_t span = next ? (size_t)(next - p) : strlen(p);
    char *one = malloc(span + 1);
    if (!one)
      break;
    memcpy(one, p, span);
    one[span] = 0;
    struct boxart_grid *g = &out[n];
    char w[16] = "", h[16] = "";
    g->url[0] = g->thumb[0] = 0;
    json_value(one, "url", g->url, sizeof g->url);
    json_value(one, "thumb", g->thumb, sizeof g->thumb);
    json_value(one, "width", w, sizeof w);
    json_value(one, "height", h, sizeof h);
    free(one);
    g->w = atoi(w);
    g->h = atoi(h);
    if (*g->url && http_url_ok(g->url)) {
      if (!http_url_ok(g->thumb))
        snprintf(g->thumb, sizeof g->thumb, "%s", g->url);
      n++;
    }
    p = next;
  }
  return n;
}

/* One page of a SteamGridDB game's grids (every size, PNG / JPEG, no adult or joke art) into the candidates. False:
 * the page brought nothing new (the last one, or a page number the API ignored) or couldn't be read. */
static bool sgdb_page(const char *id, int page) {
  char key[80];
  boxart_key(key, sizeof key);
  if (!*key)
    return false;
  char auth[160], url[1024];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
  snprintf(url, sizeof url,
           "https://www.steamgriddb.com/api/v2/grids/game/%s?mimes=image/png,image/jpeg&nsfw=false&humor=false&page=%d",
           id, page);
  size_t jlen;
  char *json = http_get(url, auth, MAX_JSON, &jlen);
  static struct boxart_grid grids[100];
  int n = boxart_parse_grids(json, grids, 100);
  free(json);
  int added = 0;
  SDL_LockMutex(lock);
  for (int i = 0; i < n; i++) {
    bool seen = false;
    for (int k = 0; k < pickCount && !seen; k++)
      seen = !strcmp(pick[k].url, grids[i].url);
    if (!seen) {
      pick_add(BOXART_PICK_STEAMGRIDDB, grids[i].url, grids[i].thumb, grids[i].w, grids[i].h, NULL, 0);
      added++;
    }
  }
  SDL_UnlockMutex(lock);
  return added > 0;
}

/* The thumbnail nearest the one in view that isn't fetched yet (within a screenful or two), or -1. */
static int pick_next_thumb(void) {
  int best = -1, bestD = 1 << 30;
  for (int i = 0; i < pickCount; i++) {
    if (pick[i].ready || pick[i].failed)
      continue;
    int d = i >= pickFocus ? i - pickFocus : (pickFocus - i) * 2; // ahead first
    if (d < bestD && d <= 48) {
      best = i;
      bestD = d;
    }
  }
  return best;
}

static void pick_clear_files(void) {
  char path[1400];
  for (int i = 0; i < PICK_MAX; i++) {
    pick_file(i, path, sizeof path);
    pocket_remove(path);
  }
}

static int pick_work(void *unused) {
  (void)unused;
  while (!SDL_AtomicGet(&pickQuit)) {
    SDL_LockMutex(lock);
    int job = pickJob, serial = pickSerial;
    pickJob = JOB_NONE;
    struct boxart_game g = pickGame;
    char term[128];
    snprintf(term, sizeof term, "%s", pickTerm);
    int entry = pickGameIndex;
    char id[32] = "";
    if (job == JOB_ENTRY && entry >= 0 && entry < pickEntryCount)
      snprintf(id, sizeof id, "%s", pickEntryIds[entry]);
    if (job != JOB_NONE)
      pickBusy = true;
    SDL_UnlockMutex(lock);
    if (job == JOB_OPEN) {
      // The game's own pictures (its box art, then its alternates) first.
      struct art_game ag;
      char home[1100];
      static struct art_file files[64];
      int nf = art_game_of(g.stem, &ag) && art_home(&ag, home, sizeof home) ? art_files(home, ag.name, files, 64) : 0;
      for (int i = 0; i < nf; i++) {
        char path[1500];
        size_t flen = 0;
        snprintf(path, sizeof path, "%s/%s", home, files[i].name);
        char *data = read_image(path, &flen);
        SDL_LockMutex(lock);
        if (data && serial == pickSerial && pickCount < PICK_MAX) {
          int fw = 0, fh = 0;
          image_size((unsigned char *)data, flen, &fw, &fh);
          pick_add(BOXART_PICK_LOCAL, "", "", fw, fh, data, flen);
          pick[pickCount - 1].local = files[i].num;
        }
        SDL_UnlockMutex(lock);
        free(data);
      }
      // libretro's own cover and the base game's, then SteamGridDB's entry of the same name and its grids.
      size_t len = 0;
      char *img = from_libretro(&g, &len);
      int w = 0, h = 0;
      SDL_LockMutex(lock);
      if (img && serial == pickSerial) {
        image_size((unsigned char *)img, len, &w, &h);
        pick_add(BOXART_PICK_LIBRETRO, "", "", w, h, img, len);
      }
      SDL_UnlockMutex(lock);
      free(img);
      img = g.base[0] ? libretro_named(g.base, g.baseSystem, &len) : NULL;
      SDL_LockMutex(lock);
      if (img && serial == pickSerial) {
        image_size((unsigned char *)img, len, &w, &h);
        pick_add(BOXART_PICK_BASE, "", "", w, h, img, len);
      }
      SDL_UnlockMutex(lock);
      free(img);
      char names[PICK_ENTRIES][128], ids[PICK_ENTRIES][32];
      int n = sgdb_search(term, names, ids, PICK_ENTRIES);
      int match = -1;
      for (int i = 0; i < n && match < 0; i++)
        if (boxart_title_matches(term, names[i]))
          match = i;
      SDL_LockMutex(lock);
      if (serial == pickSerial) {
        pickNoKey = n < 0;
        pickEntryCount = n < 0 ? 0 : n;
        memcpy(pickEntryNames, names, sizeof names);
        memcpy(pickEntryIds, ids, sizeof ids);
        if (match >= 0) {
          pickGameIndex = match;
          pickPage = 0;
          pickPagesDone = false;
        } else
          pickPagesDone = true;
      }
      SDL_UnlockMutex(lock);
    } else if (job == JOB_SEARCH) {
      char names[PICK_ENTRIES][128], ids[PICK_ENTRIES][32];
      int n = sgdb_search(term, names, ids, PICK_ENTRIES);
      SDL_LockMutex(lock);
      if (serial == pickSerial) {
        pickNoKey = n < 0;
        pickEntryCount = n < 0 ? 0 : n;
        memcpy(pickEntryNames, names, sizeof names);
        memcpy(pickEntryIds, ids, sizeof ids);
      }
      SDL_UnlockMutex(lock);
    }
    // More of the game's grids when the picker is near the end of what it has; thumbnails near the one in view.
    SDL_LockMutex(lock);
    bool wantPage = !pickPagesDone && pickGameIndex >= 0 && pickGameIndex < pickEntryCount &&
                    pickFocus + 24 >= pickCount && pick;
    int page = pickPage;
    snprintf(id, sizeof id, "%s", wantPage ? pickEntryIds[pickGameIndex] : "");
    int thumb = wantPage ? -1 : pick ? pick_next_thumb() : -1;
    char turl[512] = "";
    if (thumb >= 0)
      snprintf(turl, sizeof turl, "%s", pick[thumb].thumb);
    serial = pickSerial;
    SDL_UnlockMutex(lock);
    if (wantPage) {
      bool more = sgdb_page(id, page);
      SDL_LockMutex(lock);
      if (serial == pickSerial) {
        pickPage = page + 1;
        pickPagesDone = !more;
      }
      SDL_UnlockMutex(lock);
    } else if (thumb >= 0) {
      size_t len = 0;
      char *img = fetch_image(turl, NULL, &len);
      SDL_LockMutex(lock);
      if (serial == pickSerial && thumb < pickCount) {
        char path[1400];
        pick_file(thumb, path, sizeof path);
        pick[thumb].ready = img && write_file(path, img, len);
        pick[thumb].failed = !pick[thumb].ready;
        SDL_AtomicAdd(&generation, 1);
      }
      SDL_UnlockMutex(lock);
      free(img);
    } else {
      SDL_LockMutex(lock);
      pickBusy = false;
      SDL_UnlockMutex(lock);
      SDL_Delay(60);
    }
  }
  return 0;
}

static void pick_reset(void) { // under lock
  pickSerial++;
  pickCount = pickFocus = pickPage = 0;
  pickPagesDone = true;
  pickGameIndex = -1;
  pick_clear_files();
  SDL_AtomicAdd(&generation, 1);
}

void boxart_pick_open(const struct boxart_game *g) {
  if (!lock)
    return;
  SDL_LockMutex(lock);
  if (!pick)
    pick = calloc(PICK_MAX, sizeof *pick);
  pickGame = *g;
  snprintf(pickTerm, sizeof pickTerm, "%.127s", g->title);
  pick_reset();
  pickEntryCount = -1;
  pickNoKey = false;
  pickJob = pick ? JOB_OPEN : JOB_NONE;
  pickBusy = pick != NULL;
  SDL_UnlockMutex(lock);
  if (!pickThread && pick) {
    SDL_AtomicSet(&pickQuit, 0);
    pickThread = SDL_CreateThread(pick_work, "boxart-pick", NULL);
  }
}

void boxart_pick_search(const char *term) {
  SDL_LockMutex(lock);
  snprintf(pickTerm, sizeof pickTerm, "%.127s", term);
  pickSerial++;
  pickEntryCount = -1;
  pickJob = JOB_SEARCH;
  pickBusy = true;
  SDL_UnlockMutex(lock);
}

int boxart_pick_names(char names[][128], int max) {
  SDL_LockMutex(lock);
  int n = pickJob == JOB_SEARCH ? -1 : pickEntryCount;
  if (n > max)
    n = max;
  for (int i = 0; i < n; i++)
    snprintf(names[i], 128, "%.127s", pickEntryNames[i]);
  SDL_UnlockMutex(lock);
  return n;
}

void boxart_pick_use_entry(int i) {
  SDL_LockMutex(lock);
  if (i >= 0 && i < pickEntryCount) {
    // libretro's covers stay; SteamGridDB's are this entry's from now on.
    int keep = 0;
    for (int k = 0; k < pickCount; k++)
      if (pick[k].source != BOXART_PICK_STEAMGRIDDB)
        keep = k + 1;
    pickSerial++;
    char path[1400];
    for (int k = keep; k < pickCount; k++) {
      pick_file(k, path, sizeof path);
      pocket_remove(path);
    }
    pickCount = keep;
    pickFocus = 0;
    pickGameIndex = i;
    pickPage = 0;
    pickPagesDone = false;
    pickBusy = true;
    SDL_AtomicAdd(&generation, 1);
  }
  SDL_UnlockMutex(lock);
}

int boxart_pick_count(void) {
  SDL_LockMutex(lock);
  int n = pick ? pickCount : 0;
  SDL_UnlockMutex(lock);
  return n;
}

bool boxart_pick_item(int i, struct boxart_pick_item *out) {
  SDL_LockMutex(lock);
  bool ok = pick && i >= 0 && i < pickCount;
  if (ok) {
    out->source = pick[i].source;
    out->w = pick[i].w;
    out->h = pick[i].h;
    out->ready = pick[i].ready;
    out->local = pick[i].local;
    out->failed = pick[i].failed;
  }
  SDL_UnlockMutex(lock);
  return ok;
}

void boxart_pick_focus(int i) {
  SDL_LockMutex(lock);
  pickFocus = i < 0 ? 0 : i;
  SDL_UnlockMutex(lock);
}

int boxart_pick_state(char *entry, size_t size) {
  SDL_LockMutex(lock);
  int s = pickNoKey ? BOXART_PICK_NO_KEY : pickBusy || pickJob != JOB_NONE ? BOXART_PICK_LOADING : BOXART_PICK_DONE;
  if (entry)
    snprintf(entry, size, "%s", pickGameIndex >= 0 && pickGameIndex < pickEntryCount ? pickEntryNames[pickGameIndex] : "");
  SDL_UnlockMutex(lock);
  return s;
}

bool boxart_pick_choose(const char *stem, int i, char *err, size_t errSize) {
  SDL_LockMutex(lock);
  bool ok = pick && i >= 0 && i < pickCount;
  struct pick_cand c;
  if (ok)
    c = pick[i];
  SDL_UnlockMutex(lock);
  if (!ok) {
    snprintf(err, errSize, "That cover is gone");
    return false;
  }
  char path[1400];
  if (c.source == BOXART_PICK_LOCAL) {
    // One of the game's own pictures: an alternate swaps names with its box art.
    if (c.local == 0) {
      snprintf(err, errSize, "That's its box art already");
      return false;
    }
    if (!boxart_art_use(stem, c.local)) {
      snprintf(err, errSize, "Couldn't swap the pictures");
      return false;
    }
    return true;
  }
  char *img = NULL;
  size_t len = 0;
  if (c.source == BOXART_PICK_STEAMGRIDDB) {
    // The full picture, not its thumbnail.
    img = fetch_image(c.url, NULL, &len);
    if (!img) {
      snprintf(err, errSize, "Couldn't download that cover");
      return false;
    }
  } else {
    // libretro's: the picture is the thumbnail already.
    pick_file(i, path, sizeof path);
    img = read_image(path, &len);
  }
  struct art_game g;
  if (!img)
    ok = false;
  else if (art_game_of(stem, &g)) // into the game's art folder: the box art it had becomes an alternate
    ok = art_save_main(stem, img, len);
  else {
    path_of(path, sizeof path, stem, "custom");
    ok = write_file(path, img, len);
  }
  free(img);
  if (!ok)
    snprintf(err, errSize, "Couldn't save the box art");
  else
    SDL_AtomicAdd(&generation, 1);
  return ok;
}

void boxart_pick_close(void) {
  if (pickThread) {
    SDL_AtomicSet(&pickQuit, 1);
    SDL_WaitThread(pickThread, NULL);
    pickThread = NULL;
  }
  if (!lock)
    return;
  SDL_LockMutex(lock);
  pick_reset();
  free(pick);
  pick = NULL;
  pickCount = 0;
  SDL_UnlockMutex(lock);
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
  if (!art_main_path(stem, path, sizeof path)) {
    path_of(path, sizeof path, stem, "custom");
    if (!pocket_exists(path))
      path_of(path, sizeof path, stem, "img");
  }
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
