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

/* Every way the word gets written, matched with ASCII letters in any case: plain, the é (é / É composed, e and a
 * combining accent), an apostrophe (straight or curly), PKMN, the é read as Latin-1 ("PokÃ©mon") or stored as it. */
static const char *const POKEMON_SPELLINGS[] = {
    "pok\xC3\x83\xC2\xA9mon", "pok\xC3\x83\xC2\x89mon", "poke\xE2\x80\x99mon", "poke\xCC\x81mon",
    "pok\xC3\xA9mon",         "pok\xC3\x89mon",         "poke'mon",               "pokemon",
    "pok\xE9mon",              "pok\xC9mon",              "pkmn",
};
/* The bytes of a spelling W starts with, when what follows is the word's end or punctuation ("Pokemon-",
 * "Pokemon:", "Pokemon,", "Pokemon's" is not one); else 0. */
static size_t pokemon_prefix(const char *w) {
  for (size_t i = 0; i < sizeof POKEMON_SPELLINGS / sizeof *POKEMON_SPELLINGS; i++) {
    const char *s = POKEMON_SPELLINGS[i];
    size_t n = strlen(s), k = 0;
    for (; k < n && w[k]; k++) {
      unsigned char a = (unsigned char)w[k], b = (unsigned char)s[k];
      if (a == b || (a < 0x80 && tolower(a) == b))
        continue;
      break;
    }
    if (k == n && (!w[n] || strchr("-:,.!_", w[n])))
      return n;
  }
  return 0;
}
static bool is_pokemon_word(const char *w) {
  size_t n = pokemon_prefix(w);
  if (!n)
    return false;
  for (w += n; *w; w++)
    if (!strchr("-:,.!_", *w))
      return false;
  return true;
}
/* "Poke" / "Poké" / "POKÉ", the first half of a split "Poké mon". */
static bool is_poke_half(const char *w) {
  return !strcasecmp(w, "poke") || !strcasecmp(w, "pok\xC3\xA9") || !strcasecmp(w, "pok\xC3\x89") ||
         (!strncasecmp(w, "pok", 3) && ((unsigned char)w[3] == 0xC3) &&
          ((unsigned char)w[4] == 0xA9 || (unsigned char)w[4] == 0x89) && !w[5]);
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
    // A dot between two words of letters is a gap ("Pokemon.Fire.Ash"); "R.O.W.E." and "1.2" keep theirs.
    if (c == '.' && p - s >= 2 && isalpha((unsigned char)p[-1]) && isalpha((unsigned char)p[-2]) &&
        isalpha((unsigned char)p[1]) && isalpha((unsigned char)p[2]))
      c = ' ';
    if (c == '-' && (p == s || p[-1] == ' ' || p[1] == ' ' || !p[1]))
      c = '\x01'; // a break between parts: the next word starts one ("Zelda - The Minish Cap")
    clean[n++] = c;
  }
  clean[n] = 0;
  // Words.
  char *raw[64];
  bool rawStart[64];
  int rawCount = 0;
  for (char *tok = strtok(clean, " \t\x01"); tok && rawCount < 64; tok = strtok(NULL, " \t\x01")) {
    rawStart[rawCount] = tok > clean + 1 && (tok[-2] == '\x01' || tok[-1] == '\x01');
    raw[rawCount++] = tok;
  }
  char *words[64];
  bool starts[64];
  int count = 0;
  bool hadPokemon = false, pocketMonsters = false, start = true;
  for (int i = 0; i < rawCount; i++) {
    char *tok = raw[i];
    if (rawStart[i])
      start = true;
    // "The Pokemon ...": the "The" goes with it.
    if (!count && !hadPokemon && !strcasecmp(tok, "the") && i + 1 < rawCount && is_pokemon_word(raw[i + 1]))
      continue;
    // "Poké mon" written apart.
    if (is_poke_half(tok) && i + 1 < rawCount && is_pokemon_word(raw[i + 1]) == false &&
        !strncasecmp(raw[i + 1], "mon", 3) && (!raw[i + 1][3] || strchr("-:,.!", raw[i + 1][3]))) {
      hadPokemon = true;
      i++;
      continue;
    }
    if (is_pokemon_word(tok)) {
      hadPokemon = true;
      continue;
    }
    // "Pokemon-Fire-Ash": the word first, the rest a word of its own.
    size_t pre = pokemon_prefix(tok);
    if (pre && tok[pre]) {
      hadPokemon = true;
      tok += pre;
      while (*tok && strchr("-:,.!_", *tok))
        tok++;
      if (!*tok)
        continue;
    }
    if (is_version(tok))
      continue;
    // "Ver." / "Ver" before or after a version: the version's word.
    if (count && (!strcasecmp(tok, "ver") || !strcasecmp(tok, "ver.")))
      continue;
    if (!strcasecmp(tok, "pocket") && i + 1 < rawCount && !strncasecmp(raw[i + 1], "monsters", 8))
      pocketMonsters = true;
    // A stray "," or ":" of its own.
    if (!strcmp(tok, ",") || !strcmp(tok, ":") || !strcmp(tok, "-"))
      continue;
    starts[count] = start;
    start = false;
    words[count++] = tok;
  }
  // A trailing comma ("Fire Ash, Pokemon") goes.
  for (int i = 0; i < count; i++) {
    size_t l = strlen(words[i]);
    if (l > 1 && words[i][l - 1] == ',' && (i == count - 1))
      words[i][l - 1] = 0;
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
  // "Pocket Monsters" (the Japanese carts' own name) stays as it is, without the word put in front as well.
  if (hadPokemon || (pokemon && !pocketMonsters))
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

void game_pc_display_name(const char *title, char *out, size_t size) {
  // Made into a file name game_display_name reads: a colon is a " - " between words, and an extension of its own (so a
  // dot in the title, a version's, isn't taken for one).
  char file[300];
  size_t n = 0;
  for (const char *p = title; *p && n + 4 < sizeof file - 4; p++) {
    if (*p == ':') {
      file[n++] = ' ';
      file[n++] = '-';
      file[n++] = ' ';
    } else
      file[n++] = *p;
  }
  snprintf(file + n, sizeof file - n, ".pc");
  game_display_name(file, true, out, size);
  if (!*out)
    snprintf(out, size, "%s", title);
}
