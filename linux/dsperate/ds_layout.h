// GPL-3.0. DS LAYOUT for the SDL host: where the two DS screens go in the window, in output pixels. Pure, so
// test_layout.cpp checks every layout on every window shape.
#pragma once
#include <algorithm>

// Saved by number in ds-settings.ini (layout=N): append new layouts, never renumber.
enum DsHostLayout { DSL_ONE=0, DSL_PIP=1, DSL_WING=2, DSL_STACKED=3, DSL_SIDE=4, DSL_LARGE=5, DSL_WINGS=6, DSL_COUNT=7 };
inline const char* ds_layout_name(int layout){
  static const char* const names[]={"ONE SCREEN","PICTURE IN PICTURE","TOUCH IN WING","STACKED","SIDE BY SIDE","LARGE + SMALL","WINGS"};
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

// A window point on the touch screen as drawn, in DS pixels; clamp keeps a drag that leaves it on its edge.
inline bool ds_touch_at(const DsBox& touch,int px,int py,bool clamp,int& x,int& y){
  if(!touch.shown() || (!clamp && !touch.contains(px,py)))return false;
  x=std::clamp((px-touch.x)*256/touch.w,0,255);y=std::clamp((py-touch.y)*192/touch.h,0,191);
  return true;
}
