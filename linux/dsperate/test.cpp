// GPL-3.0. Real core and captured-RAM tests, never fake application screens.
#include "ds_core.h"
#include <algorithm>
#if __has_include("core/nds.h")
#include "core/nds.h"
#define PD_TEST_DSPERATE 1
#endif
#include "companion.h"
#include "battle_view.h"
#include "pk_nds_field.h"
#include <filesystem>
#include "map_art.h"
#include "guide_view.h"
#include <cassert>
#include <fstream>
#include <cstdio>
#include <cstring>
extern "C" {
#include "../hud.h"
#include "../quick_menu.h"
}
// The BATTLE tab's reader (linux/libretro-ds/battle_strip.cpp), as frontend.cpp declares it.
void ds_read_battle_companion(const PkNdsFieldGame*,uint32_t (*read32)(uint32_t),const uint8_t* ram,size_t ramSize,
                              const DsCompanion&,pd_snapshot&);
static std::vector<uint8_t> battleRam;
static uint32_t battle_read32(uint32_t addr){
  if(addr<0x02000000 || addr>0x023ffffc)return 0;
  uint32_t value;std::memcpy(&value,battleRam.data()+addr-0x02000000,4);return value;
}
static std::vector<uint8_t> read_file_bytes(const std::string& path){
  std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char** argv){
  {
    // Which carts the companion reads, and what it calls the rest (headers only).
    std::vector<uint8_t> rom(0x221064,0);
    auto code=[&](const char* c,int rev){std::memcpy(rom.data()+0x0C,c,4);rom[0x1E]=uint8_t(rev);};
    code("CPUE",1);assert(ds_platinum_cart(rom.data(),rom.size()));assert(!ds_renegade_platinum(rom.data(),rom.size()));
    assert(ds_cart_label(rom.data(),rom.size())=="Platinum (USA) rev 1 (CPUE)");
    const uint8_t sig[4]={0x81,0x9F,0x1E,0x02};std::memcpy(rom.data()+0x221060,sig,4);
    assert(ds_renegade_platinum(rom.data(),rom.size()));assert(ds_cart_label(rom.data(),rom.size())=="Renegade Platinum");
    code("CPUE",0);assert(ds_platinum_cart(rom.data(),rom.size()));
    code("CPUE",2);assert(!ds_platinum_cart(rom.data(),rom.size()));code("CPUE",0);
    assert(ds_cart_label(rom.data(),rom.size())=="Platinum (USA) rev 0 (CPUE)");
    code("IRDO",0);assert(ds_cart_label(rom.data(),rom.size())=="White 2 (USA / Europe) rev 0 (IRDO)");
    code("CPUF",0);assert(!ds_platinum_cart(rom.data(),rom.size()));
    code("ZZZZ",0);assert(ds_cart_label(rom.data(),rom.size())=="a cart coded ZZZZ");
  }
  assert(!ds_battle_view(0,true,true,false).bottom);
  assert(ds_battle_view(1,true,true,false).bottom);
  assert(!ds_battle_view(1,true,false,true).bottom);
  assert(ds_battle_view(1,true,false,false,1).bottom);
  assert(!ds_battle_view(1,true,true,false,0).bottom);
  assert(ds_battle_view(1,false,false,true).bottom);
  std::puts("PASS automatic screen transitions and temporary manual override");
  assert(argc==3 || argc==4);std::ifstream file(argv[1],std::ios::binary);
  std::vector<uint8_t> ram{std::istreambuf_iterator<char>(file),{}};
  assert(ram.size()==4*1024*1024);
  auto battlePath=std::filesystem::path(argv[1]).parent_path().parent_path()/"platinum_battle"/"mainram.bin";
  std::ifstream capturedBattle(battlePath,std::ios::binary);
  battleRam={std::istreambuf_iterator<char>(capturedBattle),{}};
  assert(battleRam.size()==4*1024*1024);
  const auto* game=pk_nds_field_game("CPUE",1);assert(game && game->offBattleContext==0x30 && game->offCommand==8);
  assert(pk_nds_field_state(game,battle_read32,nullptr,0)&PK_NDS_FIELD_BATTLE);
  auto fs=battle_read32(game->fieldSystemPtr),pm=battle_read32(fs+game->offProcessManager),child=battle_read32(pm+4);
  auto system=battle_read32(child+0x1c),context=battle_read32(system+game->offBattleContext);
  assert(context>=0x02000000 && context<=0x023ffff0);
  // The captured fixture omits the controller head; seed just that test field.
  uint32_t command=PK_NDS_BATTLE_SELECTING;
  std::memcpy(battleRam.data()+context-0x02000000+game->offCommand,&command,4);
  assert(pk_nds_battle_command(game,battle_read32)==pk_nds_battle_selecting(game));
  std::puts("PASS captured battle pointer chain and Platinum controller metadata");
  auto s=ds_read_companion(ram.data(),ram.size());
  assert(s.valid && s.party.size()==1);
  assert(s.party[0].species==387 && s.party[0].level==6);
  pd_snapshot hs{};hs.valid=true;hs.partyCount=1;
  hs.party[0].species=s.party[0].species;hs.party[0].level=s.party[0].level;
  hs.party[0].hp=s.party[0].hp;hs.party[0].maxHp=s.party[0].maxHp;
  std::vector<uint32_t> hpixels(341*256,QUICK_CLEAR);pd_canvas hc;pd_canvas_init(&hc,341,256,hpixels.data());
  int icons=0;
  hud_party_draw(&hc,&hs,&icons,[](pd_canvas*,void* ctx,const pd_mon* mon,int,int,bool){
    assert(mon->species==387 && mon->level==6);++*static_cast<int*>(ctx);
  });
  assert(icons==1);quick_finish(&hc);
  bool drawn=false;for(auto p:hpixels)drawn|=p!=0;assert(drawn);
  std::puts("PASS shared party HUD with captured Gen 4 data and backend asset callback");
  assert(s.dexValid && s.seenCount==2 && s.caughtCount==1);
  assert(s.dexSeen[387] && s.dexSeen[390] && s.dexCaught[387] && !s.dexCaught[390]);
  assert(s.dexSeen.size()==494);

  assert(s.mapX==3 && s.mapY==27); // Player's house: use its outside exit block.
  assert(ds_map_section(s.mapX,s.mapY)==1);
  assert(std::strcmp(ds_map_name(1),"Twinleaf Town")==0);
  assert(ds_map_section(0,0)==-1 && ds_map_section(-1,0)==-1 && ds_map_section(30,0)==-1);
  assert(ds_map_art({}).background.empty());
  assert(ds_party_icons({}).empty());
  if(argc==4){
    std::ifstream cart(argv[3],std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(cart),{}};
    auto icons=ds_party_icons(bytes);assert(icons.size()==495);
    for(int species: {1,387,412,493,494}){
      assert(icons[species].size()==1024);bool clear=false,opaque=false;
      for(auto p:icons[species]){clear|=p==0;opaque|=(p>>24)==255;}assert(clear && opaque);
    }
    std::puts("PASS real ROM party icons including Gen 4 species, Burmy and egg");
    auto art=ds_map_art(bytes);assert(art.width==216 && art.height==168);
    assert(art.background.size()==216*168 && art.blocks.size()==21 && art.heads.size()==2);
    bool varied=false;for(auto px:art.background)varied|=px!=art.background[0];assert(varied);
    std::puts("PASS real ROM Town Map tiles/palettes/cells and player heads decoded");
  }
  std::puts("PASS map grid blank/bounds and captured indoor exit-location fallback");
  bool potion=false,balls=false;
  for(auto i:s.bag){potion|=i.id==17 && i.count==3;balls|=i.id==4 && i.count==5;}
  assert(potion && balls);
  assert(std::strcmp(ds_species(387),"TURTWIG")==0);
  std::puts("PASS captured upstream Platinum RAM fixture: party, checksum/decryption and bag");
  // Truncated/corrupt input must not become a valid companion snapshot.
  assert(!ds_read_companion(ram.data(),16).valid);
  auto invalid=ram;std::memset(invalid.data(),0,invalid.size());
  assert(!ds_read_companion(invalid.data(),invalid.size()).valid);
  // Time of day and Berry Pots (HG / SS).
  assert(std::strcmp(ds_day_period(DsGame::Johto,4),"MORNING")==0&&std::strcmp(ds_day_period(DsGame::Johto,18),"DAY")==0);
  assert(std::strcmp(ds_day_period(DsGame::Johto,3),"NIGHT")==0&&std::strcmp(ds_day_period(DsGame::Platinum,18),"EVENING")==0);
  assert(ds_berry_pot_text(DsBerryPot{})=="");
  assert(ds_berry_pot_text(DsBerryPot{149,3,200,0})=="GROWING, 3H 20M, DRY");
  assert(ds_berry_pot_text(DsBerryPot{149,5,45,0})=="READY, FALL IN 45M");
  std::puts("PASS time of day and Berry Pot lines");
  {
    // HeartFelt Gold: HeartGold's reader on its save with the bigger bag (the heartfelt_gold fixture: a new game played
    // headless). Its tables come from the cart; an empty set is enough for the save.
    std::ifstream f(std::filesystem::path(argv[1]).parent_path().parent_path()/"heartfelt_gold"/"mainram.bin",std::ios::binary);
    std::vector<uint8_t> o{std::istreambuf_iterator<char>(f),{}};
    assert(o.size()==4*1024*1024);
    assert(!ds_read_companion(o.data(),o.size(),DsGame::Johto).valid);  // HeartGold's 493 species: the party fails
    DsOrigin tables;tables.lastSpecies=dsHeartfeltLastSpecies;
    ds_set_heartfelt(&tables);
    auto t=ds_read_companion(o.data(),o.size(),DsGame::Johto);
    assert(t.valid && t.party.size()==6);
    const int species[]={155,750,25,937,445,6},levels[]={12,20,9,35,40,30};
    for(int i=0;i<6;++i)assert(t.party[i].species==species[i] && t.party[i].level==levels[i] && t.party[i].hp==t.party[i].maxHp);
    assert(t.party[2].heldItem==234);
    assert(t.money==3000 && t.map==64 && t.repelSteps==0 && !t.hasPots && t.dexValid && t.nationalDex);
    // Its Pokédex: 1534 species ids, the six given creatures caught and seen (two past 493), nothing else in the list.
    assert(t.dexSeen.size()==size_t(dsHeartfeltLastSpecies+1));
    for(int id:species)assert(t.dexSeen[id] && t.dexCaught[id]);
    assert(!t.dexSeen[1] && !t.dexCaught[154] && !t.dexCaught[751]);
    auto count=[&](int id){for(auto& i:t.bag)if(i.id==id)return i.count;return 0;};
    assert(count(17)==5 && count(4)==10 && count(155)==3 && count(79)==2 && count(328)==1 && count(450)==1 && count(420)==1);
    auto pocket=[&](int id){for(auto& i:t.bag)if(i.id==id)return i.pocket;return -1;};
    assert(pocket(79)==0 && pocket(450)==1 && pocket(328)==2 && pocket(17)==4 && pocket(155)==5 && pocket(4)==6);
    assert(ds_egg_icon(DsGame::Johto)==494);
    // BATTLE on HeartGold's battle structs (heartfelt_gold_trainer: the first gym leader's fight): the player's creature
    // and the foe's (id 570, past 493), its moves past retail's 467, and the leader's six.
    {
      const auto saved=battleRam;
      battleRam=read_file_bytes((std::filesystem::path(argv[1]).parent_path().parent_path()/"heartfelt_gold_trainer"/"mainram.bin").string());
      assert(battleRam.size()==4*1024*1024);
      const auto bsave=ds_read_companion(battleRam.data(),battleRam.size(),DsGame::Johto);
      pd_snapshot fight{};
      ds_read_battle_companion(pk_nds_field_game("IPKE",0),battle_read32,battleRam.data(),battleRam.size(),bsave,fight);
      assert(fight.valid && fight.inBattle && fight.isTrainer);
      const auto& mine=fight.battlers[PD_POS_PLAYER_LEFT];const auto& foe=fight.battlers[PD_POS_OPPONENT_LEFT];
      assert(mine.species==155 && mine.level==12 && mine.hp==32 && mine.maxHp==32 && mine.moves[0]==33 && mine.pp[3]==25);
      assert(foe.species==570 && foe.level==14 && foe.maxHp==45 && foe.moves[1]==817);
      assert(fight.foeCount==6 && fight.foes[0].species==570 && fight.foeActive==0);
      battleRam=saved;
    }
    ds_set_heartfelt(nullptr);
    std::puts("PASS HeartFelt Gold's save: party past 493, its bigger bag, its shifted location");
    if(const char* path=std::getenv("PD_HEARTFELT_ROM")){
      std::ifstream cart(path,std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(cart),{}};
      assert(ds_heartfelt_cart(rom.data(),rom.size()) && !ds_origin_cart(rom.data(),rom.size()));
      DsOrigin c=ds_read_heartfelt(rom);
      assert(c.species[1]=="Bulbasaur" && c.species[544]=="Victini" && c.species[750]=="Sylveon" && c.species[937]=="Dragapult");
      assert(c.moves[33]=="Tackle" && c.items[234]=="Leftovers");
      assert((c.speciesTypes[750]==std::array<uint8_t,2>{9,9}) && (c.speciesTypes[937]==std::array<uint8_t,2>{16,7}));
      assert((c.moveMeta[33]==std::array<uint8_t,2>{0,40}) && c.iconPalettes.size()==1535);
      assert(c.chart[9*18+16]==8 && c.chart[16*18+9]==0 && c.chart[8*18+9]==8 && c.chart[0*18+7]==0 && c.chart[7*18+8]==4);
      ds_set_heartfelt(&c);
      auto icons=ds_party_icons(rom,DsGame::Johto);
      assert(icons.size()==size_t(dsHeartfeltLastSpecies+1));
      for(int id:{1,494,750,937,1534})assert(icons[id].size()==1024);
      assert(!std::strcmp(ds_species(750,DsGame::Johto),c.species[750].c_str()));
      // GUIDE's HERE from the cart: map header 9's set 111 by day, its own species (584 and 68 at 20% each, Lv79).
      assert(c.mapWild.size()==540 && c.wildSets.size()==147 && c.mapWild[9]==111);
      const auto day=ds_guide_here(DsGame::Johto,9,12,6);
      assert(day.count>=2 && day.entries[0].species==584 && day.entries[0].chance==20 && day.entries[0].low==79);
      assert(day.entries[1].species==68 && ds_guide_page_count(DsGame::Johto)==2 && !ds_guide_spots(DsGame::Johto,584).empty());
      // NEXT BOSS from its hg-engine trpoke: Falkner (20) opens with a Lv14 species 570, as a fight showed; Lance (244,
      // the count's top bit set, 33 bytes a mon) has six, his ace a Lv65 species 149.
      {
        DsCompanion s{};s.game=DsGame::Johto;s.eventFlags.assign(512,0);
        const auto rows=ds_guide_rows(s,GuidePage::NextBoss,false);
        assert(rows.size()>=7 && rows[0].title=="NEXT: LEADER FALKNER" && rows[1].detail=="Lv14");
        assert(rows[1].answer.rfind(std::string(ds_species(570,DsGame::Johto))+":",0)==0 && rows[6].detail=="Lv16");
        const auto lance=std::find_if(rows.begin(),rows.end(),[](const GuideRow& row){return row.title=="CHAMPION LANCE";});
        assert(lance!=rows.end() && rows.end()-lance>=7 && lance[6].detail=="Lv65");
        assert(lance[6].answer.rfind(std::string(ds_species(149,DsGame::Johto))+":",0)==0);
      }
      // DEX: national order over its ids (national 494 is id 544), sizes from the zukan to 493 and from the US text past
      // it (Sylveon 3'03" and 51.8 lbs), retail's fronts, the flat learnsets, evolutions.
      auto dex=ds_dex_art(rom,DsGame::Johto);
      assert(dex.details.size()==size_t(dsHeartfeltLastSpecies+1) && dex.regional.size()==1026);
      assert(dex.regional[493]==493 && dex.regional[494]==544 && dex.regional[1025]==dsHeartfeltLastNationalId);
      assert(dex.details[1].heightDm==7 && dex.details[1].weightHg==69 && dex.details[1].category=="Seed Pokémon");
      assert(dex.details[750].heightDm==10 && dex.details[750].weightHg==235 && dex.details[750].category.rfind("Intertwin",0)==0);
      assert(!dex.details[750].description.empty() && dex.details[750].stats[0]==95);
      for(int id:{1,750,937}){auto p=ds_dex_page(dex,DsGame::Johto,id);assert(p.side==80 && !p.sprite.empty() && !p.moves.empty());}
      const auto bulbasaur=ds_level_moves(dex,DsGame::Johto,1);
      assert(bulbasaur[0].level==1 && bulbasaur[0].move==33 && bulbasaur[1].move==45);
      assert(!ds_dex_evolutions(dex,1).empty() && ds_dex_evolutions(dex,1)[0].to==2);
      // Machines from hg-engine's code addons: Bulbasaur fits TM06 (Toxic) and TM22 (Solar Beam) and HM01 (Cut), not
      // TM01 (Focus Punch).
      {
        const auto machines=ds_dex_page(dex,DsGame::Johto,1).machines;
        const auto fits=[&](bool hm,int number,int move){
          return std::any_of(machines.begin(),machines.end(),[&](const DsDexMachine& m){return m.hm==hm && m.number==number && m.move==move;});
        };
        assert(fits(false,6,92) && fits(false,22,76) && fits(true,1,15) && !fits(false,1,264));
      }
      ds_set_heartfelt(nullptr);
      std::puts("PASS HeartFelt Gold's cart: names, types, moves, chart and icons");
    }
  }
  {
    // Origin HeartGold: HeartGold's reader on its grown save (the origin_heartgold fixture: a new game played headless,
    // OriginHeartGoldTest's values). The forms come from the cart; one is enough here.
    std::ifstream f(std::filesystem::path(argv[1]).parent_path().parent_path()/"origin_heartgold"/"mainram.bin",std::ios::binary);
    std::vector<uint8_t> o{std::istreambuf_iterator<char>(f),{}};
    assert(o.size()==4*1024*1024);
    assert(!ds_read_companion(o.data(),o.size(),DsGame::Johto).valid);  // HeartGold's offsets: no save there
    DsOrigin tables;tables.forms.push_back({6,1028,2});
    ds_set_origin(&tables);
    auto t=ds_read_companion(o.data(),o.size(),DsGame::Johto);
    assert(t.valid && t.party.size()==6);
    const int species[]={155,700,25,1028,445,887},levels[]={12,20,9,30,40,35},hp[]={32,73,28,92,139,117};
    for(int i=0;i<6;++i)assert(t.party[i].species==species[i] && t.party[i].level==levels[i] && t.party[i].hp==hp[i] && t.party[i].maxHp==hp[i]);
    assert(t.party[1].heldItem==234);
    assert((t.party[0].moves==std::array<int,4>{33,43,108,52}));
    assert(t.money==3000 && t.map==49 && t.repelSteps==0 && !t.hasPots);
    auto pocket=[&](int p){std::vector<std::pair<int,int>> v;for(auto& i:t.bag)if(i.pocket==p)v.push_back({i.id,i.count});return v;};
    assert((pocket(1)==std::vector<std::pair<int,int>>{{744,1},{450,1}}));
    assert((pocket(2)==std::vector<std::pair<int,int>>{{328,1}}));
    assert((pocket(3)==std::vector<std::pair<int,int>>{{137,1}}));
    assert((pocket(4)==std::vector<std::pair<int,int>>{{17,5},{780,1},{26,4}}));
    assert((pocket(5)==std::vector<std::pair<int,int>>{{155,3}}));
    assert((pocket(6)==std::vector<std::pair<int,int>>{{4,10},{1,1}}));
    assert((pocket(7)==std::vector<std::pair<int,int>>{{55,2}}));
    assert(t.dexValid && t.dexSeen.size()==1026 && t.seenCount==5 && t.caughtCount==5);
    for(int n:{25,155,445,700,887})assert(t.dexSeen[n] && t.dexCaught[n]);
    assert(ds_egg_icon(DsGame::Johto)==dsOriginEgg);
    {
      // Its own battle engine (OriginHeartGoldTest's origin_heartgold_battle / _trainer): the manager's objects.
      std::vector<uint8_t> cart(0x4000+0x2773C+4,0);std::memcpy(cart.data()+0x0C,"IPKJ",4);
      const uint32_t sig=0x021D11B0;std::memcpy(cart.data()+0x4000+0x2773C,&sig,4);
      const auto* origin=pk_nds_field_game_rom(cart.data(),uint32_t(cart.size()));
      assert(origin && origin->battleManagerPtr);
      const auto fixtures=std::filesystem::path(argv[1]).parent_path().parent_path();
      battleRam=read_file_bytes((fixtures/"origin_heartgold_battle"/"mainram.bin").string());
      pd_snapshot fight{};
      auto save=ds_read_companion(battleRam.data(),battleRam.size(),DsGame::Johto);
      assert(save.valid);
      ds_read_battle_companion(origin,battle_read32,battleRam.data(),battleRam.size(),save,fight);
      assert(fight.valid && fight.inBattle && !fight.isTrainer && fight.foeCount==0);
      const auto& me=fight.battlers[0];const auto& foe=fight.battlers[1];
      assert(me.species==887 && me.level==35 && me.hp==80 && me.maxHp==117 && me.type1==16 && me.type2==7);
      assert(me.moves[0]==372 && me.moves[1]==506 && me.moves[2]==97 && me.moves[3]==458);
      assert(me.stats[0]==83 && me.stats[1]==68 && me.stats[2]==112 && me.stats[3]==79 && me.stats[4]==74);
      assert(foe.species==16 && foe.level==50 && foe.type1==0 && foe.type2==2);
      battleRam=read_file_bytes((fixtures/"origin_heartgold_trainer"/"mainram.bin").string());
      save=ds_read_companion(battleRam.data(),battleRam.size(),DsGame::Johto);
      ds_read_battle_companion(origin,battle_read32,battleRam.data(),battleRam.size(),save,fight);
      assert(fight.valid && fight.isTrainer && fight.battlers[0].species==155 && fight.battlers[1].species==399);
      assert(fight.foeCount==1 && fight.foes[0].species==399 && fight.foeActive==0);
      std::puts("PASS Origin HeartGold's battles: its own engine's objects, a wild one and a trainer's team");
    }
    ds_set_origin(nullptr);
    assert(ds_egg_icon(DsGame::Johto)==494);
    std::puts("PASS Origin HeartGold's save: party past 493 and a form's id, its bag's pockets, its wider Pokedex");
    // Its tables and art from the cart, when one is given (PD_ORIGIN_ROM; nothing of it is in the repo).
    if(const char* path=std::getenv("PD_ORIGIN_ROM")){
      std::ifstream cart(path,std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(cart),{}};
      assert(ds_origin_cart(rom.data(),rom.size()));
      DsOrigin cartTables=ds_read_origin(rom);
      assert(cartTables.species.size()>1025 && cartTables.species[1]=="Bulbasaur" && cartTables.species[700]=="Sylveon");
      assert(cartTables.species[887]=="Dragapult" && cartTables.species[1025]=="Pecharunt" && cartTables.items[780]=="Rash Mint");
      assert((cartTables.speciesTypes[700]==std::array<uint8_t,2>{9,9}) && (cartTables.speciesTypes[887]==std::array<uint8_t,2>{16,7}));
      assert(cartTables.moves[33]=="Tackle" && (cartTables.moveMeta[33]==std::array<uint8_t,2>{0,40}));
      assert((cartTables.moveMeta[52]==std::array<uint8_t,2>{10,40}) && cartTables.moves.size()>920);
      assert(cartTables.chart[9*18+16]==8 && cartTables.chart[16*18+9]==0 && cartTables.chart[8*18+9]==8 && cartTables.chart[7*18+8]==4);
      assert(cartTables.formSpecies(6,1)==1027 && cartTables.formSpecies(6,2)==1028 && cartTables.formSpecies(6,0)==6);
      assert(cartTables.baseSpecies(1028)==6);
      assert(cartTables.machines[0]==264 && cartTables.machines[93]==19 && cartTables.machines[100]==473 && cartTables.machines[137]==815);
      ds_set_origin(&cartTables);
      // GUIDE's HERE from the cart: map header 9's set 111 by day (Caterpie and Rattata first, Lv2), map 65's (a set
      // retail has none of), a page count of one, and where a species is found.
      assert(cartTables.mapWild.size()==540 && cartTables.wildSets.size()==149 && cartTables.mapWild[9]==111 && cartTables.mapWild[65]==89);
      const auto day=ds_guide_here(DsGame::Johto,9,12,6);
      assert(day.count>=2 && day.entries[0].method==0 && day.entries[0].species==10 && day.entries[1].species==19 && day.entries[0].low==2);
      assert(ds_guide_here(DsGame::Johto,65,12,6).count>0 && ds_guide_page_count(DsGame::Johto)==2);
      assert(!ds_guide_spots(DsGame::Johto,10).empty());
      // NEXT BOSS from its trpoke (28 bytes a mon): Kanto first by level, so Brock (253) leads with a Lv12 Geodude, as
      // a headless fight showed; Lance's Lv100 ace comes after the Elite Four, and RED (moved to 956) is in the list.
      {
        assert(cartTables.trainers.size()==1024);
        DsCompanion s{};s.game=DsGame::Johto;s.eventFlags.assign(512,0);
        const auto rows=ds_guide_rows(s,GuidePage::NextBoss,false);
        assert(rows.size()>=8 && rows[0].heading && rows[0].title=="NEXT: LEADER BROCK");
        assert(rows[1].detail=="Lv12" && rows[1].answer.rfind(std::string(ds_species(74,DsGame::Johto))+":",0)==0);
        std::vector<std::string> titles;for(const auto& row:rows)if(row.heading)titles.push_back(row.title);
        assert(titles.size()==22 && titles[1]=="LEADER MISTY" && titles.back()=="CHAMPION LANCE");
        assert(std::find(titles.begin(),titles.end(),"RED")!=titles.end());
      }
      // HERE's items from its events and scripts: map 9's item ball (its own script: item 494, flag 1395) and map 60's
      // hidden item (script 8000: item 17, flag 800, as retail's guide has it), listed under ITEMS where you stand.
      {
        const auto has=[&](int map,int item,int flag,bool hidden){
          return std::any_of(cartTables.fieldItems.begin(),cartTables.fieldItems.end(),[&](const DsOrigin::FieldItem& f){
            return f.map==map && f.item==item && f.flag==flag && f.qty==1 && f.hidden==hidden;});
        };
        assert(cartTables.fieldItems.size()>200 && has(9,494,1395,false) && has(60,17,800,true));
        DsCompanion s{};s.game=DsGame::Johto;s.eventFlags.assign(512,0);s.map=60;
        const auto rows=ds_guide_rows(s,GuidePage::Here,false);
        const auto items=std::find_if(rows.begin(),rows.end(),[](const GuideRow& row){return row.heading && row.title=="ITEMS";});
        assert(items!=rows.end() && std::any_of(items,rows.end(),[](const GuideRow& row){return row.title=="HIDDEN ITEM";}));
      }
      assert(!std::strcmp(ds_species(1028,DsGame::Johto),"Charizard") && !std::strcmp(ds_move(52,DsGame::Johto),"Ember"));
      assert(ds_species_type(700,0,DsGame::Johto)==9);
      auto icons=ds_party_icons(rom,DsGame::Johto);
      assert(int(icons.size())==dsOriginEgg+1);
      for(int id:{1,700,887,1025,1028,1440,dsOriginEgg})assert(icons[id].size()==1024);
      auto dex=ds_dex_art(rom,DsGame::Johto);
      assert(dex.details.size()==1026 && dex.details[700].category=="Intertwine Pokémon" && dex.details[700].heightDm==10 && dex.details[700].weightHg==235);
      assert(dex.regional.empty());
      auto page=ds_dex_page(dex,DsGame::Johto,700);
      assert(page.side==80 && !page.sprite.empty());
      assert(!ds_dex_page(dex,DsGame::Johto,1).moves.empty() && ds_dex_page(dex,DsGame::Johto,1).moves[0].level==1 && ds_dex_page(dex,DsGame::Johto,1).moves[0].move==33);
      assert(ds_dex_page(dex,DsGame::Johto,151).machines.size()==138);
      bool sylveon=false;for(auto& e:dex.evolutions)sylveon|=e.from==133 && e.to==700 && e.method==27 && e.param==9;
      assert(sylveon);
      ds_set_origin(nullptr);
      std::puts("PASS Origin HeartGold's cart: names, types, chart, forms, machines, icons and DEX pages");
    }
    // Renegade Platinum's tables from its cart, when one is given (PD_RENEGADE_ROM; RenegadePlatinumTest's values).
    if(const char* path=std::getenv("PD_RENEGADE_ROM")){
      std::ifstream cart(path,std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(cart),{}};
      assert(ds_renegade_platinum(rom.data(),rom.size()));
      DsOrigin t=ds_read_renegade(rom);
      assert(!t.species.empty() && t.moves[32]=="Drill Run" && t.moves[66]=="Play Rough" && t.moves[33]=="Tackle");
      assert((t.speciesTypes[35]==std::array<uint8_t,2>{9,9}) && (t.speciesTypes[429]==std::array<uint8_t,2>{7,9}));
      assert((t.speciesTypes[330]==std::array<uint8_t,2>{6,16}) && (t.speciesTypes[387]==std::array<uint8_t,2>{12,12}));
      assert((t.moveMeta[33]==std::array<uint8_t,2>{0,40}) && (t.moveMeta[461]==std::array<uint8_t,2>{9,95}) && t.moveMeta[174][0]==7);
      assert((t.moveMeta[66]==std::array<uint8_t,2>{9,t.moveMeta[66][1]}));
      assert(t.chart[9*18+16]==8 && t.chart[16*18+9]==0 && t.chart[8*18+9]==8 && t.chart[9*18+8]==2);
      assert(t.chart[7*18+8]==4 && t.chart[17*18+8]==4 && t.chart[0*18+7]==0);
      assert(t.machines[56]==344 && t.machines[61]==405 && t.machines[82]==304 && t.machines[84]==295 && t.machines[87]==239);
      assert(t.machines[0]==264 && t.machines[92]==15);   // TM01 and HM01 as retail
      assert(t.iconPalettes.size()==0x21C);
      assert(t.mapWild.size()==593 && t.wildSets.size()==183 && t.mapWild[342]==140 && t.trainers.size()==1066);
      ds_set_renegade(&t);
      // GUIDE from the cart. HERE on Route 201 (map 342, set 140): by day its two Starly and Bidoof slots first (Lv4-5),
      // the day's Pidgey in slots 2 and 3; by night Hoothoot and Kricketot instead.
      const auto day=ds_guide_here(DsGame::Platinum,342,12,6);
      assert(day.count>=5 && day.entries[0].species==396 && day.entries[0].low==4 && day.entries[0].high==5 && day.entries[0].chance==30);
      assert(day.entries[1].species==399 && day.entries[2].species==16 && day.entries[2].chance==20);
      const auto night=ds_guide_here(DsGame::Platinum,342,22,6);
      assert(std::any_of(night.entries,night.entries+night.count,[](const DsGuideEncounter& e){return e.species==163;}));
      assert(ds_guide_page_count(DsGame::Platinum)==2 && !ds_guide_spots(DsGame::Platinum,396).empty());
      // NEXT BOSS: the leaders at retail's trainer ids with the hack's teams; Roark (246) has six, Lv15 and a Lv16 ace.
      {
        DsCompanion s{};s.game=DsGame::Platinum;s.eventFlags.assign(512,0);
        const auto rows=ds_guide_rows(s,GuidePage::NextBoss,false);
        assert(rows.size()>=8 && rows[0].heading && rows[0].title.rfind("NEXT: ",0)==0);
        assert(rows[1].detail=="Lv15" && rows[6].detail=="Lv16" && rows[7].heading);
      }
      // HERE's items from its events and scripts: map 3's hidden item (flag 891) and item ball (flag 1272), the flags
      // retail Platinum's guide data has for that place.
      {
        const auto* cart=ds_renegade();
        const auto has=[&](int map,int item,int flag,bool hidden){
          return std::any_of(cart->fieldItems.begin(),cart->fieldItems.end(),[&](const DsOrigin::FieldItem& f){
            return f.map==map && f.item==item && f.flag==flag && f.hidden==hidden;});
        };
        assert(cart->fieldItems.size()>500 && has(3,90,891,true) && has(3,17,1272,false) && has(411,111,931,true));
        DsCompanion s{};s.game=DsGame::Platinum;s.eventFlags.assign(512,0);s.map=3;
        const auto rows=ds_guide_rows(s,GuidePage::Here,false);
        const auto items=std::find_if(rows.begin(),rows.end(),[](const GuideRow& row){return row.heading && row.title=="ITEMS";});
        assert(items!=rows.end() && std::any_of(items,rows.end(),[](const GuideRow& row){return row.title=="HIDDEN ITEM";}));
      }
      assert(!std::strcmp(ds_move(66,DsGame::Platinum),"Play Rough") && ds_move_type(66,DsGame::Platinum)==9);
      assert(ds_species_type(35,0,DsGame::Platinum)==9 && ds_species_type(429,1,DsGame::Platinum)==9);
      assert(ds_party_icons(rom,DsGame::Platinum).size()>493);
      auto dex=ds_dex_art(rom,DsGame::Platinum);
      bool tm57=false;for(const auto& m:ds_dex_page(dex,DsGame::Platinum,151).machines)tm57|=!m.hm && m.number==57 && m.move==344;
      assert(tm57);
      // A wild battle in it (RenegadePlatinumTest's renegade_platinum_battle): TURTWIG against CLEFAIRY, Fairy here.
      battleRam=read_file_bytes((std::filesystem::path(argv[1]).parent_path().parent_path()/"renegade_platinum_battle"/"mainram.bin").string());
      assert(battleRam.size()==4*1024*1024);
      pd_snapshot fight{};
      const auto save=ds_read_companion(battleRam.data(),battleRam.size());
      ds_read_battle_companion(pk_nds_field_game("CPUE",1),battle_read32,battleRam.data(),battleRam.size(),save,fight);
      assert(fight.valid && fight.inBattle && !fight.isTrainer && !fight.isDouble);
      assert(fight.battlers[0].species==387 && fight.battlers[1].species==35 && fight.battlers[0].pp[0]==35);
      assert(ds_species_type(fight.battlers[1].species,0,DsGame::Platinum)==9);
      assert(!std::strcmp(ds_move(fight.battlers[0].moves[0],DsGame::Platinum),"Tackle"));
      ds_set_renegade(nullptr);
      assert(ds_species_type(35,0,DsGame::Platinum)!=9);
      std::puts("PASS Renegade Platinum's cart: names, Fairy types, moves, chart, machines, icon palettes and DEX machines");
    }
  }
#if PD_TEST_DSPERATE
  auto nds=std::make_unique<ds::NDS>();
  nds->bus.dma_write32(ds::Cpu::ARM9,0x02000000,0x12345678);
  assert(nds->bus.dma_read32(ds::Cpu::ARM9,0x02000000)==0x12345678);
  nds->io.set_buttons(1);assert(!(nds->io.keyinput&1));nds->io.set_buttons(0);assert(nds->io.keyinput&1);
#endif
  // The core the host runs (DSperate or melonDS), through the same interface the frontend uses.
  std::ifstream romFile(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(romFile),{}};
  auto boot=ds_core_create();std::string error;
  assert(boot->boot(argv[2],rom,error) && boot->start(error));
  // DSperate draws the cart's picture on its second frame, melonDS a few frames later: wait up to a second for it.
  for(int i=0;i<60 && boot->framebuffer(0)[96*256+128]!=0xff00fb00u;i++)boot->runFrame();
  assert(boot->framebuffer(0)[96*256+128]==0xff00fb00u);
  boot->write32(0x02010000,0x12345678);assert(boot->read32(0x02010000)==0x12345678);
  std::vector<uint8_t> state;assert(boot->saveState(state,error));
  boot->write32(0x02010000,0);
  assert(boot->loadState(state,true,error));
  assert(boot->read32(0x02010000)==0x12345678);
  assert(!boot->loadState(std::vector<uint8_t>(4),true,error));
  std::printf("PASS original DS diagnostic boot, video and state roundtrip on %s\n",boot->name());
}
