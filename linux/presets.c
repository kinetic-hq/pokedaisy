/* EDIT SAVE's PRESETS (presets.h). GPL-3.0, see ../NOTICE. */
#include "presets.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pocket_os.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

/* ---- names ---------------------------------------------------------------------------------------------------- */

/* A name to compare: letters and digits upper-cased, the rest dropped; the gender signs as F / M, é as E. */
static void norm(const char *s, char *out, size_t n) {
  size_t k = 0;
  for (const unsigned char *p = (const unsigned char *)s; p && *p && k + 1 < n; p++) {
    if (p[0] == 0xE2 && p[1] == 0x99 && (p[2] == 0x80 || p[2] == 0x82)) { /* ♀ ♂ */
      out[k++] = p[2] == 0x80 ? 'F' : 'M';
      p += 2;
    } else if (p[0] == 0xC3 && (p[1] == 0xA9 || p[1] == 0x89)) { /* é É */
      out[k++] = 'E';
      p++;
    } else if (isalnum(*p))
      out[k++] = (char)toupper(*p);
  }
  out[k] = 0;
}

static void copy(char *dst, size_t n, const char *s, size_t len) {
  while (len && isspace((unsigned char)*s))
    s++, len--;
  while (len && isspace((unsigned char)s[len - 1]))
    len--;
  if (len >= n)
    len = n - 1;
  memcpy(dst, s, len);
  dst[len] = 0;
}
static void copyz(char *dst, size_t n, const char *s) { copy(dst, n, s ? s : "", s ? strlen(s) : 0); }

/* hp atk def spa spd spe (and Showdown's HP Atk Def SpA SpD Spe) to se_mon's order; -1 for anything else. */
static int stat_index(const char *key, size_t len) {
  char k[8] = "";
  size_t j = 0;
  for (size_t i = 0; i < len && j < 7; i++)
    if (isalpha((unsigned char)key[i]))
      k[j++] = (char)tolower((unsigned char)key[i]);
  k[j] = 0;
  static const char *const names[6] = {"hp", "atk", "def", "spe", "spa", "spd"};
  for (int i = 0; i < 6; i++)
    if (!strcmp(k, names[i]))
      return i;
  return -1;
}

/* "252 HP / 4 Def" or "hp: 252; def: 252" into V (and SET). */
static void parse_stats(const char *s, size_t len, int *v, bool *set) {
  const char *end = s + len;
  while (s < end) {
    const char *part = s;
    while (s < end && *s != '/' && *s != ';' && *s != ',')
      s++;
    const char *key = NULL, *num = NULL;
    size_t keyLen = 0;
    for (const char *p = part; p < s; p++) {
      if (isdigit((unsigned char)*p) && !num)
        num = p;
      if (isalpha((unsigned char)*p) && !key) {
        key = p;
        while (p < s && isalpha((unsigned char)*p))
          p++, keyLen++;
        p--;
      }
    }
    int i = key ? stat_index(key, keyLen) : -1;
    if (i >= 0 && num) {
      v[i] = atoi(num);
      if (set)
        set[i] = true;
    }
    if (s < end)
      s++;
  }
}

static void preset_init(struct preset *p, int gen) {
  memset(p, 0, sizeof *p);
  p->gen = gen;
  for (int i = 0; i < 6; i++)
    p->ivs[i] = 31;
}

static void set_move(struct preset *p, int slot, const char *s, size_t len) {
  if (slot < 0 || slot > 3)
    return;
  /* "Surf / Hydro Pump": the first; "Hidden Power [Grass]" as "Hidden Power Grass". */
  for (size_t i = 0; i < len; i++)
    if (s[i] == '/') {
      len = i;
      break;
    }
  char m[32];
  copy(m, sizeof m, s, len);
  for (char *c = m; *c; c++)
    if (*c == '[' || *c == ']')
      *c = ' ';
  copyz(p->moves[slot], sizeof p->moves[slot], m);
}

static bool push(struct preset_list *out, const struct preset *p) {
  if (!p->species[0])
    return false;
  int moves = 0;
  for (int i = 0; i < 4; i++)
    moves += p->moves[i][0] != 0;
  if (!moves)
    return false;
  if (out->count == out->cap) {
    int cap = out->cap ? out->cap * 2 : 256;
    struct preset *v = realloc(out->v, sizeof *v * (size_t)cap);
    if (!v)
      return false;
    out->v = v, out->cap = cap;
  }
  out->v[out->count++] = *p;
  return true;
}

/* ---- Showdown text -------------------------------------------------------------------------------------------- */

static bool starts(const char *s, size_t len, const char *with) {
  size_t w = strlen(with);
  return len >= w && !strncasecmp(s, with, w);
}

static int gen_in(const char *s) {
  for (const char *p = s; *p; p++)
    if ((p[0] == 'g' || p[0] == 'G') && !strncasecmp(p, "gen", 3)) {
      const char *d = p + 3;
      while (*d == ' ')
        d++;
      if (isdigit((unsigned char)*d))
        return atoi(d);
    }
  return 0;
}

static int parse_text(const char *text, size_t len, int gen, struct preset_list *out) {
  struct preset p;
  bool open = false, named = false;
  int moveSlot = 0, added = 0;
  const char *s = text, *end = text + len;
  while (s <= end) {
    const char *line = s;
    while (s < end && *s != '\n')
      s++;
    size_t ll = (size_t)(s - line);
    s++;
    while (ll && (line[ll - 1] == '\r' || isspace((unsigned char)line[ll - 1])))
      ll--;
    while (ll && isspace((unsigned char)*line))
      line++, ll--;
    bool header = ll > 6 && !strncmp(line, "===", 3);
    if (!ll || header || s > end) {
      if (open && push(out, &p))
        added++;
      open = named = false;
      if (!header)
        continue;
    }
    if (header) { /* === [Gen 3 OU] Milotic [Standard] === */
      preset_init(&p, gen);
      open = true, moveSlot = 0;
      char h[200];
      copy(h, sizeof h, line + 3, ll - 3);
      char *a = strchr(h, '['), *b = a ? strchr(a, ']') : NULL;
      if (a && b) {
        *b = 0;
        copyz(p.format, sizeof p.format, a + 1);
        if (gen_in(p.format))
          p.gen = gen_in(p.format);
        char *sp = b + 1, *c = strchr(sp, '['), *d = c ? strchr(c, ']') : NULL;
        if (c && d) {
          *d = 0;
          copyz(p.name, sizeof p.name, c + 1);
          *c = 0;
        }
        copyz(p.species, sizeof p.species, sp);
        named = p.species[0] != 0;
      }
      continue;
    }
    if (!open) {
      preset_init(&p, gen);
      open = true, moveSlot = 0;
    }
    if (line[0] == '-' && ll > 1) {
      set_move(&p, moveSlot++, line + 1, ll - 1);
    } else if (starts(line, ll, "Ability:")) {
      copy(p.ability, sizeof p.ability, line + 8, ll - 8);
    } else if (starts(line, ll, "Item:")) {
      copy(p.item, sizeof p.item, line + 5, ll - 5);
    } else if (starts(line, ll, "Tera Type:")) {
      copy(p.tera, sizeof p.tera, line + 10, ll - 10);
    } else if (starts(line, ll, "EVs:")) {
      parse_stats(line + 4, ll - 4, p.evs, NULL);
    } else if (starts(line, ll, "IVs:")) {
      parse_stats(line + 4, ll - 4, p.ivs, p.ivSet);
    } else if (ll > 7 && !strncasecmp(line + ll - 7, " Nature", 7)) {
      copy(p.nature, sizeof p.nature, line, ll - 7);
    } else if (starts(line, ll, "Level:") || starts(line, ll, "Shiny:") || starts(line, ll, "Happiness:") ||
               starts(line, ll, "Gigantamax:") || starts(line, ll, "Dynamax Level:") || starts(line, ll, "Pokeball:")) {
      /* not part of a preset */
    } else if (!named) { /* "Nickname (Species) (M) @ Item" */
      char first[160];
      copy(first, sizeof first, line, ll);
      char *at = strstr(first, " @ ");
      if (at) {
        copyz(p.item, sizeof p.item, at + 3);
        *at = 0;
      }
      size_t fl = strlen(first);
      while (fl >= 4 && first[fl - 1] == ')' && first[fl - 3] == '(' && (first[fl - 2] == 'M' || first[fl - 2] == 'F'))
        first[fl -= 3] = 0, fl = strlen(first), first[fl] = 0;
      fl = strlen(first);
      while (fl && first[fl - 1] == ' ')
        first[--fl] = 0;
      char *open2 = fl && first[fl - 1] == ')' ? strrchr(first, '(') : NULL;
      if (open2) {
        first[fl - 1] = 0;
        copyz(p.species, sizeof p.species, open2 + 1);
      } else
        copyz(p.species, sizeof p.species, first);
      named = true;
    }
  }
  return added;
}

/* ---- CSV ------------------------------------------------------------------------------------------------------ */

/* The next field of a CSV row into OUT; false at the row's end. */
static bool csv_field(const char **s, const char *end, char *out, size_t n, bool *rowEnd) {
  size_t k = 0;
  const char *p = *s;
  if (p >= end) {
    *rowEnd = true;
    out[0] = 0;
    return false;
  }
  if (*p == '"') {
    p++;
    while (p < end) {
      if (*p == '"' && p + 1 < end && p[1] == '"')
        p++;
      else if (*p == '"') {
        p++;
        break;
      }
      if (k + 1 < n)
        out[k++] = *p;
      p++;
    }
  }
  while (p < end && *p != ',' && *p != '\n') {
    if (*p != '\r' && k + 1 < n)
      out[k++] = *p;
    p++;
  }
  out[k] = 0;
  *rowEnd = p >= end || *p == '\n';
  *s = p < end ? p + 1 : p;
  return true;
}

enum { C_GEN, C_FORMAT, C_SPECIES, C_NAME, C_ITEM, C_ABILITY, C_TERA, C_NATURE, C_EVS, C_IVS, C_MOVES, C_COUNT };
static const char *const CSV_KEYS[C_COUNT] = {"generation", "format", "pokemon", "set_name", "item", "ability",
                                              "tera_type",  "nature", "evs",     "ivs",      "moves"};

static void fill_moves(struct preset *p, const char *moves) {
  int slot = 0;
  const char *s = moves;
  while (*s && slot < 4) {
    const char *e = s;
    while (*e && *e != ';')
      e++;
    if (e > s)
      set_move(p, slot++, s, (size_t)(e - s));
    s = *e ? e + 1 : e;
  }
}

static int parse_csv(const char *text, size_t len, int gen, struct preset_list *out) {
  const char *s = text, *end = text + len;
  int col[32], cols = 0, added = 0;
  char field[512];
  bool rowEnd = false;
  while (!rowEnd && cols < 32 && csv_field(&s, end, field, sizeof field, &rowEnd)) {
    col[cols] = -1;
    for (int k = 0; k < C_COUNT; k++)
      if (!strcasecmp(field, CSV_KEYS[k]))
        col[cols] = k;
    cols++;
  }
  while (s < end) {
    struct preset p;
    preset_init(&p, gen);
    rowEnd = false;
    for (int c = 0; !rowEnd; c++) {
      if (!csv_field(&s, end, field, sizeof field, &rowEnd))
        break;
      switch (c < cols ? col[c] : -1) {
      case C_GEN: p.gen = atoi(field) ? atoi(field) : gen; break;
      case C_FORMAT: copyz(p.format, sizeof p.format, field); break;
      case C_SPECIES: copyz(p.species, sizeof p.species, field); break;
      case C_NAME: copyz(p.name, sizeof p.name, field); break;
      case C_ITEM: copyz(p.item, sizeof p.item, field); break;
      case C_ABILITY: copyz(p.ability, sizeof p.ability, field); break;
      case C_TERA: copyz(p.tera, sizeof p.tera, field); break;
      case C_NATURE: copyz(p.nature, sizeof p.nature, field); break;
      case C_EVS: parse_stats(field, strlen(field), p.evs, NULL); break;
      case C_IVS: parse_stats(field, strlen(field), p.ivs, p.ivSet); break;
      case C_MOVES: fill_moves(&p, field); break;
      default: break;
      }
    }
    if (!p.gen && gen_in(p.format))
      p.gen = gen_in(p.format);
    if (push(out, &p))
      added++;
  }
  return added;
}

/* ---- JSON ----------------------------------------------------------------------------------------------------- */

enum { J_NULL, J_NUM, J_STR, J_ARR, J_OBJ, J_OTHER };
struct jv {
  int type;
  double num;
  char *str;  /* J_STR */
  int n;      /* J_ARR / J_OBJ */
  struct jv *v;
  char **keys; /* J_OBJ */
};
struct jp {
  const char *s, *end;
  int depth;
  bool bad;
};

static void jskip(struct jp *j) {
  while (j->s < j->end && isspace((unsigned char)*j->s))
    j->s++;
}

static void jfree(struct jv *v) {
  if (v->type == J_STR)
    free(v->str);
  if (v->type == J_ARR || v->type == J_OBJ) {
    for (int i = 0; i < v->n; i++) {
      jfree(&v->v[i]);
      if (v->keys)
        free(v->keys[i]);
    }
    free(v->v);
    free(v->keys);
  }
  v->type = J_NULL;
}

static char *jstring(struct jp *j) {
  /* at the opening quote */
  j->s++;
  size_t cap = 32, k = 0;
  char *out = malloc(cap);
  while (out && j->s < j->end && *j->s != '"') {
    unsigned c = (unsigned char)*j->s++;
    if (c == '\\' && j->s < j->end) {
      char e = *j->s++;
      c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == 'b' ? '\b' : e == 'f' ? '\f' : (unsigned char)e;
      if (e == 'u' && j->end - j->s >= 4) {
        char hex[5] = {j->s[0], j->s[1], j->s[2], j->s[3], 0};
        c = (unsigned)strtoul(hex, NULL, 16);
        j->s += 4;
      }
    }
    char buf[4];
    int bl = 0;
    if (c < 0x80)
      buf[bl++] = (char)c;
    else if (c < 0x800)
      buf[bl++] = (char)(0xC0 | c >> 6), buf[bl++] = (char)(0x80 | (c & 0x3F));
    else
      buf[bl++] = (char)(0xE0 | c >> 12), buf[bl++] = (char)(0x80 | (c >> 6 & 0x3F)), buf[bl++] = (char)(0x80 | (c & 0x3F));
    /* (a raw UTF-8 byte above 0x7F is copied as it is: c < 0x100 from the source, written back as one byte) */
    if ((unsigned char)j->s[-1] >= 0x80 && c >= 0x80 && c < 0x100)
      bl = 1, buf[0] = (char)c;
    if (k + (size_t)bl + 1 > cap) {
      cap *= 2;
      char *g = realloc(out, cap);
      if (!g) {
        free(out);
        return NULL;
      }
      out = g;
    }
    memcpy(out + k, buf, (size_t)bl);
    k += (size_t)bl;
  }
  if (j->s < j->end)
    j->s++;
  if (out)
    out[k] = 0;
  return out;
}

static bool jvalue(struct jp *j, struct jv *v) {
  memset(v, 0, sizeof *v);
  jskip(j);
  if (j->s >= j->end || ++j->depth > 64)
    return false;
  bool ok = true;
  char c = *j->s;
  if (c == '"') {
    v->type = J_STR;
    ok = (v->str = jstring(j)) != NULL;
  } else if (c == '[' || c == '{') {
    bool obj = c == '{';
    v->type = obj ? J_OBJ : J_ARR;
    j->s++;
    int cap = 0;
    jskip(j);
    if (j->s < j->end && *j->s == (obj ? '}' : ']'))
      j->s++;
    else
      while (ok) {
        if (v->n == cap) {
          cap = cap ? cap * 2 : 8;
          struct jv *nv = realloc(v->v, sizeof *nv * (size_t)cap);
          char **nk = obj ? realloc(v->keys, sizeof *nk * (size_t)cap) : NULL;
          if (!nv || (obj && !nk)) {
            if (nv)
              v->v = nv;
            if (nk)
              v->keys = nk;
            ok = false;
            break;
          }
          v->v = nv;
          if (obj)
            v->keys = nk;
        }
        if (obj) {
          jskip(j);
          if (j->s >= j->end || *j->s != '"') {
            ok = false;
            break;
          }
          v->keys[v->n] = jstring(j);
          jskip(j);
          if (j->s >= j->end || *j->s != ':') {
            free(v->keys[v->n]);
            ok = false;
            break;
          }
          j->s++;
        }
        if (!jvalue(j, &v->v[v->n])) {
          if (obj)
            free(v->keys[v->n]);
          ok = false;
          break;
        }
        v->n++;
        jskip(j);
        if (j->s < j->end && *j->s == ',') {
          j->s++;
          continue;
        }
        if (j->s < j->end && *j->s == (obj ? '}' : ']')) {
          j->s++;
          break;
        }
        ok = false;
      }
  } else if (c == '-' || isdigit((unsigned char)c)) {
    v->type = J_NUM;
    char *e;
    v->num = strtod(j->s, &e);
    j->s = e > j->s ? e : j->s + 1;
  } else {
    v->type = c == 'n' ? J_NULL : J_OTHER;
    while (j->s < j->end && isalpha((unsigned char)*j->s))
      j->s++;
  }
  j->depth--;
  if (!ok)
    jfree(v);
  return ok;
}

static const struct jv *jget(const struct jv *o, const char *key) {
  if (!o || o->type != J_OBJ)
    return NULL;
  for (int i = 0; i < o->n; i++)
    if (o->keys[i] && !strcmp(o->keys[i], key))
      return &o->v[i];
  return NULL;
}
/* A string, or an array's first (Smogon lists alternatives that way). */
static const char *jstr(const struct jv *v) {
  while (v && v->type == J_ARR && v->n)
    v = &v->v[0];
  return v && v->type == J_STR ? v->str : NULL;
}

static void jstats(const struct jv *v, int *out, bool *set) {
  if (!v)
    return;
  if (v->type == J_STR) {
    parse_stats(v->str, strlen(v->str), out, set);
    return;
  }
  for (int i = 0; v->type == J_OBJ && i < v->n; i++) {
    int k = v->keys[i] ? stat_index(v->keys[i], strlen(v->keys[i])) : -1;
    if (k >= 0 && v->v[i].type == J_NUM) {
      out[k] = (int)v->v[i].num;
      if (set)
        set[k] = true;
    }
  }
}

/* One set object: the master list's flat fields, or Smogon's {moves: [...], item, nature, evs: {...}}. */
static void jset(const struct jv *o, struct preset *p) {
  const struct jv *g = jget(o, "generation");
  if (g && g->type == J_NUM)
    p->gen = (int)g->num;
  const char *s;
  if ((s = jstr(jget(o, "format"))))
    copyz(p->format, sizeof p->format, s);
  if ((s = jstr(jget(o, "pokemon"))))
    copyz(p->species, sizeof p->species, s);
  if ((s = jstr(jget(o, "set_name"))))
    copyz(p->name, sizeof p->name, s);
  if ((s = jstr(jget(o, "item"))))
    copyz(p->item, sizeof p->item, s);
  if ((s = jstr(jget(o, "ability"))))
    copyz(p->ability, sizeof p->ability, s);
  if ((s = jstr(jget(o, "nature"))))
    copyz(p->nature, sizeof p->nature, s);
  if ((s = jstr(jget(o, "tera_type"))) || (s = jstr(jget(o, "teratypes"))))
    copyz(p->tera, sizeof p->tera, s);
  jstats(jget(o, "evs"), p->evs, NULL);
  jstats(jget(o, "ivs"), p->ivs, p->ivSet);
  const struct jv *m = jget(o, "moves");
  if (m && m->type == J_STR)
    fill_moves(p, m->str);
  for (int i = 0; m && m->type == J_ARR && i < m->n && i < 4; i++)
    if ((s = jstr(&m->v[i])))
      set_move(p, i, s, strlen(s));
  if (!p->gen && gen_in(p->format))
    p->gen = gen_in(p->format);
}

static int parse_json(const char *text, size_t len, int gen, struct preset_list *out) {
  struct jp j = {text, text + len, 0, false};
  struct jv root;
  if (!jvalue(&j, &root))
    return 0;
  int added = 0;
  if (root.type == J_ARR) {
    for (int i = 0; i < root.n; i++) {
      struct preset p;
      preset_init(&p, gen);
      jset(&root.v[i], &p);
      added += push(out, &p);
    }
  } else if (root.type == J_OBJ) { /* Smogon's {species: {format: {set name: {...}}}} */
    for (int a = 0; a < root.n; a++)
      for (int b = 0; root.v[a].type == J_OBJ && b < root.v[a].n; b++) {
        const struct jv *f = &root.v[a].v[b];
        for (int c = 0; f->type == J_OBJ && c < f->n; c++) {
          struct preset p;
          preset_init(&p, gen);
          copyz(p.species, sizeof p.species, root.keys[a]);
          char fmt[32];
          snprintf(fmt, sizeof fmt, "%s", root.v[a].keys[b] ? root.v[a].keys[b] : "");
          for (char *q = fmt; *q; q++)
            *q = (char)toupper((unsigned char)*q);
          copyz(p.format, sizeof p.format, fmt);
          copyz(p.name, sizeof p.name, f->keys[c]);
          jset(&f->v[c], &p);
          added += push(out, &p);
        }
      }
  }
  jfree(&root);
  return added;
}

/* ---- files ---------------------------------------------------------------------------------------------------- */

static const char *ext_of(const char *name) {
  const char *dot = strrchr(name, '.');
  return dot ? dot : "";
}

int presets_parse(const char *name, const char *text, size_t len, int gen, struct preset_list *out) {
  const char *ext = ext_of(name ? name : "");
  int fileGen = gen_in(name ? name : "");
  if (!gen && fileGen)
    gen = fileGen;
  if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
    text += 3, len -= 3; /* a UTF-8 BOM */
  if (!strcasecmp(ext, ".csv"))
    return parse_csv(text, len, gen, out);
  if (!strcasecmp(ext, ".json"))
    return parse_json(text, len, gen, out);
  return parse_text(text, len, gen, out);
}

static char *slurp(const char *path, size_t max, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  char *buf = NULL;
  size_t cap = 0, n = 0;
  for (;;) {
    if (n + 65536 + 1 > cap) {
      cap = cap ? cap * 2 : 1 << 20;
      if (cap > max + 65536 + 1) {
        free(buf);
        fclose(f);
        return NULL;
      }
      char *g = realloc(buf, cap);
      if (!g) {
        free(buf);
        fclose(f);
        return NULL;
      }
      buf = g;
    }
    size_t got = fread(buf + n, 1, 65536, f);
    n += got;
    if (got < 65536)
      break;
  }
  fclose(f);
  if (n > max) {
    free(buf);
    return NULL;
  }
  buf[n] = 0;
  *len = n;
  return buf;
}

struct walk {
  char dir[1024];
  char (*names)[256];
  int count, cap;
};
static void walk_each(const char *name, void *user) {
  struct walk *w = user;
  if (w->count == w->cap) {
    int cap = w->cap ? w->cap * 2 : 32;
    char(*g)[256] = realloc(w->names, sizeof *g * (size_t)cap);
    if (!g)
      return;
    w->names = g, w->cap = cap;
  }
  snprintf(w->names[w->count++], 256, "%s", name);
}

/* "Gen 3 OU" as "OU" (Smogon's own files name the tier alone). */
static const char *tier(const char *format) {
  if (!strncasecmp(format, "Gen ", 4)) {
    format += 4;
    while (isdigit((unsigned char)*format))
      format++;
    while (*format == ' ')
      format++;
  }
  return format;
}

static int by_key(const void *a, const void *b) {
  const struct preset *x = a, *y = b;
  int c = x->gen - y->gen;
  if (!c)
    c = strcasecmp(x->species, y->species);
  if (!c)
    c = strcasecmp(tier(x->format), tier(y->format));
  if (!c)
    c = strcasecmp(x->name, y->name);
  for (int i = 0; i < 4 && !c; i++)
    c = strcasecmp(x->moves[i], y->moves[i]);
  return c;
}

static void load(const char *dir, int depth, struct preset_list *out) {
  struct walk w = {{0}, NULL, 0, 0};
  if (!pocket_list_dir(dir, walk_each, &w)) {
    free(w.names);
    return;
  }
  for (int i = 0; i < w.count; i++) {
    char path[1300];
    snprintf(path, sizeof path, "%s/%s", dir, w.names[i]);
    if (pocket_is_dir(path)) {
      if (depth < 3)
        load(path, depth + 1, out);
      continue;
    }
    const char *ext = ext_of(w.names[i]);
    if (strcasecmp(ext, ".txt") && strcasecmp(ext, ".csv") && strcasecmp(ext, ".json"))
      continue;
    size_t len;
    char *text = slurp(path, 64u << 20, &len);
    if (!text)
      continue;
    presets_parse(w.names[i], text, len, 0, out);
    free(text);
  }
  free(w.names);
}

int presets_load_dir(const char *dir, struct preset_list *out) {
  load(dir, 0, out);
  if (out->count > 1) { /* the same set from the .txt, the .csv and the .json: once */
    qsort(out->v, (size_t)out->count, sizeof *out->v, by_key);
    int k = 1;
    for (int i = 1; i < out->count; i++)
      if (by_key(&out->v[i], &out->v[k - 1]))
        out->v[k++] = out->v[i];
    out->count = k;
  }
  return out->count;
}

void presets_free(struct preset_list *list) {
  free(list->v);
  memset(list, 0, sizeof *list);
}

static bool copy_file(const char *from, const char to[1300]) {
  size_t len;
  char *data = slurp(from, 256u << 20, &len);
  if (!data)
    return false;
  char tmp[1310];
  snprintf(tmp, sizeof tmp, "%s.part", to);
  FILE *f = fopen(tmp, "wb");
  bool ok = f && fwrite(data, 1, len, f) == len;
  if (f && fclose(f))
    ok = false;
  free(data);
  ok = ok && pocket_replace(tmp, to);
  if (!ok)
    pocket_remove(tmp);
  return ok;
}

/* ARGV run to its end; true when it exited 0. */
static bool run(const char *const *argv) {
#ifdef _WIN32
  char cmd[4200] = "";
  for (int i = 0; argv[i]; i++) {
    size_t k = strlen(cmd);
    snprintf(cmd + k, sizeof cmd - k, i ? " \"%s\"" : "\"%s\"", argv[i]);
  }
  STARTUPINFOA si;
  memset(&si, 0, sizeof si);
  si.cb = sizeof si;
  PROCESS_INFORMATION pi;
  if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    return false;
  WaitForSingleObject(pi.hProcess, 120000);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return code == 0;
#else
  pid_t pid = fork();
  if (pid < 0)
    return false;
  if (!pid) {
    int null = open("/dev/null", 1);
    if (null >= 0)
      dup2(null, 1), dup2(null, 2);
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  }
  int status = 0;
  return waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

static bool unpack(const char *file, const char *into, bool rar) {
#ifdef _WIN32
  /* Windows' own tar (libarchive) reads .zip and .rar. */
  (void)rar;
  char tar[MAX_PATH];
  UINT k = GetSystemDirectoryA(tar, sizeof tar);
  if (!k || k > sizeof tar - 9)
    return false;
  strcat(tar, "\\tar.exe");
  const char *a[] = {tar, "-xf", file, "-C", into, NULL};
  return run(a);
#else
  const char *bsd[] = {"bsdtar", "-xf", file, "-C", into, NULL};
  if (run(bsd))
    return true;
  if (rar) {
    char to[1100];
    snprintf(to, sizeof to, "%s/", into);
    const char *unrar[] = {"unrar", "x", "-o+", "-inul", file, to, NULL};
    char o[1100];
    snprintf(o, sizeof o, "-o%s", into);
    const char *z7[] = {"7z", "x", "-y", o, file, NULL};
    return run(unrar) || run(z7);
  }
  const char *unzip[] = {"unzip", "-oq", file, "-d", into, NULL};
  const char *py[] = {"python3", "-m", "zipfile", "-e", file, into, NULL};
  return run(unzip) || run(py);
#endif
}

bool presets_import(const char *file, const char *dir, char *msg, size_t n) {
  const char *base = file;
  for (const char *s = file; *s; s++)
    if (*s == '/' || *s == '\\')
      base = s + 1;
  const char *ext = ext_of(base);
  bool zip = !strcasecmp(ext, ".zip"), rar = !strcasecmp(ext, ".rar");
  if (!zip && !rar && strcasecmp(ext, ".txt") && strcasecmp(ext, ".csv") && strcasecmp(ext, ".json")) {
    snprintf(msg, n, "Sets come as .txt, .csv, .json, .zip or .rar files");
    return false;
  }
  if (!pocket_mkdir(dir)) {
    snprintf(msg, n, "Couldn't make the presets folder");
    return false;
  }
  struct preset_list before = {0};
  presets_load_dir(dir, &before);
  int had = before.count;
  presets_free(&before);
  char to[1300];
  if (zip || rar) {
    snprintf(to, sizeof to, "%s/%.*s", dir, (int)(strlen(base) - strlen(ext)), base);
    if (!pocket_mkdir(to) || !unpack(file, to, rar)) {
      snprintf(msg, n, rar ? "Couldn't unpack the .rar: unpack it yourself and import its files"
                           : "Couldn't unpack the .zip: unpack it yourself and import its files");
      return false;
    }
  } else {
    snprintf(to, sizeof to, "%s/%s", dir, base);
    if (!copy_file(file, to)) {
      snprintf(msg, n, "Couldn't copy that file into the presets folder");
      return false;
    }
  }
  struct preset_list after = {0};
  presets_load_dir(dir, &after);
  int added = after.count - had;
  presets_free(&after);
  snprintf(msg, n, added > 0 ? "Imported %d new sets" : "No new sets in that file", added);
  return added > 0;
}

/* ---- filling a Pokemon ---------------------------------------------------------------------------------------- */

static void note(struct preset_notes *nt, const char *fmt, const char *a, const char *b) {
  if (nt->count < PRESET_NOTES)
    snprintf(nt->line[nt->count++], sizeof nt->line[0], fmt, a, b);
}

/* The id NAME has among E's NAMEFN's 1..MAX, by normalized name; -1 if none. */
static int find(struct save_edit *e, const char *(*namefn)(struct save_edit *, int), int max, const char *name) {
  char want[48], got[48];
  norm(name, want, sizeof want);
  if (!want[0])
    return -1;
  for (int id = 1; id <= max; id++) {
    const char *s = namefn(e, id);
    if (!s)
      continue;
    norm(s, got, sizeof got);
    if (!strcmp(got, want))
      return id;
  }
  return -1;
}

bool preset_for(const struct preset *p, const char *species) {
  char a[48], b[48];
  norm(p->species, a, sizeof a);
  norm(species, b, sizeof b);
  if (!strcmp(a, b))
    return true;
  const char *dash = strchr(p->species, '-');
  if (!dash)
    return false;
  char base[32];
  snprintf(base, sizeof base, "%.*s", (int)(dash - p->species), p->species);
  norm(base, a, sizeof a);
  return !strcmp(a, b);
}

int preset_species(struct save_edit *e, const struct preset *p) {
  int id = find(e, e->ops->species_name, e->ops->species_max(e), p->species);
  if (id < 0 && strchr(p->species, '-')) { /* a form: the base species */
    char base[32];
    snprintf(base, sizeof base, "%.*s", (int)(strchr(p->species, '-') - p->species), p->species);
    id = find(e, e->ops->species_name, e->ops->species_max(e), base);
  }
  return id;
}

/* Hidden Power's type from the IVs' lowest bits (HP, Atk, Def, Spe, SpA, SpD), as Gen 3 to 7 work it out. */
static const char *const HP_TYPES[16] = {"FIGHTING", "FLYING", "POISON", "GROUND", "ROCK",  "BUG",
                                         "GHOST",    "STEEL",  "FIRE",   "WATER",  "GRASS", "ELECTRIC",
                                         "PSYCHIC",  "ICE",    "DRAGON", "DARK"};
static int hp_type(const int *iv) {
  int sum = 0;
  for (int i = 0; i < 6; i++)
    sum |= (iv[i] & 1) << i;
  return sum * 15 / 63;
}

int preset_apply(struct save_edit *e, const struct preset *p, struct se_mon *m, struct preset_notes *nt) {
  memset(nt, 0, sizeof *nt);
  int filled = 0;
  int species = preset_species(e, p);
  if (species < 0) {
    note(nt, "%s isn't in this game", p->species, NULL);
    return 0;
  }
  if (species != m->species) {
    m->species = species;
    m->ability = 0;
    filled++;
  }
  if (p->tera[0])
    note(nt, "Tera Type %s isn't in this game", p->tera, NULL);

  /* Moves: the ones it can learn, in the set's order; the old ones fill what's left. */
  static uint16_t ok[512];
  bool complete = false;
  int okCount = e->ops->learnable ? e->ops->learnable(e, m->species, m->level, ok, 512, &complete) : -1;
  int moves[4] = {0}, count = 0, hpType = -1;
  for (int i = 0; i < 4; i++) {
    if (!p->moves[i][0])
      continue;
    char want[48];
    norm(p->moves[i], want, sizeof want);
    int id = -1;
    if (!strncmp(want, "HIDDENPOWER", 11)) {
      id = find(e, e->ops->move_name, e->ops->move_max(e), "Hidden Power");
      for (int t = 0; t < 16 && want[11]; t++)
        if (!strcmp(want + 11, HP_TYPES[t]))
          hpType = t;
    } else
      id = find(e, e->ops->move_name, e->ops->move_max(e), p->moves[i]);
    if (id < 0) {
      note(nt, "%s isn't a move in this game", p->moves[i], NULL);
      continue;
    }
    bool can = !complete;
    for (int k = 0; k < 4 && !can; k++)
      can = m->moves[k] == id;
    for (int k = 0; k < okCount && !can; k++)
      can = ok[k] == id;
    if (!can) {
      char who[32];
      snprintf(who, sizeof who, "%s", e->ops->species_name(e, m->species) ? e->ops->species_name(e, m->species) : "It");
      note(nt, "%s can't learn %s here", who, p->moves[i]);
      continue;
    }
    bool dup = false;
    for (int k = 0; k < count; k++)
      dup |= moves[k] == id;
    if (!dup)
      moves[count++] = id;
  }
  if (count) {
    for (int k = 0; k < 4 && count < 4; k++) {
      bool dup = !m->moves[k];
      for (int j = 0; j < count; j++)
        dup |= moves[j] == m->moves[k];
      if (!dup)
        moves[count++] = m->moves[k];
    }
    for (int k = 0; k < 4; k++) {
      if (moves[k] != m->moves[k])
        m->ppUps[k] = 0;
      m->moves[k] = moves[k];
    }
    filled++;
  }

  if (p->nature[0]) {
    char want[16], got[16];
    norm(p->nature, want, sizeof want);
    int found = -1;
    for (int i = 0; i < 25 && found < 0; i++) {
      norm(se_nature_name(i), got, sizeof got);
      if (!strcmp(got, want))
        found = i;
    }
    if (found >= 0)
      m->nature = found, filled++;
    else
      note(nt, "%s isn't a nature", p->nature, NULL);
  }

  int total = 0;
  bool evOk = true;
  for (int i = 0; i < 6; i++) {
    total += p->evs[i];
    evOk &= p->evs[i] >= 0 && p->evs[i] <= 255;
  }
  if (evOk && total <= 510) {
    for (int i = 0; i < 6; i++)
      m->evs[i] = p->evs[i];
    filled++;
  } else
    note(nt, "Its EVs go past what this game allows%s%s", "", "");

  int iv[6];
  for (int i = 0; i < 6; i++)
    iv[i] = p->ivs[i] < 0 ? 0 : p->ivs[i] > 31 ? 31 : p->ivs[i];
  if (hpType >= 0 && hp_type(iv) != hpType) {
    /* The fewest IVs the set leaves open taken down by one to give Hidden Power its type. */
    int best = -1, bestCost = 99;
    for (int mask = 0; mask < 64; mask++) {
      int t[6], cost = 0;
      bool fine = true;
      for (int i = 0; i < 6; i++) {
        t[i] = iv[i];
        if (mask >> i & 1) {
          fine &= !p->ivSet[i] && iv[i] > 0;
          t[i] = iv[i] - 1, cost++;
        }
      }
      if (fine && hp_type(t) == hpType && cost < bestCost)
        best = mask, bestCost = cost;
    }
    if (best >= 0)
      for (int i = 0; i < 6; i++)
        iv[i] -= best >> i & 1;
    else
      note(nt, "Its IVs can't give Hidden Power %s%s", HP_TYPES[hpType], "");
  }
  for (int i = 0; i < 6; i++)
    m->ivs[i] = iv[i];
  filled++;

  if (p->item[0]) {
    int id = find(e, e->ops->item_name, e->ops->item_max(e), p->item);
    int pocket = id > 0 && e->ops->item_pocket ? e->ops->item_pocket(e, id) : 0;
    if (id < 0)
      note(nt, "%s isn't an item in this game", p->item, NULL);
    else if (pocket == 1) /* key items */
      note(nt, "%s can't be held%s", p->item, "");
    else
      m->held = id, filled++;
  }

  if (p->ability[0]) {
    const char *a1 = NULL, *a2 = NULL;
    char want[32], got[32];
    norm(p->ability, want, sizeof want);
    int slot = -1;
    if (e->ops->abilities && e->ops->abilities(e, m->species, &a1, &a2)) {
      norm(a1, got, sizeof got);
      if (a1 && !strcmp(got, want))
        slot = 0;
      norm(a2, got, sizeof got);
      if (a2 && !strcmp(got, want))
        slot = 1;
    }
    if (slot >= 0)
      m->ability = slot, filled++;
    else
      note(nt, "%s isn't an ability it has here%s", p->ability, "");
  }
  return filled;
}
