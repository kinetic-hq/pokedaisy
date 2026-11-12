// GPL-3.0. Real core and captured-RAM tests, never fake application screens.
#include "ds_core.h"
#if __has_include("core/nds.h")
#include "core/nds.h"
#define PD_TEST_DSPERATE 1
#endif
#include "companion.h"
#include "battle_view.h"
#include "pk_nds_field.h"
#include <filesystem>
#include "map_art.h"
#include <cassert>
#include <fstream>
#include <cstdio>
#include <cstring>
extern "C" {
#include "../hud.h"
#include "../quick_menu.h"
}
static std::vector<uint8_t> battleRam;
static uint32_t battle_read32(uint32_t addr){
  if(addr<0x02000000 || addr>0x023ffffc)return 0;
  uint32_t value;std::memcpy(&value,battleRam.data()+addr-0x02000000,4);return value;
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
