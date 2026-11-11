// GPL-3.0. DS LAYOUT for the SDL host: where the two DS screens go in the window, in output pixels. Pure, so
// test_layout.cpp checks every layout on every window shape.
#pragma once
#include <algorithm>

// Saved by number in ds-settings.ini (layout=N): append new layouts, never renumber.
enum DsHostLayout { DSL_ONE=0, DSL_PIP=1, DSL_WING=2, DSL_STACKED=3, DSL_SIDE=4, DSL_LARGE=5, DSL_COUNT=6 };
inline const char* ds_layout_name(int layout){
  static const char* const names[]={"ONE SCREEN","PICTURE IN PICTURE","TOUCH IN WING","STACKED","SIDE BY SIDE","LARGE + SMALL"};
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

// layout: the player's pick. swapped: R2's swap (ONE SCREEN shows the touch screen, PICTURE IN PICTURE and
// LARGE + SMALL make it the large one). touchWanted: the game wants the touch screen now (or the player asked),
// which floats it over a ONE SCREEN game.
inline DsPlacement ds_place(int layout,int ow,int oh,bool swapped,bool touchWanted){
  DsPlacement p;p.used=std::clamp(layout,0,DSL_COUNT-1);
  if(ow<=0 || oh<=0)return p;
  // Fallbacks: TOUCH IN WING needs room beside a full-height top screen for a touch screen at least a quarter its
  // width; SIDE BY SIDE on a window taller than wide is STACKED.
  if(p.used==DSL_WING){
    int topW=oh*4/3,wing=ow-topW;
    if(wing<topW/4)p.used=DSL_PIP;
  }
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
    // The top screen as tall as the window, the touch screen in the room beside it, level with its bottom.
    int topW=std::min(ow,oh*4/3);DsBox top=ds_fit(0,0,topW,oh,256,192,false);
    int wing=ow-top.w,tw=std::min(wing,top.w),th=tw*3/4;
    int gap=(ow-top.w-tw)/2;top.x=gap;
    p.top=top;p.touch={top.x+top.w,top.y+top.h-th,tw,th};
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
