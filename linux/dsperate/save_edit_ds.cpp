// GPL-3.0. SAVE EDITOR's HeartGold / SoulSilver part (save_edit.h): the general and PC blocks of the battery save, each
// changed where the game last saved it and its CRC-16 written again, the way the save's own footer checks it
// (pokeheartgold's save_data.c; the Pokemon records are Gen4Reader.kt's). Species, moves, items and abilities are the
// cart's own (its personal / move NARCs and its ability names), so nothing is written that the game doesn't have.
#include "../save_edit.h"

#include "companion.h"
#include "data_gen.h"

#include <array>
#include <iterator>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include "pd_dex.h"
}

namespace {
using Bytes = std::vector<uint8_t>;

constexpr size_t COPY = 0x40000;
constexpr size_t GENERAL_SIZE = 0xF628, STORAGE_OFF = 0xF700, STORAGE_SIZE = 0x12310;
constexpr uint32_t MAGIC = 0x20060623;
constexpr size_t MONEY = 0x78, PARTY_COUNT = 0x94, PARTY = 0x98, BAG = 0x644;
constexpr int PARTY_MON = 236, BOX_MON = 136, BOXES = 18, BOX_SLOTS = 30, BOX_STRIDE = 0x1000;
constexpr int POCKETS = 8, P_KEY = 1, P_TMHM = 2;
constexpr int SPECIES_MAX = 493, MOVE_MAX = 467, FIRST_HM = 420, LAST_HM = 427;
constexpr int SPECIES_UNOWN = 201, SPECIES_SHEDINJA = 292;
const char *const POCKET_NAME[POCKETS] = {"ITEMS",   "KEY ITEMS", "TMs & HMs",  "MAIL",
                                          "MEDICINE", "BERRIES",   "POKé BALLS", "BATTLE ITEMS"};

uint16_t rd16(const uint8_t *p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t rd32(const uint8_t *p) { return rd16(p) | uint32_t(rd16(p + 2)) << 16; }
void wr16(uint8_t *p, unsigned v) { p[0] = uint8_t(v), p[1] = uint8_t(v >> 8); }
void wr32(uint8_t *p, uint32_t v) { wr16(p, v & 0xFFFF), wr16(p + 2, v >> 16); }
void say(char *err, size_t n, const char *why) {
  if (err && n)
    std::snprintf(err, n, "%s", why);
}

uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= uint16_t(p[i] << 8);
    for (int b = 0; b < 8; b++)
      crc = uint16_t(crc & 0x8000 ? crc << 1 ^ 0x1021 : crc << 1);
  }
  return crc;
}

// The block at BASE (SIZE with its 16-byte footer): its save number, or -1 when the footer doesn't check.
long block_number(const uint8_t *save, size_t saveSize, size_t base, size_t size) {
  if (base + size > saveSize)
    return -1;
  const uint8_t *f = save + base + size - 16;
  if (rd32(f + 4) != size || rd32(f + 8) != MAGIC || rd16(f + 14) != crc16(save + base, size - 16))
    return -1;
  return long(rd32(f));
}
// The newer copy of a block that checks, or SIZE_MAX.
size_t active(const uint8_t *save, size_t saveSize, size_t off, size_t size) {
  long a = block_number(save, saveSize, off, size), b = block_number(save, saveSize, COPY + off, size);
  if (a < 0 && b < 0)
    return SIZE_MAX;
  return b > a ? COPY + off : off;
}

// The cart's file system (map_art.cpp's).
Bytes rom_file(const Bytes &rom, const std::string &path) {
  auto r16 = [&](size_t p) { return p + 2 <= rom.size() ? rd16(rom.data() + p) : throw std::runtime_error("rom"); };
  auto r32 = [&](size_t p) { return p + 4 <= rom.size() ? rd32(rom.data() + p) : throw std::runtime_error("rom"); };
  size_t fnt = r32(0x40);
  unsigned id = 0xF000;
  size_t begin = 1;
  while (begin < path.size()) {
    size_t end = path.find('/', begin);
    std::string name = path.substr(begin, end - begin);
    size_t entry = fnt + (id & 0xFFF) * 8, p = fnt + r32(entry);
    unsigned next = r16(entry + 4);
    bool found = false;
    for (;;) {
      if (p >= rom.size())
        throw std::runtime_error("fnt");
      int len = rom[p++];
      if (!len)
        break;
      int n = len & 127;
      if (p + n > rom.size())
        throw std::runtime_error("fnt");
      std::string label(rom.begin() + long(p), rom.begin() + long(p + n));
      p += n;
      unsigned value = (len & 128) ? r16(p) : next++;
      if (len & 128)
        p += 2;
      if (label == name) {
        id = value, found = true;
        break;
      }
    }
    if (!found)
      throw std::runtime_error("missing file");
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  size_t fat = r32(0x48) + id * 8, start = r32(fat), end = r32(fat + 4);
  if (end < start || end > rom.size())
    throw std::runtime_error("fat");
  return Bytes(rom.begin() + long(start), rom.begin() + long(end));
}
Bytes narc_member(const Bytes &narc, unsigned id) {
  if (narc.size() < 16 || std::memcmp(narc.data(), "NARC", 4))
    throw std::runtime_error("narc");
  auto r16 = [&](size_t p) { return p + 2 <= narc.size() ? rd16(narc.data() + p) : throw std::runtime_error("narc"); };
  auto r32 = [&](size_t p) { return p + 4 <= narc.size() ? rd32(narc.data() + p) : throw std::runtime_error("narc"); };
  size_t fat = r16(12);
  if (id >= r16(fat + 8))
    throw std::runtime_error("member");
  size_t offsets = fat + 12 + id * 8, btnf = fat + r32(fat + 4), base = btnf + r32(btnf + 4) + 8;
  size_t a = r32(offsets), b = r32(offsets + 4);
  if (b < a || base + b > narc.size())
    throw std::runtime_error("member");
  return Bytes(narc.begin() + long(base + a), narc.begin() + long(base + b));
}
unsigned narc_count(const Bytes &narc) {
  if (narc.size() < 16)
    return 0;
  size_t fat = rd16(narc.data() + 12);
  return fat + 10 <= narc.size() ? rd16(narc.data() + fat + 8) : 0;
}

std::string char_text(uint16_t code) {
  for (const auto &c : pd_ds_data::chars)
    if (c.code == code)
      return c.text;
  return "";
}
// A Gen 4 message bank's lines (Gen4Text.kt).
std::vector<std::string> bank_lines(const Bytes &bank) {
  std::vector<std::string> lines;
  if (bank.size() < 4)
    return lines;
  size_t count = rd16(bank.data());
  unsigned key = rd16(bank.data() + 2);
  for (size_t i = 0; i < count; i++) {
    size_t rec = 4 + i * 8;
    if (rec + 8 > bank.size())
      break;
    uint32_t mask = (765u * uint32_t(i + 1) * key) & 0xFFFF, rep = mask | mask << 16;
    size_t start = rd32(bank.data() + rec) ^ rep, length = rd32(bank.data() + rec + 4) ^ rep;
    std::string line;
    if (start <= bank.size() && length <= (bank.size() - start) / 2) {
      uint16_t k = uint16_t(0x91BD3u * uint32_t(i + 1));
      for (size_t j = 0; j < length; j++) {
        uint16_t c = rd16(bank.data() + start + 2 * j) ^ k;
        k = uint16_t(k + 0x493D);
        if (c == 0xFFFF)
          break;
        if (c >= 0xFFFE)
          continue;
        line += char_text(c);
      }
    }
    lines.push_back(line);
  }
  return lines;
}

std::string nick_text(const uint8_t *p) {
  std::string out;
  for (int i = 0; i < 11; i++) {
    uint16_t v = rd16(p + 2 * i);
    if (v == 0xFFFF)
      break;
    out += char_text(v);
  }
  return out;
}
// UTF-8 to the game's codes (10 at most, then the terminator): false for a letter the game has none of.
bool encode_nick(const char *s, uint8_t out[22]) {
  int n = 0;
  while (*s) {
    if (n >= 10)
      return false;
    size_t best = 0;
    uint16_t code = 0;
    for (const auto &c : pd_ds_data::chars) {
      size_t len = std::strlen(c.text);
      if (len > best && c.text[0] != '\\' && !std::strncmp(s, c.text, len))
        best = len, code = c.code;
    }
    if (!best)
      return false;
    wr16(out + 2 * n++, code);
    s += best;
  }
  if (!n)
    return false;
  for (int i = n; i < 11; i++)
    wr16(out + 2 * i, 0xFFFF);
  return true;
}

void crypt(uint8_t *p, size_t n, uint32_t seed) {
  for (size_t i = 0; i + 1 < n; i += 2) {
    seed = seed * 0x41C64E6Du + 0x6073u;
    wr16(p + i, rd16(p + i) ^ (seed >> 16));
  }
}

// One record, its blocks decrypted and put in A, B, C, D order.
struct Mon {
  uint8_t head[8];
  uint8_t data[128];
  uint8_t party[100];
  bool ok;
  uint8_t *A() { return data; }
  uint8_t *B() { return data + 32; }
  uint8_t *C() { return data + 64; }
};
uint16_t sum128(const uint8_t *p) {
  uint16_t s = 0;
  for (int i = 0; i < 128; i += 2)
    s = uint16_t(s + rd16(p + i));
  return s;
}
Mon decode(const uint8_t *r, bool party) {
  Mon m{};
  std::memcpy(m.head, r, 8);
  uint32_t pid = rd32(r);
  uint8_t shuffled[128];
  std::memcpy(shuffled, r + 8, 128);
  if (!(rd16(r + 4) & 2))
    crypt(shuffled, 128, rd16(r + 6));
  const char *order = pd_ds_data::orders[((pid >> 13) & 31) % 24];
  for (int i = 0; i < 4; i++)
    std::memcpy(m.data + 32 * (order[i] - 'A'), shuffled + 32 * i, 32);
  m.ok = sum128(m.data) == rd16(r + 6);
  if (party) {
    std::memcpy(m.party, r + BOX_MON, 100);
    if (!(rd16(r + 4) & 1))
      crypt(m.party, 100, pid);
  }
  return m;
}
void encode(const Mon &m, uint8_t *r, bool party) {
  uint32_t pid = rd32(m.head);
  uint16_t sum = sum128(m.data);
  std::memcpy(r, m.head, 8);
  wr16(r + 4, rd16(m.head + 4) & ~7u); // encrypted, not a bad egg
  wr16(r + 6, sum);
  const char *order = pd_ds_data::orders[((pid >> 13) & 31) % 24];
  for (int i = 0; i < 4; i++)
    std::memcpy(r + 8 + 32 * i, m.data + 32 * (order[i] - 'A'), 32);
  crypt(r + 8, 128, sum);
  if (party) {
    std::memcpy(r + BOX_MON, m.party, 100);
    crypt(r + BOX_MON, 100, pid);
  }
}

bool is_shiny(uint32_t pid, uint32_t ot) { return ((ot >> 16) ^ (ot & 0xFFFF) ^ (pid >> 16) ^ (pid & 0xFFFF)) < 8; }
int unown_letter(uint32_t p) {
  return int((((p >> 24) & 3) << 6 | ((p >> 16) & 3) << 4 | ((p >> 8) & 3) << 2 | (p & 3)) % 28);
}
int nature_up(int n) { return n / 5 == n % 5 ? -1 : n / 5 + 1; }
int nature_down(int n) { return n / 5 == n % 5 ? -1 : n % 5 + 1; }
bool stat_forms(int species) {
  return species == 386 || species == 413 || species == 479 || species == 487 || species == 492;
}

struct G4 {
  Bytes raw;
  size_t general = 0, storage = 0; // the active copies' offsets in RAW
  Bytes gen, sto;                  // their bytes, as edited
  std::vector<std::array<uint8_t, 28>> personal;
  std::vector<uint8_t> movePp; // 0 = no such move
  std::vector<std::string> abilityNames;
  std::string title;
};
G4 *G(save_edit *e) { return static_cast<G4 *>(e->impl); }

const char *species_name(save_edit *, int id) {
  if (id < 1 || id > SPECIES_MAX)
    return nullptr;
  const char *s = ds_species(id, DsGame::Johto);
  return *s && std::strcmp(s, "?") ? s : nullptr;
}
int species_max(save_edit *) { return SPECIES_MAX; }
const char *move_name(save_edit *e, int id) {
  if (id < 1 || id > MOVE_MAX || size_t(id) >= G(e)->movePp.size() || !G(e)->movePp[size_t(id)])
    return nullptr;
  const char *s = ds_move(id, DsGame::Johto);
  return *s && std::strcmp(s, "?") ? s : nullptr;
}
int move_max(save_edit *) { return MOVE_MAX; }
const char *item_name(save_edit *, int id) {
  if (id < 1)
    return nullptr;
  const char *s = ds_item(id, DsGame::Johto);
  return *s && std::strcmp(s, "?") ? s : nullptr;
}
int item_max(save_edit *) { return int(std::size(pd_ds_data::johtoItems)) - 1; }
// Items a Pokemon can hold: everything up to the TMs but Mail, and the Apricorn balls.
bool holdable(save_edit *e, int id) {
  return item_name(e, id) && ((id >= 1 && id <= 327 && !(id >= 137 && id <= 148)) || (id >= 492 && id <= 500));
}
const uint8_t *base_stats(G4 *g, int species) {
  return species >= 1 && size_t(species) < g->personal.size() ? g->personal[size_t(species)].data() : nullptr;
}
int growth_rate(G4 *g, int species) {
  const uint8_t *b = base_stats(g, species);
  return b && b[19] < 6 ? b[19] : -1;
}
int level_for(int rate, uint32_t exp) {
  int level = 1;
  while (level < 100 && pd_exp_at(rate, level + 1) <= long(exp))
    level++;
  return level;
}
const char *ability_name(G4 *g, int id) {
  return id > 0 && size_t(id) < g->abilityNames.size() && !g->abilityNames[size_t(id)].empty()
             ? g->abilityNames[size_t(id)].c_str()
             : nullptr;
}
bool abilities(save_edit *e, int species, const char **first, const char **second) {
  G4 *g = G(e);
  const uint8_t *b = base_stats(g, species);
  if (!b)
    return false;
  *first = ability_name(g, b[22]);
  *second = b[23] && b[23] != b[22] ? ability_name(g, b[23]) : nullptr;
  return *first != nullptr;
}

uint8_t *record(G4 *g, int box, int slot) {
  if (box == SE_PARTY)
    return slot >= 0 && slot < 6 && uint32_t(slot) < rd32(g->gen.data() + PARTY_COUNT)
               ? g->gen.data() + PARTY + size_t(slot) * PARTY_MON
               : nullptr;
  if (box < 0 || box >= BOXES || slot < 0 || slot >= BOX_SLOTS)
    return nullptr;
  return g->sto.data() + size_t(box) * BOX_STRIDE + size_t(slot) * BOX_MON;
}
bool empty_record(const uint8_t *r) {
  for (int i = 0; i < BOX_MON; i++)
    if (r[i])
      return false;
  return true;
}

bool get_mon(save_edit *e, int box, int slot, se_mon *out) {
  G4 *g = G(e);
  const uint8_t *r = record(g, box, slot);
  if (!r || empty_record(r) || (rd16(r + 4) & 4))
    return false;
  Mon m = decode(r, box == SE_PARTY);
  int species = rd16(m.A());
  if (!m.ok || species < 1 || species > SPECIES_MAX)
    return false;
  *out = se_mon{};
  out->species = species;
  out->held = rd16(m.A() + 2);
  uint32_t exp = rd32(m.A() + 8), iv = rd32(m.B() + 16);
  int rate = growth_rate(g, species);
  out->level = box == SE_PARTY ? m.party[4] : rate >= 0 ? level_for(rate, exp) : 0;
  for (int i = 0; i < 4; i++)
    out->moves[i] = rd16(m.B() + 2 * i);
  for (int i = 0; i < 6; i++) {
    out->evs[i] = m.A()[16 + i];
    out->ivs[i] = int(iv >> (5 * i) & 31);
  }
  out->egg = (iv >> 30) & 1;
  const uint8_t *b = base_stats(g, species);
  out->ability = b && b[23] && b[23] != b[22] && m.A()[13] == b[23] ? 1 : 0;
  uint32_t pid = rd32(m.head);
  out->nature = int(pid % 25);
  out->shiny = is_shiny(pid, rd32(m.A() + 4));
  std::snprintf(out->nick, sizeof out->nick, "%s", nick_text(m.C()).c_str());
  if (box == SE_PARTY) {
    out->hp = rd16(m.party + 6);
    out->maxHp = rd16(m.party + 8);
  }
  return true;
}

bool new_personality(G4 *g, int species, uint32_t old, uint32_t ot, int nature, bool shiny, int abilitySlot,
                     uint32_t *out) {
  const uint8_t *b = base_stats(g, species);
  int ratio = b ? b[16] : 255;
  bool gendered = ratio > 0 && ratio < 254, twoAbilities = b && b[23] && b[23] != b[22];
  uint32_t seed = old ^ uint32_t(std::time(nullptr)) ^ 0x9E3779B9u;
  for (long tries = 0; tries < 50000000L; tries++) {
    seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
    uint32_t p = seed;
    if (shiny) {
      uint32_t lo = p & 0xFFFF, hi = ((ot >> 16) ^ (ot & 0xFFFF) ^ lo ^ ((p >> 16) & 7)) & 0xFFFF;
      p = hi << 16 | lo;
    } else if (is_shiny(p, ot))
      continue;
    if (int(p % 25) != nature)
      continue;
    if (gendered && (((p & 0xFF) < uint32_t(ratio)) != ((old & 0xFF) < uint32_t(ratio))))
      continue;
    if (twoAbilities && int(p & 1) != abilitySlot)
      continue;
    if (species == SPECIES_UNOWN && unown_letter(p) != unown_letter(old))
      continue;
    *out = p;
    return true;
  }
  return false;
}

bool set_mon(save_edit *e, int box, int slot, const se_mon *want, char *err, size_t n) {
  G4 *g = G(e);
  uint8_t *r = record(g, box, slot);
  se_mon now;
  if (!r || !get_mon(e, box, slot, &now))
    return say(err, n, "There's no Pokemon there to change"), false;
  if (now.egg)
    return say(err, n, "Eggs aren't changed: hatch it first"), false;
  if (!species_name(e, want->species))
    return say(err, n, "That isn't a Pokemon this game has"), false;
  if (want->level < 1 || want->level > 100)
    return say(err, n, "Levels go from 1 to 100"), false;
  if (want->nature < 0 || want->nature > 24)
    return say(err, n, "That isn't a nature"), false;
  int evTotal = 0;
  for (int i = 0; i < 6; i++) {
    if (want->ivs[i] < 0 || want->ivs[i] > 31)
      return say(err, n, "IVs go from 0 to 31"), false;
    if (want->evs[i] < 0 || want->evs[i] > 255)
      return say(err, n, "An EV goes from 0 to 255"), false;
    evTotal += want->evs[i];
  }
  if (evTotal > 510)
    return say(err, n, "EVs add up to 510 at most"), false;
  int moveCount = 0;
  for (int i = 0; i < 4; i++) {
    int m = want->moves[i];
    if (!m)
      continue;
    if (!move_name(e, m))
      return say(err, n, "That isn't a move this game has"), false;
    for (int k = 0; k < i; k++)
      if (want->moves[k] == m)
        return say(err, n, "A Pokemon can't know one move twice"), false;
    moveCount++;
  }
  if (!moveCount)
    return say(err, n, "A Pokemon needs at least one move"), false;
  if (want->held && !holdable(e, want->held))
    return say(err, n, "That isn't an item a Pokemon can hold"), false;
  const char *a1 = nullptr, *a2 = nullptr;
  if (!abilities(e, want->species, &a1, &a2))
    return say(err, n, "This game's abilities couldn't be read"), false;
  if (want->ability && !a2)
    return say(err, n, "This Pokemon has only one ability"), false;
  int rate = growth_rate(g, want->species);
  if (rate < 0)
    return say(err, n, "This game's growth tables couldn't be read"), false;

  Mon m = decode(r, box == SE_PARTY);
  int form = m.B()[24] >> 3;
  if (box == SE_PARTY && form && stat_forms(now.species))
    return say(err, n, "This form's stats aren't known here: put it in a box to change it"), false;

  uint8_t nick[22];
  bool rename = false, nicknamed = (rd32(m.B() + 16) >> 31) & 1;
  if (std::strcmp(want->nick, now.nick)) {
    if (!encode_nick(want->nick, nick))
      return say(err, n, "That name has letters the game can't show (10 at most)"), false;
    rename = true;
    nicknamed = std::strcmp(want->nick, species_name(e, want->species));
  } else if (want->species != now.species && !nicknamed) {
    // An unnamed Pokemon takes its new species' name, as an evolution renames it.
    rename = encode_nick(species_name(e, want->species), nick);
  }

  uint32_t pid = rd32(m.head), ot = rd32(m.A() + 4);
  const uint8_t *b = base_stats(g, want->species);
  bool twoAbilities = b[23] && b[23] != b[22];
  if (want->nature != int(pid % 25) || want->shiny != is_shiny(pid, ot) ||
      (twoAbilities && int(pid & 1) != want->ability) ||
      (want->species == SPECIES_UNOWN) != (now.species == SPECIES_UNOWN)) {
    if (!new_personality(g, want->species, pid, ot, want->nature, want->shiny, want->ability, &pid))
      return say(err, n, "Couldn't find a personality with that nature"), false;
  }
  int stats[6] = {0};
  if (box == SE_PARTY) {
    for (int s = 0; s < 6; s++) {
      int v = (2 * b[s] + want->ivs[s] + want->evs[s] / 4) * want->level / 100;
      if (s == 0)
        stats[0] = want->species == SPECIES_SHEDINJA ? 1 : v + want->level + 10;
      else {
        v += 5;
        if (s == nature_up(want->nature))
          v = v * 110 / 100;
        if (s == nature_down(want->nature))
          v = v * 90 / 100;
        stats[s] = v;
      }
    }
  }

  wr32(m.head, pid);
  wr16(m.A(), unsigned(want->species));
  wr16(m.A() + 2, unsigned(want->held));
  if (want->level != now.level || want->species != now.species || level_for(rate, rd32(m.A() + 8)) != want->level)
    wr32(m.A() + 8, uint32_t(pd_exp_at(rate, want->level)));
  m.A()[13] = want->ability ? b[23] : b[22];
  for (int i = 0; i < 6; i++)
    m.A()[16 + i] = uint8_t(want->evs[i]);
  for (int i = 0; i < 4; i++) {
    int mv = want->moves[i];
    if (mv != rd16(m.B() + 2 * i)) {
      wr16(m.B() + 2 * i, unsigned(mv));
      m.B()[8 + i] = mv ? g->movePp[size_t(mv)] : 0;
      m.B()[12 + i] = 0;
    }
  }
  uint32_t iv = rd32(m.B() + 16) & (1u << 30);
  for (int i = 0; i < 6; i++)
    iv |= uint32_t(want->ivs[i]) << (5 * i);
  if (nicknamed)
    iv |= 1u << 31;
  wr32(m.B() + 16, iv);
  // Gender (from the personality, as the game works it out) and the form: a new species starts on its first form.
  int ratio = b[16];
  int gender = ratio == 255 ? 4 : ratio == 254 || (ratio && (pid & 0xFF) < uint32_t(ratio)) ? 2 : 0;
  if (want->species != now.species)
    form = want->species == SPECIES_UNOWN ? unown_letter(pid) : 0;
  m.B()[24] = uint8_t((m.B()[24] & 1) | gender | form << 3);
  if (rename)
    std::memcpy(m.C(), nick, 22);
  if (box == SE_PARTY) {
    int oldMax = rd16(m.party + 8), oldHp = rd16(m.party + 6);
    m.party[4] = uint8_t(want->level);
    for (int s = 0; s < 6; s++)
      wr16(m.party + 8 + 2 * s, unsigned(stats[s]));
    int hp = oldMax ? int(long(oldHp) * stats[0] / oldMax) : stats[0];
    if (oldHp && !hp)
      hp = 1;
    wr16(m.party + 6, unsigned(hp > stats[0] ? stats[0] : hp));
  }
  encode(m, r, box == SE_PARTY);
  e->dirty = true;
  return true;
}

int heal(save_edit *e) {
  G4 *g = G(e);
  uint32_t count = rd32(g->gen.data() + PARTY_COUNT);
  int healed = 0;
  for (uint32_t i = 0; i < count && i < 6; i++) {
    uint8_t *r = g->gen.data() + PARTY + i * PARTY_MON;
    Mon m = decode(r, true);
    if (!m.ok || !rd16(m.A()) || ((rd32(m.B() + 16) >> 30) & 1))
      continue;
    bool changed = rd32(m.party) || rd16(m.party + 6) != rd16(m.party + 8);
    for (int k = 0; k < 4; k++) {
      int mv = rd16(m.B() + 2 * k);
      if (mv < 1 || size_t(mv) >= g->movePp.size())
        continue;
      int base = g->movePp[size_t(mv)], full = base + base * m.B()[12 + k] / 5;
      if (m.B()[8 + k] != full)
        m.B()[8 + k] = uint8_t(full), changed = true;
    }
    if (!changed)
      continue;
    wr32(m.party, 0);
    wr16(m.party + 6, rd16(m.party + 8));
    encode(m, r, true);
    healed++;
  }
  if (healed)
    e->dirty = true;
  return healed;
}

const char *title(save_edit *e) { return G(e)->title.c_str(); }
long money(save_edit *e) { return long(rd32(G(e)->gen.data() + MONEY)); }
long money_max(save_edit *) { return 999999; }
bool set_money(save_edit *e, long v, char *err, size_t n) {
  if (v < 0 || v > 999999)
    return say(err, n, "Money goes from 0 to 999999"), false;
  wr32(G(e)->gen.data() + MONEY, uint32_t(v));
  e->dirty = true;
  return true;
}
long coins(save_edit *) { return -1; }
bool set_coins(save_edit *, long, char *err, size_t n) {
  return say(err, n, "Coins aren't covered for this game"), false;
}
int pockets(save_edit *) { return POCKETS; }
const char *pocket_name(save_edit *, int p) { return p >= 0 && p < POCKETS ? POCKET_NAME[p] : ""; }
int pocket_slots(save_edit *, int p) { return p >= 0 && p < POCKETS ? pd_ds_data::johtoPockets[p] : 0; }
uint8_t *pocket_base(G4 *g, int p) {
  size_t off = BAG;
  for (int i = 0; i < p; i++)
    off += size_t(pd_ds_data::johtoPockets[i]) * 4;
  return g->gen.data() + off;
}
bool pocket_item(save_edit *e, int p, int i, se_item *out) {
  if (p < 0 || p >= POCKETS || i < 0 || i >= pd_ds_data::johtoPockets[p])
    return false;
  const uint8_t *s = pocket_base(G(e), p) + i * 4;
  out->id = rd16(s);
  out->count = out->id ? rd16(s + 2) : 0;
  return true;
}
const char *pocket_locked(save_edit *, int p) {
  return p == P_KEY ? "Key items are the story's: the editor leaves them be" : nullptr;
}
int item_max_count(save_edit *, int p, int id) {
  if (p == P_TMHM)
    return id >= FIRST_HM && id <= LAST_HM ? 1 : 99;
  return 999;
}
bool set_item(save_edit *e, int p, int i, int id, int count, char *err, size_t n) {
  if (p < 0 || p >= POCKETS || i < 0 || i >= pd_ds_data::johtoPockets[p])
    return say(err, n, "There's no such bag slot"), false;
  if (pocket_locked(e, p))
    return say(err, n, pocket_locked(e, p)), false;
  se_item old;
  pocket_item(e, p, i, &old);
  if (old.id >= FIRST_HM && old.id <= LAST_HM && p == P_TMHM)
    return say(err, n, "HMs are the story's: the editor leaves them be"), false;
  uint8_t *base = pocket_base(G(e), p);
  const int slots = pd_ds_data::johtoPockets[p];
  if (!count || !id) {
    if (!old.id)
      return true;
    std::memmove(base + i * 4, base + (i + 1) * 4, size_t(slots - i - 1) * 4);
    wr32(base + (slots - 1) * 4, 0);
    e->dirty = true;
    return true;
  }
  if (!old.id || id != old.id)
    return say(err, n, "In DS games the editor changes how many you have, or takes an item out; it doesn't add new ones"),
           false;
  if (count < 1 || count > item_max_count(e, p, id)) {
    std::snprintf(err, n, "One stack holds 1 to %d", item_max_count(e, p, id));
    return false;
  }
  wr16(base + i * 4 + 2, unsigned(count));
  e->dirty = true;
  return true;
}
int boxes(save_edit *) { return BOXES; }
int box_slots(save_edit *) { return BOX_SLOTS; }

void finish(Bytes &out, size_t base, size_t size, const Bytes &block) {
  std::memcpy(out.data() + base, block.data(), size - 16);
  wr16(out.data() + base + size - 2, crc16(out.data() + base, size - 16));
}
bool verify(save_edit *e, const uint8_t *b, size_t size, char *err, size_t n) {
  G4 *g = G(e);
  if (size != g->raw.size())
    return say(err, n, "The save's size changed"), false;
  if (active(b, size, 0, GENERAL_SIZE) != g->general || active(b, size, STORAGE_OFF, STORAGE_SIZE) != g->storage)
    return say(err, n, "The game wouldn't load the changed save"), false;
  if (std::memcmp(b + g->general, g->gen.data(), GENERAL_SIZE - 16) ||
      std::memcmp(b + g->storage, g->sto.data(), STORAGE_SIZE - 16))
    return say(err, n, "The save read back differently"), false;
  for (size_t i = 0; i < size; i++) {
    bool inGeneral = i >= g->general && i < g->general + GENERAL_SIZE;
    bool inStorage = i >= g->storage && i < g->storage + STORAGE_SIZE;
    if (!inGeneral && !inStorage && b[i] != g->raw[i])
      return say(err, n, "A byte outside the game's save blocks changed"), false;
    // Inside them only the data and the CRC may change: the save numbers, sizes and magic stay.
    size_t at = inGeneral ? i - g->general : inStorage ? i - g->storage : 0;
    size_t blockSize = inGeneral ? GENERAL_SIZE : STORAGE_SIZE;
    if ((inGeneral || inStorage) && at >= blockSize - 16 && at < blockSize - 2 && b[i] != g->raw[i])
      return say(err, n, "The save's footer changed"), false;
  }
  return true;
}
bool build(save_edit *e, uint8_t **out, size_t *size, char *err, size_t n) {
  G4 *g = G(e);
  Bytes bytes = g->raw;
  finish(bytes, g->general, GENERAL_SIZE, g->gen);
  finish(bytes, g->storage, STORAGE_SIZE, g->sto);
  if (!verify(e, bytes.data(), bytes.size(), err, n))
    return false;
  *out = static_cast<uint8_t *>(std::malloc(bytes.size()));
  if (!*out)
    return say(err, n, "Out of memory"), false;
  std::memcpy(*out, bytes.data(), bytes.size());
  *size = bytes.size();
  return true;
}
void free_edit(save_edit *e) {
  delete G(e);
  delete e;
}

const se_ops G4_OPS = {
    title,         money,      money_max,    set_money,    coins,     set_coins,  pockets,
    pocket_name,   pocket_slots, pocket_item, pocket_locked, set_item, item_max_count, boxes,
    box_slots,     get_mon,    set_mon,      heal,         species_name, species_max, move_name,
    move_max,      item_name,  item_max,     abilities,    build,     verify,     free_edit,
};
} // namespace

extern "C" struct save_edit *se_open_gen4(const uint8_t *save, size_t saveSize, const uint8_t *rom, size_t romSize,
                                          char *err, size_t errSize) {
  if (romSize < 0x200)
    return say(err, errSize, "This isn't a DS game the editor knows"), nullptr;
  char code[5] = {};
  std::memcpy(code, rom + 0x0C, 4);
  const bool johto = !std::strncmp(code, "IPK", 3) || !std::strncmp(code, "IPG", 3);
  if (!johto)
    return say(err, errSize,
               !std::strncmp(code, "CPU", 3) ? "Platinum saves aren't covered yet"
                                              : "The save editor covers HeartGold / SoulSilver among the DS games so far"),
           nullptr;
  if (code[3] != 'E')
    return say(err, errSize, "Only English HeartGold / SoulSilver saves are covered so far"), nullptr;
  if (saveSize < 2 * COPY)
    return say(err, errSize, "This save is too small to be a HeartGold / SoulSilver save"), nullptr;
  auto g = std::make_unique<G4>();
  g->raw.assign(save, save + saveSize);
  g->general = active(save, saveSize, 0, GENERAL_SIZE);
  g->storage = active(save, saveSize, STORAGE_OFF, STORAGE_SIZE);
  if (g->general == SIZE_MAX || g->storage == SIZE_MAX)
    return say(err, errSize, "The save doesn't check out, so the editor won't change it"), nullptr;
  if (block_number(save, saveSize, g->general, GENERAL_SIZE) != block_number(save, saveSize, g->storage, STORAGE_SIZE))
    return say(err, errSize, "The save's blocks are from different saves: save in the game once, then try again"),
           nullptr;
  g->gen.assign(save + g->general, save + g->general + GENERAL_SIZE);
  g->sto.assign(save + g->storage, save + g->storage + STORAGE_SIZE);
  if (rd32(g->gen.data() + PARTY_COUNT) > 6)
    return say(err, errSize, "The save's party doesn't read right, so the editor won't change it"), nullptr;
  try {
    Bytes cart(rom, rom + romSize);
    Bytes personal = rom_file(cart, "/a/0/0/2"), moves = rom_file(cart, "/a/0/1/1");
    g->personal.resize(SPECIES_MAX + 1);
    for (int s = 1; s <= SPECIES_MAX; s++) {
      Bytes rec = narc_member(personal, unsigned(s));
      if (rec.size() < 28)
        throw std::runtime_error("personal");
      std::memcpy(g->personal[size_t(s)].data(), rec.data(), 28);
    }
    g->movePp.assign(MOVE_MAX + 1, 0);
    for (unsigned mv = 1; mv <= MOVE_MAX && mv < narc_count(moves); mv++) {
      Bytes rec = narc_member(moves, mv);
      if (rec.size() >= 7)
        g->movePp[mv] = rec[6];
    }
    g->abilityNames = bank_lines(narc_member(rom_file(cart, "/a/0/2/7"), 720));
  } catch (const std::exception &) {
    return say(err, errSize, "This cart's tables couldn't be read, so the editor won't change its save"), nullptr;
  }
  // Bulbasaur's record and the first ability's name prove the tables are the ones read here.
  if (g->personal[1][0] != 45 || g->personal[1][22] != 65 || g->abilityNames.size() < 124 ||
      g->abilityNames[65] != "Overgrow" || g->movePp[33] != 35)
    return say(err, errSize, "This cart's tables aren't the ones the editor knows"), nullptr;
  g->title = code[2] == 'K' ? "POKéMON HEARTGOLD" : "POKéMON SOULSILVER";
  auto *e = new save_edit{};
  e->ops = &G4_OPS;
  e->impl = g.release();
  std::free(const_cast<uint8_t *>(rom)); // owned on success (se_open's contract); the tables are copied out
  return e;
}
