/* GPL-3.0, see ../NOTICE. */
#include "game_name.h"
#include "pocket_os.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#define POKEMON "Pok\xC3\xA9mon"

/* Words the games spell their own way (matched in any case). */
static const char *const OWN_CASE[] = {
    "FireRed", "LeafGreen", "HeartGold", "SoulSilver", "SoulGold", "PureRed", "PureBlue", "GS", "R.O.W.E.",
    "TMT2",    "UPR",       "QoL",       "II",         "III",      "IV",      "EX",       "DX", "ES",
};
/* Small words left lower case inside a name ("Wishes of Tomorrow", "Sword and Shield"). */
static const char *const SMALL[] = {"of", "and", "the", "a", "an", "in", "on", "to", "for", "or", "de", "du", "la"};

static bool is_pokemon_word(const char *w) {
  if (!strcasecmp(w, "pokemon"))
    return true;
  // "Pokémon" / "POKéMON" / "POKÉMON": "pok", an é (C3 A9) or É (C3 89), "mon".
  return strlen(w) == 8 && !strncasecmp(w, "pok", 3) && (unsigned char)w[3] == 0xC3 &&
         ((unsigned char)w[4] == 0xA9 || (unsigned char)w[4] == 0x89) && !strcasecmp(w + 5, "mon");
}

/* "v1", "v1.2.3", "V2.0b", "1.0", "1.2.3": a version, dropped. */
static bool is_version(const char *w) {
  const char *p = w;
  if (*p == 'v' || *p == 'V')
    p++;
  if (!isdigit((unsigned char)*p))
    return false;
  bool dot = false;
  for (; *p; p++) {
    if (*p == '.')
      dot = true;
    else if (!isdigit((unsigned char)*p))
      return (*p >= 'a' && *p <= 'z') && !p[1] && (dot || w[0] == 'v' || w[0] == 'V'); // "1.0b"
  }
  return dot || w[0] == 'v' || w[0] == 'V';
}

static bool all_case(const char *w, bool upper) {
  bool letter = false;
  for (; *w; w++)
    if (isalpha((unsigned char)*w)) {
      letter = true;
      if (upper ? islower((unsigned char)*w) : isupper((unsigned char)*w))
        return false;
    }
  return letter;
}

/* One word cased: the games' own spelling, else a small word (not first), else a word all one case capitalised;
 * a word already in mixed case ("McDonald", "pureRGB") is kept. */
static void case_word(char *w, bool first) {
  for (size_t i = 0; i < sizeof OWN_CASE / sizeof *OWN_CASE; i++)
    if (!strcasecmp(w, OWN_CASE[i])) {
      strcpy(w, OWN_CASE[i]);
      return;
    }
  if (!first)
    for (size_t i = 0; i < sizeof SMALL / sizeof *SMALL; i++)
      if (!strcasecmp(w, SMALL[i])) {
        for (char *p = w; *p; p++)
          *p = (char)tolower((unsigned char)*p);
        return;
      }
  if (!all_case(w, false) && !all_case(w, true))
    return;
  bool start = true;
  for (char *p = w; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalpha(c)) {
      *p = (char)(start ? toupper(c) : tolower(c));
      start = false;
    } else if (c == '-' || c == '.') {
      start = true; // "Smaragd-Edition", "R.O.W.E."
    } else if (c != '\'') {
      start = !(c & 0x80); // inside a UTF-8 letter, keep going
    }
  }
}

void game_display_name(const char *file, bool pokemon, char *out, size_t size) {
  if (!size)
    return;
  char buf[512];
  snprintf(buf, sizeof buf, "%s", file);
  char *dot = strrchr(buf, '.');
  if (dot && dot != buf && strlen(dot) <= 5)
    *dot = 0;
  // Dump numbering: "1636 - Pokemon ...".
  char *s = buf;
  {
    char *p = s;
    while (isdigit((unsigned char)*p))
      p++;
    if (p > s && p[0] == ' ' && p[1] == '-' && p[2] == ' ')
      s = p + 3;
  }
  // (tags) and [tags] go, "_" and lone dashes become spaces.
  char clean[512];
  size_t n = 0;
  int depth = 0;
  for (const char *p = s; *p && n + 1 < sizeof clean; p++) {
    char c = *p;
    if (c == '(' || c == '[') {
      depth++;
      continue;
    }
    if ((c == ')' || c == ']') && depth) {
      depth--;
      continue;
    }
    if (depth)
      continue;
    if (c == '_')
      c = ' ';
    if (c == '-' && (p == s || p[-1] == ' ' || p[1] == ' ' || !p[1]))
      c = '\x01'; // a break between parts: the next word starts one ("Zelda - The Minish Cap")
    clean[n++] = c;
  }
  clean[n] = 0;
  // Words.
  char *words[64];
  bool starts[64];
  int count = 0;
  bool hadPokemon = false, start = true;
  for (char *tok = strtok(clean, " \t\x01"); tok && count < 64; tok = strtok(NULL, " \t\x01")) {
    if (tok > clean + 1 && (tok[-2] == '\x01' || tok[-1] == '\x01'))
      start = true;
    if (is_pokemon_word(tok)) {
      hadPokemon = true;
      continue;
    }
    if (is_version(tok))
      continue;
    starts[count] = start;
    start = false;
    words[count++] = tok;
  }
  // English "Version" after the name goes ("FireRed Version", "Yellow Version Special Pikachu Edition"); one that
  // starts it ("Version Jaune") stays.
  {
    int kept = 0;
    for (int i = 0; i < count; i++)
      if (!i || strcasecmp(words[i], "version")) {
        starts[kept] = starts[i];
        words[kept++] = words[i];
      }
    count = kept;
  }
  if (count && words[count - 1][strlen(words[count - 1]) - 1] == ':')
    words[count - 1][strlen(words[count - 1]) - 1] = 0;
  out[0] = 0;
  size_t used = 0;
  if (hadPokemon || pokemon)
    used = (size_t)snprintf(out, size, "%s", count ? POKEMON " " : POKEMON);
  for (int i = 0; i < count && used + 1 < size; i++) {
    case_word(words[i], i == 0 || starts[i]);
    used += (size_t)snprintf(out + used, size - used, "%s%s", i ? " " : "", words[i]);
  }
  if (!out[0]) // nothing left: the file's own name
    snprintf(out, size, "%s", file);
}

bool game_header_is_pokemon(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return false;
  long at;
  if (!strcasecmp(ext, ".gba"))
    at = 0xA0; // the 12-byte title, then the game code
  else if (!strcasecmp(ext, ".gb") || !strcasecmp(ext, ".gbc"))
    at = 0x134;
  else if (!strcasecmp(ext, ".nds"))
    at = 0;
  else
    return false;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  char h[16] = {0};
  bool ok = !fseek(f, at, SEEK_SET) && fread(h, 1, sizeof h, f) == sizeof h;
  fclose(f);
  if (!ok)
    return false;
  if (!strncmp(h, "POKEMON", 7) || !strncmp(h, "PM_CRYSTAL", 10))
    return true;
  if (at == 0xA0) {
    static const char *const CODES[] = {"BPR", "BPG", "BPE", "AXV", "AXP", "MONE", "PUVV"};
    for (size_t i = 0; i < sizeof CODES / sizeof *CODES; i++)
      if (!strncmp(h + 12, CODES[i], strlen(CODES[i])))
        return true;
  }
  return false;
}

/* --- the grid's version line and tags --- */

static bool version_word(const char *w, size_t n) {
  char buf[64];
  if (n >= sizeof buf)
    return false;
  memcpy(buf, w, n);
  buf[n] = 0;
  return is_version(buf);
}

void game_version(const char *file, char *out, size_t size) {
  if (!size)
    return;
  out[0] = 0;
  char buf[512];
  snprintf(buf, sizeof buf, "%s", file);
  char *dot = strrchr(buf, '.');
  if (dot && dot != buf && strlen(dot) <= 5)
    *dot = 0;
  // "(Rev 1)" / "(Rev A)": a revision.
  const char *rev = strstr(buf, "(Rev ");
  if (rev) {
    const char *end = strchr(rev, ')');
    if (end && end - rev - 1 < 16) {
      snprintf(out, size, "%.*s", (int)(end - rev - 1), rev + 1);
      return;
    }
  }
  // The first word that is a version, inside (tags) or not: "v4.5.3", "1.2.6" -> shown with a "v".
  for (const char *p = buf; *p;) {
    while (*p && (*p == ' ' || *p == '(' || *p == ')' || *p == '[' || *p == ']' || *p == '_' || *p == '-'))
      p++;
    const char *s = p;
    while (*p && *p != ' ' && *p != '(' && *p != ')' && *p != '[' && *p != ']' && *p != '_')
      p++;
    size_t n = (size_t)(p - s);
    if (n && version_word(s, n)) {
      snprintf(out, size, "%s%.*s", (*s == 'v' || *s == 'V') ? "" : "v", (int)n, s);
      if (out[0] == 'V')
        out[0] = 'v';
      return;
    }
  }
}

bool game_is_original(const char *file) {
  static const char *const REGIONS[] = {"(USA",    "(Europe", "(Japan",   "(World",  "(France", "(Germany",
                                        "(Spain",  "(Italy",  "(Korea",   "(Australia", "(Netherlands", "(Sweden",
                                        "(UK",     "(Canada", "(Brazil"};
  char low[512];
  size_t n = 0;
  for (const char *p = file; *p && n + 1 < sizeof low; p++)
    low[n++] = (char)tolower((unsigned char)*p);
  low[n] = 0;
  if (strstr(low, "hack") || strstr(low, "[h") || strstr(low, "patched") || strstr(low, "randomizer") ||
      strstr(low, "nuzlocke"))
    return false;
  for (size_t i = 0; i < sizeof REGIONS / sizeof *REGIONS; i++)
    if (strstr(file, REGIONS[i]))
      return true;
  // No tag, no version: a plain retail title ("pokemon firered.gba").
  static const char *const RETAIL[] = {"FireRed", "LeafGreen", "Emerald", "Ruby", "Sapphire", "Red", "Blue",
                                       "Yellow", "Gold", "Silver", "Crystal", "Diamond", "Pearl", "Platinum",
                                       "HeartGold", "SoulSilver", "Black", "White", "Black 2", "White 2"};
  char version[64], name[256];
  game_version(file, version, sizeof version);
  if (*version)
    return false;
  game_display_name(file, true, name, sizeof name);
  const char *rest = strncmp(name, POKEMON " ", 9) ? name : name + 9;
  for (size_t i = 0; i < sizeof RETAIL / sizeof *RETAIL; i++)
    if (!strcmp(rest, RETAIL[i]))
      return true;
  return false;
}

/* --- RENAME: DATA/names.txt, "file<TAB>name" a line --- */

bool game_custom_name(const char *root, const char *file, char *out, size_t size) {
  char path[1100];
  snprintf(path, sizeof path, "%s/names.txt", root);
  FILE *f = fopen(path, "r");
  if (!f)
    return false;
  char line[1200];
  bool found = false;
  size_t flen = strlen(file);
  while (!found && fgets(line, sizeof line, f)) {
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
      line[--n] = 0;
    if (n > flen + 1 && !strncmp(line, file, flen) && line[flen] == '\t') {
      snprintf(out, size, "%s", line + flen + 1);
      found = out[0] != 0;
    }
  }
  fclose(f);
  return found;
}

bool game_custom_name_set(const char *root, const char *file, const char *name) {
  char path[1100], tmp[1200];
  snprintf(path, sizeof path, "%s/names.txt", root);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (strchr(file, '\t') || strchr(file, '\n') || (name && (strchr(name, '\t') || strchr(name, '\n'))))
    return false;
  FILE *in = fopen(path, "r"), *out = fopen(tmp, "w");
  if (!out) {
    if (in)
      fclose(in);
    return false;
  }
  size_t flen = strlen(file);
  char line[1200];
  while (in && fgets(line, sizeof line, in)) {
    if (!strncmp(line, file, flen) && line[flen] == '\t')
      continue;
    fputs(line, out);
    size_t n = strlen(line);
    if (n && line[n - 1] != '\n')
      fputc('\n', out);
  }
  if (in)
    fclose(in);
  if (name && *name)
    fprintf(out, "%s\t%s\n", file, name);
  bool ok = fclose(out) == 0;
  if (ok)
    ok = pocket_replace(tmp, path);
  else
    pocket_remove(tmp);
  return ok;
}
