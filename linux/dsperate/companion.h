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
// Platinum (USA) rev 1 by its header (CPUE, 0x1E = 1), as the Android app tells it: any dump of it (trimmed, decrypted,
// re-signed) and Renegade Platinum, which keeps the retail RAM layout. Rev 0 isn't mapped.
bool ds_platinum_cart(const uint8_t* rom,size_t size);
// Renegade Platinum (v1.2.0 / v1.3.0 / Classic): overlay 16 points its type chart into overlay 21 (RenegadePlatinum.kt).
bool ds_renegade_platinum(const uint8_t* rom,size_t size);
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
