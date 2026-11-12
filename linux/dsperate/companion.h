// GPL-3.0. Live Gen 4/5 reader adapted from the Android companion.
#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <string>

enum class DsGame { Unsupported, Platinum, Johto, Black, White, Black2, White2 };
bool ds_unova(DsGame game);
bool ds_unova_sequel(DsGame game);
DsGame ds_companion_game(const char* sha1);
// Maps the digest of a known decrypted dump to the digest of the retail dump it
// is the same ROM as, so every digest check downstream sees one id per game.
const char* ds_canonical_sha1(const char* sha1);
int ds_egg_icon(DsGame game);
// Platinum (USA) by its header (CPUE): rev 1, as the Android app tells it (any dump of it, trimmed, decrypted or
// re-signed, and Renegade Platinum, which keeps the retail RAM layout), and rev 0, whose save, party, field and battle
// structs sit where rev 1's do (checked on a rev 0 save and a battle state: party, bag, map, battle phases).
bool ds_platinum_cart(const uint8_t* rom,size_t size);
// Renegade Platinum (v1.2.0 / v1.3.0 / Classic): overlay 16 points its type chart into overlay 21 (RenegadePlatinum.kt).
bool ds_renegade_platinum(const uint8_t* rom,size_t size);
// Origin HeartGold (v4.0.3, English 1.0.0): a Japanese HeartGold's header (IPKJ rev 0), told apart by SaveData_Get's
// pool word in its ARM9 (OriginHeartGold.kt). It runs as DsGame::Johto on its own save offsets and, once ds_set_origin
// hands them over, its own tables read from the cart (ds_read_origin, map_art.h).
bool ds_origin_cart(const uint8_t* rom,size_t size);
constexpr int dsOriginLastNational=1025, dsOriginLastSpecies=1440, dsOriginEgg=1441;
struct DsOrigin {
  std::vector<std::string> species,moves,items;      // the cart's names by id (species to 1025)
  std::vector<std::array<uint8_t,2>> speciesTypes;   // by species / form id, 1..1440 (Fairy is type 9)
  std::vector<std::array<uint8_t,2>> moveMeta;       // by move id: type, power
  std::array<uint8_t,18*18> chart{};                 // attacker by defender, in quarters (4 = 1x)
  std::vector<std::array<uint16_t,3>> forms;         // species, its form's own species id, form
  std::array<int,138> machines{};                    // TM01..TM92, HM01..HM08, TM93..TM130
  std::vector<uint8_t> iconPalettes;                 // Renegade Platinum: each species' icon palette (empty: the game's own)
  int formSpecies(int species,int form) const;       // a form's own id (species when form 0 or unknown)
  int baseSpecies(int id) const;                     // the National Dex species of a form id
};
// The running Origin HeartGold's tables, or nullptr (any other game). The data outlives the companion.
void ds_set_origin(const DsOrigin* data);
const DsOrigin* ds_origin();
// The running Renegade Platinum's tables (RenegadePlatinum.kt: names, types, moves, Fairy's chart, TMs, icon palettes
// from its cart), or nullptr. Platinum's reader and RAM layout stay as they are.
void ds_set_renegade(const DsOrigin* data);
const DsOrigin* ds_renegade();
// What the cart is, by its header, for messages: "Platinum (USA) rev 0", "Black 2 (USA / Europe)"...
std::string ds_cart_label(const uint8_t* rom,size_t size);
struct DsMon {
  int species=0, level=0, hp=0, maxHp=0, heldItem=0;
  uint32_t personality=0, experience=0, status=0;
  bool egg=false;
  int gender=0, nature=0, friendship=0;
  // Live max HP, Attack, Defense, Speed, Sp. Atk, Sp. Def, in cartridge order.
  std::array<int,6> stats{},ivs{},evs{};
  std::array<int,4> moves{}, pp{};
  std::string nickname;
};
struct DsItem { int id, count, pocket; };
// A HeartGold / SoulSilver Berry Pot (SAVE_MISC_DATA's berry_pots, pret/pokeheartgold include/save_misc_data.h).
struct DsBerryPot {
  int item=0;     // the berry's item id, 0 = empty
  int stage=0;    // 1 planted, 2 sprouted, 3 growing, 4 blooming, 5 berries
  int minutes=0;  // left in this stage (at the berries: until they fall)
  int moisture=0; // 0 dry .. 100
};
struct DsCompanion {
  DsGame game=DsGame::Platinum;
  bool valid=false, dexValid=false, nationalDex=false;
  std::vector<uint8_t> dexSeen, dexCaught;
  std::vector<uint8_t> eventFlags;
  int seenCount=0, caughtCount=0;
  int mapX=-1,mapY=-1,mapSkin=0,mapUnlock=0,playerGender=0;
  uint64_t arrivals=0;
  int repelSteps=0, money=0, badges=0, map=0;
  std::string trainer;
  std::vector<DsMon> party;
  std::vector<DsItem> bag;
  bool hasPots=false; // HG / SS: the Berry Pots key item is in the bag
  DsBerryPot pots[4];
};
bool ds_event_flag(const DsCompanion&,unsigned flag);
// The game's time of day for an hour of the DS clock: HG / SS MORNING 4-9, DAY 10-19, NIGHT 20-3; Platinum adds
// EVENING 17-19. Black / White and their sequels go by season, so nothing there.
const char* ds_day_period(DsGame game,int hour);
// "GROWING, 3H 20M" and the like for a pot, "" when it's empty.
std::string ds_berry_pot_text(const DsBerryPot& pot);
int ds_unova_month(const uint8_t* ram,size_t size,DsGame game);
bool ds_decode_mon(const uint8_t* raw, size_t size, DsMon& mon, DsGame game=DsGame::Platinum);
DsCompanion ds_read_companion(const uint8_t* ram, size_t size,DsGame game=DsGame::Platinum);
const char* ds_species(int id,DsGame game=DsGame::Platinum);
const char* ds_move(int id,DsGame game=DsGame::Platinum);
const char* ds_item(int id,DsGame game=DsGame::Platinum);
const char* ds_place(int id,DsGame game=DsGame::Platinum);

const char* ds_nature(int id);
const char* ds_status(const DsMon& mon);

int ds_move_type(int id,DsGame game=DsGame::Platinum);
int ds_move_power(int id,DsGame game=DsGame::Platinum);
int ds_species_type(int id,int slot,DsGame game=DsGame::Platinum);

enum class DsRepelResult { Used, NotAvailable, StillActive, NoneLeft };
DsRepelResult ds_use_repel(const uint8_t* ram,size_t size,DsGame game,int item,
                         void (*write8)(uint32_t,uint8_t));
