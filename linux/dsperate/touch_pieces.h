// GPL-3.0. TOUCH PIECES for the SDL host: a game's touch screen lifted out button by button and laid out in the two
// wings beside the top screen. Pure (no SDL), so test_pieces.cpp checks every screen on every window shape.
//
// A screen is a list of pieces in DS pixels (each button whole, with its outline) and the wing and row each goes
// in; it's recognised by its buttons' outlines, so nothing is read from game memory. A screen it doesn't know shows
// the touch screen's two halves instead (the old SPLIT TOUCH).
#pragma once
#include "ds_layout.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

struct DsPiece {
  short x,y,w,h;        // the button on the DS touch screen (art and tap target)
  signed char wing;     // 0 the left wing, 1 the right
  signed char row;      // rows in a wing, top down; pieces in one row sit side by side
  const char* label;    // drawn under the art (art with no words of its own), or the whole face of an own piece
  bool own;             // no art: our own card with the label, sized lw x lh, a tap presses the button's middle
  short lw,lh;          // an own piece's size in the layout (DS pixels)
  unsigned char corners;// rounded corners to clear (1 top left, 2 top right, 4 bottom left, 8 bottom right)
};
struct DsPieceCheck { short x,y,w,h; };   // a button outline that must be there
struct DsPieceScreen {
  const char* name;
  const DsPiece* pieces;int count;
  const DsPieceCheck* checks;int checkCount;
};
struct DsPieceGame {
  const DsPieceScreen* screens;int count;
  bool (*edge)(uint32_t argb);   // a pixel of the buttons' outline
};

// ---- HeartGold / SoulSilver (every language: the buttons sit in the same places) ----
inline bool ds_pieces_hgss_edge(uint32_t p){
  int r=p>>16&255,g=p>>8&255,b=p&255;
  return std::max({r,g,b})<100 && std::max({r,g,b})-std::min({r,g,b})<20;
}
namespace ds_pieces_hgss {
constexpr unsigned char ALL=15,TOP=3;
// The battle's command menu: FIGHT alone in one wing, BAG / POKEMON / RUN in the other.
inline constexpr DsPiece COMMAND[]={
  {24,40,208,88,0,0,nullptr,false,0,0,ALL},
  {1,145,78,46,1,0,nullptr,false,0,0,ALL},
  {177,145,78,46,1,0,nullptr,false,0,0,ALL},
  {89,153,78,39,1,1,nullptr,false,0,0,TOP},
};
inline constexpr DsPieceCheck COMMAND_CHECK[]={{24,40,208,88},{1,145,78,46},{177,145,78,46}};
// Picking a move: two moves in each wing, CANCEL under the right ones (the game's bar is too wide to scale up).
inline constexpr DsPiece MOVES[]={
  {2,25,124,54,0,0,nullptr,false,0,0,ALL},
  {130,25,124,54,1,0,nullptr,false,0,0,ALL},
  {2,89,124,54,0,1,nullptr,false,0,0,ALL},
  {130,89,124,54,1,1,nullptr,false,0,0,ALL},
  {9,153,238,39,1,2,"CANCEL",true,124,24,0},
};
inline constexpr DsPieceCheck MOVES_CHECK[]={{2,25,124,54},{130,25,124,54},{2,89,124,54},{130,89,124,54}};
// The battle bag's pockets.
inline constexpr DsPiece BAG[]={
  {1,9,126,70,0,0,nullptr,false,0,0,ALL},
  {129,9,126,70,1,0,nullptr,false,0,0,ALL},
  {1,81,126,70,0,1,nullptr,false,0,0,ALL},
  {129,81,126,70,1,1,nullptr,false,0,0,ALL},
  {217,153,38,38,1,2,"BACK",true,126,24,0},
};
inline constexpr DsPieceCheck BAG_CHECK[]={{1,9,126,70},{129,9,126,70},{1,81,126,70},{129,81,126,70},{217,153,38,38}};
// The party: the left column's slots in the left wing, the right column's in the right.
inline constexpr DsPiece PARTY[]={
  {1,1,126,46,0,0,nullptr,false,0,0,ALL},
  {129,9,126,46,1,0,nullptr,false,0,0,ALL},
  {1,49,126,46,0,1,nullptr,false,0,0,ALL},
  {129,57,126,46,1,1,nullptr,false,0,0,ALL},
  {1,97,126,46,0,2,nullptr,false,0,0,ALL},
  {129,105,126,46,1,2,nullptr,false,0,0,ALL},
  {217,153,38,38,1,3,"BACK",true,126,24,0},
};
inline constexpr DsPieceCheck PARTY_CHECK[]={{1,1,126,46},{129,9,126,46},{1,49,126,46},{129,57,126,46},{217,153,38,38}};
inline constexpr DsPieceScreen SCREENS[]={
  {"COMMAND",COMMAND,4,COMMAND_CHECK,3},
  {"MOVES",MOVES,5,MOVES_CHECK,4},
  {"BAG",BAG,5,BAG_CHECK,5},
  {"PARTY",PARTY,7,PARTY_CHECK,5},
};
inline constexpr DsPieceGame GAME={SCREENS,4,ds_pieces_hgss_edge};
}

// The game's pieces by its header's code (IPKx HeartGold, IPGx SoulSilver), else none (halves only).
inline const DsPieceGame* ds_pieces_game(const char* code){
  if(code && (!std::strncmp(code,"IPK",3) || !std::strncmp(code,"IPG",3)))return &ds_pieces_hgss::GAME;
  return nullptr;
}

// The screen showing on a 256 x 192 touch screen (ARGB), or -1: every check box's outline is there at the middle
// of its top, left and right sides.
inline int ds_pieces_detect(const DsPieceGame& game,const uint32_t* px){
  auto at=[&](int x,int y){return game.edge(px[std::clamp(y,0,191)*256+std::clamp(x,0,255)]);};
  for(int s=0;s<game.count;s++){
    const DsPieceScreen& sc=game.screens[s];bool all=true;
    for(int i=0;i<sc.checkCount && all;i++){
      const DsPieceCheck& c=sc.checks[i];
      all=at(c.x+c.w/2,c.y) && at(c.x,c.y+c.h/2) && at(c.x+c.w-1,c.y+c.h/2);
    }
    if(all)return s;
  }
  return -1;
}

// Room round pieces, in DS pixels at the pieces' scale: a card's padding, the gap between cards, the wing's margin,
// and a label's line under its art.
constexpr int DS_PIECE_PAD=3,DS_PIECE_GAP=4,DS_PIECE_MARGIN=4,DS_PIECE_LABEL=10;

inline int ds_piece_lw(const DsPiece& p){return p.own?p.lw:p.w;}
inline int ds_piece_lh(const DsPiece& p){return (p.own?p.lh:p.h)+(!p.own && p.label?DS_PIECE_LABEL:0);}

struct DsPiecePlaced {
  DsBox art;     // the art (or an own piece's face) in output pixels
  DsBox card;    // the card round it
  DsBox label;   // the label's line (empty: none)
};
struct DsPieceLayout {
  int screen=-1;
  double scale=0;                 // output pixels per DS pixel, one for every piece of both wings
  std::vector<DsPiecePlaced> placed;  // in the screen's piece order
  bool shown()const{return screen>=0 && !placed.empty();}
};

// One wing's size in DS pixels at scale 1: rows side by side, stacked, with padding, gaps and the margin.
inline void ds_pieces_wing_size(const DsPieceScreen& sc,int wing,int& w,int& h){
  w=0;h=0;int rows=0;
  for(int row=0;row<16;row++){
    int rw=0,rh=0,n=0;
    for(int i=0;i<sc.count;i++){const DsPiece& p=sc.pieces[i];if(p.wing!=wing || p.row!=row)continue;
      rw+=ds_piece_lw(p)+2*DS_PIECE_PAD+(n?DS_PIECE_GAP:0);rh=std::max(rh,ds_piece_lh(p)+2*DS_PIECE_PAD);n++;}
    if(!n)continue;
    w=std::max(w,rw);h+=rh+(rows?DS_PIECE_GAP:0);rows++;
  }
  if(rows){w+=2*DS_PIECE_MARGIN;h+=2*DS_PIECE_MARGIN;}
}

// The largest scale the screen's pieces take in these wings: whole from 2 up, quarter steps from 1 to 2 (drawn
// sharp: whole pixels first, then smoothed to size), below 1 whatever fits (the set made smaller, never cut).
inline double ds_pieces_scale(const DsPieceScreen& sc,const DsBox wing[2]){
  double s=1e9;
  for(int i=0;i<2;i++){
    int w,h;ds_pieces_wing_size(sc,i,w,h);if(!w)continue;
    if(!wing[i].shown())return 0;
    s=std::min(s,std::min(double(wing[i].w)/w,double(wing[i].h)/h));
  }
  if(s>=1e9)return 0;
  return s>=2?std::floor(s):s>=1?std::floor(s*4)/4:s;
}

// The pieces laid out in the wings: each wing's rows centred across it and the stack centred down it.
inline DsPieceLayout ds_pieces_arrange(const DsPieceGame& game,int screen,const DsBox wing[2]){
  DsPieceLayout out;
  if(screen<0 || screen>=game.count)return out;
  const DsPieceScreen& sc=game.screens[screen];
  double s=ds_pieces_scale(sc,wing);if(s<=0)return out;
  out.screen=screen;out.scale=s;out.placed.resize(sc.count);
  auto px=[&](int ds){return int(std::lround(ds*s));};
  for(int wi=0;wi<2;wi++){
    int w,h;ds_pieces_wing_size(sc,wi,w,h);if(!w)continue;
    const DsBox& room=wing[wi];
    int y=room.y+(room.h-px(h))/2+px(DS_PIECE_MARGIN);
    for(int row=0;row<16;row++){
      int rw=0,rh=0,n=0;
      for(int i=0;i<sc.count;i++){const DsPiece& p=sc.pieces[i];if(p.wing!=wi || p.row!=row)continue;
        rw+=px(ds_piece_lw(p)+2*DS_PIECE_PAD)+(n?px(DS_PIECE_GAP):0);rh=std::max(rh,px(ds_piece_lh(p)+2*DS_PIECE_PAD));n++;}
      if(!n)continue;
      int x=room.x+(room.w-rw)/2;
      for(int i=0;i<sc.count;i++){const DsPiece& p=sc.pieces[i];if(p.wing!=wi || p.row!=row)continue;
        int cw=px(ds_piece_lw(p)+2*DS_PIECE_PAD),ch=px(ds_piece_lh(p)+2*DS_PIECE_PAD);
        DsPiecePlaced& pl=out.placed[i];
        pl.card={x,y+(rh-ch)/2,cw,ch};
        int pad=px(DS_PIECE_PAD);
        pl.art={pl.card.x+pad,pl.card.y+pad,px(ds_piece_lw(p)),px(p.own?p.lh:p.h)};
        if(!p.own && p.label)pl.label={pl.art.x,pl.art.y+pl.art.h,pl.art.w,px(DS_PIECE_LABEL)};
        x+=cw+px(DS_PIECE_GAP);
      }
      y+=rh+px(DS_PIECE_GAP);
    }
  }
  return out;
}

// The wings beside a top screen: everything left and right of it, the window's full height.
inline void ds_pieces_wings(int ow,int oh,const DsBox& top,DsBox wing[2]){
  wing[0]={0,0,top.x,oh};wing[1]={top.x+top.w,0,ow-top.x-top.w,oh};
}

// TOUCH PIECES' top screen: centred at a whole scale, picked from the largest that fits and the two under it (never
// under 3x) as the one giving the game's smallest screen of pieces the largest scale (a tie keeps the larger top
// screen). It's the
// same for every screen of the game, so the top screen never jumps when the touch screen changes.
inline DsBox ds_pieces_top(const DsPieceGame& game,int ow,int oh){
  int most=std::min(ow/256,oh/192);
  DsBox best{};double bestScale=-1;
  for(int s=most;s>=std::max(std::min(most,3),most-2);s--){
    DsBox top{(ow-256*s)/2,(oh-192*s)/2,256*s,192*s};
    DsBox wing[2];ds_pieces_wings(ow,oh,top,wing);
    double least=1e9;
    for(int i=0;i<game.count;i++)least=std::min(least,ds_pieces_scale(game.screens[i],wing));
    if(least>bestScale+1e-9){bestScale=least;best=top;}
  }
  return bestScale>0?best:DsBox{};
}

// The touch screen's halves (the fallback) in the wings beside a given top screen, level with its bottom.
inline bool ds_pieces_halves(int ow,int oh,const DsBox& top,DsBox half[2],bool swapHalves){
  int wing=std::min(top.x,ow-top.x-top.w),bottom=top.y+top.h;
  if(wing<64 || bottom<96)return false;
  double scale=std::min(double(wing)/128,double(bottom)/192);
  if(scale>=3.0)scale=int(scale);
  int w=int(128*scale),h=int(192*scale);
  DsBox left{top.x-w,bottom-h,w,h},right{top.x+top.w,bottom-h,w,h};
  half[0]=swapHalves?right:left;half[1]=swapHalves?left:right;
  (void)oh;return true;
}

// TOUCH PIECES' placement: as ds_place() places SPLIT TOUCH, but for a game with pieces the top screen is the one
// ds_pieces_top() picks, the halves (the fallback) beside it, and the wings are its whole sides.
inline DsPlacement ds_place_with_pieces(const DsPieceGame* game,int layout,int ow,int oh,bool swapped,bool wanted,bool left){
  DsPlacement p=ds_place(layout,ow,oh,swapped,wanted,left);
  if(!game || p.used!=DSL_SPLIT || swapped)return p;
  DsBox top=ds_pieces_top(*game,ow,oh),half[2];
  if(!top.shown() || !ds_pieces_halves(ow,oh,top,half,left))return p;
  p.top=top;p.half[0]=half[0];p.half[1]=half[1];ds_pieces_wings(ow,oh,top,p.wing);p.pieces=true;
  return p;
}

// A window point on a laid out piece, as the DS point to press; -1 when it's on none. An own piece presses its
// button's middle.
inline int ds_pieces_hit(const DsPieceGame& game,const DsPieceLayout& layout,int px,int py,int& x,int& y){
  if(!layout.shown())return -1;
  const DsPieceScreen& sc=game.screens[layout.screen];
  for(int i=0;i<sc.count;i++){
    const DsPiecePlaced& pl=layout.placed[i];if(!pl.card.contains(px,py))continue;
    const DsPiece& p=sc.pieces[i];
    if(p.own){x=p.x+p.w/2;y=p.y+p.h/2;return i;}
    x=p.x+std::clamp((px-pl.art.x)*p.w/std::max(1,pl.art.w),0,p.w-1);
    y=p.y+std::clamp((py-pl.art.y)*p.h/std::max(1,pl.art.h),0,p.h-1);
    return i;
  }
  return -1;
}
// The same while a press goes on: the point stays on the piece it began on (its nearest edge).
inline void ds_pieces_drag(const DsPieceGame& game,const DsPieceLayout& layout,int piece,int px,int py,int& x,int& y){
  const DsPiece& p=game.screens[layout.screen].pieces[piece];const DsPiecePlaced& pl=layout.placed[piece];
  if(p.own){x=p.x+p.w/2;y=p.y+p.h/2;return;}
  x=p.x+std::clamp((px-pl.art.x)*p.w/std::max(1,pl.art.w),0,p.w-1);
  y=p.y+std::clamp((py-pl.art.y)*p.h/std::max(1,pl.art.h),0,p.h-1);
}

// A piece's art with its rounded corners see-through: copies the touch screen's piece into out (w x h ARGB) and
// clears, from each rounded corner, the background reached without crossing the outline (a few pixels in).
inline void ds_piece_art(const DsPieceGame& game,const DsPiece& p,const uint32_t* px,uint32_t* out){
  for(int y=0;y<p.h;y++)for(int x=0;x<p.w;x++)out[y*p.w+x]=px[(p.y+y)*256+p.x+x]|0xFF000000u;
  const int R=6;
  for(int c=0;c<4;c++){
    if(!(p.corners&(1<<c)))continue;
    int cx=c&1?p.w-1:0,cy=c&2?p.h-1:0;
    int x0=c&1?p.w-R:0,y0=c&2?p.h-R:0;
    std::vector<int> stack{cy*p.w+cx};
    while(!stack.empty()){
      int i=stack.back();stack.pop_back();int x=i%p.w,y=i/p.w;
      if(x<x0 || x>=x0+R || y<y0 || y>=y0+R)continue;
      uint32_t& v=out[i];if(!(v>>24) || game.edge(v))continue;
      v=0;
      if(x>0)stack.push_back(i-1);if(x<p.w-1)stack.push_back(i+1);
      if(y>0)stack.push_back(i-p.w);if(y<p.h-1)stack.push_back(i+p.w);
    }
  }
}
