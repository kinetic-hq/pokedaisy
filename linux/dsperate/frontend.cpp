// GPL-3.0. The SDL host's DS frontend, over whichever DS core the build has (ds_core.h: DSperate or melonDS); never
// used by mGBA.
#include "ds_core.h"
#include "companion.h"
#include "data_gen.h"
#include "battle_view.h"
#include "display.h"
#include "map_art.h"
#include "pk_nds_field.h"
#include "ds_layout.h"
#include "touch_pieces.h"
#include "../shader.h"
#include "../backdrop.h"
#include "../wing_look.h"
#include <SDL.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <deque>
#include <ctime>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
extern "C" {
#include "pd_sha1.h"
#include "pd_canvas.h"
#include "pd_style.h"
#include "pd_layout_pick.h"
#include "pd_png.h"
#include "pd_tables.h"
#include "../quick_menu.h"
#include "../hud.h"
bool pocket_save_backup(const char*,time_t);
extern const char* pocket_backup_dir;
extern int pd_ds_xbox_layout;  // BUTTON LAYOUT XBOX, set by the host (outside the unnamed namespace, so it links)
}
namespace {
DsCore* active=nullptr;
uint32_t read32(uint32_t a){return active->read32(a);}
void write32(uint32_t a,uint32_t v){active->write32(a,v);}
std::vector<uint8_t> read_file(const std::string& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
bool atomic_file(const std::string& path,const uint8_t* data,size_t size){
  std::string temp=path+".tmp";FILE* f=std::fopen(temp.c_str(),"wb");if(!f)return false;
  bool ok=std::fwrite(data,1,size,f)==size;
#ifdef _WIN32
  if(std::fflush(f) || _commit(_fileno(f)))ok=false;
#else
  if(std::fflush(f) || fsync(fileno(f)))ok=false;
#endif
  if(std::fclose(f))ok=false;
  // std::filesystem::rename replaces the file in one step on Windows too, where std::rename refuses an existing one.
  std::error_code ec;
  if(ok){std::filesystem::rename(temp,path,ec);if(!ec)return true;}
  std::remove(temp.c_str());return false;
}
// A DS state carries the cart's save memory, so loading one would write the state's (maybe older) save over the file.
// States never carry the save here, as the GBA host's don't: DsCore::loadState(keepBattery) puts back the battery save
// the console held before the load, still waiting for its flush if it was.
struct DsPrefs {
  bool bottom=false, resume=false, hud=true;
  int battle=0, layout=DSL_ONE;
  int shader=POCKET_SHADER_OFF, colors=POCKET_COLORS_ORIGINAL;bool dim=false;  // SHADER / COLORS / DIM COMPANION (shader.h)
  bool wingLeft=false;  // WING SIDE: TOUCH IN WING's touch screen (and WINGS' party) in the left wing
  int background=POCKET_BACKGROUND_AUTO;  // BACKGROUND (backdrop.h): -1 = BLACK (a DS window is mostly room round the screens)
  bool operator!=(const DsPrefs& other)const{return bottom!=other.bottom || resume!=other.resume || battle!=other.battle || hud!=other.hud || layout!=other.layout || shader!=other.shader || colors!=other.colors || dim!=other.dim || wingLeft!=other.wingLeft || background!=other.background;}
  int look()const{return background==POCKET_BACKGROUND_AUTO?POCKET_BACKGROUND_BLACK:pocket_background_resolve(background,true,false);}
};
DsPrefs load_prefs(const std::string& path){
  DsPrefs p;std::ifstream f(path);std::string line;
  while(std::getline(f,line)){
    if(!line.empty() && line.back()=='\r')line.pop_back();
    if(line=="battle=0")p.battle=0;else if(line=="battle=1")p.battle=1;else if(line=="battle=2")p.battle=1;
    else if(line=="hud=1")p.hud=true;else if(line=="hud=0")p.hud=false;
    else if(line=="bottom=1")p.bottom=true;else if(line=="bottom=0")p.bottom=false;
    else if(line=="resume=1")p.resume=true;else if(line=="resume=0")p.resume=false;
    else if(line.rfind("layout=",0)==0){int v=std::atoi(line.c_str()+7);if(v>=0 && v<DSL_COUNT)p.layout=v;}
    else if(line.rfind("shader=",0)==0){int v=std::atoi(line.c_str()+7);if(v>=0 && v<POCKET_SHADERS)p.shader=v;}
    else if(line.rfind("colors=",0)==0){int v=std::atoi(line.c_str()+7);if(v>=0 && v<POCKET_COLORS_COUNT)p.colors=v;}
    else if(line=="dim=1")p.dim=true;else if(line=="dim=0")p.dim=false;
    else if(line=="wingside=1")p.wingLeft=true;else if(line=="wingside=0")p.wingLeft=false;
    else if(line.rfind("background=",0)==0){int v=std::atoi(line.c_str()+11);if(v>=-1 && v<POCKET_BACKGROUNDS)p.background=v;}
  }
  return p;
}
bool save_prefs(const std::string& path,const DsPrefs& p){
  std::string text="# PokeDaisy DS preferences\nbottom="+std::to_string(p.bottom)+"\nresume="+std::to_string(p.resume)+"\nbattle="+std::to_string(p.battle)+"\nhud="+std::to_string(p.hud)+"\nlayout="+std::to_string(p.layout)+"\nshader="+std::to_string(p.shader)+"\ncolors="+std::to_string(p.colors)+"\ndim="+std::to_string(p.dim)+"\nwingside="+std::to_string(p.wingLeft)+"\nbackground="+std::to_string(p.background)+"\n";
  return atomic_file(path,reinterpret_cast<const uint8_t*>(text.data()),text.size());
}
int keyboard(SDL_Keycode k){switch(k){
  case SDLK_z:return 0;case SDLK_x:return 1;case SDLK_BACKSPACE:return 2;case SDLK_RETURN:return 3;
  case SDLK_RIGHT:return 4;case SDLK_LEFT:return 5;case SDLK_UP:return 6;case SDLK_DOWN:return 7;
  case SDLK_e:return 8;case SDLK_q:return 9;case SDLK_s:return 10;case SDLK_a:return 11;
  case SDLK_TAB:return 12;case SDLK_SPACE:return 13;case SDLK_ESCAPE:return 15;default:return -1;}}
int pad(int b){
  if(pd_ds_xbox_layout)switch(b){case SDL_CONTROLLER_BUTTON_A:return 0;case SDL_CONTROLLER_BUTTON_B:return 1;
    case SDL_CONTROLLER_BUTTON_X:return 11;case SDL_CONTROLLER_BUTTON_Y:return 10;default:break;}
  switch(b){
  case SDL_CONTROLLER_BUTTON_B:return 0;case SDL_CONTROLLER_BUTTON_A:return 1;
  case SDL_CONTROLLER_BUTTON_BACK:return 2;case SDL_CONTROLLER_BUTTON_START:return 3;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return 4;case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return 5;
  case SDL_CONTROLLER_BUTTON_DPAD_UP:return 6;case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return 7;
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return 8;case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return 9;
  case SDL_CONTROLLER_BUTTON_Y:return 10;case SDL_CONTROLLER_BUTTON_X:return 11;
  case SDL_CONTROLLER_BUTTON_GUIDE:return 15;case SDL_CONTROLLER_BUTTON_RIGHTSTICK:return 14;default:return -1;}}
// DS LAYOUT's drawings for the picker: each layout placed by ds_place() on a 16:9 screen. ONE SCREEN and WINGS are drawn
// with the touch screen floated in over the dimmed game, as they show when the game wants it. All but TOUCH IN WING,
// WINGS and TOUCH PIECES are the app's.
const pd_layout_option* ds_layout_options(){
  static pd_layout_option options[DSL_COUNT];static bool made=false;
  if(made)return options;
  made=true;
  const int ow=1280,oh=720;
  auto box=[&](const DsBox& b,int part){pd_layout_box r;r.x=short(b.x*1000/ow);r.y=short(b.y*1000/oh);r.w=short(b.w*1000/ow);r.h=short(b.h*1000/oh);r.part=(unsigned char)part;return r;};
  for(int l=0;l<DSL_COUNT;l++){
    DsPlacement pl=ds_place(l,ow,oh,false,l==DSL_ONE || l==DSL_WINGS);
    pd_layout_option& o=options[l];
    o.name=ds_layout_name(l);o.original=l!=DSL_WING && l!=DSL_WINGS && l!=DSL_SPLIT;o.boxCount=0;
    for(auto& panel:pl.panel)if(panel.shown())o.box[o.boxCount++]=box(panel,PD_PART_PANEL);
    if(pl.top.shown())o.box[o.boxCount++]=box(pl.top,pl.touchFloats?PD_PART_DIM:PD_PART_GAME);
    if(pl.touch.shown())o.box[o.boxCount++]=box(pl.touch,PD_PART_TOUCH);
    if(l==DSL_SPLIT){
      DsPlacement pp=ds_place_with_pieces(&ds_pieces_hgss::GAME,l,ow,oh,false,false,false);
      DsPieceLayout pieces=ds_pieces_arrange(ds_pieces_hgss::GAME,0,pp.wing);
      if(pp.pieces && pieces.shown()){
        o.boxCount=0;o.box[o.boxCount++]=box(pp.top,PD_PART_GAME);
        for(auto& pc:pieces.placed)if(o.boxCount<PD_LAYOUT_BOXES)o.box[o.boxCount++]=box(pc.card,PD_PART_TOUCH);
        continue;
      }
    }
    for(auto& half:pl.half)if(half.shown())o.box[o.boxCount++]=box(half,PD_PART_TOUCH);
  }
  return options;
}
// For the host's Settings (main.c): DS LAYOUT's drawings, and the pick in DATA/ds-settings.ini.
extern "C" const pd_layout_option* pd_ds_layouts(int* count){*count=DSL_COUNT;return ds_layout_options();}
extern "C" int pd_ds_layout_load(const char* root){return load_prefs((std::filesystem::u8path(root)/"ds-settings.ini").string()).layout;}
extern "C" bool pd_ds_layout_save(const char* root,int layout){
  std::string path=(std::filesystem::u8path(root)/"ds-settings.ini").string();
  DsPrefs p=load_prefs(path);p.layout=std::clamp(layout,0,DSL_COUNT-1);return save_prefs(path,p);
}
extern "C" bool pd_ds_wing_left_load(const char* root){return load_prefs((std::filesystem::u8path(root)/"ds-settings.ini").string()).wingLeft;}
extern "C" bool pd_ds_wing_left_save(const char* root,bool left){
  std::string path=(std::filesystem::u8path(root)/"ds-settings.ini").string();
  DsPrefs p=load_prefs(path);p.wingLeft=left;return save_prefs(path,p);
}
// The SET tab's rows: SCREEN, DS LAYOUT, AUTO-RESUME, BATTLES, HUD, SHADER, COLORS, DIM COMPANION, WING SIDE, BACKGROUND.
constexpr int DS_SETTINGS=10;
struct Texture {SDL_Texture* p;~Texture(){SDL_DestroyTexture(p);}};
struct Audio {SDL_AudioDeviceID id=0;SDL_AudioStream* stream=nullptr;~Audio(){SDL_FreeAudioStream(stream);if(id)SDL_CloseAudioDevice(id);}};
void blit(SDL_Renderer* r,SDL_Texture* t,const uint32_t* px,int w,int h,int ow,int oh){
  SDL_UpdateTexture(t,nullptr,px,w*4);int scale=std::max(1,std::min(ow/w,oh/h));
  SDL_Rect dst{(ow-w*scale)/2,(oh-h*scale)/2,w*scale,h*scale};SDL_RenderCopy(r,t,nullptr,&dst);
}
// A DS screen drawn into its DS LAYOUT box.
void draw_screen(SDL_Renderer* r,SDL_Texture* t,const uint32_t* px,const DsBox& box){
  if(!box.shown())return;
  SDL_Rect dst{box.x,box.y,box.w,box.h};
  if(pocket_shader_active() && pocket_shader_draw(r,px,256,256,192,true,&dst))return;  // SHADER / COLORS
  SDL_UpdateTexture(t,nullptr,px,256*4);SDL_RenderCopy(r,t,nullptr,&dst);
}
// TOUCH PIECES' fallback: the touch screen's two halves, each into its wing.
void draw_halves(SDL_Renderer* r,SDL_Texture* t,const uint32_t* px,const DsPlacement& p){
  bool uploaded=false;
  for(int i=0;i<2;i++){
    const DsBox& box=p.half[i];if(!box.shown())continue;
    SDL_Rect dst{box.x,box.y,box.w,box.h},src{i*128,0,128,192};
    if(pocket_shader_active() && pocket_shader_draw(r,px+i*128,256,128,192,true,&dst))continue;
    if(!uploaded){SDL_UpdateTexture(t,nullptr,px,256*4);uploaded=true;}
    SDL_RenderCopy(r,t,&src,&dst);
  }
}
// A thin dark frame round an inset or floating screen, so it reads as its own screen over the other.
void frame_screen(SDL_Renderer* r,const DsBox& box){
  int t=std::max(2,box.w/128);SDL_SetRenderDrawColor(r,24,24,32,255);
  SDL_Rect sides[4]={{box.x-t,box.y-t,box.w+2*t,t},{box.x-t,box.y+box.h,box.w+2*t,t},{box.x-t,box.y,t,box.h},{box.x+box.w,box.y,t,box.h}};
  SDL_RenderFillRects(r,sides,4);
}
void companion(pd_canvas& c,const DsCompanion& s,bool supported,int tab,int selected,bool summary,int scroll,int page,const DsPrefs& prefs,int setting,const DsMapArt& art,int mapX,int mapY){
  pd_fill(&c,0,0,c.w,c.h,0x50a8a8);pd_title_box(&c,5,5,c.w-10,25,0xffffff);
  pd_text(&c,12,10,"PokeDaisy DS",0x303030,PD_NO_SHADOW);
  const char* tabs[]={"PARTY","BAG","DEX","STATUS","SET","MAP"};
  for(int i=0;i<6;i++){int x=5+i*(c.w-10)/6;pd_title_box(&c,x,34,(c.w-10)/6-2,23,i==tab?0xffefbc:0xffffff);pd_text(&c,x+8,38,tabs[i],0x303030,PD_NO_SHADOW);}
  pd_list_box(&c,5,61,c.w-10,c.h-86);int y=68;
  auto line=[&](const std::string& t){pd_text_fit(&c,13,y,c.w-26,t.c_str(),0x303030,PD_NO_SHADOW);y+=20;};
  if(tab==4){
    const std::string rows[]={std::string("SCREEN: ")+(prefs.bottom?"BOTTOM":"TOP"),std::string("DS LAYOUT: ")+ds_layout_name(prefs.layout),std::string("AUTO-RESUME: ")+(prefs.resume?"ON":"OFF"),std::string("BATTLES: ")+(prefs.battle?"AUTO":"OFF"),std::string("HUD: ")+(prefs.hud?"ON":"OFF"),std::string("SHADER: ")+pocket_shader_name(prefs.shader),std::string("COLORS: ")+pocket_colors_name(prefs.colors),std::string("DIM COMPANION: ")+(prefs.dim?"ON":"OFF"),std::string("DS WING SIDE: TOUCH ")+(prefs.wingLeft?"LEFT":"RIGHT"),std::string("BACKGROUND: ")+pocket_background_name(prefs.look())};
    for(int i=0;i<DS_SETTINGS;i++){if(i==setting)pd_fill(&c,10,y-2,c.w-20,18,0xffe4a0);pd_text_fit(&c,13,y,c.w-26,rows[i].c_str(),0x303030,PD_NO_SHADOW);y+=17;}
  }
  else if(!supported)line("Companion: Platinum USA Rev 1 only");
  else if(!s.valid){line("Waiting for the game's save data");line("Return to the game to finish booting");}
  else if(tab==5){
    if(art.background.empty())line("Map art unavailable");
    else {
      int ox=(c.w-art.width)/2,oy=61;
      for(int y=0;y<art.height;y++)for(int x=0;x<art.width;x++)c.px[(oy+y)*c.w+ox+x]=art.background[y*art.width+x]&0xffffff;
      auto sprite=[&](const std::vector<uint32_t>& img,int cx,int cy){for(int y=0;y<16;y++)for(int x=0;x<16;x++){
        int dx=cx-8+x,dy=cy-8+y;if(dx>=0 && dx<art.width && dy>=0 && dy<art.height && (img[y*16+x]>>24))c.px[(oy+dy)*c.w+ox+dx]=img[y*16+x]&0xffffff;
      }};
      for(auto mark:pd_ds_data::mapMarks){
        bool visited=mark.arrival<56 && ((s.arrivals>>mark.arrival)&1);
        if(mark.hidden && !visited)continue;
        int variant=visited?1+mark.palette:0,index=mark.shape*3+variant;
        if(index<int(art.blocks.size()))sprite(art.blocks[index],mark.x-pd_ds_data::mapLeft,mark.y-pd_ds_data::mapTop);
      }
      if(s.mapX>=0 && s.mapY>=0 && !art.heads.empty())sprite(art.heads[s.playerGender==1?1:0],pd_ds_data::mapOriginX+s.mapX*pd_ds_data::mapTile+3,pd_ds_data::mapOriginY+s.mapY*pd_ds_data::mapTile+3);
      int cx=ox+pd_ds_data::mapOriginX+mapX*pd_ds_data::mapTile,cy=oy+pd_ds_data::mapOriginY+mapY*pd_ds_data::mapTile;
      pd_clip(&c,ox,oy,art.width,art.height);
      pd_fill(&c,cx,cy,7,1,0xe84840);pd_fill(&c,cx,cy+6,7,1,0xe84840);pd_fill(&c,cx,cy,1,7,0xe84840);pd_fill(&c,cx+6,cy,1,7,0xe84840);pd_unclip(&c);
      pd_fill(&c,10,9,c.w-20,17,0xffffff);
      const char* name=ds_map_name(ds_map_section(mapX,mapY));pd_text_fit(&c,12,10,c.w-24,*name?name:"No location",0x303030,PD_NO_SHADOW);
    }
  }
  else if(tab==0 && summary && selected<int(s.party.size())){
    auto& m=s.party[selected];line(m.nickname+"  Lv"+std::to_string(m.level));
    line(std::string(ds_species(m.species))+"  HP "+std::to_string(m.hp)+"/"+std::to_string(m.maxHp));
    line(std::string("ITEM: ")+ds_item(m.heldItem));
    if(page){
      line(std::string("NATURE: ")+ds_nature(m.nature));
      line("EXP: "+std::to_string(m.experience)+(m.level==100?"  MAX LEVEL":""));
      line(std::string("STATUS: ")+ds_status(m));
      line(std::string(m.egg?"EGG  ":"")+"GENDER: "+(m.gender==1?"MALE":m.gender==2?"FEMALE":"NONE"));
    }else for(int i=0;i<4;i++)if(m.moves[i])line(std::string(ds_move(m.moves[i]))+"  PP "+std::to_string(m.pp[i]));
  }else if(tab==0){for(size_t i=0;i<s.party.size();i++){
    auto& m=s.party[i];if(int(i)==selected)pd_fill(&c,10,y-2,c.w-20,20,0xffe4a0);
    line(m.nickname+" Lv"+std::to_string(m.level)+"  "+std::to_string(m.hp)+"/"+std::to_string(m.maxHp));
  }}else if(tab==1){int rows=(c.h-100)/20;for(int i=scroll;i<int(s.bag.size()) && i<scroll+rows;i++)line(std::string(ds_item(s.bag[i].id))+" x"+std::to_string(s.bag[i].count));}
  else if(tab==2){
    if(!s.dexValid)line("Pokedex data unavailable");
    else {
      line("SEEN "+std::to_string(s.seenCount)+"  OWN "+std::to_string(s.caughtCount));
      int rows=(c.h-120)/20;
      for(int n=scroll+1;n<int(s.dexSeen.size()) && n<scroll+1+rows;n++){
        char number[16];std::snprintf(number,sizeof number,"%03d ",n);
        line(std::string(number)+(s.dexSeen[n]?ds_species(n):"???")+(s.dexCaught[n]?"  OWN":s.dexSeen[n]?"  SEEN":""));
      }
    }
  }
  else {line("TRAINER: "+s.trainer);line("MONEY: "+std::to_string(s.money));line(std::string("PLACE: ")+ds_place(s.map));line("MAP: "+std::to_string(s.map));line(s.dexValid?(s.nationalDex?"NATIONAL DEX: UNLOCKED":"NATIONAL DEX: LOCKED"):"DEX: UNAVAILABLE");}
  pd_text_fit(&c,9,c.h-20,c.w-18,tab==5?"D-pad move  A player  L/R tabs":tab==4?"Up/Down select  A change  L/R tabs":tab==0 && summary?"Left/Right info/moves  Up/Down Pokemon":tab==1 || tab==2?"Up/Down scroll  Left/Right page  L/R tabs":"L/R tabs  A summary  B back  L2 game",0xffffff,PD_NO_SHADOW);
}
}
// The party as the GBA panels and HUD read it: nicknames, HP, level, status (the DS species drawn by the caller).
pd_snapshot ds_party_snapshot(const DsCompanion& snap){
  pd_snapshot hs{};hs.valid=snap.valid;hs.partyCount=std::min(6,int(snap.party.size()));
  for(int i=0;i<hs.partyCount;i++){
    auto& m=snap.party[i];auto& h=hs.party[i];
    // A draw-only adapter. Do not put national DS IDs in its internal GBA species field (Burmy 412 collides with
    // its egg).
    h.species=m.egg?PD_SPECIES_EGG:0;h.hp=m.hp;h.maxHp=m.maxHp;h.level=m.level;h.status=m.status;
    std::snprintf(h.nickname,sizeof h.nickname,"%s",m.egg?"EGG":ds_species(m.species,snap.game));
  }
  return hs;
}
// A party icon (32x32, the cart's own) at x, y on a canvas, greyed when fainted; false when there's none.
bool ds_party_icon(pd_canvas* c,const DsMon& mon,const std::vector<std::vector<uint32_t>>& art,int x,int y,bool faded){
  int species=mon.egg?pd_ds_data::iconEgg:mon.species;
  if(species<=0 || size_t(species)>=art.size() || art[species].size()!=1024)return false;
  auto& icon=art[species];
  for(int yy=0;yy<32;yy++)for(int xx=0;xx<32;xx++){
    uint32_t p=icon[yy*32+xx];if(!(p>>24))continue;
    if(faded)p=((p&0xfefefe)>>1)&0xffffff;
    pd_fill(c,x+xx,y+yy,1,1,p&0xffffff);
  }
  return true;
}
// TOUCH IN WING's and WINGS' companion panels, each filling its wing: 0 the party (the cart's icons), 1 the
// trainer's facts. Drawn at a whole scale, at least 120 canvas pixels across.
constexpr int WING_W=512,WING_H=1024;
void set_color(SDL_Renderer* r,uint32_t c,uint8_t a=255){SDL_SetRenderDrawColor(r,c>>16,c>>8&0xFF,c&0xFF,a);}
// BLACK / DARK: a thin dark grey line round a screen beside the wings, so a bright edge doesn't meet pure black.
// TOUCH PIECES: a card in whole-pixel rounded steps (the app's pixel corners), its shadow first.
void fill_card(SDL_Renderer* r,const DsBox& b,int step,uint32_t color,uint8_t alpha=255){
  if(!b.shown())return;
  SDL_SetRenderDrawBlendMode(r,alpha<255?SDL_BLENDMODE_BLEND:SDL_BLENDMODE_NONE);set_color(r,color,alpha);
  SDL_Rect parts[3]={{b.x+2*step,b.y,b.w-4*step,b.h},{b.x+step,b.y+step,b.w-2*step,b.h-2*step},{b.x,b.y+2*step,b.w,b.h-4*step}};
  SDL_RenderFillRects(r,parts,3);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
}
// Each piece of the touch screen showing, on its card in its wing: the game's own button (corners see-through) or,
// for a piece of ours, its label in the app's font. art: a 256 x 192 ARGB texture with blending; label: 128 x 32.
void draw_pieces(SDL_Renderer* r,SDL_Texture* art,SDL_Texture* label,const DsPieceGame& game,const DsPieceLayout& lay,
                 const uint32_t* px,int background){
  const DsPieceScreen& sc=game.screens[lay.screen];
  pocket_backdrop look=pocket_backdrop_for(background);
  uint32_t card=look.cards?look.card:0xf8f8f8,edge=look.cards?look.border:0x283040,shadow=0x000000;
  int step=std::max(1,int(std::lround(lay.scale*2)));
  static std::vector<uint32_t> buf(256*192),piece;
  std::fill(buf.begin(),buf.end(),0u);
  for(int i=0;i<sc.count;i++){
    const DsPiece& p=sc.pieces[i];if(p.own)continue;
    piece.resize(size_t(p.w)*p.h);ds_piece_art(game,p,px,piece.data());
    for(int y=0;y<p.h;y++)std::memcpy(&buf[(p.y+y)*256+p.x],&piece[y*p.w],p.w*4);
  }
  SDL_UpdateTexture(art,nullptr,buf.data(),256*4);
  for(int i=0;i<sc.count;i++){
    const DsPiece& p=sc.pieces[i];const DsPiecePlaced& pl=lay.placed[i];if(!pl.card.shown())continue;
    int sh=std::max(1,step);
    fill_card(r,{pl.card.x+sh,pl.card.y+sh,pl.card.w,pl.card.h},step,shadow,look.cards?160:70);
    fill_card(r,pl.card,step,edge,look.cards?(look.borderAlpha?look.borderAlpha:255):255);
    int t=std::max(1,step/2);
    fill_card(r,{pl.card.x+t,pl.card.y+t,pl.card.w-2*t,pl.card.h-2*t},step,card);
    if(!p.own){
      SDL_Rect src{p.x,p.y,p.w,p.h},dst{pl.art.x,pl.art.y,pl.art.w,pl.art.h};SDL_RenderCopy(r,art,&src,&dst);
      continue;
    }
    // Our own piece: its label, centred on a face the game's button colour would sit on.
    std::vector<uint32_t> lp(128*32);pd_canvas c;pd_canvas_init(&c,128,32,lp.data());
    int lw=std::min<int>(128,p.lw),lh=std::min<int>(32,p.lh);
    uint32_t face=look.cards?0x3a4258:0x5878c0;
    pd_fill(&c,0,0,128,32,face);
    int tw=std::min(lw-4,pd_text_width(p.label));
    pd_text_fit(&c,(lw-tw)/2,(lh-16)/2,lw-4,p.label,0xf8f8f8,0x202838);
    SDL_UpdateTexture(label,nullptr,lp.data(),128*4);
    fill_card(r,pl.art,step,face);
    SDL_Rect src{0,0,lw,lh},dst{pl.art.x,pl.art.y,pl.art.w,pl.art.h};SDL_RenderCopy(r,label,&src,&dst);
  }
}
// A DS touch point to the window: the middle of where it lands on a laid out piece, false when it's on none.
bool ds_pieces_to_window(const DsPieceGame& game,const DsPieceLayout& lay,int x,int y,int& wx,int& wy){
  if(!lay.shown())return false;
  const DsPieceScreen& sc=game.screens[lay.screen];
  for(int i=0;i<sc.count;i++){const DsPiece& p=sc.pieces[i];const DsPiecePlaced& pl=lay.placed[i];
    if(x<p.x || y<p.y || x>=p.x+p.w || y>=p.y+p.h)continue;
    if(p.own){wx=pl.art.x+pl.art.w/2;wy=pl.art.y+pl.art.h/2;return true;}
    wx=pl.art.x+(x-p.x)*pl.art.w/p.w;wy=pl.art.y+(y-p.y)*pl.art.h/p.h;return true;}
  return false;
}
void gutter_screen(SDL_Renderer* r,const DsBox& box,int background){
  pocket_backdrop look=pocket_backdrop_for(background);if(!look.cards || !box.shown())return;
  int t=box.w>=768?2:1;set_color(r,look.gutter);
  SDL_Rect sides[4]={{box.x-t,box.y-t,box.w+2*t,t},{box.x-t,box.y+box.h,box.w+2*t,t},{box.x-t,box.y,t,box.h},{box.x+box.w,box.y,t,box.h}};
  SDL_RenderFillRects(r,sides,4);
}
void draw_wing_panel(SDL_Renderer* r,SDL_Texture* t,std::vector<uint32_t>& px,DsBox box,int which,const DsCompanion& s,bool supported,bool facts,const std::vector<std::vector<uint32_t>>& art,bool dim,int oh,int background,uint32_t idleMs){
  if(!box.shown() || box.w<64 || box.h<64)return;
  // BLACK / DARK: the wing is a card inset in its room, on the background, its content moved a step now and then.
  pocket_backdrop look=pocket_backdrop_for(background);int dx=0,dy=0;
  if(look.cards){int pad=std::max(3,box.w/40);pocket_wing_shift(background,SDL_GetTicks(),pad/3,&dx,&dy);box={box.x+pad,box.y+pad,box.w-2*pad-2,box.h-2*pad-2};}
  // One scale for both wings (from the wing's width and the window's height), whatever room each panel has.
  int scale=std::max(1,std::min(box.w/120,oh/300));
  int w=std::min(WING_W,box.w/scale),h=std::min(WING_H,box.h/scale);
  pd_canvas c;pd_canvas_init(&c,w,h,px.data());pd_fill(&c,0,0,w,h,look.card);
  pocket_wing_look wl;pocket_wing_look_begin(background,&wl);
  if(which==0){
    pd_snapshot hs=ds_party_snapshot(s);if(!facts)hs.valid=false;
    struct Ctx {const DsCompanion* s;const std::vector<std::vector<uint32_t>>* art;} ctx{&s,&art};
    pd_ui_party_panel_icons(&c,nullptr,&hs,[](pd_canvas* c,void* data,int slot,int x,int y,bool fainted){
      auto& ctx=*static_cast<Ctx*>(data);return ds_party_icon(c,ctx.s->party[slot],*ctx.art,x,y,fainted);
    },&ctx);
  }else{
    std::vector<std::string> rows;
    if(!facts)rows.push_back("COMPANION\tPLATINUM ONLY");
    else if(!s.valid)rows.push_back("WAITING\tFor the game's save");
    else{
      rows.push_back("TRAINER\t"+s.trainer);
      const char* place=ds_place(s.map,s.game);rows.push_back(std::string("PLACE\t")+(place&&*place?place:"?"));
      rows.push_back("MONEY\t$"+std::to_string(s.money));
      {int n=0;for(int b=s.badges;b;b&=b-1)n++;rows.push_back("BADGES\t"+std::to_string(n));} // a bit per badge
      if(s.dexValid)rows.push_back("POKEDEX\t"+std::to_string(s.seenCount)+" SEEN, "+std::to_string(s.caughtCount)+" OWN");
      if(s.repelSteps>0)rows.push_back("REPEL\t"+std::to_string(s.repelSteps)+" STEPS");
      // The time of day the game goes by (its clock is the computer's: core_melonds sets it from local time).
      std::time_t now=std::time(nullptr);
      if(const std::tm* local=std::localtime(&now))
        if(const char* period=ds_day_period(s.game,local->tm_hour))rows.push_back(std::string("TIME\t")+period);
      if(s.hasPots)
        for(int i=0;i<4;++i){
          const DsBerryPot& pot=s.pots[i];
          std::string name=pot.item?ds_item(pot.item,s.game):"";
          if(name.size()>6&&name.compare(name.size()-6,6," Berry")==0)name.resize(name.size()-6);
          rows.push_back("POT "+std::to_string(i+1)+"\t"+(pot.item?name+": "+ds_berry_pot_text(pot):std::string("EMPTY")));
        }
    }
    std::vector<const char*> list;for(auto& row:rows)list.push_back(row.c_str());
    pd_ui_facts_panel(&c,"TRAINER",list.data(),int(list.size()));
  }
  pocket_wing_look_end(&wl);
  SDL_Rect back{box.x+dx,box.y+dy,box.w,box.h};set_color(r,look.card);SDL_RenderFillRect(r,&back);
  if(look.cards){SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);set_color(r,look.border,look.borderAlpha);SDL_RenderDrawRect(r,&back);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);}
  SDL_Rect src{0,0,w,h};SDL_UpdateTexture(t,&src,px.data(),w*4);
  uint8_t v=pocket_wing_shade(background,dim,idleMs);SDL_SetTextureColorMod(t,v,v,v);
  SDL_Rect dst{box.x+dx+(box.w-w*scale)/2,box.y+dy+(box.h-h*scale)/2,w*scale,h*scale};SDL_RenderCopy(r,t,&src,&dst);
  SDL_SetTextureColorMod(t,255,255,255);
}
// The host's controller bookkeeping (main.c): a pad switched on or off mid-game is opened / closed there,
// so its buttons reach this loop and the GBA host still has it afterwards.
extern "C" { void (*pd_ds_device_event)(const SDL_Event*)=nullptr; }
// The host's own keys (main.c's FULLSCREEN: F11, Alt+Enter, L3 + R3): true = taken, the game doesn't get it.
extern "C" { bool (*pd_ds_host_event)(const SDL_Event*)=nullptr; }
// BUTTON LAYOUT XBOX: the bottom face button is A (SDL reports pads by position); else the right one, as before.
extern "C" { int pd_ds_xbox_layout=0; }
// The host's HOTKEYS (main.c's HA_* bits: 1 GAME MENU, 2 COMPANION, 4 SAVE STATE, 5 LOAD STATE, 8 / 9 DS LAYOUT
// NEXT / BACK, 10 SWAP SCREENS), read and cleared once a frame; and its screenshot, taken before a present.
extern "C" { unsigned (*pd_ds_host_actions)(void)=nullptr; }
// The host's present (its screenshot, then the picture turned for DISPLAY ORIENTATION), and a window point (0..1)
// in drawing pixels, turned back the same way; GAME TOUCH OFF leaves the DS touch screen to the D-pad.
extern "C" { void (*pd_ds_present)(void)=nullptr; void (*pd_ds_point)(float,float,float*,float*)=nullptr;
             int pd_ds_game_touch_off=0; }
// REWIND (the host's setting, or a REWIND HOLD hotkey): a state a second, the last 20 kept (DS states are a few MB,
// so the ring is coarser than the GBA's); held, it steps back a second every 6 frames, muted. States keep the battery
// save in memory, so rewinding never takes back a save the game wrote.
extern "C" { bool (*pd_ds_rewind_held)(void)=nullptr; int pd_ds_rewind_on=0; }
extern "C" int pd_ds_run(const char* rom,const char* root,int limit,const char* shot,SDL_Renderer* renderer,bool (*keepRunning)()) {
  auto bytes=read_file(rom);if(bytes.size()<512 || bytes.size()>512u*1024*1024){std::fprintf(stderr,"DS: invalid ROM size\n");return 1;}
  char hex[41];{pd_sha1 h;pd_sha1_init(&h);pd_sha1_update(&h,bytes.data(),bytes.size());pd_sha1_hex(&h,hex);}
  bool supported=!std::strcmp(hex,pd_ds_data::sha1);
  // HeartGold / SoulSilver: the TRAINER wing's facts only (time of day, Berry Pots, money, place) for now.
  // It only reads RAM, so a US cart's other dumps (by its header) count too.
  const bool johto=!supported&&(ds_companion_game(ds_canonical_sha1(hex))==DsGame::Johto||
                                ((!std::memcmp(bytes.data()+12,"IPKE",4)||!std::memcmp(bytes.data()+12,"IPGE",4))&&bytes[30]==0));char code[5]={};std::memcpy(code,bytes.data()+12,4);int revision=bytes[30];
  DsMapArt mapArt=supported?ds_map_art(bytes):DsMapArt{};
  auto partyArt=supported?ds_party_icons(bytes):std::vector<std::vector<uint32_t>>{};
  auto nds=ds_core_create();active=nds.get();
  std::fprintf(stderr,"%s %s rev %d SHA1 %s, companion %s\n",nds->name(),code,revision,hex,supported?"Platinum":"unsupported");
  std::deque<std::vector<uint8_t>> rewindRing;int rewindTick=0,rewindStep=0;
  std::string err;if(!nds->boot(rom,bytes,err)){std::fprintf(stderr,"DS boot failed: %s\n",err.c_str());return 1;}
  std::filesystem::path dir=std::filesystem::u8path(root);auto romPath=std::filesystem::u8path(rom);auto filename=romPath.filename().u8string();
  std::filesystem::create_directories(dir/"saves");std::filesystem::create_directories(dir/"states");
  // melonDS keeps the battery save beside the ROM as <name>.sav, as standalone melonDS and the Android app do, so a
  // synced ROMs folder carries it; its startup backups then go to DATA/backups, not into that folder.
  std::string save=nds->saveBesideRom()?(romPath.parent_path()/(romPath.stem().u8string()+".sav")).u8string():(dir/"saves"/(filename+".sav")).u8string();
  std::string tag=nds->stateTag(),state=(dir/"states"/(filename+tag+".ss")).u8string();
  std::string backups=(dir/"backups").u8string();const char* backupDir=pocket_backup_dir;
  if(nds->saveBesideRom() && !pocket_backup_dir)pocket_backup_dir=backups.c_str();
  bool backedUp=pocket_save_backup(save.c_str(),std::time(nullptr));
  pocket_backup_dir=backupDir;
  if(!backedUp)return 1;
  std::fprintf(stderr,"DS save: %s\n",save.c_str());
  if(std::filesystem::exists(std::filesystem::u8path(save))) {auto data=read_file(save);if(data.empty() || !nds->loadBattery(data)){std::fprintf(stderr,"DS: save size mismatch; refusing to overwrite it\n");return 1;}}
  std::string config=(dir/"ds-settings.ini").string(),resume=(dir/"states"/(filename+tag+".resume.ss")).u8string();
  DsPrefs prefs=load_prefs(config);
  if(auto value=std::getenv("POCKET_DS_RESUME"))prefs.resume=std::strcmp(value,"0")!=0;
  if(auto value=std::getenv("POCKET_DS_BATTLE"))prefs.battle=std::clamp(std::atoi(value),0,1);
  bool autoResumed=false;
  if(prefs.resume && std::filesystem::exists(resume)){
    if(std::filesystem::exists(std::filesystem::u8path(save)) && std::filesystem::last_write_time(std::filesystem::u8path(save))>std::filesystem::last_write_time(resume)){
      std::fprintf(stderr,"DS resume skipped: battery save is newer\n");
    }else{
      // Deserialize into a separate console: a failed load can mutate its target.
      // Keep the fresh console and the supplied battery save intact on failure.
      auto candidate=ds_core_create();std::string problem;
      auto data=read_file(resume);
      if(data.size()<8)std::fprintf(stderr,"DS resume ignored: truncated state; booting normally\n");
      else {
      bool ready=candidate->boot(rom,bytes,problem);
      if(ready && candidate->loadState(data,false,problem)){
        // The file's save wins over the state's copy (they match unless the file changed since the capture).
        if(std::filesystem::exists(std::filesystem::u8path(save)))candidate->restoreBattery(nds->battery(),false);
        nds=std::move(candidate);active=nds.get();autoResumed=true;std::fprintf(stderr,"DS auto-resumed at core frame %llu\n",(unsigned long long)nds->frameCount());
      }else std::fprintf(stderr,"DS resume ignored: %s; booting normally\n",problem.c_str());
      }
    }
  }
  bytes.clear();bytes.shrink_to_fit();
  if(!nds->start(err)){std::fprintf(stderr,"DS start failed: %s\n",err.c_str());return 1;}
  Audio audio;SDL_AudioSpec want{},got{};want.freq=32768;want.format=AUDIO_S16SYS;want.channels=2;want.samples=512;
  audio.id=SDL_OpenAudioDevice(nullptr,0,&want,&got,SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if(audio.id){audio.stream=SDL_NewAudioStream(AUDIO_S16SYS,2,int(std::lround(nds->audioRate())),got.format,got.channels,got.freq);SDL_PauseAudioDevice(audio.id,0);}
  Texture top{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,256,192)};
  Texture touchTex{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,256,192)};
  Texture overlay{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,341,256)};
  Texture ui{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,341,256)};
  if(!top.p || !touchTex.p || !ui.p || !overlay.p)return 1;
  SDL_SetTextureBlendMode(overlay.p,SDL_BLENDMODE_BLEND);
  std::vector<uint32_t> uiPixels(341*256);pd_canvas canvas;pd_canvas_init(&canvas,341,256,uiPixels.data());
  Texture wingTex[2]{{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,WING_W,WING_H)},{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,WING_W,WING_H)}};
  std::vector<uint32_t> wingPixels(size_t(WING_W)*WING_H);
  Texture pieceTex{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,256,192)};
  Texture pieceLabel{SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,128,32)};
  if(pieceTex.p)SDL_SetTextureBlendMode(pieceTex.p,SDL_BLENDMODE_BLEND);
  const DsPieceGame* pieceGame=ds_pieces_game(code);
  DsPieceLayout pieceLay;int pointerPiece=-1;  // TOUCH PIECES as last drawn; the piece a press began on
  const auto* field=supported?pk_nds_field_game(code,revision):nullptr;
  bool& bottom=prefs.bottom;
  bool done=false,showCompanion=false,showQuick=false,paused=false,summary=false,failed=false;
  int quickSelected=0;uint32_t blocked=0;
  bool layoutPick=false;int layoutSel=0,layoutScroll=0,layoutTiles[DSL_COUNT][4]={};
  // R3: a tap steps DS LAYOUT (skipping layouts drawn the same on this window), a hold opens the picker; the
  // layout's name shows a moment either way (toast). r3Chord: L3 + R3 went to FULLSCREEN, no step.
  uint32_t r3Since=0;bool r3Held=false,r3Chord=false;std::string toast;uint32_t toastUntil=0;
  auto showToast=[&](const char* text){toast=text;toastUntil=SDL_GetTicks()+1500;std::fprintf(stderr,"DS LAYOUT %s\n",text);};
  int mapX=-1,mapY=-1,setting=0,page=0,tab=0,selected=0,scroll=0,touchX=128,touchY=96;uint32_t keys=0,lastActivityKeys=0,wingActivity=SDL_GetTicks(),wingSign=0;unsigned repeat[8]={};int frame=0,lastPhase=-1,overrideScreen=-1;
  const int raw[]={1,0,11,10,9,8,2,3,15,-1,12,13};const bool rawTrimui=std::getenv("POCKET_RAW_TRIMUI")!=nullptr;
  struct Press{int at,n;uint32_t mask;};std::vector<Press> script,hostScript;
  // POCKET_DS_REWIND=AT:FRAMES holds REWIND from loop frame AT for FRAMES (headless checks).
  int rewindAt=-1,rewindFor=0;if(auto s=std::getenv("POCKET_DS_REWIND"))std::sscanf(s,"%d:%d",&rewindAt,&rewindFor);
  if(auto s=std::getenv("POCKET_DS_KEYS")){std::string text=s;size_t pos=0;while(pos<text.size()){Press p{};int used=0;if(std::sscanf(text.c_str()+pos,"%d:%u:%d%n",&p.at,&p.mask,&p.n,&used)!=3)break;script.push_back(p);pos+=used+1;}}
  if(auto s=std::getenv("POCKET_DS_HOST_KEYS")){std::string text=s;size_t pos=0;while(pos<text.size()){Press p{};int used=0;if(std::sscanf(text.c_str()+pos,"%d:%u:%d%n",&p.at,&p.mask,&p.n,&used)!=3)break;hostScript.push_back(p);pos+=used+1;}}
  int companionAt=std::getenv("POCKET_DS_COMPANION_AT")?std::atoi(std::getenv("POCKET_DS_COMPANION_AT")):-1;
  // Tests: left-button presses pushed through SDL's own queue (frame:x:y:frames, window pixels).
  struct Tap{int at,x,y,n;};std::vector<Tap> hostTaps;
  if(auto s=std::getenv("POCKET_DS_HOST_TAPS")){std::string text=s;size_t pos=0;while(pos<text.size()){Tap t{};int used=0;if(std::sscanf(text.c_str()+pos,"%d:%d:%d:%d%n",&t.at,&t.x,&t.y,&t.n,&used)!=4)break;hostTaps.push_back(t);pos+=used+1;}}
  // The mouse's left button and a finger on a touch display press the DS touch screen where it's drawn.
  bool pointerDown=false;const char* driver=SDL_GetCurrentVideoDriver();const bool headless=driver && !std::strcmp(driver,"dummy");int pointerX=0,pointerY=0;SDL_FingerID pointerFinger=-1;
  DsPlacement touchPlace;  // where the touch screen was last drawn, in output pixels (none = not shown)
  // DS LAYOUT: ONE SCREEN floats the touch screen in when the game wants it (PK_NDS_FIELD_TOUCH, held a few frames
  // so a flicker doesn't flash it). A game without touch rules can't say so, so its ONE SCREEN is PICTURE IN PICTURE.
  int touchOn=0,touchOff=0;bool touchWanted=false;
  auto pointerAt=[&](float px,float py,bool start){
    if(pd_ds_game_touch_off || !ds_touch_shown(touchPlace) || showCompanion || showQuick || paused){pointerDown=false;return;}
    int x,y;
    if(touchPlace.pieces && pieceLay.shown()){
      if(start)pointerPiece=ds_pieces_hit(*pieceGame,pieceLay,int(px),int(py),x,y);
      else if(pointerPiece>=0 && pointerPiece<int(pieceLay.placed.size()))ds_pieces_drag(*pieceGame,pieceLay,pointerPiece,int(px),int(py),x,y);
      else {pointerDown=false;return;}
      if(pointerPiece<0){if(start)pointerDown=false;return;}
    }else if(!ds_touch_at(touchPlace,int(px),int(py),!start,x,y)){if(start)pointerDown=false;return;}
    pointerX=x;pointerY=y;if(start)pointerDown=true;
  };
  // Mouse events are in window coordinates; the picture is laid out in the renderer's output pixels.
  auto toOutput=[&](int wx,int wy,float& px,float& py){
    int ww=1,wh=1,ow=1,oh=1;SDL_GetWindowSize(SDL_RenderGetWindow(renderer),&ww,&wh);
    if(pd_ds_point){pd_ds_point(float(wx)/std::max(1,ww),float(wy)/std::max(1,wh),&px,&py);return;}
    SDL_GetRendererOutputSize(renderer,&ow,&oh);
    px=float(wx)*ow/std::max(1,ww);py=float(wy)*oh/std::max(1,wh);
  };
  uint64_t started=SDL_GetPerformanceCounter();uint32_t lastWrites=nds->batteryWrites();int quiet=autoResumed?60:0;
  while(!done && keepRunning() && (!limit || frame<limit)) {
    DsPrefs previous=prefs;
    uint64_t tick=SDL_GetPerformanceCounter();uint32_t pressed=0;SDL_Event e;
    auto key=[&](int k,bool down){if(k<0)return;uint32_t bit=1u<<k;if(down){if(!(keys&bit))pressed|=bit;keys|=bit;}else keys&=~bit;};
    for(auto t:hostTaps)if(frame==t.at || frame==t.at+t.n){
      SDL_Event m{};m.type=frame==t.at?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;m.button.button=SDL_BUTTON_LEFT;m.button.which=0;
      m.button.state=frame==t.at?SDL_PRESSED:SDL_RELEASED;m.button.x=t.x;m.button.y=t.y;SDL_PushEvent(&m);
    }
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT)done=true;
      if(e.type==SDL_JOYDEVICEADDED || e.type==SDL_CONTROLLERDEVICEADDED || e.type==SDL_CONTROLLERDEVICEREMOVED){
        if(pd_ds_device_event)pd_ds_device_event(&e);
        if(e.type==SDL_CONTROLLERDEVICEREMOVED)keys=0;  // nothing stays pressed on a pad that's gone
        continue;
      }
      if(e.type==SDL_CONTROLLERBUTTONDOWN && e.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSTICK && (keys&(1u<<14)))r3Chord=true;
      if(pd_ds_host_event && pd_ds_host_event(&e))continue;
      int k=-1;bool down=false;
      if(e.type==SDL_KEYDOWN || e.type==SDL_KEYUP){if(e.key.repeat)continue;k=keyboard(e.key.keysym.sym);down=e.type==SDL_KEYDOWN;}
      if(e.type==SDL_CONTROLLERBUTTONDOWN || e.type==SDL_CONTROLLERBUTTONUP){k=pad(e.cbutton.button);down=e.type==SDL_CONTROLLERBUTTONDOWN;}
      if(e.type==SDL_CONTROLLERAXISMOTION && (e.caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT)){k=e.caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT?12:13;down=e.caxis.value>16000;}
      if(e.type==SDL_CONTROLLERAXISMOTION){
        if(e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX){key(5,e.caxis.value<-16000);key(4,e.caxis.value>16000);}
        if(e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTY){key(6,e.caxis.value<-16000);key(7,e.caxis.value>16000);}
      }
      if((e.type==SDL_JOYBUTTONDOWN || e.type==SDL_JOYBUTTONUP) && rawTrimui && !SDL_IsGameController(0)){k=e.jbutton.button<12?raw[e.jbutton.button]:-1;down=e.type==SDL_JOYBUTTONDOWN;}
      if(rawTrimui && !SDL_IsGameController(0)){
        if(e.type==SDL_JOYHATMOTION){key(4,e.jhat.value&SDL_HAT_RIGHT);key(5,e.jhat.value&SDL_HAT_LEFT);key(6,e.jhat.value&SDL_HAT_UP);key(7,e.jhat.value&SDL_HAT_DOWN);}
        if(e.type==SDL_JOYAXISMOTION && (e.jaxis.axis==2 || e.jaxis.axis==5)){k=e.jaxis.axis==2?12:13;down=e.jaxis.value>16000;}
      }
      key(k,down);
      if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT && e.button.which!=SDL_TOUCH_MOUSEID){float px,py;toOutput(e.button.x,e.button.y,px,py);pointerAt(px,py,true);}
      if(e.type==SDL_MOUSEMOTION && pointerDown && pointerFinger<0 && (e.motion.state&SDL_BUTTON_LMASK) && e.motion.which!=SDL_TOUCH_MOUSEID){float px,py;toOutput(e.motion.x,e.motion.y,px,py);pointerAt(px,py,false);}
      if(e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_LEFT && e.button.which!=SDL_TOUCH_MOUSEID && pointerFinger<0)pointerDown=false;
      auto fingerAt=[&](float nx,float ny,float& px,float& py){
        if(pd_ds_point){pd_ds_point(nx,ny,&px,&py);return;}
        int ow=1,oh=1;SDL_GetRendererOutputSize(renderer,&ow,&oh);px=nx*ow;py=ny*oh;
      };
      if(e.type==SDL_FINGERDOWN && !pointerDown){float px,py;fingerAt(e.tfinger.x,e.tfinger.y,px,py);pointerAt(px,py,true);if(pointerDown)pointerFinger=e.tfinger.fingerId;}
      if(e.type==SDL_FINGERMOTION && pointerDown && e.tfinger.fingerId==pointerFinger){float px,py;fingerAt(e.tfinger.x,e.tfinger.y,px,py);pointerAt(px,py,false);}
      if(e.type==SDL_FINGERUP && e.tfinger.fingerId==pointerFinger){pointerDown=false;pointerFinger=-1;}
      if(!pointerDown)pointerFinger=-1;
      // A headless (dummy driver) window has no focus to lose: it pausing there kept test runs from playing.
      if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_FOCUS_LOST && !headless){keys=0;pointerDown=false;nds->setButtons(0);nds->setTouch(0,0,false);paused=true;}
    }
    // Test the same host input path as real controls, rather than only guest buttons.
    for(auto p:hostScript)for(int k=0;k<16;k++)if(p.mask&(1u<<k)){
      if(frame==p.at)key(k,true);else if(frame==p.at+p.n)key(k,false);
    }
    unsigned now=SDL_GetTicks();for(int k=4;k<8;k++){if(pressed&(1u<<k))repeat[k]=now+400;else if((keys&(1u<<k)) && int(now-repeat[k])>=0){pressed|=1u<<k;repeat[k]=now+90;}}
    if(companionAt>=0 && frame==companionAt)showCompanion=true;
    if(unsigned act=pd_ds_host_actions?pd_ds_host_actions():0u){
      if(act&(1u<<1))pressed|=1u<<15;                                 // GAME MENU: the pause menu
      if(act&(1u<<2)){showCompanion=!showCompanion;showQuick=false;}  // COMPANION
      if(act&((1u<<8)|(1u<<9))){
        prefs.layout=(prefs.layout+((act&(1u<<8))?1:DSL_COUNT-1))%DSL_COUNT;
        showToast(ds_layout_name(prefs.layout));
      }
      if(act&(1u<<10))pressed|=1u<<13;                                // SWAP SCREENS: R2's own
      if(act&((1u<<4)|(1u<<5))){paused=true;pressed=(act&(1u<<4))?1u:1u<<10;}  // SAVE / LOAD STATE, as the quick menu's
    }
    blocked &= keys;
    // SELECT + R2 / L2 step DS LAYOUT forward / back (the app's R2 hotkey); SELECT is then kept from the game.
    if((keys&(1u<<2)) && (pressed&((1u<<12)|(1u<<13))) && !showCompanion && !paused){
      prefs.layout=(prefs.layout+((pressed&(1u<<13))?1:DSL_COUNT-1))%DSL_COUNT;
      showToast(ds_layout_name(prefs.layout));
      blocked|=keys&4095u;pressed&=~((1u<<12)|(1u<<13));nds->setButtons(0);nds->setTouch(0,0,false);
    }
    if(pressed&(1u<<14)){r3Since=SDL_GetTicks();r3Held=false;r3Chord=false;}
    if(r3Since && !showCompanion && !showQuick && !paused){
      bool r3=(keys&(1u<<14))!=0;
      if(r3 && !r3Held && !r3Chord && SDL_GetTicks()-r3Since>=450){
        // Held: the picker with every layout drawn (in the companion, where its SET row opens it too).
        r3Held=true;showCompanion=true;layoutPick=true;layoutSel=prefs.layout;layoutScroll=0;
        blocked|=keys&4095u;nds->setButtons(0);nds->setTouch(0,0,false);
      }else if(!r3){
        if(!r3Held && !r3Chord){
          int ow=1024,oh=768;SDL_GetRendererOutputSize(renderer,&ow,&oh);
          prefs.layout=ds_layout_cycle(prefs.layout,ow,oh,prefs.wingLeft,field!=nullptr);
          showToast(ds_layout_name(prefs.layout));
        }
        r3Since=0;
      }
    }else if(!(keys&(1u<<14)))r3Since=0;
    if(pressed&(1u<<12)){
      if(showCompanion)showCompanion=false;else showQuick=!showQuick;
      paused=false;blocked|=keys&4095u;nds->setButtons(0);nds->setTouch(0,0,false);
    }
    if(auto value=std::getenv("POCKET_DS_QUICK_AT"))if(frame==std::atoi(value))showQuick=true;
    int fieldBits=field?pk_nds_field_state(field,read32,write32,0):0;
    bool inBattle=(fieldBits&PK_NDS_FIELD_BATTLE)!=0;
    bool choosing=inBattle && pk_nds_battle_command(field,read32)==pk_nds_battle_selecting(field);
    int phase=inBattle?(choosing?2:1):0;
    if(phase!=lastPhase){overrideScreen=-1;lastPhase=phase;std::fprintf(stderr,"DS battle phase %d at core frame %llu\n",phase,(unsigned long long)nds->frameCount());}
    auto view=ds_battle_view(prefs.battle,inBattle,choosing,bottom,overrideScreen);
    if(pressed&(1u<<13)){
      if(prefs.battle && inBattle){
        overrideScreen=overrideScreen<0?(view.bottom?0:1):-1;
      }else bottom=!bottom;
    }
    view=ds_battle_view(prefs.battle,inBattle,choosing,bottom,overrideScreen);
    bool wants=field && !inBattle && (fieldBits&PK_NDS_FIELD_TOUCH);
    if(wants){touchOff=0;if(++touchOn>=2)touchWanted=true;}else{touchOn=0;if(++touchOff>=6)touchWanted=false;}
    int layout=prefs.layout==DSL_ONE && !field?DSL_PIP:prefs.layout;
    if(choosing && std::getenv("POCKET_DS_STOP_ON_BATTLE_INPUT"))limit=frame+1;
    if(pressed&(1u<<15)){paused=!paused;showQuick=false;blocked|=keys&4095u;}
    DsCompanion snap=supported?ds_read_companion(nds->mainRam(),nds->mainRamSize()):
      johto?ds_read_companion(nds->mainRam(),nds->mainRamSize(),DsGame::Johto):DsCompanion{};
    quick_item quickItems[4]{};
    quickItems[0].action=QUICK_COMPANION;
    quickItems[1].action=QUICK_SAVE_STATE;
    quickItems[2].action=QUICK_LOAD_STATE;
    quickItems[2].why=std::filesystem::exists(state)?nullptr:"No saved state";
    quickItems[3].action=QUICK_COMPANION;quickItems[3].label="LEAVE GAME";
    if(showQuick){
      blocked|=keys&4095u;
      int dx=(pressed&(1u<<4)?1:0)-(pressed&(1u<<5)?1:0);
      int dy=(pressed&(1u<<7)?1:0)-(pressed&(1u<<6)?1:0);
      if(dx || dy)quickSelected=quick_step(4,quickSelected,dx,dy,341,256);
      if(pressed&(1u<<1))showQuick=false;
      else if((pressed&1) && !quickItems[quickSelected].why){
        showQuick=false;
        if(quickSelected==0)showCompanion=true;
        else if(quickSelected==3)done=true;
        else {paused=true;pressed=quickSelected==1?1u:1u<<10;}
      }
      // Never forward the key which opened, navigated or closed the overlay.
      if(showQuick || showCompanion || done)pressed=0;
    }
    if(paused){if(pressed&(1u<<1))paused=false;if(pressed&(1u<<3))done=true;
      if(pressed&1){std::vector<uint8_t> w;if(!nds->saveState(w,err) || !atomic_file(state,w.data(),w.size())){std::fprintf(stderr,"DS state save failed: %s\n",err.c_str());failed=true;done=true;}paused=false;}
      if(pressed&(1u<<10)){auto data=read_file(state);if(!data.empty()){rewindRing.clear();if(!nds->loadState(data,true,err)){failed=true;done=true;std::fprintf(stderr,"DS state load failed: %s\n",err.c_str());}else std::fprintf(stderr,"DS manual state loaded at core frame %llu\n",(unsigned long long)nds->frameCount());}paused=false;}
    }else if(showQuick){
      nds->setButtons(0);nds->setTouch(0,0,false);
    }else if(showCompanion){
      blocked|=keys&4095u;
      // DS LAYOUT's picker (A on its SET row): the D-pad walks the tiles, A wears one, B leaves it as it was.
      if(layoutPick){
        int cols=pd_layout_picker_columns(341);
        if(pressed&(1u<<4))layoutSel=std::min(DSL_COUNT-1,layoutSel+1);
        if(pressed&(1u<<5))layoutSel=std::max(0,layoutSel-1);
        if(pressed&(1u<<6))layoutSel=std::max(0,layoutSel-cols);
        if(pressed&(1u<<7))layoutSel=std::min(DSL_COUNT-1,layoutSel+cols);
        if(pressed&1){prefs.layout=layoutSel;layoutPick=false;showToast(ds_layout_name(prefs.layout));if(r3Held)showCompanion=false;}
        if(pressed&(1u<<1)){layoutPick=false;if(r3Held)showCompanion=false;}
        if(!layoutPick)r3Held=false;
        pressed=0;
      }
      if(tab==0 && summary && (pressed&((1u<<4)|(1u<<5))))page=1-page;
      if(pressed&(1u<<8)){tab=(tab+1)%6;summary=false;scroll=0;}
      if(pressed&(1u<<9)){tab=(tab+5)%6;summary=false;scroll=0;}
      if(tab==5){
        int minY=std::max(0,int(std::ceil(double(-pd_ds_data::mapOriginY-3)/pd_ds_data::mapTile)));
        int maxY=std::min(29,int((pd_ds_data::mapHeight-1-pd_ds_data::mapOriginY-3)/pd_ds_data::mapTile));
        if(mapX<0 || (pressed&((1u<<8)|(1u<<9)|1))){mapX=snap.mapX>=0?snap.mapX:0;mapY=snap.mapY>=0?snap.mapY:minY;}
        if(pressed&(1u<<4))mapX++;
        if(pressed&(1u<<5))mapX--;
        if(pressed&(1u<<6))mapY--;
        if(pressed&(1u<<7))mapY++;
        mapX=std::clamp(mapX,0,29);mapY=std::clamp(mapY,minY,maxY);
      }
      if(tab==4){
        if(pressed&(1u<<6))setting=(setting+DS_SETTINGS-1)%DS_SETTINGS;
        if(pressed&(1u<<7))setting=(setting+1)%DS_SETTINGS;
        if(pressed&1){if(setting==0)bottom=!bottom;else if(setting==1){layoutPick=true;layoutSel=prefs.layout;}else if(setting==2)prefs.resume=!prefs.resume;else if(setting==3)prefs.battle=1-prefs.battle;else if(setting==4)prefs.hud=!prefs.hud;
          else if(setting==5)prefs.shader=(prefs.shader+1)%POCKET_SHADERS;else if(setting==6)prefs.colors=(prefs.colors+1)%POCKET_COLORS_COUNT;else if(setting==7)prefs.dim=!prefs.dim;else if(setting==8)prefs.wingLeft=!prefs.wingLeft;else prefs.background=(prefs.look()+1)%POCKET_BACKGROUNDS;}
      }
      if(tab!=4 && tab!=5 && (pressed&(1u<<6))){if(tab==1 || tab==2)scroll=std::max(0,scroll-1);else selected=std::max(0,selected-1);}
      if(tab!=4 && tab!=5 && (pressed&(1u<<7))){if(tab==1 || tab==2)scroll=std::min(std::max(0,(tab==1?int(snap.bag.size()):int(snap.dexSeen.size())-1)-1),scroll+1);else selected=std::min(std::max(0,int(snap.party.size())-1),selected+1);}
      if(tab==1 || tab==2){
        int last=std::max(0,(tab==1?int(snap.bag.size()):int(snap.dexSeen.size())-1)-1);
        if(pressed&(1u<<4))scroll=std::min(last,scroll+7);
        if(pressed&(1u<<5))scroll=std::max(0,scroll-7);
      }
      if(tab==0 && (pressed&1))summary=true;
      if(pressed&(1u<<1)){if(summary)summary=false;else showCompanion=false;}
    }else if(!done){
      uint32_t gameKeys=keys&~blocked;
      bool touchMode=ds_touch_shown(touchPlace) && (gameKeys&(1u<<2));
      if(touchMode){if(gameKeys&(1u<<4))touchX=std::min(255,touchX+3);if(gameKeys&(1u<<5))touchX=std::max(0,touchX-3);if(gameKeys&(1u<<6))touchY=std::max(0,touchY-3);if(gameKeys&(1u<<7))touchY=std::min(191,touchY+3);}
      if(!ds_touch_shown(touchPlace))pointerDown=false;
      if(pointerDown)nds->setTouch(pointerX,pointerY,true);
      else nds->setTouch(touchX,touchY,touchMode && (gameKeys&1));
      uint32_t held=touchMode?0:gameKeys&4095u;for(auto p:script)if(frame>=p.at && frame<p.at+p.n)held|=p.mask;
      bool rewindScripted=rewindAt>=0 && frame>=rewindAt && frame<rewindAt+rewindFor;
      bool rewinding=pd_ds_rewind_on && ((pd_ds_rewind_held && pd_ds_rewind_held()) || rewindScripted) && !rewindRing.empty();
      if(rewinding){
        if(rewindStep++%6==0){
          if(!nds->loadState(rewindRing.back(),true,err))std::fprintf(stderr,"DS rewind failed: %s\n",err.c_str());
          if(rewindRing.size()>1)rewindRing.pop_back();
          if(rewindScripted)std::fprintf(stderr,"DS rewind to core frame %llu (%zu kept)\n",(unsigned long long)nds->frameCount(),rewindRing.size());
          nds->setButtons(0);nds->runFrame();
        }
        int16_t drop[2048];while(nds->takeAudio(drop,1024)){}
      }else{
        rewindStep=0;
        if(pd_ds_rewind_on && ++rewindTick>=60){
          rewindTick=0;std::vector<uint8_t> w;
          if(nds->saveState(w,err)){rewindRing.push_back(std::move(w));if(rewindRing.size()>20)rewindRing.pop_front();}
        }
        nds->setButtons(held);
        nds->runFrame();
      }
      if(nds->batteryWrites()!=lastWrites){lastWrites=nds->batteryWrites();quiet=0;}else quiet++;
      if(quiet>=60 && nds->batteryDirty()){auto s=nds->battery();if(!s.empty() && !atomic_file(save,s.data(),s.size())){failed=true;done=true;}else nds->clearBatteryDirty();}
      int16_t samples[2048];size_t n;while((n=nds->takeAudio(samples,1024)))if(audio.stream)SDL_AudioStreamPut(audio.stream,samples,int(n*4));
      if(audio.stream){int available=SDL_AudioStreamAvailable(audio.stream);std::vector<uint8_t> out(std::max(0,available));if(available>0){SDL_AudioStreamGet(audio.stream,out.data(),available);if(SDL_GetQueuedAudioSize(audio.id)>unsigned(got.freq*2))SDL_ClearQueuedAudio(audio.id);SDL_QueueAudio(audio.id,out.data(),available);}}
    }
    if(prefs!=previous && !save_prefs(config,prefs))std::fprintf(stderr,"DS settings write failed\n");
    if((paused || showCompanion || showQuick) && audio.id)SDL_ClearQueuedAudio(audio.id);
    int ow=1024,oh=768;
    if(!ds_display_pixels(renderer,ow,oh)){std::fprintf(stderr,"DS display setup failed: %s\n",SDL_GetError());failed=true;break;}
    const int look=prefs.look();
    set_color(renderer,showCompanion?0:pocket_backdrop_for(look).outside);SDL_RenderClear(renderer);SDL_SetRenderDrawColor(renderer,0,0,0,255);
    pocket_shader_set(prefs.shader,prefs.colors,true);
    // The still wings' dim: any press, a finger, or the wings' facts changing count as activity.
    {uint32_t sign=uint32_t(snap.map)*2654435761u^uint32_t(snap.money)^uint32_t(snap.badges)<<20^uint32_t(snap.party.size())<<26;
     for(auto& m:snap.party)sign=sign*31+uint32_t(m.hp);
     if(keys!=lastActivityKeys || pointerDown || sign!=wingSign){wingActivity=SDL_GetTicks();lastActivityKeys=keys;wingSign=sign;}}
    bool wingPanels=false;  // the party is in a wing: no HUD party row over the game
    if(showCompanion){companion(canvas,snap,supported,tab,selected,summary,scroll,page,prefs,setting,mapArt,mapX,mapY);
      if(layoutPick)pd_layout_picker_draw(&canvas,"DS LAYOUT",ds_layout_options(),DSL_COUNT,prefs.layout,layoutSel,-1,&layoutScroll,layoutTiles);
      uint8_t v=prefs.dim?150:255;SDL_SetTextureColorMod(ui.p,v,v,v);blit(renderer,ui.p,uiPixels.data(),341,256,ow,oh);SDL_SetTextureColorMod(ui.p,255,255,255);}
    else{
      DsPlacement place=ds_place_with_pieces(pieceGame,layout,ow,oh,view.bottom,touchWanted && !view.bottom,prefs.wingLeft);
      DsPieceLayout lay;
      if(place.pieces){int sc=ds_pieces_detect(*pieceGame,nds->framebuffer(1));if(sc>=0)lay=ds_pieces_arrange(*pieceGame,sc,place.wing);}
      if(lay.shown()!=pieceLay.shown() || lay.screen!=pieceLay.screen)pointerDown=false;  // the screen changed under a press
      bool topInset=place.inset && view.bottom;  // PICTURE IN PICTURE swapped: the top screen is the small one
      if(topInset)draw_screen(renderer,touchTex.p,nds->framebuffer(1),place.touch);
      draw_screen(renderer,top.p,nds->framebuffer(0),place.top);
      // TOUCH IN WING / WINGS: the companion's panels in the wings (under a floating touch screen's dim).
      if(place.panel[0].shown() || place.panel[1].shown() || place.split())gutter_screen(renderer,topInset?place.touch:place.top,look);
      for(int i=0;i<2;i++)if(wingTex[i].p)draw_wing_panel(renderer,wingTex[i].p,wingPixels,place.panel[i],i,snap,supported,supported||johto,partyArt,prefs.dim,oh,look,SDL_GetTicks()-wingActivity);
      wingPanels=place.panel[0].shown();
      if(place.touchFloats){SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer,0,0,0,150);SDL_RenderFillRect(renderer,nullptr);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);}
      if(place.touchFloats || place.inset)frame_screen(renderer,topInset?place.top:place.touch);
      if(topInset)draw_screen(renderer,top.p,nds->framebuffer(0),place.top);
      else draw_screen(renderer,touchTex.p,nds->framebuffer(1),place.touch);
      if(lay.shown() && pieceTex.p && pieceLabel.p)draw_pieces(renderer,pieceTex.p,pieceLabel.p,*pieceGame,lay,nds->framebuffer(1),look);
      else if(place.split()){for(int i=0;i<2;i++)gutter_screen(renderer,place.half[i],look);draw_halves(renderer,touchTex.p,nds->framebuffer(1),place);}
      pieceLay=paused?DsPieceLayout{}:lay;
      touchPlace=paused?DsPlacement{}:place;
    }
    if(showCompanion){touchPlace={};pieceLay={};}
    bool hudShown=!showCompanion && !paused && (showQuick || (prefs.hud && !wingPanels && snap.valid && !view.bottom && !inBattle && (fieldBits&PK_NDS_FIELD_SCREEN) && !(fieldBits&(PK_NDS_FIELD_LOCKED|PK_NDS_FIELD_MENU))));
    bool toastShown=!showCompanion && !paused && !toast.empty() && int(toastUntil-SDL_GetTicks())>0;
    if(hudShown || toastShown){
      for(auto& p:uiPixels)p=QUICK_CLEAR;
      if(toastShown){int tw=pd_text_width(toast.c_str())+20;pd_title_box(&canvas,(341-tw)/2,8,tw,22,0xffffff);pd_text(&canvas,(341-tw)/2+10,12,toast.c_str(),0x303030,PD_NO_SHADOW);}
      if(!hudShown){}
      else if(showQuick)quick_draw(&canvas,nullptr,quickItems,4,quickSelected);
      else {
        pd_snapshot hs=ds_party_snapshot(snap);hs.valid=true;
        struct HudArt {const DsCompanion* snap;const pd_snapshot* view;const std::vector<std::vector<uint32_t>>* art;} context{&snap,&hs,&partyArt};
        hud_party_draw(&canvas,&hs,&context,[](pd_canvas* c,void* data,const pd_mon* m,int x,int y,bool faded){
          auto& ctx=*static_cast<HudArt*>(data);auto& mon=ctx.snap->party[m-ctx.view->party];
          if(!ds_party_icon(c,mon,*ctx.art,x,y,faded))pd_text_fit(c,x,y+10,32,m->nickname,faded?0xa8b0c0:0xf8f8f8,0x303840);
        });
      }
      quick_finish(&canvas);blit(renderer,overlay.p,uiPixels.data(),341,256,ow,oh);
    }
    if(ds_touch_shown(touchPlace) && !showCompanion && !paused && !showQuick && (keys&(1u<<2))){
      int x,y;bool on=true;
      if(touchPlace.pieces && pieceLay.shown())on=ds_pieces_to_window(*pieceGame,pieceLay,touchX,touchY,x,y);else ds_touch_to_window(touchPlace,touchX,touchY,x,y);
      if(on){
      SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_Rect outline{x-5,y-5,11,11};SDL_RenderDrawRect(renderer,&outline);
      SDL_SetRenderDrawColor(renderer,255,255,255,255);SDL_RenderDrawLine(renderer,x-4,y,x+4,y);SDL_RenderDrawLine(renderer,x,y-4,x,y+4);
    }}
    if(paused){pd_fill(&canvas,0,0,341,256,0x303838);pd_title_box(&canvas,15,40,311,170,0xffffff);pd_text(&canvas,28,55,"PokeDaisy DS / PAUSED",0x303030,PD_NO_SHADOW);pd_text(&canvas,28,90,"B resume   A save state",0x303030,PD_NO_SHADOW);pd_text(&canvas,28,120,"X load state   START quit",0x303030,PD_NO_SHADOW);blit(renderer,ui.p,uiPixels.data(),341,256,ow,oh);}
    if(shot && limit && frame==limit-1){std::vector<uint32_t> pixels(size_t(ow)*oh);if(!SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),ow*4))pd_png_write(shot,ow,oh,pixels.data());}
    if(pd_ds_present)pd_ds_present();else SDL_RenderPresent(renderer);
    frame++;
    if(!limit){double ms=1000.0*(SDL_GetPerformanceCounter()-tick)/SDL_GetPerformanceFrequency();if(ms<1000.0/nds->fps())SDL_Delay(unsigned(1000.0/nds->fps()-ms));}
  }
  nds->setButtons(0);nds->setTouch(0,0,false);
  // Let an in-progress battery write finish before replacing the disk copy.
  // A bounded failure preserves the last completed save and its startup backup.
  if(!failed && nds->batteryDirty()){
    for(int i=0;i<300 && quiet<60;i++){
      nds->runFrame();
      if(nds->batteryWrites()!=lastWrites){lastWrites=nds->batteryWrites();quiet=0;}else quiet++;
    }
    if(quiet<60){std::fprintf(stderr,"DS: save never settled; preserving disk save\n");failed=true;}
  }
  if(!failed && nds->batteryDirty()){auto s=nds->battery();if(!s.empty() && !atomic_file(save,s.data(),s.size()))failed=true;}
  if(!failed && prefs.resume){
    std::vector<uint8_t> writer;err.clear();
    if(!nds->saveState(writer,err) || !atomic_file(resume,writer.data(),writer.size())){
      std::fprintf(stderr,"DS resume capture failed: %s\n",err.c_str());failed=true;
    }else std::fprintf(stderr,"DS resume captured at core frame %llu\n",(unsigned long long)nds->frameCount());
  }
  double seconds=double(SDL_GetPerformanceCounter()-started)/SDL_GetPerformanceFrequency();
  std::fprintf(stderr,"DS: %d host iterations in %.2fs, core frames %llu\n",frame,seconds,(unsigned long long)nds->frameCount());
  pocket_shader_free();
  active=nullptr;return failed?1:0;
}
