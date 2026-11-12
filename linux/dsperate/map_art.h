// GPL-3.0. DS map assets decoded from the owner's cartridge, as NdsArt.kt does.
#pragma once
#include <cstdint>
#include <vector>
#include "companion.h"
struct DsMapArt {
  std::vector<uint32_t> background,base;
  std::vector<std::vector<uint32_t>> blocks,heads;
  std::vector<uint32_t> cursor;
  std::vector<uint8_t> tiles,screen;
  std::vector<std::vector<uint32_t>> skins;
  int width=0,height=0,skin=-1,unlock=-1;
  uint64_t arrivals=0;
};
DsMapArt ds_map_art(const std::vector<uint8_t>& rom,DsGame game=DsGame::Platinum);
void ds_map_update(DsMapArt&,const DsCompanion&);
int ds_map_section(int x,int y);
const char* ds_map_name(int section);

// First 32x32 frame per national species; zero pixels are transparent.
std::vector<std::vector<uint32_t>> ds_party_icons(const std::vector<uint8_t>& rom,DsGame game=DsGame::Platinum);

// Dex metadata and the compressed archives. Decode art and moves only for the open entry.
struct DsDexDetails {
  std::array<uint8_t,6> stats{}; // cartridge order: HP, ATK, DEF, SPE, SPA, SPD
  uint32_t heightDm=0,weightHg=0;
  int growth=-1;
  std::string category,description;
  std::array<uint8_t,13> machineBits{};
  bool hasMachines=false;
};
struct DsDexMove { int level,move; };
struct DsDexMachine { bool hm; int number,move; };
struct DsDexPage {
  std::vector<uint32_t> sprite;
  std::vector<DsDexMove> moves;
  std::vector<DsDexMachine> machines;
  int side=0;
};
struct DsDexEvolution { int from,to,method,param; };
struct DsDexArt {
  std::vector<DsDexDetails> details;
  std::vector<int> regional; // species by displayed regional number (Gen 5 starts at zero)
  std::vector<uint8_t> pictures,learnsets;
  std::vector<uint8_t> tutors;  // Origin HeartGold: /data/tutor_moves.narc (its machines' fit rule, ds_dex_page)
  std::vector<uint8_t> machineBits;  // HeartFelt Gold: hg-engine's machine learnsets, a bit per machine, by species id
  std::vector<int> machineMoves;     // HeartFelt Gold: each machine's move, in hg-engine's machine order
  std::vector<DsDexEvolution> evolutions;
};
DsDexArt ds_dex_art(const std::vector<uint8_t>&,DsGame);
DsDexPage ds_dex_page(const DsDexArt&,DsGame,int species);
std::vector<DsDexMove> ds_level_moves(const DsDexArt&,DsGame,int species);
std::vector<DsDexEvolution> ds_dex_evolutions(const DsDexArt&,int species);

// Origin HeartGold's tables from its cart (OriginHeartGold.kt / NdsArt.readOrigin); species empty if it can't be read.
DsOrigin ds_read_origin(const std::vector<uint8_t>& rom);
// Renegade Platinum's tables from its cart (NdsArt.readRenegade); empty species on failure.
DsOrigin ds_read_renegade(const std::vector<uint8_t>& rom);
// HeartFelt Gold's names, types, moves, Fairy's chart and icon palettes from its cart (empty species on failure).
DsOrigin ds_read_heartfelt(const std::vector<uint8_t>& rom);

std::vector<std::vector<uint32_t>> ds_hud_item_icons(const std::vector<uint8_t>& rom, DsGame game);
