// GPL-3.0. DS LAYOUT for the SDL host: where the two DS screens go in the window, in output pixels. Pure, so
// test_layout.cpp checks every layout on every window shape.
#pragma once
#include <algorithm>

// Saved by number in ds-settings.ini (layout=N): append new layouts, never renumber.
enum DsHostLayout { DSL_ONE=0, DSL_PIP=1, DSL_WING=2, DSL_STACKED=3, DSL_SIDE=4, DSL_LARGE=5, DSL_WINGS=6, DSL_SPLIT=7, DSL_COUNT=8 };
inline const char* ds_layout_name(int layout){
  static const char* const names[]={"ONE SCREEN","PICTURE IN PICTURE","TOUCH IN WING","STACKED","SIDE BY SIDE","LARGE + SMALL","WINGS","SPLIT TOUCH"};
  return names[std::clamp(layout,0,DSL_COUNT-1)];
}

struct DsBox {
  int x=0,y=0,w=0,h=0;
  bool shown()const{return w>0 && h>0;}
  bool contains(int px,int py)const{return shown() && px>=x && py>=y && px<x+w && py<y+h;}
};
struct DsPlacement {
  DsBox top,touch;       // a box of zero size isn't drawn
  bool touchFloats=false; // the touch screen sits over the dimmed top screen
  bool inset=false;       // the touch screen sits inside the top screen's picture (framed)
  int used=DSL_ONE;       // the layout drawn, after a fallback
  // The companion's panels in the wings (TOUCH IN WING, WINGS): panel[0] the party, panel[1] the trainer's facts.
  DsBox panel[2];
  // SPLIT TOUCH: the touch screen cut in two, half[0] its left half (DS x 0-127) and half[1] its right (128-255),
  // each in a wing; touch is then empty.
  DsBox half[2];
  bool split()const{return half[0].shown();}
};

// A w x h picture (in DS pixels) as large as fits in the box: whole multiples from 3x up, so pixels stay even
// (below that, flooring would throw away up to half the room).
inline DsBox ds_fit(int boxX,int boxY,int boxW,int boxH,int w,int h,bool whole=true){
  if(boxW<=0 || boxH<=0)return {};
  double scale=std::min(double(boxW)/w,double(boxH)/h);
  if(whole && scale>=3.0)scale=int(scale);
  int fw=std::max(1,int(w*scale)),fh=std::max(1,int(h*scale));
  return {boxX+(boxW-fw)/2,boxY+(boxH-fh)/2,fw,fh};
}

// The top screen centred with a wing on each side at least minWing / 8 of its width: the largest whole scale that
// leaves them (one step under the largest that fits, at most), or under 3x the full height. Zero size: no room.
inline DsBox ds_winged_top(int ow,int oh,int minWing){
  int most=std::min(ow/256,oh/192);
  if(most>=3){
    for(int s=most;s>=most-1;s--){
      int w=256*s,h=192*s;
      if((ow-w)/2*8>=w*minWing)return {(ow-w)/2,(oh-h)/2,w,h};
    }
    return {};
  }
  int w=oh*4/3;
  if((ow-w)/2*8<w*minWing)return {};
  return {(ow-w)/2,0,w,oh};
}

// SPLIT TOUCH: the top screen centred as WINGS places it, each half of the touch screen (128 x 192) filling a wing,
// level with the top screen's bottom. Zero size: the wings are narrower than a half at 1x.
inline bool ds_split_halves(int ow,int oh,DsBox& top,DsBox half[2],bool swapHalves){
  top=ds_winged_top(ow,oh,1);
  if(!top.shown())return false;
  int wing=std::min(top.x,ow-top.x-top.w),bottom=top.y+top.h;
  if(wing<128 || bottom<192)return false;
  double scale=std::min(double(wing)/128,double(bottom)/192);
  if(scale>=3.0)scale=int(scale);   // whole from 3x up, as ds_fit
  int w=int(128*scale),h=int(192*scale);
  DsBox left{top.x-w,bottom-h,w,h},right{top.x+top.w,bottom-h,w,h};
  half[0]=swapHalves?right:left;half[1]=swapHalves?left:right;
  return true;
}

// layout: the player's pick. swapped: R2's swap (ONE SCREEN shows the touch screen, PICTURE IN PICTURE and
// LARGE + SMALL make it the large one; WINGS floats it in). touchWanted: the game wants the touch screen now (or the
// player asked), which floats it over a ONE SCREEN or WINGS game. touchLeft: TOUCH IN WING's touch screen (and WINGS'
// party) in the left wing.
inline DsPlacement ds_place(int layout,int ow,int oh,bool swapped,bool touchWanted,bool touchLeft=false){
  DsPlacement p;p.used=std::clamp(layout,0,DSL_COUNT-1);
  if(ow<=0 || oh<=0)return p;
  // Fallbacks: TOUCH IN WING needs a wing a quarter of the top screen's width on each side, WINGS an eighth (else
  // PICTURE IN PICTURE / ONE SCREEN); SIDE BY SIDE on a window taller than wide is STACKED.
  DsBox winged;
  DsBox halves[2];
  if(p.used==DSL_SPLIT && !ds_split_halves(ow,oh,winged,halves,touchLeft))p.used=DSL_WING;
  if(p.used==DSL_WING && !(winged=ds_winged_top(ow,oh,2)).shown())p.used=DSL_PIP;
  if(p.used==DSL_WINGS && !(winged=ds_winged_top(ow,oh,1)).shown())p.used=DSL_ONE;
  if(p.used==DSL_SIDE && oh>ow)p.used=DSL_STACKED;
  switch(p.used){
  case DSL_ONE:
    if(touchWanted && !swapped){
      p.top=ds_fit(0,0,ow,oh,256,192);
      p.touch=ds_fit(ow/8,oh/8,ow-ow/4,oh-oh/4,256,192,false);
      p.touchFloats=true;
    }else if(swapped)p.touch=ds_fit(0,0,ow,oh,256,192);
    else p.top=ds_fit(0,0,ow,oh,256,192);
    break;
  case DSL_PIP:{
    DsBox big=ds_fit(0,0,ow,oh,256,192);
    if(touchWanted && !swapped){
      p.top=big;p.touch=ds_fit(ow/8,oh/8,ow-ow/4,oh-oh/4,256,192,false);p.touchFloats=true;break;
    }
    int w=std::max(64,big.w*28/100),h=w*3/4,m=std::max(4,big.w/64);
    DsBox small{big.x+big.w-w-m,big.y+big.h-h-m,w,h};
    if(swapped){p.touch=big;p.top=small;}else{p.top=big;p.touch=small;}
    p.inset=true;
    break;}
  case DSL_WING:{
    // The top screen centred at a whole scale, the touch screen in one wing level with its bottom, the companion's
    // party filling the other wing and its facts the room over the touch screen. R2 trades the two screens.
    DsBox top=winged;int wing=top.x,tw=wing,th=tw*3/4;
    int touchX=touchLeft?0:top.x+top.w,partyX=touchLeft?top.x+top.w:0;
    DsBox touch{touchX,top.y+top.h-th,tw,th};
    p.top=top;p.touch=touch;
    p.panel[0]={partyX,0,ow-top.x-top.w,oh};
    if(touch.y>=oh/4)p.panel[1]={touchX,0,tw,touch.y};
    if(swapped){
      // The touch screen in the middle at the top screen's size, the top screen in the wing.
      p.touch=top;p.top=touch;
    }
    break;}
  case DSL_WINGS:{
    // ONE SCREEN with the companion's panels in both wings; the touch screen floats in over the game when it's
    // wanted (or R2 asks for it).
    p.top=winged;int wing=winged.x;
    DsBox left{0,0,wing,oh},right{winged.x+winged.w,0,ow-winged.x-winged.w,oh};
    p.panel[0]=touchLeft?right:left;p.panel[1]=touchLeft?left:right;
    if(touchWanted || swapped){
      p.touch=ds_fit(ow/8,oh/8,ow-ow/4,oh-oh/4,256,192,false);
      p.touchFloats=true;
    }
    break;}
  case DSL_SPLIT:
    // The touch screen's halves in the wings, the top screen between them; R2 shows the touch screen whole in the
    // middle (the top screen in the right wing). The companion's panels don't show: the wings are the touch screen's.
    if(swapped){
      p.touch=winged;int tw=ow-winged.x-winged.w,th=tw*3/4;
      p.top={winged.x+winged.w,winged.y+winged.h-th,tw,th};
    }else{p.top=winged;p.half[0]=halves[0];p.half[1]=halves[1];}
    break;
  case DSL_STACKED:{
    DsBox both=ds_fit(0,0,ow,oh,256,384);
    p.top={both.x,both.y,both.w,both.h/2};p.touch={both.x,both.y+both.h/2,both.w,both.h-both.h/2};
    break;}
  case DSL_SIDE:{
    DsBox both=ds_fit(0,0,ow,oh,512,192);
    p.top={both.x,both.y,both.w/2,both.h};p.touch={both.x+both.w/2,both.y,both.w-both.w/2,both.h};
    break;}
  case DSL_LARGE:{
    // The large screen four fifths of the width, the small one beside it, level with its bottom.
    DsBox both=ds_fit(0,0,ow,oh,320,192,false);
    int bw=both.w*4/5,sw=both.w-bw,sh=sw*3/4;
    DsBox big{both.x,both.y,bw,both.h},small{both.x+bw,both.y+both.h-sh,sw,sh};
    if(swapped){p.touch=big;p.top=small;}else{p.top=big;p.touch=small;}
    break;}
  }
  return p;
}

inline bool ds_same_box(const DsBox& a,const DsBox& b){
  return a.shown()==b.shown() && (!a.shown() || (a.x==b.x && a.y==b.y && a.w==b.w && a.h==b.h));
}
// Two placements that put everything in the same place, drawn the same way.
inline bool ds_same_placement(const DsPlacement& a,const DsPlacement& b){
  if(a.touchFloats!=b.touchFloats || a.inset!=b.inset || !ds_same_box(a.top,b.top) || !ds_same_box(a.touch,b.touch))return false;
  for(int i=0;i<2;i++)if(!ds_same_box(a.panel[i],b.panel[i]) || !ds_same_box(a.half[i],b.half[i]))return false;
  return true;
}
// What a layout is drawn as: a game without touch rules has PICTURE IN PICTURE for ONE SCREEN (frontend.cpp).
inline int ds_layout_drawn(int layout,bool touchRules){return layout==DSL_ONE && !touchRules?DSL_PIP:layout;}
// first[l]: the first layout in the picker's order drawn exactly like l on an ow x oh window, with the touch screen
// wanted and not (a fallback, or two layouts that come out the same on this shape). first[l]==l: l is its own.
inline void ds_layout_firsts(int ow,int oh,bool touchLeft,bool touchRules,int first[DSL_COUNT]){
  for(int l=0;l<DSL_COUNT;l++){
    first[l]=l;
    for(int k=0;k<l && first[l]==l;k++){
      if(first[k]!=k)continue;
      bool same=true;
      for(int want=0;want<2 && same;want++)
        same=ds_same_placement(ds_place(ds_layout_drawn(k,touchRules),ow,oh,false,want,touchLeft),
                               ds_place(ds_layout_drawn(l,touchRules),ow,oh,false,want,touchLeft));
      if(same)first[l]=k;
    }
  }
}
// R3's tap: the next layout in the picker's order that doesn't draw the same as one before it on this window.
inline int ds_layout_cycle(int current,int ow,int oh,bool touchLeft,bool touchRules){
  int first[DSL_COUNT];ds_layout_firsts(ow,oh,touchLeft,touchRules,first);
  int from=first[std::clamp(current,0,DSL_COUNT-1)];
  for(int step=1;step<=DSL_COUNT;step++){int n=(from+step)%DSL_COUNT;if(first[n]==n)return n;}
  return from;
}

// A window point on the touch screen as drawn, in DS pixels; clamp keeps a drag that leaves it on its edge.
inline bool ds_touch_at(const DsBox& touch,int px,int py,bool clamp,int& x,int& y){
  if(!touch.shown() || (!clamp && !touch.contains(px,py)))return false;
  x=std::clamp((px-touch.x)*256/touch.w,0,255);y=std::clamp((py-touch.y)*192/touch.h,0,191);
  return true;
}

// The touch screen is drawn somewhere (whole or in halves).
inline bool ds_touch_shown(const DsPlacement& p){return p.touch.shown() || p.split();}

// The same over a placement: SPLIT TOUCH's halves map back to their side of the DS screen, and with clamp a drag
// over the top screen between them stays on the nearer half's edge, so a stroke crossing from wing to wing goes on.
inline bool ds_touch_at(const DsPlacement& p,int px,int py,bool clamp,int& x,int& y){
  if(!p.split())return ds_touch_at(p.touch,px,py,clamp,x,y);
  int pick=-1;
  for(int i=0;i<2;i++)if(p.half[i].contains(px,py))pick=i;
  if(pick<0){
    if(!clamp)return false;
    auto gap=[&](const DsBox& b){return px<b.x?b.x-px:px>=b.x+b.w?px-(b.x+b.w-1):0;};
    pick=gap(p.half[0])<=gap(p.half[1])?0:1;
  }
  const DsBox& b=p.half[pick];
  x=pick*128+std::clamp((px-b.x)*128/b.w,0,127);y=std::clamp((py-b.y)*192/b.h,0,191);
  return true;
}

// A DS touch point back in the window (the stylus marker).
inline void ds_touch_to_window(const DsPlacement& p,int x,int y,int& wx,int& wy){
  if(p.split()){
    const DsBox& b=p.half[x>=128];
    wx=b.x+(x%128)*b.w/128;wy=b.y+y*b.h/192;
  }else{wx=p.touch.x+x*p.touch.w/256;wy=p.touch.y+y*p.touch.h/192;}
}
