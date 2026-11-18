/* GPL-3.0, see ../NOTICE. PC FAN GAMES (pc_game.h). */
#include "pc_game.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "pocket_os.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* ---------------------------------------------------------------- finding them */

static void join(char *out, size_t size, const char *dir, const char *name) {
  size_t n = strlen(dir);
  snprintf(out, size, "%s%s%s", dir, n && (dir[n - 1] == '/' || dir[n - 1] == '\\') ? "" : "/", name);
}

static bool exists_in(const char *dir, const char *name) {
  char p[1400];
  join(p, sizeof p, dir, name);
  return pocket_exists(p);
}

static long file_size(const char *path) {
  struct stat st;
  return stat(path, &st) ? -1 : (long)st.st_size;
}

static bool ends_with(const char *s, const char *tail) {
  size_t a = strlen(s), b = strlen(tail);
  return a >= b && !strcasecmp(s + a - b, tail);
}

struct scan {
  const char *dir;
  char bigExe[260], namedExe[260], rgssDll[64];
  long bigSize;
  bool rubyDll, hasExe;
};

/* Programs that come with these games but aren't the game: editors, tools, updaters, an old player kept aside. */
static bool tool_exe(const char *name) {
  static const char *const tools[] = {"editor", "animmaker", "mapmaker", "positioner", "extendtext", "updater",
                                      "gameold", "game (old)", "gamea", "gamergss", "unins", "setup", "config",
                                      "crashpad", "notification_helper"};
  char low[260];
  size_t i = 0;
  for (; name[i] && i < sizeof low - 1; i++)
    low[i] = (char)tolower((unsigned char)name[i]);
  low[i] = 0;
  for (size_t t = 0; t < sizeof tools / sizeof tools[0]; t++)
    if (!strncmp(low, tools[t], strlen(tools[t])))
      return true;
  return false;
}

static void scan_each(const char *name, void *user) {
  struct scan *s = user;
  if (ends_with(name, ".exe")) {
    s->hasExe = true;
    if (tool_exe(name))
      return;
    char p[1400];
    join(p, sizeof p, s->dir, name);
    long n = file_size(p);
    if (n > s->bigSize) {
      s->bigSize = n;
      snprintf(s->bigExe, sizeof s->bigExe, "%s", name);
    }
    if (!strcasecmp(name, "Game.exe") || !s->namedExe[0])
      snprintf(s->namedExe, sizeof s->namedExe, "%s", name);
  } else if (ends_with(name, ".dll")) {
    if (!strncasecmp(name, "x64-msvcrt-ruby", 15) || !strncasecmp(name, "msvcrt-ruby", 11))
      s->rubyDll = true;
    else if (!strncasecmp(name, "RGSS", 4))
      snprintf(s->rgssDll, sizeof s->rgssDll, "%s", name);
  }
}

/* Game.ini's KEY in [Game] (any case), trimmed. */
static bool ini_value(const char *dir, const char *key, char *out, size_t size) {
  char p[1400];
  join(p, sizeof p, dir, "Game.ini");
  FILE *f = fopen(p, "rb");
  if (!f)
    return false;
  char line[512];
  bool found = false;
  size_t k = strlen(key);
  while (!found && fgets(line, sizeof line, f)) {
    char *s = line;
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
      s += 3;
    while (*s == ' ' || *s == '\t')
      s++;
    if (strncasecmp(s, key, k) || (s[k] != '=' && s[k] != ' '))
      continue;
    s += k;
    while (*s == ' ' || *s == '=')
      s++;
    size_t n = strcspn(s, "\r\n");
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t'))
      n--;
    snprintf(out, size, "%.*s", (int)n, s);
    found = n > 0;
  }
  fclose(f);
  return found;
}

static bool has_scripts(const char *dir) {
  return exists_in(dir, "Data/Scripts.rxdata") || exists_in(dir, "Data/Scripts.rvdata2") ||
         exists_in(dir, "Data/Scripts.rvdata") || exists_in(dir, "Game.rgssad") || exists_in(dir, "Game.rgss2a") ||
         exists_in(dir, "Game.rgss3a");
}

bool pc_game_dir(const char *dir) {
  if (!pocket_is_dir(dir))
    return false;
  if (exists_in(dir, "Game.ini") && has_scripts(dir))
    return true;
  // No Game.ini (Phoenix Rising): an RGSS player's DLL and a program beside a Data folder.
  if (!exists_in(dir, "Data"))
    return false;
  struct scan s = {.dir = dir};
  pocket_list_dir(dir, scan_each, &s);
  return s.rgssDll[0] && s.hasExe;
}

bool pc_game_path(const char *path) { return path && pocket_is_dir(path) && pc_game_dir(path); }

static void leaf(const char *path, char *out, size_t size) {
  const char *a = strrchr(path, '/'), *b = strrchr(path, '\\');
  const char *l = a > b ? a : b;
  snprintf(out, size, "%s", l ? l + 1 : path);
}

/* ---------------------------------------------------------------- a borrowed player */

bool pc_ruby18_player(const char *dir, char *exe, size_t exeSize) {
  if (!exists_in(dir, "Game.ini"))
    return false;
  struct scan s = {.dir = dir};
  pocket_list_dir(dir, scan_each, &s);
  if (!s.bigExe[0] || s.bigSize < 4L * 1024 * 1024 || s.bigSize > 64L * 1024 * 1024)
    return false;
  char path[1400];
  join(path, sizeof path, dir, s.bigExe);
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  // Ruby's version is written in the program: "1.8.7" in a Ruby 1.8 build.
  static char buf[1 << 16];
  bool found = false;
  size_t n, keep = 0;
  while (!found && (n = fread(buf + keep, 1, sizeof buf - keep, f)) > 0) {
    n += keep;
    for (size_t i = 0; i + 5 <= n && !found; i++)
      found = !memcmp(buf + i, "1.8.7", 5);
    keep = n >= 4 ? 4 : n;
    memmove(buf, buf + n - keep, keep);
  }
  fclose(f);
  if (found)
    snprintf(exe, exeSize, "%s", s.bigExe);
  return found;
}

struct borrow {
  const char *self;
  char parent[1024];
  struct pc_game_info *out;
  bool done;
};
static void borrow_each(const char *name, void *user) {
  struct borrow *b = user;
  if (b->done || name[0] == '.')
    return;
  char dir[1100], exe[260];
  join(dir, sizeof dir, b->parent, name);
  if (!strcmp(dir, b->self) || !pc_ruby18_player(dir, exe, sizeof exe))
    return;
  if (strlen(dir) >= sizeof b->out->playerDir)
    return;
  snprintf(b->out->playerDir, sizeof b->out->playerDir, "%.1000s", dir);
  snprintf(b->out->exe, sizeof b->out->exe, "%s", exe);
  b->out->player = PC_PLAYER_MKXPZ;
  b->out->companion = true;
  b->done = true;
}
static void borrow_player(const char *dir, struct pc_game_info *out) {
  struct borrow b = {.self = dir, .out = out};
  snprintf(b.parent, sizeof b.parent, "%s", dir);
  char *cut = strrchr(b.parent, '/'), *bs = strrchr(b.parent, '\\');
  if (bs > cut)
    cut = bs;
  if (!cut)
    return;
  *cut = 0;
  // The same result for a while: a library load asks about every game.
  static char lastParent[1024], lastDir[1024], lastExe[260];
  static bool lastFound, cached;
  if (cached && !strcmp(lastParent, b.parent)) {
    if (lastFound) {
      snprintf(out->playerDir, sizeof out->playerDir, "%s", lastDir);
      snprintf(out->exe, sizeof out->exe, "%s", lastExe);
      out->player = PC_PLAYER_MKXPZ;
      out->companion = true;
    }
    return;
  }
  pocket_list_dir(b.parent, borrow_each, &b);
  cached = true;
  snprintf(lastParent, sizeof lastParent, "%s", b.parent);
  lastFound = b.done;
  if (b.done) {
    snprintf(lastDir, sizeof lastDir, "%s", out->playerDir);
    snprintf(lastExe, sizeof lastExe, "%s", out->exe);
  }
}

bool pc_game_info(const char *dir, struct pc_game_info *out) {
  memset(out, 0, sizeof *out);
  if (!pc_game_dir(dir))
    return false;
  snprintf(out->dir, sizeof out->dir, "%s", dir);
  if (!ini_value(dir, "Title", out->title, sizeof out->title))
    leaf(dir, out->title, sizeof out->title);
  struct scan s = {.dir = dir};
  pocket_list_dir(dir, scan_each, &s);
  char lib[64] = "";
  if (!ini_value(dir, "Library", lib, sizeof lib))
    snprintf(lib, sizeof lib, "%s", s.rgssDll);
  out->playerDir[0] = 0;
  out->rgss = !strncasecmp(lib, "RGSS1", 5) ? 1 : !strncasecmp(lib, "RGSS2", 5) ? 2 : !strncasecmp(lib, "RGSS3", 5) ? 3 : 0;
  if (!out->rgss && exists_in(dir, "Data/Scripts.rxdata"))
    out->rgss = 1;
  // mkxp-z: a big player (an RGSS player is well under a megabyte) beside Ruby's DLL or a mkxp.json.
  if (s.bigExe[0] && s.bigSize >= 4L * 1024 * 1024 && (s.rubyDll || exists_in(dir, "mkxp.json") || s.bigSize >= 8L * 1024 * 1024)) {
    out->player = PC_PLAYER_MKXPZ;
    snprintf(out->exe, sizeof out->exe, "%s", s.bigExe);
    snprintf(out->ownExe, sizeof out->ownExe, "%s", out->exe);
    out->companion = out->rgss == 1 || out->rgss == 0;
  } else if (s.namedExe[0] || s.bigExe[0]) {
    out->player = PC_PLAYER_RGSS;
    snprintf(out->exe, sizeof out->exe, "%s", s.bigExe[0] ? s.bigExe : s.namedExe);
    snprintf(out->ownExe, sizeof out->ownExe, "%s", out->exe);
    // An XP game: a Ruby 1.8 mkxp-z player beside it, in the same folder of games, plays it with the wings.
    if (out->rgss == 1)
      borrow_player(dir, out);
  } else
    out->player = PC_PLAYER_NONE;
  return out->player != PC_PLAYER_NONE;
}

/* ---------------------------------------------------------------- mkxp.json */

struct jtok {
  const char *p, *end;
};

static void jskip(struct jtok *t) {
  for (;;) {
    while (t->p < t->end && isspace((unsigned char)*t->p))
      t->p++;
    if (t->p + 1 < t->end && t->p[0] == '/' && t->p[1] == '/') {
      while (t->p < t->end && *t->p != '\n')
        t->p++;
    } else if (t->p + 1 < t->end && t->p[0] == '/' && t->p[1] == '*') {
      t->p += 2;
      while (t->p + 1 < t->end && !(t->p[0] == '*' && t->p[1] == '/'))
        t->p++;
      t->p = t->p + 2 <= t->end ? t->p + 2 : t->end;
    } else
      return;
  }
}

/* Past one value (a string, a number, a word, or a whole object / array). */
static bool jvalue_skip(struct jtok *t) {
  jskip(t);
  if (t->p >= t->end)
    return false;
  char c = *t->p;
  if (c == '"' || c == '\'') {
    t->p++;
    while (t->p < t->end && *t->p != c) {
      if (*t->p == '\\')
        t->p++;
      t->p++;
    }
    if (t->p >= t->end)
      return false;
    t->p++;
    return true;
  }
  if (c == '{' || c == '[') {
    int depth = 0;
    while (t->p < t->end) {
      jskip(t);
      if (t->p >= t->end)
        break;
      c = *t->p;
      if (c == '"' || c == '\'') {
        if (!jvalue_skip(t))
          return false;
        continue;
      }
      if (c == '{' || c == '[')
        depth++;
      else if (c == '}' || c == ']') {
        depth--;
        if (depth == 0) {
          t->p++;
          return true;
        }
      }
      t->p++;
    }
    return false;
  }
  while (t->p < t->end && *t->p != ',' && *t->p != '}' && *t->p != ']' && !isspace((unsigned char)*t->p) &&
         *t->p != '/')
    t->p++;
  return true;
}

struct out {
  char *s;
  size_t size, n;
  bool over;
};
static void put(struct out *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void put(struct out *o, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int w = vsnprintf(o->s + o->n, o->n < o->size ? o->size - o->n : 0, fmt, ap);
  va_end(ap);
  if (w < 0 || o->n + (size_t)w >= o->size)
    o->over = true;
  else
    o->n += (size_t)w;
}

/* A path as a JSON string's insides: forward slashes, quotes escaped. */
static void put_path(struct out *o, const char *path) {
  put(o, "\"");
  for (const char *c = path; *c; c++) {
    if (*c == '\\')
      put(o, "/");
    else if (*c == '"')
      put(o, "\\\"");
    else
      put(o, "%c", *c);
  }
  put(o, "\"");
}

static const char *const OVERRIDDEN[] = {"gameFolder", "defScreenW", "defScreenH", "winResizable", "fullscreen",
                                         "fixedAspectRatio", "smoothScaling", "integerScalingActive",
                                         "integerScalingLastMile", "printFPS", "preloadScript", "rgssVersion",
                                         "enableSettings", "vsync", "syncToRefreshrate"};

int pc_mkxp_json(const char *text, const struct pc_mkxp_overrides *o, char *buf, size_t size) {
  struct out out = {buf, size, 0, false};
  char theirPreload[4096] = "";
  put(&out, "{\n");
  if (text) {
    struct jtok t = {text, text + strlen(text)};
    jskip(&t);
    if (t.p < t.end && *t.p == '{') {
      t.p++;
      for (;;) {
        jskip(&t);
        if (t.p >= t.end || *t.p == '}')
          break;
        if (*t.p == ',') {
          t.p++;
          continue;
        }
        const char *ks = t.p;
        if (!jvalue_skip(&t))
          break;
        const char *ke = t.p;
        jskip(&t);
        if (t.p >= t.end || *t.p != ':')
          break;
        t.p++;
        jskip(&t);
        const char *vs = t.p;
        if (!jvalue_skip(&t))
          break;
        const char *ve = t.p;
        char key[64];
        const char *k0 = ks, *k1 = ke;
        if (k1 - k0 >= 2 && (*k0 == '"' || *k0 == '\''))
          k0++, k1--;
        snprintf(key, sizeof key, "%.*s", (int)(k1 - k0), k0);
        bool ours = false;
        for (size_t i = 0; i < sizeof OVERRIDDEN / sizeof OVERRIDDEN[0]; i++)
          ours |= !strcmp(key, OVERRIDDEN[i]);
        // The game's own RGSS version stays unless PokeDaisy picks one (a borrowed player): without it, a game
        // that names its version only here (Rejuvenation) is started as the wrong one and stops at once.
        if (!strcmp(key, "rgssVersion") && o->rgssVersion <= 0)
          ours = false;
        if (!strcmp(key, "preloadScript"))
          snprintf(theirPreload, sizeof theirPreload, "%.*s", (int)(ve - vs), vs);
        if (!ours)
          put(&out, "  \"%s\": %.*s,\n", key, (int)(ve - vs), vs);
      }
    }
  }
  put(&out, "  \"gameFolder\": ");
  put_path(&out, o->gameFolder);
  put(&out, ",\n  \"defScreenW\": %d,\n  \"defScreenH\": %d,\n", o->windowW, o->windowH);
  put(&out, "  \"winResizable\": true,\n  \"fullscreen\": false,\n  \"fixedAspectRatio\": true,\n");
  put(&out, "  \"smoothScaling\": %d,\n  \"integerScalingActive\": %s,\n  \"integerScalingLastMile\": %s,\n",
      o->smooth ? 1 : 0, o->smooth ? "true" : "false", o->smooth ? "true" : "false");
  if (o->rgssVersion > 0)
    put(&out, "  \"rgssVersion\": %d,\n", o->rgssVersion);
  // F1's key binding window off: it needs a mouse and a keyboard, so a controller alone can't close it, and keys
  // bound anew there would no longer be the ones PokeDaisy presses for the controller (pc_pad_mkxpz).
  put(&out, "  \"enableSettings\": false,\n");
  // The game's own frame timer paces it: waiting on the screen's refresh as well stalls a window that sits over
  // PokeDaisy's (Indigo fell to 7 frames a second with vsync and syncToRefreshrate on, 40 with them off).
  put(&out, "  \"vsync\": false,\n  \"syncToRefreshrate\": false,\n");
  put(&out, "  \"printFPS\": false,\n  \"preloadScript\": [");
  // The stand-ins first, the game's own next (a string or a list), then the companion.
  bool any = false;
  if (o->standIns && *o->standIns) {
    put_path(&out, o->standIns);
    any = true;
  }
  char *p = theirPreload;
  while (*p == ' ' || *p == '[')
    p++;
  size_t n = strlen(p);
  while (n && (p[n - 1] == ']' || p[n - 1] == ' ' || p[n - 1] == ',' || p[n - 1] == '\n' || p[n - 1] == '\r' ||
               p[n - 1] == '\t'))
    n--;
  if (n) {
    put(&out, "%.*s", (int)n, p);
    any = true;
  }
  if (o->preload) {
    put(&out, "%s", any ? ", " : "");
    put_path(&out, o->preload);
    any = true;
  }
  if (o->extraPreload && *o->extraPreload) {
    put(&out, "%s", any ? ", " : "");
    put_path(&out, o->extraPreload);
  }
  put(&out, "]\n}\n");
  return out.over ? -1 : (int)out.n;
}

/* ---------------------------------------------------------------- the companion's JSON */

/* A tiny reader for what pc_companion.rb writes: objects, arrays, strings, numbers, true / false / null. */
struct jr {
  const char *p, *end;
};
static void jr_ws(struct jr *r) {
  while (r->p < r->end && isspace((unsigned char)*r->p))
    r->p++;
}
static bool jr_string(struct jr *r, char *out, size_t size) {
  jr_ws(r);
  if (r->p >= r->end || *r->p != '"')
    return false;
  r->p++;
  size_t n = 0;
  while (r->p < r->end && *r->p != '"') {
    unsigned c = (unsigned char)*r->p++;
    if (c == '\\' && r->p < r->end) {
      char e = *r->p++;
      if (e == 'n')
        c = ' ';
      else if (e == 'u' && r->end - r->p >= 4) {
        char hex[5] = {r->p[0], r->p[1], r->p[2], r->p[3], 0};
        r->p += 4;
        unsigned cp = (unsigned)strtoul(hex, NULL, 16);
        char u[4];
        int k = 0;
        if (cp < 0x80)
          u[k++] = (char)cp;
        else if (cp < 0x800) {
          u[k++] = (char)(0xC0 | cp >> 6);
          u[k++] = (char)(0x80 | (cp & 0x3F));
        } else {
          u[k++] = (char)(0xE0 | cp >> 12);
          u[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          u[k++] = (char)(0x80 | (cp & 0x3F));
        }
        for (int i = 0; i < k; i++)
          if (out && n + 1 < size)
            out[n++] = u[i];
        continue;
      } else
        c = (unsigned char)e;
    }
    if (out && n + 1 < size)
      out[n++] = (char)c;
  }
  if (out && size)
    out[n] = 0;
  if (r->p >= r->end)
    return false;
  r->p++;
  return true;
}
static bool jr_skip(struct jr *r);
static bool jr_skip(struct jr *r) {
  jr_ws(r);
  if (r->p >= r->end)
    return false;
  if (*r->p == '"')
    return jr_string(r, NULL, 0);
  if (*r->p == '{' || *r->p == '[') {
    char close = *r->p == '{' ? '}' : ']';
    r->p++;
    for (;;) {
      jr_ws(r);
      if (r->p >= r->end)
        return false;
      if (*r->p == close) {
        r->p++;
        return true;
      }
      if (*r->p == ',' || *r->p == ':') {
        r->p++;
        continue;
      }
      if (!jr_skip(r))
        return false;
    }
  }
  while (r->p < r->end && *r->p != ',' && *r->p != '}' && *r->p != ']')
    r->p++;
  return true;
}
static long jr_number(struct jr *r) {
  jr_ws(r);
  char *e;
  long v = strtol(r->p, &e, 10);
  if (e == r->p) {
    jr_skip(r);
    return 0;
  }
  r->p = e;
  if (r->p < r->end && *r->p == '.')
    jr_skip(r);
  return v;
}
static bool jr_bool(struct jr *r) {
  jr_ws(r);
  bool v = r->end - r->p >= 4 && !strncmp(r->p, "true", 4);
  jr_skip(r);
  return v;
}
/* Walks an object: calls EACH(key) with r at the value; EACH reads it or returns false to have it skipped. */
typedef bool (*jr_each)(struct jr *r, const char *key, void *user);
static bool jr_object(struct jr *r, jr_each each, void *user) {
  jr_ws(r);
  if (r->p >= r->end || *r->p != '{')
    return jr_skip(r) && false;
  r->p++;
  for (;;) {
    jr_ws(r);
    if (r->p >= r->end)
      return false;
    if (*r->p == '}') {
      r->p++;
      return true;
    }
    if (*r->p == ',') {
      r->p++;
      continue;
    }
    char key[32];
    if (!jr_string(r, key, sizeof key))
      return false;
    jr_ws(r);
    if (r->p >= r->end || *r->p != ':')
      return false;
    r->p++;
    if (!each(r, key, user) && !jr_skip(r))
      return false;
  }
}
/* Walks an array: EACH(index) with r at the item. */
typedef bool (*jr_item)(struct jr *r, int i, void *user);
static bool jr_array(struct jr *r, jr_item each, void *user) {
  jr_ws(r);
  if (r->p >= r->end || *r->p != '[')
    return jr_skip(r) && false;
  r->p++;
  for (int i = 0;;) {
    jr_ws(r);
    if (r->p >= r->end)
      return false;
    if (*r->p == ']') {
      r->p++;
      return true;
    }
    if (*r->p == ',') {
      r->p++;
      continue;
    }
    if (!each(r, i++, user) && !jr_skip(r))
      return false;
  }
}

struct types_ctx {
  char (*types)[12];
};
static bool type_item(struct jr *r, int i, void *user) {
  struct types_ctx *t = user;
  if (i >= 2)
    return false;
  return jr_string(r, t->types[i], sizeof t->types[i]);
}
static bool mon_key(struct jr *r, const char *k, void *user) {
  struct pc_mon *m = user;
  if (!strcmp(k, "species"))
    return jr_string(r, m->species, sizeof m->species);
  if (!strcmp(k, "id"))
    return jr_string(r, m->id, sizeof m->id);
  if (!strcmp(k, "name"))
    return jr_string(r, m->name, sizeof m->name);
  if (!strcmp(k, "item"))
    return jr_string(r, m->item, sizeof m->item);
  if (!strcmp(k, "status"))
    return jr_string(r, m->status, sizeof m->status);
  if (!strcmp(k, "icon"))
    return jr_string(r, m->icon, sizeof m->icon);
  if (!strcmp(k, "level"))
    return m->level = (int)jr_number(r), true;
  if (!strcmp(k, "hp"))
    return m->hp = (int)jr_number(r), true;
  if (!strcmp(k, "maxhp"))
    return m->maxHp = (int)jr_number(r), true;
  if (!strcmp(k, "form"))
    return m->form = (int)jr_number(r), true;
  if (!strcmp(k, "egg"))
    return m->egg = jr_bool(r), true;
  if (!strcmp(k, "shiny"))
    return m->shiny = jr_bool(r), true;
  if (!strcmp(k, "types")) {
    struct types_ctx t = {m->types};
    return jr_array(r, type_item, &t);
  }
  return false;
}
static bool wild_key(struct jr *r, const char *k, void *user) {
  struct pc_wild *w = user;
  if (!strcmp(k, "species"))
    return jr_string(r, w->species, sizeof w->species);
  if (!strcmp(k, "id"))
    return jr_string(r, w->id, sizeof w->id);
  if (!strcmp(k, "min"))
    return w->min = (int)jr_number(r), true;
  if (!strcmp(k, "max"))
    return w->max = (int)jr_number(r), true;
  if (!strcmp(k, "types")) {
    struct types_ctx t = {w->types};
    return jr_array(r, type_item, &t);
  }
  return false;
}
static bool party_item(struct jr *r, int i, void *user) {
  struct pc_state *s = user;
  if (i >= PC_PARTY_MAX)
    return false;
  memset(&s->party[i], 0, sizeof s->party[i]);
  if (!jr_object(r, mon_key, &s->party[i]))
    return true;
  s->partyCount = i + 1;
  return true;
}
static bool foe_item(struct jr *r, int i, void *user) {
  struct pc_state *s = user;
  if (i >= PC_FOES_MAX)
    return false;
  memset(&s->foes[i], 0, sizeof s->foes[i]);
  if (!jr_object(r, mon_key, &s->foes[i]))
    return true;
  s->foeCount = i + 1;
  return true;
}
static bool wild_item(struct jr *r, int i, void *user) {
  struct pc_state *s = user;
  if (i >= PC_WILD_MAX)
    return false;
  memset(&s->wild[i], 0, sizeof s->wild[i]);
  if (!jr_object(r, wild_key, &s->wild[i]))
    return true;
  s->wildCount = i + 1;
  return true;
}
static bool team_key(struct jr *r, const char *k, void *user) {
  struct pc_state *s = user;
  if (!strcmp(k, "count"))
    return s->foeTeam = (int)jr_number(r), true;
  if (!strcmp(k, "alive"))
    return s->foeAlive = (int)jr_number(r), true;
  return false;
}
static const char *const PAD_NAMES[PC_PAD_BUTTONS] = {"C", "B", "A", "X", "Y", "Z", "L", "R", "F5",
                                                       "UP", "DOWN", "LEFT", "RIGHT"};
static int pad_button(const char *name, size_t n) {
  for (int i = 0; i < PC_PAD_BUTTONS; i++)
    if (strlen(PAD_NAMES[i]) == n && !memcmp(PAD_NAMES[i], name, n))
      return i;
  return -1;
}
/* "C:67,B:88" (pad_keys) or "C:CONFIRM,B:BACK/MENU" (pad_roles), into S. */
static void pad_list(const char *text, struct pc_state *s, bool roles) {
  for (const char *p = text; *p;) {
    const char *end = strchr(p, ','), *colon = strchr(p, ':');
    if (!end)
      end = p + strlen(p);
    if (colon && colon < end) {
      int b = pad_button(p, (size_t)(colon - p));
      if (b >= 0 && roles) {
        size_t n = (size_t)(end - colon - 1);
        if (n >= sizeof s->padRole[b])
          n = sizeof s->padRole[b] - 1;
        memcpy(s->padRole[b], colon + 1, n);
        s->padRole[b][n] = 0;
      } else if (b >= 0) {
        int vk = atoi(colon + 1);
        s->padVk[b] = vk > 0 && vk < 256 ? (unsigned char)vk : 0;
      }
    }
    p = *end ? end + 1 : end;
  }
}
static bool state_key(struct jr *r, const char *k, void *user) {
  struct pc_state *s = user;
  if (!strcmp(k, "pad_feed"))
    return s->padFeed = jr_number(r) > 0, true;
  if (!strcmp(k, "pad") || !strcmp(k, "pad_keys") || !strcmp(k, "pad_roles")) {
    char text[400];
    if (!jr_string(r, text, sizeof text))
      return false;
    if (!strcmp(k, "pad"))
      s->pad = !strcmp(text, "keys") ? PC_PAD_KEYS : !strcmp(text, "native") ? PC_PAD_NATIVE : PC_PAD_UNKNOWN;
    else
      pad_list(text, s, !strcmp(k, "pad_roles"));
    return true;
  }
  if (!strcmp(k, "player"))
    return jr_string(r, s->player, sizeof s->player);
  if (!strcmp(k, "map"))
    return jr_string(r, s->map, sizeof s->map);
  if (!strcmp(k, "scene"))
    return jr_string(r, s->scene, sizeof s->scene);
  if (!strcmp(k, "map_id"))
    return s->mapId = (int)jr_number(r), true;
  if (!strcmp(k, "badges"))
    return s->badges = (int)jr_number(r), true;
  if (!strcmp(k, "money"))
    return s->money = jr_number(r), true;
  if (!strcmp(k, "time"))
    return s->seconds = jr_number(r), true;
  if (!strcmp(k, "battle"))
    return s->battle = jr_bool(r), true;
  if (!strcmp(k, "screen_w"))
    return s->screenW = (int)jr_number(r), true;
  if (!strcmp(k, "screen_h"))
    return s->screenH = (int)jr_number(r), true;
  if (!strcmp(k, "seen"))
    return s->seen = (int)jr_number(r), true;
  if (!strcmp(k, "caught"))
    return s->caught = (int)jr_number(r), true;
  if (!strcmp(k, "party"))
    return jr_array(r, party_item, s);
  if (!strcmp(k, "foes"))
    return jr_array(r, foe_item, s);
  if (!strcmp(k, "wild"))
    return jr_array(r, wild_item, s);
  if (!strcmp(k, "foe_team"))
    return jr_object(r, team_key, s);
  return false;
}

bool pc_state_parse(const char *json, struct pc_state *out) {
  memset(out, 0, sizeof *out);
  out->money = -1;
  if (!json)
    return false;
  struct jr r = {json, json + strlen(json)};
  out->valid = jr_object(&r, state_key, out);
  return out->valid;
}

int pc_pad_target(const struct pc_state *s, int in) {
  static const int fixed[PC_INS] = {PC_PAD_C,  PC_PAD_B, PC_PAD_A,  PC_PAD_X,    PC_PAD_L,    PC_PAD_R,    PC_PAD_Z,
                                    -1,        -1,       PC_PAD_UP, PC_PAD_DOWN, PC_PAD_LEFT, PC_PAD_RIGHT};
  if (in < 0 || in >= PC_INS)
    return -1;
  if (in == PC_IN_SELECT || in == PC_IN_START) {
    if (s->pad != PC_PAD_KEYS)
      return -1;
    if (in == PC_IN_SELECT)
      return s->padVk[PC_PAD_F5] ? PC_PAD_F5 : s->padVk[PC_PAD_Y] ? PC_PAD_Y : -1;
    for (int b = 0; b < PC_PAD_F5; b++) // the game's menu button, else B (Essentials' own)
      if (strstr(s->padRole[b], "MENU"))
        return b;
    return PC_PAD_B;
  }
  return fixed[in];
}

void pc_pad_mkxpz(struct pc_state *s) {
  if (s->pad == PC_PAD_KEYS && !s->padOurs)
    return; // the game's own keys (the companion read them)
  // mkxp-z's default keyboard: ENTER C, ESC B, SHIFT A, then A X, S Y, D Z, Q L, W R (by the key's place, so any
  // keyboard layout). ENTER and ESC rather than SPACE and X: a name typed on the keyboard keeps its letters.
  static const unsigned char vk[PC_PAD_BUTTONS] = {0x0D, 0x1B, 0x10, 'A', 'S', 'D', 'Q', 'W', 0,
                                                   0x26, 0x28, 0x25, 0x27};
  memcpy(s->padVk, vk, sizeof s->padVk);
  s->pad = PC_PAD_KEYS;
  s->padOurs = true;
}

unsigned pc_pad_mask(const struct pc_state *s, unsigned held) {
  if (s->pad != PC_PAD_KEYS)
    return 0;
  unsigned chord = (1u << PC_IN_SELECT) | (1u << PC_IN_START);
  if ((held & chord) == chord)
    held &= ~chord;
  unsigned mask = 0;
  for (int in = 0; in < PC_INS; in++) {
    int b = held & (1u << in) ? pc_pad_target(s, in) : -1;
    if (b >= 0 && s->padVk[b])
      mask |= 1u << b;
  }
  return mask;
}

void pc_pad_keys(const struct pc_state *s, unsigned held, bool down[256]) {
  memset(down, 0, 256 * sizeof *down);
  unsigned mask = pc_pad_mask(s, held);
  for (int b = 0; b < PC_PAD_BUTTONS; b++)
    if (mask & (1u << b))
      down[s->padVk[b]] = true;
}

unsigned pc_stick_dir(int x, int y, unsigned was) {
  const unsigned h = 1u << PC_IN_LEFT | 1u << PC_IN_RIGHT, v = 1u << PC_IN_UP | 1u << PC_IN_DOWN;
  long long r2 = (long long)x * x + (long long)y * y, need = was & (h | v) ? PC_STICK_OFF : PC_STICK_ON;
  if (r2 < need * need)
    return 0;
  long long ax = x < 0 ? -(long long)x : x, ay = y < 0 ? -(long long)y : y;
  unsigned across = 1u << (x < 0 ? PC_IN_LEFT : PC_IN_RIGHT), upDown = 1u << (y < 0 ? PC_IN_UP : PC_IN_DOWN);
  if ((was & h) && ay * 4 <= ax * 5)
    return across;
  if ((was & v) && ax * 4 <= ay * 5)
    return upDown;
  return ax > ay ? across : upDown;
}

void pc_stick_keys(unsigned dir, bool down[256]) {
  memset(down, 0, 256 * sizeof *down);
  down[0x26] = (dir & (1u << PC_IN_UP)) != 0; // VK_UP
  down[0x28] = (dir & (1u << PC_IN_DOWN)) != 0;
  down[0x25] = (dir & (1u << PC_IN_LEFT)) != 0;
  down[0x27] = (dir & (1u << PC_IN_RIGHT)) != 0;
}

bool pc_pad_feed(const char *path, unsigned mask) {
  // Kept open for the whole play and written in place, always the same 8 bytes ("m%05u;\n"), so nothing is cut short
  // and a read caught halfway still takes only a whole "m<mask>;". Opened and closed for each press, the file was
  // scanned by Windows' antivirus after every write and the game, opening it to read, waited for the scan: the game
  // lagged after each button press.
  static FILE *f;
  static char at[1400];
  if (!path || strcmp(path, at)) {
    if (f)
      fclose(f);
    f = NULL;
    at[0] = 0;
    if (!path)
      return true;
    f = fopen(path, "wb");
    if (!f)
      return false;
    snprintf(at, sizeof at, "%s", path);
  }
  if (!f || fseek(f, 0, SEEK_SET))
    return false;
  return fprintf(f, "m%05u;\n", mask & 0xFFFFu) == 8 && fflush(f) == 0;
}

/* A key's name as a keyboard prints it ("S", "F5", "SHIFT"), else "". */
static void key_name(int vk, char *out, size_t size) {
  static const struct {
    int vk;
    const char *name;
  } named[] = {{0x08, "BACKSPACE"}, {0x09, "TAB"},     {0x0D, "ENTER"}, {0x10, "SHIFT"}, {0x11, "CTRL"},
               {0x12, "ALT"},       {0x1B, "ESC"},     {0x20, "SPACE"}, {0x21, "PG UP"}, {0x22, "PG DN"},
               {0x23, "END"},       {0x24, "HOME"},    {0x2D, "INS"},   {0x2E, "DEL"}};
  out[0] = 0;
  if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
    snprintf(out, size, "%c", vk);
  else if (vk >= 0x70 && vk <= 0x7B)
    snprintf(out, size, "F%d", vk - 0x6F);
  else
    for (size_t i = 0; i < sizeof named / sizeof named[0]; i++)
      if (named[i].vk == vk)
        snprintf(out, size, "%s", named[i].name);
}

int pc_pad_hints(const struct pc_state *s, bool xbox, struct pc_hint *out, int max) {
  static const int order[] = {PC_IN_A, PC_IN_B, PC_IN_X, PC_IN_Y, PC_IN_START, PC_IN_SELECT, PC_IN_L, PC_IN_R, PC_IN_R3};
  static const char *const names[] = {"A", "B", "X", "Y", "START", "SELECT", "LB", "RB", "R3"};
  int n = 0;
  if (s->pad == PC_PAD_UNKNOWN)
    return 0;
  for (size_t i = 0; i < sizeof order / sizeof order[0] && n < max; i++) {
    int b = pc_pad_target(s, order[i]);
    if (b < 0)
      continue;
    char label[24];
    snprintf(label, sizeof label, "%s", order[i] == PC_IN_START ? "MENU" : s->padRole[b]);
    if (!label[0] && s->pad == PC_PAD_KEYS && !s->padOurs && s->padVk[b] && b != PC_PAD_C && b != PC_PAD_B) {
      char k[16];
      key_name(s->padVk[b], k, sizeof k);
      if (k[0])
        snprintf(label, sizeof label, "%s KEY", k);
    }
    if (!label[0])
      continue;
    snprintf(out[n].button, sizeof out[n].button, "%s",
             !xbox && order[i] == PC_IN_L ? "L" : !xbox && order[i] == PC_IN_R ? "R" : names[i]);
    snprintf(out[n].label, sizeof out[n].label, "%s", label);
    n++;
  }
  return n;
}

int pc_type_index(const char *id) {
  // The GBA's type numbers; Fairy takes 9 (the "???" slot), as the DS hosts with Fairy do (pd_ds_types).
  static const char *const names[19] = {"NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND", "ROCK",
                                        "BUG",    "GHOST",    "STEEL",  "FAIRY",  "FIRE",   "WATER",
                                        "GRASS",  "ELECTRIC", "PSYCHIC", "ICE",   "DRAGON", "DARK", NULL};
  if (!id || !*id)
    return -1;
  for (int i = 0; names[i]; i++)
    if (!strcasecmp(id, names[i]))
      return i;
  return -1;
}

unsigned pc_status_bits(const char *w) {
  if (!w || !*w)
    return 0;
  if (!strcmp(w, "SLP"))
    return 1;
  if (!strcmp(w, "PSN"))
    return 1u << 3;
  if (!strcmp(w, "BRN"))
    return 1u << 4;
  if (!strcmp(w, "FRZ"))
    return 1u << 5;
  if (!strcmp(w, "PAR"))
    return 1u << 6;
  return 0;
}

/* ---------------------------------------------------------------- layout */

bool pc_swap_face(const char *mapping, char *out, size_t size) {
  if (!mapping || !out || size < 40)
    return false;
  const char *comma = strchr(mapping, ',');
  if (!comma || comma - mapping != 32 || strchr(mapping, '\n'))
    return false;
  size_t n = 0;
  for (const char *p = mapping; *p;) {
    const char *end = strchr(p, ',');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    char item[128];
    if (len >= sizeof item)
      return false;
    memcpy(item, p, len);
    item[len] = 0;
    if (p == mapping && item[2] == '0' && item[3] == '0') // a bus-and-IDs GUID: its CRC part (bytes 2, 3) zeroed
      memcpy(item + 4, "0000", 4);
    else if (len > 2 && item[1] == ':' && strchr("abxy", item[0])) {
      static const char from[] = "abxy", to[] = "bayx";
      item[0] = to[strchr(from, item[0]) - from];
    }
    if (n + len + 2 > size)
      return false;
    memcpy(out + n, item, len);
    n += len;
    if (end)
      out[n++] = ',';
    out[n] = 0;
    p = end ? end + 1 : p + len;
  }
  return true;
}

void pc_wide_layout(int ow, int oh, int room, struct pc_layout *out) {
  pc_layout_for(ow, oh, 512, 384, room, out);
  int wing = oh / 10 > 120 ? oh / 10 : 120;
  int w = (ow - 2 * wing) & ~1;
  if (w > out->w) {
    out->w = w;
    out->x = (ow - w) / 2;
    out->wing = out->x;
  }
}

bool pc_undouble(const uint32_t *px, int pitch, int w, int h, uint32_t *out, bool always) {
  if (!px || !out || w < 4 || h < 4 || pitch < w)
    return false;
  int hw = w / 2, hh = h / 2;
  long same = 0, all = (long)hw * hh;
  for (int y = 0; y < hh; y++) {
    const uint32_t *a = px + (size_t)(2 * y) * pitch, *b = a + pitch;
    for (int x = 0; x < hw; x++) {
      uint32_t p = a[2 * x] & 0xffffff;
      same += (a[2 * x + 1] & 0xffffff) == p && (b[2 * x] & 0xffffff) == p && (b[2 * x + 1] & 0xffffff) == p;
    }
  }
  bool doubled = same * 100 >= all * 85; // else a picture drawn at its full size
  if (!doubled && !always)
    return false;
  for (int y = 0; y < hh; y++) {
    const uint32_t *a = px + (size_t)(2 * y) * pitch, *b = a + pitch;
    uint32_t *o = out + (size_t)y * hw;
    for (int x = 0; x < hw; x++) {
      uint32_t q[4] = {a[2 * x], a[2 * x + 1], b[2 * x], b[2 * x + 1]}, r = 0;
      for (int c = 0; c < 24; c += 8)
        r |= (uint32_t)((((q[0] >> c) & 255) + ((q[1] >> c) & 255) + ((q[2] >> c) & 255) + ((q[3] >> c) & 255) + 2) /
                        4)
             << c;
      o[x] = r | 0xff000000u;
    }
  }
  return doubled;
}

void pc_zoom_view(float scale, float panX, float panY, int w, int h, float *x, float *y, float *vw, float *vh) {
  if (scale < 1)
    scale = 1;
  panX = panX < -1 ? -1 : panX > 1 ? 1 : panX;
  panY = panY < -1 ? -1 : panY > 1 ? 1 : panY;
  *vw = w / scale;
  *vh = h / scale;
  *x = (w - *vw) / 2 * (1 + panX);
  *y = (h - *vh) / 2 * (1 + panY);
}

void pc_layout_for(int ow, int oh, int bw, int bh, int room, struct pc_layout *out) {
  if (bw <= 0 || bh <= 0)
    bw = 512, bh = 384;
  int gw = 0, gh = 0;
  if (room != 0) {
    int wingMin = oh * (room == 2 ? 27 : 20) / 100, barMin = oh * 5 / 100;
    for (int q = 40; q >= 4 && !gw; q--) { // q quarters of the game's own size: 10x down to 1x
      int w = bw * q / 4, h = bh * q / 4;
      if ((ow - w) / 2 >= wingMin && (oh - h) / 2 >= barMin)
        gw = w, gh = h;
    }
  }
  if (!gw) { // GAME FIRST, or a screen too small for the rule: the full height
    gh = oh;
    gw = (int)((long long)gh * bw / bh);
    if (gw > ow) {
      gw = ow;
      gh = (int)((long long)gw * bh / bw);
    }
  }
  out->w = gw;
  out->h = gh;
  out->x = (ow - gw) / 2;
  out->y = (oh - gh) / 2;
  out->wing = out->x;
  out->bar = room != 0 ? out->y : 0;
}

/* ---------------------------------------------------------------- playing */

static bool copy_if_changed(const char *from, const char *to) {
  long a = file_size(from), b = file_size(to);
  if (a >= 0 && a == b)
    return true;
  FILE *in = fopen(from, "rb");
  if (!in)
    return false;
  char tmp[1500];
  snprintf(tmp, sizeof tmp, "%s.tmp", to);
  FILE *out = fopen(tmp, "wb");
  bool ok = out != NULL;
  static char buf[1 << 16];
  size_t n;
  while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0)
    ok = fwrite(buf, 1, n, out) == n;
  fclose(in);
  if (out && fclose(out))
    ok = false;
  ok = ok && pocket_replace(tmp, to);
  if (!ok)
    pocket_remove(tmp);
  return ok;
}

int pc_large_address(const char *path) {
  FILE *f = fopen(path, "r+b");
  if (!f)
    return -1;
  unsigned char h[512];
  int r = 0;
  size_t n = fread(h, 1, sizeof h, f);
  long pe = n >= 0x40 && h[0] == 'M' && h[1] == 'Z'
                ? (long)(h[0x3C] | h[0x3D] << 8 | h[0x3E] << 16 | (long)h[0x3F] << 24)
                : -1;
  static const unsigned char sig[4] = {'P', 'E', 0, 0};
  if (pe < 0 || pe + 24 > (long)n || memcmp(h + pe, sig, 4))
    r = -1;
  else if ((h[pe + 4] | h[pe + 5] << 8) == 0x14C && !(h[pe + 22] & 0x20)) { // i386, not yet LARGE_ADDRESS_AWARE
    unsigned char c = h[pe + 22] | 0x20;
    r = !fseek(f, pe + 22, SEEK_SET) && fwrite(&c, 1, 1, f) == 1 ? 1 : -1;
  }
  if (fclose(f))
    r = -1;
  return r;
}

static bool write_text(const char *path, const char *text, size_t n) {
  char tmp[1500];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return false;
  bool ok = fwrite(text, 1, n, f) == n;
  ok = fclose(f) == 0 && ok;
  ok = ok && pocket_replace(tmp, path);
  if (!ok)
    pocket_remove(tmp);
  return ok;
}

static char *read_text(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  char *s = n >= 0 && n < (1 << 20) ? malloc((size_t)n + 1) : NULL;
  if (s) {
    size_t got = fread(s, 1, (size_t)n, f);
    s[got] = 0;
  }
  fclose(f);
  return s;
}

struct dll_copy {
  const char *from, *to;
  bool ok;
};
static void dll_each(const char *name, void *user) {
  struct dll_copy *d = user;
  if (!ends_with(name, ".dll"))
    return;
  char a[1400], b[1400];
  join(a, sizeof a, d->from, name);
  join(b, sizeof b, d->to, name);
  d->ok &= copy_if_changed(a, b);
}

/* Some games' players stop at once when their folder's path has a letter outside ASCII in it ("Pokémon Itinerant"):
 * started from its own folder, the game never reads that path, but PokeDaisy has to give it one. Such a folder is
 * given as a junction with a plain name, DATA/pc/links/<name>, that leads to it (made with Windows' own mklink, which
 * needs no administrator). The game's files stay where they are; nothing is copied or moved. OUT is the path to
 * give the player: DIR itself when it's plain already or no junction could be made. Never delete a links folder
 * with its contents: they are the game's own files. */
static void plain_path(const char *pcRoot, const char *dir, char *out, size_t size) {
  snprintf(out, size, "%s", dir);
  bool plain = true;
  for (const unsigned char *p = (const unsigned char *)dir; *p; p++)
    plain &= *p < 0x80;
  if (plain)
    return;
#ifdef _WIN32
  char links[1200], name[96], link[1400];
  join(links, sizeof links, pcRoot, "links");
  if (!pocket_mkdir(links))
    return;
  // A plain name from the folder's own: its letters and digits, and a check number of the whole path.
  const char *leafName = dir + strlen(dir);
  while (leafName > dir && leafName[-1] != '/' && leafName[-1] != '\\')
    leafName--;
  size_t n = 0;
  unsigned long h = 5381;
  for (const unsigned char *p = (const unsigned char *)dir; *p; p++)
    h = h * 33 + *p;
  for (const unsigned char *p = (const unsigned char *)leafName; *p && n < 60; p++)
    if (isalnum(*p) && *p < 0x80)
      name[n++] = (char)*p;
  snprintf(name + n, sizeof name - n, "-%08lx", h & 0xffffffffUL);
  join(link, sizeof link, links, name);
  if (strlen(link) >= size)
    return;
  if (pocket_is_dir(link)) {
    snprintf(out, size, "%s", link);
    return;
  }
  char cmd[3200];
  int at = snprintf(cmd, sizeof cmd, "cmd.exe /d /c mklink /J \"%s\" \"%s\"", link, dir);
  for (char *c = cmd + 22; at > 0 && c < cmd + at; c++) // the two paths, Windows' way
    if (*c == '/')
      *c = '\\';
  int wn = MultiByteToWideChar(CP_UTF8, 0, cmd, -1, NULL, 0);
  wchar_t *w = wn > 0 ? malloc((size_t)wn * sizeof *w) : NULL;
  if (!w)
    return;
  MultiByteToWideChar(CP_UTF8, 0, cmd, -1, w, wn);
  STARTUPINFOW si = {.cb = sizeof si, .dwFlags = STARTF_USESHOWWINDOW, .wShowWindow = SW_HIDE};
  PROCESS_INFORMATION pi;
  if (CreateProcessW(NULL, w, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  else if (getenv("POCKET_PC_DEBUG"))
    fprintf(stderr, "PC game: mklink didn't start (%lu)\n", (unsigned long)GetLastError());
  free(w);
  if (getenv("POCKET_PC_DEBUG"))
    fprintf(stderr, "PC game: %s: %s\n", cmd, pocket_is_dir(link) ? "made" : "not made");
  if (pocket_is_dir(link))
    snprintf(out, size, "%s", link);
#endif
}

/* The words in TEXT, lowercased, one of NAMES among them as a whole word (Añil in UTF-8 or Latin-1 too). */
static bool has_word(const char *text, const char *const *names) {
  char low[300];
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char *)text; *p && n + 1 < sizeof low; p++)
    low[n++] = (char)(*p < 128 ? tolower(*p) : *p);
  low[n] = 0;
  for (const char *const *w = names; *w; w++) {
    size_t len = strlen(*w);
    for (const char *at = strstr(low, *w); at; at = strstr(at + 1, *w)) {
      bool before = at == low || !isalnum((unsigned char)at[-1]);
      bool after = !isalnum((unsigned char)at[len]);
      if (before && after)
        return true;
    }
  }
  return false;
}

bool pc_lean_game(const struct pc_game_info *info) {
  static const char *const names[] = {"indigo", "a\xc3\xb1il", "a\xc3\x91il", "a\xf1il", "a\xd1il", "anil",
                                      "reminiscencia", "reborn", "rejuvenation", NULL};
  const char *a = strrchr(info->dir, '/'), *b = strrchr(info->dir, '\\');
  const char *folder = a > b ? a + 1 : b ? b + 1 : info->dir;
  return has_word(info->title, names) || has_word(folder, names);
}

const char *pc_input_path(const struct pc_game_info *info) {
  const char *want = getenv("POCKET_PC_INPUT");
  if (want && (!strcmp(want, "feed") || !strcmp(want, "source") || !strcmp(want, "native")))
    return want;
  return pc_lean_game(info) ? "source" : "feed";
}

void pc_companion_log(const char *dir, FILE *to) {
  char p[1400];
  join(p, sizeof p, dir, "companion.log");
  FILE *f = fopen(p, "rb");
  if (!f)
    return;
  char line[600];
  int n = 0;
  while (fgets(line, sizeof line, f) && n++ < 500)
    fprintf(to, "PC companion: %s%s", line, strchr(line, '\n') ? "" : "\n");
  fclose(f);
}

bool pc_prepare(const struct pc_game_info *info, const struct pc_launch_opts *o, char *dir, size_t dirSize, char *err,
                size_t errSize) {
  char pcRoot[1100];
  snprintf(pcRoot, sizeof pcRoot, "%s/pc", o->root);
  join(dir, dirSize, pcRoot, o->stem);
  if (!pocket_mkdir(pcRoot) || !pocket_mkdir(dir)) {
    snprintf(err, errSize, "Couldn't make %s", dir);
    return false;
  }
  char oldLog[1400];
  join(oldLog, sizeof oldLog, dir, "companion.log"); // the last play's (a restart within this one keeps adding)
  remove(oldLog);
  if (info->player != PC_PLAYER_MKXPZ)
    return true; // the original player runs from the game's own folder
  char from[1400], to[1400];
  const char *playerDir = info->playerDir[0] ? info->playerDir : info->dir;
  join(from, sizeof from, playerDir, info->exe);
  join(to, sizeof to, dir, info->exe);
  if (!copy_if_changed(from, to)) {
    snprintf(err, errSize, "Couldn't copy the game's player");
    return false;
  }
  if (pc_large_address(to) > 0)
    fprintf(stderr, "PC game: the 32-bit player may use 4 GB\n");
  struct dll_copy d = {playerDir, dir, true};
  pocket_list_dir(playerDir, dll_each, &d);
  if (!d.ok) {
    snprintf(err, errSize, "Couldn't copy the game's DLLs");
    return false;
  }
  char rb[1400];
  join(rb, sizeof rb, pcRoot, "pokedaisy_companion.rb");
  if (!write_text(rb, o->companionRb, o->companionLen)) {
    snprintf(err, errSize, "Couldn't write the companion");
    return false;
  }
  // A borrowed player: none of the other game's settings, the XP player's version, and the DLLs' stand-ins.
  char standIns[1400] = "";
  if (info->playerDir[0]) {
    join(standIns, sizeof standIns, pcRoot, "pokedaisy_rgss_standins.rb");
    if (!o->standInsRb || !write_text(standIns, o->standInsRb, o->standInsLen)) {
      snprintf(err, errSize, "Couldn't write the stand-ins");
      return false;
    }
  }
  join(from, sizeof from, info->dir, "mkxp.json");
  char *theirs = info->playerDir[0] ? NULL : read_text(from);
  char gameFolder[1300];
  plain_path(pcRoot, info->dir, gameFolder, sizeof gameFolder);
  struct pc_mkxp_overrides ov = {gameFolder, rb, getenv("POCKET_PC_PRELOAD"), o->windowW, o->windowH, true,
                                 standIns[0] ? standIns : NULL, info->playerDir[0] ? 1 : 0};
  static char json[1 << 16];
  int n = pc_mkxp_json(theirs, &ov, json, sizeof json);
  free(theirs);
  join(to, sizeof to, dir, "mkxp.json");
  if (n < 0 || !write_text(to, json, (size_t)n)) {
    snprintf(err, errSize, "Couldn't write mkxp.json");
    return false;
  }
  return true;
}

bool pc_restart_asked(const char *dir) {
  char p[1400];
  join(p, sizeof p, dir, "restart.txt");
  struct stat sb;
  if (stat(p, &sb))
    return false;
  remove(p);
  // Only as the game closes to start again: a game that went on playing after it asked (the note left behind) is
  // not started again when the player closes it later.
  double age = difftime(time(NULL), sb.st_mtime);
  return age >= -5 && age <= 60;
}

#ifdef _WIN32
static PROCESS_INFORMATION proc;
static HWND gameWindow;
static bool started;

static wchar_t *wide(const char *s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  wchar_t *w = n > 0 ? malloc((size_t)n * sizeof *w) : NULL;
  if (w)
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
  return w;
}

static void slashes(wchar_t *w) {
  for (; *w; w++)
    if (*w == L'/')
      *w = L'\\';
}

bool pc_start(const struct pc_game_info *info, const char *dir, const char *jsonOut, const char *cmdFile) {
  if (started && pc_running())
    return false;
  if (started) {
    CloseHandle(proc.hProcess);
    CloseHandle(proc.hThread);
    started = false;
  }
  gameWindow = NULL;
  const char *from = info->player == PC_PLAYER_MKXPZ ? dir : info->dir;
  char exe[1400];
  join(exe, sizeof exe, from, info->exe);
  SetEnvironmentVariableA("POKEDAISY_PC_OUT", jsonOut);
  SetEnvironmentVariableA("POKEDAISY_PC_CMD", cmdFile);
  // PAD FEED: the buttons PokeDaisy reads, for the companion (pc_pad_feed); none held as the game starts.
  char padFile[1400];
  join(padFile, sizeof padFile, dir, "pad.txt");
  pc_pad_feed(padFile, 0);
  SetEnvironmentVariableA("POKEDAISY_PC_PAD", padFile);
  // RESTART: the companion notes there a game starting its own exe again (pc_restart_asked), none yet.
  char restartFile[1400];
  join(restartFile, sizeof restartFile, dir, "restart.txt");
  remove(restartFile);
  SetEnvironmentVariableA("POKEDAISY_PC_RESTART", restartFile);
  SetEnvironmentVariableA("POKEDAISY_PC_EXE", info->exe);
  // The companion's log (a step that failed or held the game over 20 ms, the frame stats), copied into pokedaisy.log
  // after the play (pc_companion_log). A check's own POKEDAISY_PC_LOG is kept.
  if (!getenv("POKEDAISY_PC_LOG") || strstr(getenv("POKEDAISY_PC_LOG"), "companion.log")) {
    char logFile[1400];
    join(logFile, sizeof logFile, dir, "companion.log");
    SetEnvironmentVariableA("POKEDAISY_PC_LOG", logFile);
  }
  // INPUT PATH and LEAN (pc_input_path, pc_lean_game) for the companion.
  SetEnvironmentVariableA("POKEDAISY_PC_INPUT", pc_input_path(info));
  SetEnvironmentVariableA("POKEDAISY_PC_LEAN", pc_lean_game(info) ? "1" : "0");
  wchar_t *wexe = wide(exe), *wdir = wide(from);
  if (!wexe || !wdir) {
    free(wexe);
    free(wdir);
    return false;
  }
  slashes(wexe);
  slashes(wdir);
  size_t len = wcslen(wexe) + 3;
  wchar_t *cmd = malloc(len * sizeof *cmd);
  if (cmd)
    swprintf(cmd, len, L"\"%ls\"", wexe);
  STARTUPINFOW si = {.cb = sizeof si};
  memset(&proc, 0, sizeof proc);
  BOOL ok = cmd && CreateProcessW(wexe, cmd, NULL, NULL, FALSE, 0, NULL, wdir, &si, &proc);
  free(cmd);
  free(wexe);
  free(wdir);
  if (!ok)
    return false;
  AllowSetForegroundWindow(proc.dwProcessId);
  started = true;
  return true;
}

bool pc_running(void) {
  return started && WaitForSingleObject(proc.hProcess, 0) == WAIT_TIMEOUT;
}

int pc_exit_code(void) {
  DWORD code = 0;
  if (started)
    GetExitCodeProcess(proc.hProcess, &code);
  return (int)code;
}

struct find {
  DWORD pid;
  HWND best;
  long area;
};
static BOOL CALLBACK find_each(HWND h, LPARAM user) {
  struct find *f = (struct find *)user;
  DWORD pid = 0;
  GetWindowThreadProcessId(h, &pid);
  if (pid != f->pid || !IsWindowVisible(h))
    return TRUE;
  RECT r;
  GetWindowRect(h, &r);
  long area = (long)(r.right - r.left) * (r.bottom - r.top);
  // The game's window is its biggest (a console or a splash may come and go).
  if (area > f->area) {
    f->area = area;
    f->best = h;
  }
  return TRUE;
}

bool pc_place(void *owner, int x, int y, int w, int h, bool focus) {
  if (!pc_running())
    return false;
  if (!gameWindow || !IsWindow(gameWindow)) {
    struct find f = {proc.dwProcessId, NULL, 0};
    EnumWindows(find_each, (LPARAM)&f);
    gameWindow = f.best;
    if (!gameWindow)
      return false;
  }
  // No frame. Not WS_POPUP: its high bit makes the style a big number to a 32-bit game's own window script, which
  // then stops with an error (Phoenix Rising).
  LONG_PTR style = GetWindowLongPtrW(gameWindow, GWL_STYLE),
           want = style & ~(LONG_PTR)(WS_OVERLAPPEDWINDOW | WS_POPUP | WS_MAXIMIZE);
  LONG_PTR ex = GetWindowLongPtrW(gameWindow, GWL_EXSTYLE),
           wantEx = ex & ~(LONG_PTR)(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
  bool restyle = style != want || ex != wantEx;
  if (restyle) {
    SetWindowLongPtrW(gameWindow, GWL_STYLE, want);
    SetWindowLongPtrW(gameWindow, GWL_EXSTYLE, wantEx);
  }
  if (owner && (HWND)GetWindowLongPtrW(gameWindow, GWLP_HWNDPARENT) != (HWND)owner)
    SetWindowLongPtrW(gameWindow, GWLP_HWNDPARENT, (LONG_PTR)owner);
  RECT r;
  GetWindowRect(gameWindow, &r);
  if (restyle || r.left != x || r.top != y || r.right - r.left != w || r.bottom - r.top != h) {
    if (getenv("POCKET_PC_DEBUG"))
      fprintf(stderr, "PC game: window was %ld,%ld %ldx%ld, put back to %d,%d %dx%d\n", r.left, r.top,
              r.right - r.left, r.bottom - r.top, x, y, w, h);
    SetWindowPos(gameWindow, HWND_TOP, x, y, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }
  if (focus) {
    SetForegroundWindow(gameWindow);
    SetFocus(gameWindow);
  }
  return true;
}

void pc_pad_env(const char *mappings, const char *only) {
  SetEnvironmentVariableA("SDL_GAMECONTROLLER_USE_BUTTON_LABELS", "0");
  SetEnvironmentVariableA("SDL_GAMECONTROLLERCONFIG", mappings && *mappings ? mappings : NULL);
  // PC_PAD_HIDE: only a controller nobody has (vendor and product FFFF) may be read, so SDL lists no joystick at all.
  // A real one ("0x045E/0x028E", INPUT PATH native): that pad alone, so it is joystick 0, the one mkxp-z opens, and
  // read while PokeDaisy's window has the foreground too.
  SetEnvironmentVariableA("SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT", only);
  bool real = only && strcmp(only, PC_PAD_HIDE);
  SetEnvironmentVariableA("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", real ? "1" : NULL);
}

bool pc_self_foreground(void *self) { return self && GetForegroundWindow() == (HWND)self; }


static bool keysDown[256];
void pc_keys_send(const bool *down) {
  HWND fg = GetForegroundWindow();
  DWORD pid = 0;
  if (fg)
    GetWindowThreadProcessId(fg, &pid);
  bool ours = started && pid && pid == proc.dwProcessId;
  INPUT in[32];
  int n = 0;
  for (int vk = 1; vk < 256; vk++) {
    bool want = ours && down && down[vk];
    if (want == keysDown[vk])
      continue;
    UINT scan = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
    keysDown[vk] = want;
    if (getenv("POCKET_PC_DEBUG"))
      fprintf(stderr, "PC game: key %02X %s%s\n", vk, want ? "down" : "up", ours ? "" : " (the game isn't in front)");
    if (!scan)
      continue;
    // The keys with an E0 prefix (the arrows, PAGE UP and the like): without it they'd be the number pad's.
    bool ext = (vk >= VK_PRIOR && vk <= VK_DOWN) || vk == VK_INSERT || vk == VK_DELETE || vk == VK_RCONTROL ||
               vk == VK_RMENU || vk == VK_DIVIDE;
    memset(&in[n], 0, sizeof in[n]);
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wScan = (WORD)scan;
    in[n].ki.dwFlags = KEYEVENTF_SCANCODE | (ext ? KEYEVENTF_EXTENDEDKEY : 0) | (want ? 0 : KEYEVENTF_KEYUP);
    if (++n == (int)(sizeof in / sizeof in[0])) {
      SendInput((UINT)n, in, sizeof in[0]);
      n = 0;
    }
  }
  if (n)
    SendInput((UINT)n, in, sizeof in[0]);
}

static bool find_window(void) {
  if (gameWindow && IsWindow(gameWindow))
    return true;
  struct find f = {proc.dwProcessId, NULL, 0};
  EnumWindows(find_each, (LPARAM)&f);
  gameWindow = f.best;
  return gameWindow != NULL;
}

bool pc_capture_place(void *owner, bool focus) {
  if (!pc_running() || !find_window())
    return false;
  // Left at its own size and place, frame and all (its own scripts size it): only made see-through, so the picture
  // PokeDaisy draws from it shows, while it keeps the keyboard and the mouse.
  LONG_PTR ex = GetWindowLongPtrW(gameWindow, GWL_EXSTYLE);
  if (!(ex & WS_EX_LAYERED)) {
    SetWindowLongPtrW(gameWindow, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    SetLayeredWindowAttributes(gameWindow, 0, 1, LWA_ALPHA);
  }
  if (owner && (HWND)GetWindowLongPtrW(gameWindow, GWLP_HWNDPARENT) != (HWND)owner)
    SetWindowLongPtrW(gameWindow, GWLP_HWNDPARENT, (LONG_PTR)owner);
  if (focus) {
    SetForegroundWindow(gameWindow);
    SetFocus(gameWindow);
  }
  return true;
}

/* PokeDaisy is per-monitor DPI aware and an old player isn't: on a screen scaled past 100% Windows stretches the
 * game's window, and its size read from here (scaled) would not match the picture PrintWindow gives (unscaled), so
 * the picture came out cut off. Both are read in the window's own DPI terms instead: its real picture, whole. */
typedef void *(WINAPI *dpi_get_fn)(HWND);
typedef void *(WINAPI *dpi_set_fn)(void *);
static void *dpi_enter(HWND h) {
  static dpi_get_fn get;
  static dpi_set_fn set;
  static bool looked;
  if (!looked) {
    HMODULE u = GetModuleHandleA("user32.dll");
    get = u ? (dpi_get_fn)(void (*)(void))GetProcAddress(u, "GetWindowDpiAwarenessContext") : NULL;
    set = u ? (dpi_set_fn)(void (*)(void))GetProcAddress(u, "SetThreadDpiAwarenessContext") : NULL;
    looked = true;
  }
  return get && set ? set(get(h)) : NULL;
}
static void dpi_leave(void *was) {
  HMODULE u = GetModuleHandleA("user32.dll");
  dpi_set_fn set = u ? (dpi_set_fn)(void (*)(void))GetProcAddress(u, "SetThreadDpiAwarenessContext") : NULL;
  if (was && set)
    set(was);
}

static HDC capDc;
static HBITMAP capBmp;
static uint32_t *capPx;
static int capW, capH;
const uint32_t *pc_capture(int *w, int *h) {
  if (!pc_running() || !find_window())
    return NULL;
  // The client area only (no frame, no title bar), as the game sees it, every frame: it follows the game's own
  // window size changes (its options, its scripts).
  void *was = dpi_enter(gameWindow);
  RECT r;
  if (!GetClientRect(gameWindow, &r) || r.right < 16 || r.bottom < 16 || r.right > 8192 || r.bottom > 8192) {
    dpi_leave(was);
    return NULL;
  }
  if (r.right != capW || r.bottom != capH || !capDc) {
    if (capBmp)
      DeleteObject(capBmp);
    if (!capDc)
      capDc = CreateCompatibleDC(NULL);
    BITMAPINFO bi = {.bmiHeader = {sizeof(BITMAPINFOHEADER), r.right, -r.bottom, 1, 32, BI_RGB, 0, 0, 0, 0, 0}};
    void *bits = NULL;
    capBmp = CreateDIBSection(capDc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!capBmp) {
      capW = capH = 0;
      dpi_leave(was);
      return NULL;
    }
    SelectObject(capDc, capBmp);
    capPx = bits;
    capW = r.right;
    capH = r.bottom;
  }
  // The window's own picture (as the desktop composes it), whatever is over it: PW_CLIENTONLY | PW_RENDERFULLCONTENT.
  BOOL printed = PrintWindow(gameWindow, capDc, 1 | 2);
  dpi_leave(was);
  if (!printed)
    return NULL;
  GdiFlush();
  *w = capW;
  *h = capH;
  return capPx;
}

void pc_capture_free(void) {
  if (capBmp)
    DeleteObject(capBmp);
  if (capDc)
    DeleteDC(capDc);
  capBmp = NULL;
  capDc = NULL;
  capPx = NULL;
  capW = capH = 0;
}

static BOOL CALLBACK box_each(HWND h, LPARAM user) {
  DWORD pid = 0;
  GetWindowThreadProcessId(h, &pid);
  char cls[32];
  if (pid == proc.dwProcessId && IsWindowVisible(h) && GetClassNameA(h, cls, sizeof cls) && !strcmp(cls, "#32770")) {
    *(bool *)user = true;
    return FALSE;
  }
  return TRUE;
}

bool pc_error_box(void) {
  if (!pc_running())
    return false;
  bool found = false;
  EnumWindows(box_each, (LPARAM)&found);
  return found;
}

bool pc_client_rect(int *x, int *y, int *w, int *h);
/* One window's own picture (client area, as the desktop composes it) into OUT (stride ow) at OX, OY: what the window
 * shows whatever is in front of it on the desktop. */
static void print_into(HWND h, uint32_t *out, int ow, int oh, int ox, int oy) {
  RECT r;
  if (!h || !GetClientRect(h, &r) || r.right < 1 || r.bottom < 1)
    return;
  HDC dc = CreateCompatibleDC(NULL);
  BITMAPINFO bi = {.bmiHeader = {sizeof(BITMAPINFOHEADER), r.right, -r.bottom, 1, 32, BI_RGB, 0, 0, 0, 0, 0}};
  void *bits = NULL;
  HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (bmp) {
    HGDIOBJ old = SelectObject(dc, bmp);
    if (PrintWindow(h, dc, 1 | 2)) {
      GdiFlush();
      for (int y = 0; y < r.bottom; y++)
        for (int x = 0; x < r.right; x++)
          if (ox + x >= 0 && ox + x < ow && oy + y >= 0 && oy + y < oh)
            out[(size_t)(oy + y) * ow + ox + x] = ((uint32_t *)bits)[(size_t)y * r.right + x] & 0xFFFFFF;
    }
    SelectObject(dc, old);
    DeleteObject(bmp);
  }
  DeleteDC(dc);
}

bool pc_compose(void *self, int ow, int oh, uint32_t *out) {
  if (!self)
    return false;
  memset(out, 0, (size_t)ow * oh * 4);
  print_into((HWND)self, out, ow, oh, 0, 0);
  int x, y, w, h;
  if (find_window() && pc_client_rect(&x, &y, &w, &h)) {
    POINT o = {0, 0};
    ClientToScreen((HWND)self, &o);
    LONG ex = (LONG)GetWindowLongPtrW(gameWindow, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) // a see-through window (drawn by PokeDaisy itself) isn't on the screen
      print_into(gameWindow, out, ow, oh, x - o.x, y - o.y);
  }
  return true;
}

bool pc_client_rect(int *x, int *y, int *w, int *h) {
  if (!pc_running() || !find_window())
    return false;
  RECT r;
  POINT o = {0, 0};
  if (!GetClientRect(gameWindow, &r) || !ClientToScreen(gameWindow, &o))
    return false;
  *x = o.x, *y = o.y, *w = r.right, *h = r.bottom;
  return true;
}

void pc_show(bool shown) {
  if (gameWindow && IsWindow(gameWindow))
    ShowWindow(gameWindow, shown ? SW_SHOWNA : SW_HIDE);
}

void pc_close(bool force) {
  if (!pc_running())
    return;
  if (force)
    TerminateProcess(proc.hProcess, 1);
  else if (gameWindow && IsWindow(gameWindow))
    PostMessageW(gameWindow, WM_CLOSE, 0, 0);
}
#else
bool pc_start(const struct pc_game_info *info, const char *dir, const char *jsonOut, const char *cmdFile) {
  (void)info, (void)dir, (void)jsonOut, (void)cmdFile;
  return false;
}
bool pc_running(void) { return false; }
int pc_exit_code(void) { return 0; }
bool pc_place(void *owner, int x, int y, int w, int h, bool focus) {
  (void)owner, (void)x, (void)y, (void)w, (void)h, (void)focus;
  return false;
}
bool pc_capture_place(void *owner, bool focus) {
  (void)owner, (void)focus;
  return false;
}
void pc_pad_env(const char *mappings, const char *only) { (void)mappings, (void)only; }
void pc_keys_send(const bool *down) { (void)down; }
bool pc_self_foreground(void *self) {
  (void)self;
  return false;
}
const uint32_t *pc_capture(int *w, int *h) {
  (void)w, (void)h;
  return NULL;
}
void pc_capture_free(void) {}
bool pc_error_box(void) { return false; }
bool pc_client_rect(int *x, int *y, int *w, int *h) {
  (void)x, (void)y, (void)w, (void)h;
  return false;
}
bool pc_compose(void *self, int ow, int oh, uint32_t *out) {
  (void)self, (void)ow, (void)oh, (void)out;
  return false;
}
void pc_show(bool shown) { (void)shown; }
void pc_close(bool force) { (void)force; }
#endif
