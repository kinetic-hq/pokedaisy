/* The game list's names (game_display_name): file names only, no game data. */
#include "game_name.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define E "\xC3\xA9"

static void check(const char *file, bool pokemon, const char *want) {
  char out[256];
  game_display_name(file, pokemon, out, sizeof out);
  if (strcmp(out, want)) {
    fprintf(stderr, "%s -> \"%s\", want \"%s\"\n", file, out, want);
    assert(0);
  }
}

int main(void) {
  check("pokemon firered.gba", false, "Pok" E "mon FireRed");
  check("pokemon - ash gray.gba", false, "Pok" E "mon Ash Gray");
  check("Pokemon - FireRed Version (USA, Europe) (Rev 1).gba", false, "Pok" E "mon FireRed");
  check("1636 - Pokemon Emerald (U)(TrashMan).gba", false, "Pok" E "mon Emerald");
  check("POKEMON_LEAFGREEN_[!].gba", false, "Pok" E "mon LeafGreen");
  check("Pokemon Unbound (v2.1.1.1).gba", false, "Pok" E "mon Unbound");
  check("pokemon radical red v4.1.gba", false, "Pok" E "mon Radical Red");
  check("Pokemon - HeartGold Version (USA).nds", false, "Pok" E "mon HeartGold");
  check("pokemon soulsilver (E).nds", false, "Pok" E "mon SoulSilver");
  check("Pokemon - Black 2 (USA, Europe) (NDSi Enhanced).nds", false, "Pok" E "mon Black 2");
  check("Pok" E "mon White 2.nds", false, "Pok" E "mon White 2");
  check("POK" E "MON Platinum.nds", false, "Pok" E "mon Platinum");
  check("Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced).gbc", false,
        "Pok" E "mon Yellow Special Pikachu Edition");
  check("Pokemon Polished Crystal (v3.2.3) (Faithful).gbc", false, "Pok" E "mon Polished Crystal");
  check("pokemon wishes of tomorrow 1.2.6.gba", false, "Pok" E "mon Wishes of Tomorrow");
  check("Pokemon Sword and Shield Ultimate Plus.gba", false, "Pok" E "mon Sword and Shield Ultimate Plus");
  check("pokemon gs chronicles.gba", false, "Pok" E "mon GS Chronicles");
  check("Pokemon R.O.W.E. (v2.0).gba", false, "Pok" E "mon R.O.W.E.");
  check("pokemon purered.gbc", false, "Pok" E "mon PureRed");
  check("Pokemon Let's Go Pikachu.gba", false, "Pok" E "mon Let's Go Pikachu");
  // No "Pokemon" in the file name: the header says so.
  check("firered.gba", true, "Pok" E "mon FireRed");
  check("Emerald_Rogue.gba", true, "Pok" E "mon Emerald Rogue");
  // Not a Pokemon game: no prefix, still tidied.
  check("my homebrew (v0.3).gba", false, "My Homebrew");
  check("A hack.GBA", false, "A Hack");
  check("Zelda - The Minish Cap (USA).gba", false, "Zelda The Minish Cap");
  // A name already in mixed case keeps it.
  check("Pokemon McGuffin Quest.gba", false, "Pok" E "mon McGuffin Quest");
  // Nothing left: the file's own name.
  check("(USA).gba", false, "(USA).gba");
  puts("game names ok");
  return 0;
}
