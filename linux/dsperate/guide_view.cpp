// GPL-3.0. Guide HERE uses the same encounter sets as Android.
#include "guide_view.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string_view>
extern "C" {
#include "pd_tables.h"
}

namespace ds_guide_data {
using Encounter=DsGuideEncounter;
struct Location {int key,offset,count;};
struct AreaItem {int place; bool hidden; int item,flag,qty; const char* where;};
struct HeaderPlace {int map,place;};
struct GuideTip {const char* section;const char* title;const char* hint;const char* answer;int item,flag;const char* areas;};
struct BossMon {int level,species,item;std::array<int,4> moves;};
struct BossTeam {
  const char *title,*where,*variant;
  int done,doneAfter,rematchFlag,offset,count,rematchOffset,rematchCount,trainer;
};
}
#include "guide_gen.h"

namespace {
using namespace ds_guide_data;
template<size_t N>
constexpr DsWildRates rates(const int32_t (&data)[N]){
  static_assert(N%3==0);
  return {data,int(N/3)};
}
template<size_t N>
DsGuideHere here(const Location (&locations)[N],const Encounter* entries,int key){
  const auto end=std::end(locations);
  const auto match=std::lower_bound(std::begin(locations),end,key,[](const Location& p,int n){return p.key<n;});
  return match!=end && match->key==key ? DsGuideHere{entries+match->offset,match->count}:DsGuideHere{};
}
constexpr const char* methods[]={"LAND","DARK GRASS","RUSTLING GRASS","SURF","RIPPLING WATER",
  "SUPER ROD","RIPPLING FISHING","OLD ROD","GOOD ROD","ROCK SMASH"};

// Origin HeartGold's wild encounters, read from its cart (DsOrigin::wildSets): HeartGold's 196-byte record, turned into
// rows as gen_guide.py turns Android's (the same methods, odds and merging). Land: 12 levels at 8, then the morning, day
// and night species (12 u16 each) at 20 / 44 / 68; surf (5 slots of min, max, species) at 100, rock smash (2) at 120,
// the old, good and super rods (5 each) at 128 / 148 / 168; their rates at 0..5; the night's fish at 192.
std::vector<DsGuideEncounter> origin_encounters(const std::vector<uint8_t>& d,int time){
  static constexpr uint8_t land[12]={20,20,10,10,10,10,5,5,4,4,1,1},water[5]={60,30,5,4,1},rock[2]={80,20},
                           rod[5]={40,30,15,10,5};
  const auto u16=[&](size_t at){return unsigned(d[at]|d[at+1]<<8);};
  struct Method{uint8_t method,rate;size_t at;int count;const uint8_t* odds;};
  const Method list[]={{0,d[0],8,12,land},{3,d[1],100,5,water},{9,d[2],120,2,rock},{7,d[3],128,5,rod},
                       {8,d[4],148,5,rod},{5,d[5],168,5,rod}};
  std::vector<DsGuideEncounter> out;
  for(const auto& m:list){
    if(!m.rate)continue;
    std::vector<DsGuideEncounter> merged;
    for(int i=0;i<m.count;++i){
      unsigned species,low,high;
      if(m.method==0){species=u16(20+24*time+2*i);low=high=d[8+i];}
      else{low=d[m.at+4*i];high=d[m.at+4*i+1];species=u16(m.at+4*i+2);}
      if(time==2 && ((m.method==8 && i==3) || (m.method==5 && i==1)) && u16(192))species=u16(192);
      if(!species)continue;
      auto it=std::find_if(merged.begin(),merged.end(),[&](const DsGuideEncounter& e){return e.species==species;});
      if(it==merged.end())merged.push_back({m.method,uint16_t(species),uint8_t(low),uint8_t(high),m.odds[i]});
      else{it->low=std::min<uint8_t>(it->low,low);it->high=std::max<uint8_t>(it->high,high);it->chance+=m.odds[i];}
    }
    std::stable_sort(merged.begin(),merged.end(),[](const auto& a,const auto& b){return a.chance>b.chance;});
    out.insert(out.end(),merged.begin(),merged.end());
  }
  return out;
}
// Renegade Platinum's, from pl_enc_data.narc's 424-byte record as gen_guide.py reads Android's Platinum lines: the land
// rate at 0 and 12 { u32 level, u32 species } at 4, the day's and the night's two species (u32) at 108 / 116 standing in
// for land slots 2 and 3, then surf, the old, good and super rods at 204 / 292 / 336 / 380 (a u32 rate, 5 slots of
// { u8 max, u8 min, u16, u32 species }). Proven on retail Platinum's cart: all 154 of Android's sets come out the same.
std::vector<DsGuideEncounter> platinum_encounters(const std::vector<uint8_t>& d,int time){
  static constexpr uint8_t land[12]={20,20,10,10,10,10,5,5,4,4,1,1},water[5]={60,30,5,4,1},
                           oldRod[5]={60,30,5,4,1},rod[5]={40,40,15,4,1};
  const auto u32=[&](size_t at){return unsigned(d[at]|d[at+1]<<8|d[at+2]<<16|unsigned(d[at+3])<<24);};
  struct Method{uint8_t method;size_t at;int count;const uint8_t* odds;};
  const Method list[]={{0,0,12,land},{3,204,5,water},{7,292,5,oldRod},{8,336,5,rod},{5,380,5,rod}};
  std::vector<DsGuideEncounter> out;
  for(const auto& m:list){
    if(!u32(m.at))continue;
    std::vector<DsGuideEncounter> merged;
    for(int i=0;i<m.count;++i){
      unsigned species,low,high;
      if(m.method==0){
        low=high=u32(4+8*i)&0xFF;species=u32(8+8*i);
        if(time && (i==2 || i==3))species=u32((time==1?108:116)+4*(i-2));
      }else{high=d[m.at+4+8*i];low=d[m.at+5+8*i];species=u32(m.at+8+8*i);}
      species&=0x3FF;
      if(!species)continue;
      auto it=std::find_if(merged.begin(),merged.end(),[&](const DsGuideEncounter& e){return e.species==species;});
      if(it==merged.end())merged.push_back({m.method,uint16_t(species),uint8_t(low),uint8_t(high),m.odds[i]});
      else{it->low=std::min<uint8_t>(it->low,low);it->high=std::max<uint8_t>(it->high,high);it->chance+=m.odds[i];}
    }
    std::stable_sort(merged.begin(),merged.end(),[](const auto& a,const auto& b){return a.chance>b.chance;});
    out.insert(out.end(),merged.begin(),merged.end());
  }
  return out;
}
// A hack whose wild encounters come from its own cart: Origin HeartGold or HeartFelt Gold (Johto), Renegade Platinum.
const DsOrigin* origin_wild(DsGame game){
  const auto* o=game==DsGame::Johto?(ds_origin()?ds_origin():ds_heartfelt()):game==DsGame::Platinum?ds_renegade():nullptr;
  return o && !o->mapWild.empty()?o:nullptr;
}
const std::vector<DsGuideEncounter>* origin_set(const DsOrigin& o,int map,int time){
  if(map<0 || size_t(map)>=o.mapWild.size() || o.mapWild[map]>=o.wildSets.size())return nullptr;
  // Built once per set and time, for the game's run (the tables don't change under it).
  static const DsOrigin* owner=nullptr;
  static std::vector<std::array<std::vector<DsGuideEncounter>,3>> cache;
  static std::vector<std::array<bool,3>> built;
  if(owner!=&o || cache.size()!=o.wildSets.size()){owner=&o;cache.assign(o.wildSets.size(),{});built.assign(o.wildSets.size(),{});}
  const unsigned set=o.mapWild[map];
  if(!built[set][time]){
    const auto& d=o.wildSets[set];
    cache[set][time]=d.size()>=424?platinum_encounters(d,time):origin_encounters(d,time);
    built[set][time]=true;
  }
  return &cache[set][time];
}

template<size_t N>
std::vector<DsGuideSpot> spots(const Location (&locations)[N],const Encounter* entries,DsGame game,int species){
  std::vector<DsGuideSpot> result;
  for(const auto& loc:locations){
    const int map=loc.key/4;
    const unsigned variant=1u<<(loc.key%4);
    for(int i=0;i<loc.count;++i){
      const auto& e=entries[loc.offset+i];
      if(e.species!=species)continue;
      const auto* name=ds_place(map,game);
      const auto match=std::find_if(result.begin(),result.end(),[&](const DsGuideSpot& spot){
        const auto& other=spot.encounter;
        const bool samePlace=spot.map==map || (*name!='?' && std::strcmp(name,ds_place(spot.map,game))==0);
        return samePlace && other.method==e.method && other.low==e.low && other.high==e.high && other.chance==e.chance;
      });
      if(match==result.end())result.push_back({map,e,variant});
      else match->variants|=variant;
    }
  }
  return result;
}
}

DsWildRates ds_wild_rates(DsGame game,int month,bool soulSilver){
  using namespace ds_guide_data;
  const int season=(std::clamp(month,1,12)-1)%4;
  switch(game){
    case DsGame::Platinum:return rates(ptRates0);
    case DsGame::Johto:return soulSilver?rates(ssRates0):rates(hgRates0);
    case DsGame::Black:{
      constexpr DsWildRates seasons[]={rates(blackRates0),rates(blackRates1),rates(blackRates2),rates(blackRates3)};
      return seasons[season];
    }
    case DsGame::White:{
      constexpr DsWildRates seasons[]={rates(whiteRates0),rates(whiteRates1),rates(whiteRates2),rates(whiteRates3)};
      return seasons[season];
    }
    case DsGame::Black2:{
      constexpr DsWildRates seasons[]={rates(black2Rates0),rates(black2Rates1),rates(black2Rates2),rates(black2Rates3)};
      return seasons[season];
    }
    case DsGame::White2:{
      constexpr DsWildRates seasons[]={rates(white2Rates0),rates(white2Rates1),rates(white2Rates2),rates(white2Rates3)};
      return seasons[season];
    }
    default:return {};
  }
}

const std::vector<DsFlySpot>& ds_fly_spots(DsGame game){
  using namespace ds_guide_data;
  static const std::vector<DsFlySpot> platinum(std::begin(platinumFly),std::end(platinumFly));
  static const std::vector<DsFlySpot> johto(std::begin(heartgoldFly),std::end(heartgoldFly));
  static const std::vector<DsFlySpot> black(std::begin(blackFly),std::end(blackFly));
  static const std::vector<DsFlySpot> white(std::begin(whiteFly),std::end(whiteFly));
  static const std::vector<DsFlySpot> white2(std::begin(white2Fly),std::end(white2Fly));
  static const std::vector<DsFlySpot> empty;
  switch(game){
    case DsGame::Platinum:return platinum;
    case DsGame::Johto:return johto;
    case DsGame::Black:return black;
    case DsGame::White:return white;
    case DsGame::White2:return white2;
    default:return empty;
  }
}

bool ds_fly_unlocked(const DsCompanion& s,const DsFlySpot& spot){
  if(spot.flag<0 || !ds_event_flag(s,spot.flag))return false;
  if(s.game!=DsGame::Johto || spot.region==2 || s.mapX<0 || s.mapY<2)return true;
  // Android's Pokégear region rule uses the cartridge Y, two cells above the rendered map Y.
  const int x=s.mapX,y=s.mapY-2;
  const int here=x<=21 || (x==25 && y==8)?0:x==28 && (y==6 || (y>=9 && y<=12))?2:1;
  return here==2 || spot.region==here;
}

std::vector<DsGuideSpot> ds_guide_spots(DsGame game,int species,bool soulSilver){
  using namespace ds_guide_data;
  if(const auto* o=origin_wild(game)){
    std::vector<DsGuideSpot> result;
    for(int map=0;map<int(o->mapWild.size());++map)for(int time=0;time<3;++time){
      const auto* set=origin_set(*o,map,time);
      if(!set)break;
      for(const auto& e:*set){
        if(e.species!=species)continue;
        const auto* name=ds_place(map,game);
        const auto match=std::find_if(result.begin(),result.end(),[&](const DsGuideSpot& spot){
          const auto& other=spot.encounter;
          const bool samePlace=spot.map==map || (*name!='?' && std::strcmp(name,ds_place(spot.map,game))==0);
          return samePlace && other.method==e.method && other.low==e.low && other.high==e.high && other.chance==e.chance;
        });
        if(match==result.end())result.push_back({map,e,1u<<time});
        else match->variants|=1u<<time;
      }
    }
    return result;
  }
  switch(game){
    case DsGame::Platinum:return spots(ptLocations,ptEncounters,game,species);
    case DsGame::Johto:return soulSilver?spots(ssLocations,ssEncounters,game,species):spots(hgLocations,hgEncounters,game,species);
    case DsGame::Black:return spots(blackLocations,blackEncounters,game,species);
    case DsGame::White:return spots(whiteLocations,whiteEncounters,game,species);
    case DsGame::Black2:return spots(black2Locations,black2Encounters,game,species);
    case DsGame::White2:return spots(white2Locations,white2Encounters,game,species);
    default:return {};
  }
}

const char* ds_guide_method(int method){return methods[method];}
DsGuideHere ds_guide_here(DsGame game,int map,int hour,int month,bool soulSilver){
  using namespace ds_guide_data;
  if(map<0 || map>65535 || hour<0 || hour>23 || month<1 || month>12)return {};
  const int time=hour>=4 && hour<10?0:hour>=10 && hour<20?1:2;
  const int key=map*4+(ds_unova(game)?(month-1)%4:time);
  if(const auto* o=origin_wild(game)){
    const auto* set=origin_set(*o,map,time);
    return set?DsGuideHere{set->data(),int(set->size())}:DsGuideHere{};
  }
  switch(game){
    case DsGame::Platinum:return here(ptLocations,ptEncounters,key);
    case DsGame::Johto:return soulSilver?here(ssLocations,ssEncounters,key):here(hgLocations,hgEncounters,key);
    case DsGame::Black:return here(blackLocations,blackEncounters,key);
    case DsGame::White:return here(whiteLocations,whiteEncounters,key);
    case DsGame::Black2:return here(black2Locations,black2Encounters,key);
    case DsGame::White2:return here(white2Locations,white2Encounters,key);
    default:return {};
  }
}
DsGuideHere ds_guide_here(const DsCompanion& s,bool soulSilver){
  const auto now=std::time(nullptr);
  std::tm local{};
  return localtime_r(&now,&local)?ds_guide_here(s.game,s.map,local.tm_hour,local.tm_mon+1,soulSilver):DsGuideHere{};
}

namespace {
template<size_t N,size_t M>
void append_items(std::vector<GuideRow>& rows,const DsCompanion& s,
                  const HeaderPlace (&places)[N],const AreaItem (&items)[M]){
  const auto map=std::lower_bound(std::begin(places),std::end(places),s.map,
    [](const HeaderPlace& p,int n){return p.map<n;});
  if(map==std::end(places) || map->map!=s.map)return;
  const auto first=std::lower_bound(std::begin(items),std::end(items),map->place,
    [](const AreaItem& i,int p){return i.place<p;});
  // Picked-up items follow the remaining items, as in the Android Guide.
  for(bool done:{false,true}){
    for(auto item=first;item!=std::end(items) && item->place==map->place;++item){
      if(ds_event_flag(s,item->flag)!=done)continue;
      std::string name=ds_item(item->item,s.game);
      if(item->qty>1)name+=" x"+std::to_string(item->qty);
      const auto where=*item->where?std::string("in ")+item->where:std::string("around ")+ds_place(s.map,s.game);
      if(item->hidden && !done)rows.push_back({"HIDDEN ITEM","ITEMS","Buried "+where+".",name,false});
      else rows.push_back({name,"ITEMS",{},(item->hidden?"Hidden ":"Lying ")+where+".",done});
    }
  }
}
// Origin HeartGold's and Renegade Platinum's item balls and hidden items from their carts, for every map with the same place name as yours (its
// buildings included), each once.
void append_cart_items(std::vector<GuideRow>& rows,const DsCompanion& s,const DsOrigin& o){
  const char* here=ds_place(s.map,s.game);
  std::vector<std::pair<int,int>> listed;
  for(bool done:{false,true}){
    for(const auto& item:o.fieldItems){
      if(item.map!=s.map && (*here=='?' || std::strcmp(here,ds_place(item.map,s.game))!=0))continue;
      if(ds_event_flag(s,item.flag)!=done)continue;
      if(std::find(listed.begin(),listed.end(),std::pair<int,int>{item.flag,item.item})!=listed.end())continue;
      listed.push_back({item.flag,item.item});
      std::string name=ds_item(item.item,s.game);
      if(item.qty>1)name+=" x"+std::to_string(item.qty);
      const std::string where=std::string("around ")+here;
      if(item.hidden && !done)rows.push_back({"HIDDEN ITEM","ITEMS","Buried "+where+".",name,false});
      else rows.push_back({name,"ITEMS",{},(item.hidden?"Hidden ":"Lying ")+where+".",done});
    }
  }
}
std::string area_key(const char* place){
  std::string key;
  for(;*place;++place){
    if(uint8_t(place[0])==0xc3 && (uint8_t(place[1])==0xa9 || uint8_t(place[1])==0x89)){
      key+='E';++place;
    }else if((*place>='A' && *place<='Z') || (*place>='0' && *place<='9'))key+=*place;
    else if(*place>='a' && *place<='z')key+=char(*place-'a'+'A');
  }
  return key;
}

template<size_t N>
void append_tips(std::vector<GuideRow>& rows,const DsCompanion& s,const GuideTip (&entries)[N],
                 const std::string* area){
  for(const auto& tip:entries){
    if(area){
      const std::string_view tags=tip.areas;
      bool match=false;
      for(size_t start=0;start<tags.size();){
        const size_t end=tags.find('|',start);
        if(tags.substr(start,end-start)==*area){match=true;break;}
        if(end==std::string_view::npos)break;
        start=end+1;
      }
      if(!match || std::any_of(rows.begin(),rows.end(),[&](const GuideRow& row){return row.title==tip.title;}))continue;
    }
    const bool owned=tip.item && std::any_of(s.bag.begin(),s.bag.end(),
      [&](const DsItem& item){return item.id==tip.item && item.count>0;});
    rows.push_back({tip.title,area?"TO DO":tip.section,tip.hint,tip.answer,owned||ds_event_flag(s,tip.flag)});
  }
}

void authored_rows(std::vector<GuideRow>& rows,const DsCompanion& s,GuidePage page,bool ss,
                   const std::string* area=nullptr){
  using namespace ds_guide_data;
  if(page==GuidePage::WhereIs){
    switch(s.game){
      case DsGame::Platinum:append_tips(rows,s,ptWhere,area);break;
      case DsGame::Johto:ss?append_tips(rows,s,ssWhere,area):append_tips(rows,s,hgWhere,area);break;
      case DsGame::Black:case DsGame::White:append_tips(rows,s,blackWhere,area);break;
      case DsGame::Black2:case DsGame::White2:append_tips(rows,s,black2Where,area);break;
      default:break;
    }
  }else if(page==GuidePage::Tips){
    switch(s.game){
      case DsGame::Platinum:append_tips(rows,s,ptTips,area);break;
      case DsGame::Johto:ss?append_tips(rows,s,ssTips,area):append_tips(rows,s,hgTips,area);break;
      case DsGame::Black:case DsGame::White:append_tips(rows,s,blackTips,area);break;
      case DsGame::Black2:case DsGame::White2:append_tips(rows,s,black2Tips,area);break;
      default:break;
    }
  }else if(page==GuidePage::Stuck){
    if(s.game==DsGame::Platinum)append_tips(rows,s,ptStuck,area);
    if(s.game==DsGame::Johto)ss?append_tips(rows,s,ssStuck,area):append_tips(rows,s,hgStuck,area);
  }
}

bool boss_flag(const DsCompanion& s,int flag){
  if(flag>=GEN4_BADGE_FLAG && flag<GEN4_BADGE_FLAG+16)
    return s.badges&(1u<<(flag-GEN4_BADGE_FLAG));
  if(flag==GEN5_NATIONAL_DEX_FLAG){
    // Black/White's regional Dex has only species 494 and up.
    const size_t end=std::min(s.dexSeen.size(),size_t(494));
    return std::any_of(s.dexSeen.begin(),s.dexSeen.begin()+end,[](uint8_t seen){return seen!=0;});
  }
  return ds_event_flag(s,flag);
}

// A hack's team for a trainer id, from its trpoke. Renegade Platinum keeps Platinum's format (the trdata type's bit 0
// adds 4 moves, bit 1 an item, to { u8, u8, u16 level, u16 species (10 bits) and form, ..., u16 seal }); Origin
// HeartGold gives every mon a fixed 28 bytes (level at 6, species at 8, item at 10, the moves at 12), which no Platinum
// type's size matches.
std::vector<BossMon> cart_team(const DsOrigin& o,int trainer){
  std::vector<BossMon> team;
  if(trainer<0 || size_t(trainer)>=o.trainers.size() || o.trainers[trainer].size()<4)return team;
  const auto& t=o.trainers[trainer];
  const auto& p=o.trainerParties[trainer];
  const auto u16=[&](size_t at){return int(p[at]|p[at+1]<<8);};
  if(t[3] && p.size()==size_t(t[3])*28){
    for(int i=0;i<std::min<int>(t[3],6);++i){
      BossMon mon{u16(i*28+6),u16(i*28+8)&0x7FF,u16(i*28+10),{}};
      for(int k=0;k<4;++k)mon.moves[k]=u16(i*28+12+2*k);
      team.push_back(mon);
    }
    return team;
  }
  const int type=t[0],size=8+(type&2?2:0)+(type&1?8:0);
  for(int i=0;i<std::min<int>(t[3],6) && size_t((i+1)*size)<=p.size();++i){
    size_t at=size_t(i*size)+6;
    BossMon mon{u16(i*size+2),u16(i*size+4)&0x3FF,0,{}};
    if(type&2){mon.item=u16(at);at+=2;}
    if(type&1)for(int k=0;k<4;++k)mon.moves[k]=u16(at+2*k);
    team.push_back(mon);
  }
  return team;
}

constexpr int originRed=956;

template<size_t N>
std::vector<GuideRow> boss_rows(const DsCompanion& s,const BossTeam (&teams)[N],const BossMon* mons,
                                const DsOrigin* cart=nullptr){
  std::vector<GuideRow> rows;
  const char* first=nullptr;
  int best=0;
  for(const auto& member:s.party)if(!member.egg)best=std::max(best,int(member.level));
  // Origin HeartGold sends you to Kanto first, its leaders the weakest: its bosses go in order of their aces' levels.
  std::vector<const BossTeam*> order;
  for(const auto& boss:teams)order.push_back(&boss);
  if(cart && cart==ds_origin()){
    const auto ace=[&](const BossTeam* boss){
      int level=0;
      for(const auto& mon:cart_team(*cart,boss->trainer==260?originRed:boss->trainer))level=std::max(level,mon.level);
      return level;
    };
    std::stable_sort(order.begin(),order.end(),[&](const BossTeam* a,const BossTeam* b){return ace(a)<ace(b);});
  }
  for(const BossTeam* entry:order){
    BossTeam boss=*entry;
    // Origin moved RED to its own trainer id; the defeat flag follows it (trainer flags run in id order).
    if(cart && cart==ds_origin() && boss.trainer==260){boss.done+=originRed-260;boss.trainer=originRed;}
    if(boss_flag(s,boss.done) || boss_flag(s,boss.doneAfter))continue;
    if(!first)first=boss.title;
    const bool next=std::strcmp(first,boss.title)==0;
    const bool rematch=boss_flag(s,boss.rematchFlag);
    const int offset=rematch?boss.rematchOffset:boss.offset;
    // Renegade Platinum: the same leaders at the same ids, with the hack's own teams.
    const auto own=cart?cart_team(*cart,boss.trainer):std::vector<BossMon>{};
    const BossMon* team=cart?own.data():mons+offset;
    const int count=cart?int(own.size()):rematch?boss.rematchCount:boss.count;
    int ace=0;
    for(int i=0;i<count;++i)ace=std::max(ace,team[i].level);
    char note[120];
    pd_ui_guide_boss_note(note,sizeof note,boss.where,count,ace,best);
    std::string title=(next?"NEXT: ":"")+std::string(boss.title);
    if(*boss.variant)title+=" ("+std::string(boss.variant)+")";
    rows.push_back({title,note,{},{},false,true});
    for(int i=0;i<count;++i){
      const auto& mon=team[i];
      const int primary=ds_species_type(mon.species,0,s.game),secondary=ds_species_type(mon.species,1,s.game);
      std::string hint=pd_type_name(nullptr,primary);
      if(primary!=secondary)hint+=" / "+std::string(pd_type_name(nullptr,secondary));
      std::string answer=ds_species(mon.species,s.game);
      answer+=":";
      bool firstMove=true;
      for(int move:mon.moves)if(move){
        answer+=firstMove?" ":", ";
        answer+=ds_move(move,s.game);
        firstMove=false;
      }
      if(mon.item)answer+=". HOLDS "+std::string(ds_item(mon.item,s.game));
      rows.push_back({"Pokémon "+std::to_string(i+1),"Lv"+std::to_string(mon.level),hint,answer});
    }
  }
  return rows;
}
}

std::vector<GuideRow> ds_guide_rows(const DsCompanion& s,GuidePage page,bool soulSilver){
  using namespace ds_guide_data;
  if(page==GuidePage::NextBoss){
    if(s.eventFlags.empty())return {};
    switch(s.game){
      case DsGame::Platinum:return boss_rows(s,ptBossTeams,ptBossMons,origin_wild(s.game));
      case DsGame::Johto:return boss_rows(s,hgBossTeams,hgBossMons,origin_wild(s.game)==ds_origin()?ds_origin():nullptr);
      case DsGame::Black:return boss_rows(s,blackBossTeams,blackBossMons);
      case DsGame::White:return boss_rows(s,whiteBossTeams,whiteBossMons);
      case DsGame::Black2:return boss_rows(s,black2BossTeams,black2BossMons);
      case DsGame::White2:return boss_rows(s,white2BossTeams,white2BossMons);
      default:return {};
    }
  }
  std::vector<GuideRow> rows;
  // Origin HeartGold and Renegade Platinum: HERE's wild encounters from the cart only (their tips are their own,
  // unmapped; their bosses come from their carts above, and their items from the carts below).
  const bool origin=origin_wild(s.game)!=nullptr;
  if(page!=GuidePage::Here){
    if(!origin)authored_rows(rows,s,page,soulSilver);
    return rows;
  }
  const auto place=area_key(ds_place(s.map,s.game));
  if(!origin){
    authored_rows(rows,s,GuidePage::WhereIs,soulSilver,&place);
    authored_rows(rows,s,GuidePage::Stuck,soulSilver,&place);
  }
  if(!rows.empty())rows.insert(rows.begin(),{"TO DO",{},{},{},false,true});
  const auto encounters=ds_guide_here(s,soulSilver);
  int previousMethod=-1;
  for(int i=0;i<encounters.count;++i){
    const auto& mon=encounters.entries[i];
    if(mon.method!=previousMethod){
      rows.push_back({ds_guide_method(mon.method),{},{},{},false,true});
      previousMethod=mon.method;
    }
    const bool seen=s.dexValid && mon.species<s.dexSeen.size() && s.dexSeen[mon.species];
    const bool caught=mon.species<s.dexCaught.size() && s.dexCaught[mon.species];
    const auto* name=ds_species(mon.species,s.game);
    char levels[32];
    if(mon.low==mon.high)std::snprintf(levels,sizeof levels,"Lv%d  %d%%",mon.low,mon.chance);
    else std::snprintf(levels,sizeof levels,"Lv%d-%d  %d%%",mon.low,mon.high,mon.chance);
    rows.push_back({seen?name:"???",levels,{},seen?"":name,caught});
  }
  const size_t itemsBegin=rows.size();
  if(origin && !origin_wild(s.game)->fieldItems.empty())append_cart_items(rows,s,*origin_wild(s.game));
  else if(!origin)switch(s.game){
    case DsGame::Platinum:append_items(rows,s,ptPlaces,ptItems);break;
    case DsGame::Johto:append_items(rows,s,hgPlaces,hgItems);break;
    case DsGame::Black:append_items(rows,s,blackPlaces,blackItems);break;
    case DsGame::White:append_items(rows,s,blackPlaces,whiteItems);break;
    case DsGame::Black2:append_items(rows,s,black2Places,black2Items);break;
    case DsGame::White2:append_items(rows,s,black2Places,white2Items);break;
    default:break;
  }
  if(rows.size()>itemsBegin)rows.insert(rows.begin()+itemsBegin,{"ITEMS",{},{},{},false,true});
  return rows;
}

int ds_guide_page_count(DsGame game){
  if(const auto* o=origin_wild(game))return o->trainers.empty()?1:2;  // HERE, and NEXT BOSS from the cart
  return game==DsGame::Platinum || game==DsGame::Johto?int(GuidePage::Count):int(GuidePage::Stuck);
}

void ds_guide_view(pd_ui* ui,pd_canvas* c,const DsCompanion& s,const std::vector<GuideRow>& entries,bool soulSilver){
  constexpr const char* labels[]={"HERE","NEXT BOSS","TIPS","WHERE IS","STUCK?"};
  const auto page=static_cast<GuidePage>(ui->guidePage);
  std::vector<pd_guide_row> view;
  view.reserve(entries.size()+1);
  std::string title;
  if(page==GuidePage::Here){
    const auto* place=ds_place(s.map,s.game);
    title=*place=='?'?"THIS MAP":place;
    for(char& ch:title)if(ch>='a' && ch<='z')ch-='a'-'A';
    view.push_back({title.c_str(),
      entries.empty()?"Nothing listed for this area.":"Its buildings included. A ball = done.",
      nullptr,nullptr,false,true});
  }else if(entries.empty()){
    const bool bosses=page==GuidePage::NextBoss;
    const bool beaten=bosses && !s.eventFlags.empty();
    const char* note=beaten?"Every gym leader and the League are beaten.":
      bosses?"Waiting for save progress.":"Nothing listed yet.";
    view.push_back({beaten?"ALL BEATEN":labels[int(page)],note,nullptr,nullptr,false,true});
  }
  const bool group=page==GuidePage::WhereIs || page==GuidePage::Tips || page==GuidePage::Stuck;
  const std::string* section=nullptr;
  for(const auto& entry:entries){
    if(group && (!section || *section!=entry.detail)){
      section=&entry.detail;
      view.push_back({section->c_str(),"",nullptr,nullptr,false,true});
    }
    const char* detail=group || (page==GuidePage::Here && (entry.detail=="TO DO" || entry.detail=="ITEMS"))?
      "":entry.detail.c_str();
    view.push_back({entry.title.c_str(),detail,entry.hint.c_str(),entry.answer.c_str(),entry.done,entry.heading});
  }
  // The DS companion has its own settings file. HeartGold and SoulSilver use different notice bits.
  const int noticeIndex=s.game==DsGame::Johto?1+int(soulSilver):
    s.game==DsGame::Platinum?0:int(s.game);
  const int noticeBit=1<<noticeIndex;
  const pd_guide_source source{labels,ds_guide_page_count(s.game),view.data(),int(view.size()),noticeBit,false};
  pd_ui_guide_view(ui,c,&source);
}
