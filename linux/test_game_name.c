/* The game list's names (game_display_name): file names only, no game data. */
#include "boxart.h"
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

static void check_pc(const char *title, const char *want) {
  char out[256];
  game_pc_display_name(title, out, sizeof out);
  if (strcmp(out, want)) {
    fprintf(stderr, "PC %s -> \"%s\", want \"%s\"\n", title, out, want);
    assert(0);
  }
}

int main(void) {
  // PC fan games: "Pokémon" first, however the game wrote its title.
  // Every way "Pokemon" gets written ends up "Pok\xC3\xA9mon <Title>", never twice.
  static const struct {
    const char *file;
    bool pokemon;
    const char *want;
  } VARIANTS[] = {
      {"Pokemon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pok" E "mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"POKEMON FIRE ASH.gba", false, "Pok" E "mon Fire Ash"},
      {"pokemon fire ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pok" E "Mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"POK\xC3\x89MON Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Poke'mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Poke\xE2\x80\x99mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pok" E " mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Poke mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon- Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon-Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon_Fire_Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon - Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon:Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon.Fire.Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon   Fire    Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"PKMN Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Pkmn Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"pkmn fire ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Poke\xCC\x81mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},          // e and a combining accent
      {"Pok\xC3\x83\xC2\xA9mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},   // read as Latin-1
      {"Pok\xE9mon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},                 // stored as Latin-1
      {"The Pokemon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Fire Ash, Pokemon.gba", false, "Pok" E "mon Fire Ash"},
      {"Fire Ash Pokemon.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon Fire Ash Version.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon Fire Ash Ver. 1.2.gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon Fire Ash (U) [!].gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon Fire Ash (USA) (Rev 1).gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon Fire Ash (V1.1).gba", false, "Pok" E "mon Fire Ash"},
      {"Pokemon Pokemon Fire Ash.gba", false, "Pok" E "mon Fire Ash"},
      {"Fire Ash.gba", true, "Pok" E "mon Fire Ash"},                              // a Pokemon cart by its header
      {"Pocket Monsters Green (Japan).gb", true, "Pocket Monsters Green"},         // kept, tidied
      {"POCKET MONSTERS - GOLD.gbc", true, "Pocket Monsters Gold"},
      {"Dragonsden.gba", false, "Dragonsden"},                                       // not a Pokemon title by name
      {"Monomyth.gba", false, "Monomyth"},
      {"Pokemon's Day Out.gba", false, "Pokemon's Day Out"},                         // not the word on its own
  };
  for (size_t i = 0; i < sizeof VARIANTS / sizeof *VARIANTS; i++)
    check(VARIANTS[i].file, VARIANTS[i].pokemon, VARIANTS[i].want);
  check_pc("PKMN Fire Ash", "Pok" E "mon Fire Ash");
  check_pc("The Pokemon Fire Ash", "Pok" E "mon Fire Ash");
  check_pc("Fire Ash", "Pok" E "mon Fire Ash");
  check_pc("Pokemon Africanvs", "Pok" E "mon Africanvs");
  check_pc("POKEMON DESOLATION", "Pok" E "mon Desolation");
  check_pc("Pokemon: Eon Guardians", "Pok" E "mon Eon Guardians");
  check_pc("Pok" E "mon Insurgence 1.2.7", "Pok" E "mon Insurgence");
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
  // The grid's version line and ORIGINAL / ROMHACK tag.
  {
    static const char *const V[][2] = {
        {"Pokemon - Ash Gray (v4.5.3).gba", "v4.5.3"},
        {"pokemon wishes of tomorrow 1.2.6.gba", "v1.2.6"},
        {"Pokemon - FireRed Version (USA, Europe) (Rev 1).gba", "Rev 1"},
        {"Pokemon - Emerald Version (USA, Europe).gba", ""},
        {"Pokemon R.O.W.E. (V2.0).gba", "v2.0"},
    };
    for (size_t i = 0; i < sizeof V / sizeof *V; i++) {
      char out[64];
      game_version(V[i][0], out, sizeof out);
      if (strcmp(out, V[i][1])) {
        fprintf(stderr, "version %s -> \"%s\", want \"%s\"\n", V[i][0], out, V[i][1]);
        assert(0);
      }
    }
    assert(game_is_original("Pokemon - FireRed Version (USA, Europe) (Rev 1).gba"));
    assert(game_is_original("Pokemon - SoulSilver Version (USA).nds"));
    assert(!game_is_original("Pokemon - Ash Gray (v4.5.3).gba"));
    assert(!game_is_original("Pokemon Emerald (USA) (Hack).gba"));
    assert(!game_is_original("Pokemon Unbound (v2.1.1.1).gba"));
    assert(game_is_original("pokemon firered.gba"));
    assert(game_is_original("Pokemon Black 2.nds"));
    assert(!game_is_original("pokemon radical red v4.1.gba"));
    assert(!game_is_original("Pokemon Emerald Rogue.gba"));
  }
  {
    // SteamGridDB's entry must be the same game: no sequels, no spin-offs.
    assert(boxart_title_matches("Pokemon Black", "Pok\xC3\xA9mon Black Version"));
    assert(!boxart_title_matches("Pokemon Black", "Pok\xC3\xA9mon Black Version 2"));
    assert(!boxart_title_matches("Pok\xC3\xA9mon HeartFelt Gold", "Pok\xC3\xA9mon Pinball"));
    assert(!boxart_title_matches("Pokemon HeartFelt Gold", "Pokemon HeartGold Version"));
    assert(boxart_title_matches("Pokemon Radical Red", "Pok\xC3\xA9mon Radical Red"));
    assert(!boxart_title_matches("Pokemon", "Pokemon Pinball"));
    // SteamGridDB's own names for the hacks: "Pokémon" in its JSON, unescaped first.
    {
      char name[64];
      assert(boxart_json_string("{\"id\":5267499,\"name\":\"Pok\\u00e9mon Glazed\"}", "name", name, sizeof name));
      assert(!strcmp(name, "Pok\xC3\xA9mon Glazed"));
      assert(boxart_title_matches("Glazed", name));
    }
    assert(boxart_title_matches("Ash Gray", "Pok\xC3\xA9mon AshGray"));
    assert(boxart_title_matches("Heart and Soul", "Pok\xC3\xA9mon Heart & Soul"));
    assert(boxart_title_matches("FireRed Rocket Edition", "Pok\xC3\xA9mon FireRed: Rocket Edition"));
    assert(boxart_title_matches("Gaia", "Pok\xC3\xA9mon Gaia Version"));
    assert(boxart_title_matches("Amethyst", "Pok\xC3\xA9mon Amethyst"));
    assert(boxart_title_matches("Lazarus", "Pok\xC3\xA9mon Lazarus"));
    assert(boxart_title_matches("Dreamstone Mysteries", "Pok\xC3\xA9mon Dreamstone Mysteries"));
    assert(boxart_title_matches("Odyssey", "Pok\xC3\xA9mon Odyssey"));
    assert(boxart_title_matches("Unbound", "Pok\xC3\xA9mon Unbound"));
    // ... and still never another game.
    assert(!boxart_title_matches("HeartFelt Gold", "Pok\xC3\xA9mon Pinball"));
    assert(!boxart_title_matches("HeartFelt Gold", "Pok\xC3\xA9mon HeartGold Version"));
    assert(!boxart_title_matches("Black", "Pok\xC3\xA9mon Black Version 2"));
    assert(!boxart_title_matches("FireRed Rocket Edition", "Pok\xC3\xA9mon FireRed Version"));
    assert(!boxart_title_matches("Pinball", "Pok\xC3\xA9mon Pinball: Generations"));
    // BOX ART's picker: a SteamGridDB grids page, each grid's own fields (its author's object in between).
    {
      static const char page[] =
          "{\"success\":true,\"page\":0,\"total\":2,\"limit\":50,\"data\":["
          "{\"id\":11,\"score\":0,\"style\":\"alternate\",\"width\":600,\"height\":900,\"nsfw\":false,"
          "\"mime\":\"image/png\",\"language\":\"en\",\"url\":\"https:\\/\\/cdn2.steamgriddb.com\\/grid\\/a.png\","
          "\"thumb\":\"https:\\/\\/cdn2.steamgriddb.com\\/thumb\\/a.jpg\",\"author\":{\"name\":\"x\",\"steam64\":\"1\"}},"
          "{\"id\":12,\"width\":920,\"height\":430,\"language\":\"en\",\"url\":\"https://cdn2.steamgriddb.com/grid/b.jpg\","
          "\"thumb\":\"https://cdn2.steamgriddb.com/thumb/b.jpg\",\"author\":{\"name\":\"y\"}}]}";
      struct boxart_grid grids[4];
      assert(boxart_parse_grids(page, grids, 4) == 2);
      assert(!strcmp(grids[0].url, "https://cdn2.steamgriddb.com/grid/a.png"));
      assert(!strcmp(grids[0].thumb, "https://cdn2.steamgriddb.com/thumb/a.jpg"));
      assert(grids[0].w == 600 && grids[0].h == 900);
      assert(grids[1].w == 920 && grids[1].h == 430);
      assert(boxart_parse_grids("{\"success\":true,\"data\":[]}", grids, 4) == 0);
    }
  }
  puts("game names ok");
  return 0;
}
