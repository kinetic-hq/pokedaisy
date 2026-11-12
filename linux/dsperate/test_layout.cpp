// GPL-3.0. Every DS LAYOUT on every window shape: inside the window, 4:3 screens, no overlaps but the ones meant.
#include "ds_layout.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static void inside(const DsBox& b,int ow,int oh){
  if(!b.shown())return;
  assert(b.x>=0 && b.y>=0 && b.x+b.w<=ow && b.y+b.h<=oh);
  assert(std::abs(b.w*3-b.h*4)<=8);  // 4:3, give or take rounding
}
static bool overlap(const DsBox& a,const DsBox& b){
  return a.shown() && b.shown() && a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}
static void within(const DsBox& b,int ow,int oh){
  if(b.shown())assert(b.x>=0 && b.y>=0 && b.x+b.w<=ow && b.y+b.h<=oh);
}
int main(){
  const int sizes[][2]={{1920,1080},{1280,800},{1024,768},{800,1280},{640,480},{2560,1080},{1280,720},{512,384}};
  for(auto& s:sizes)for(int layout=0;layout<DSL_COUNT;layout++)for(int swapped=0;swapped<2;swapped++)for(int want=0;want<2;want++)for(int left=0;left<2;left++){
    DsPlacement p=ds_place(layout,s[0],s[1],swapped,want,left);
    inside(p.top,s[0],s[1]);inside(p.touch,s[0],s[1]);
    // The companion's panels: inside the window, clear of both screens and of each other (a floating touch
    // screen goes over them).
    for(auto& panel:p.panel){
      within(panel,s[0],s[1]);assert(!overlap(panel,p.top));
      if(!p.touchFloats)assert(!overlap(panel,p.touch));
    }
    assert(!overlap(p.panel[0],p.panel[1]));
    if(p.used==DSL_WING || p.used==DSL_WINGS)assert(p.panel[0].shown());
    if(p.used!=DSL_WING && p.used!=DSL_WINGS)assert(!p.panel[0].shown() && !p.panel[1].shown());
    assert(p.top.shown() || p.touch.shown());
    if(!p.inset && !p.touchFloats)assert(!overlap(p.top,p.touch));
    if(p.inset)assert(overlap(p.top,p.touch));
    if(p.used==DSL_STACKED || p.used==DSL_SIDE || p.used==DSL_WING || p.used==DSL_LARGE)assert(p.top.shown() && p.touch.shown());
  }
  // ONE SCREEN: the top screen alone, the touch screen alone when swapped, floating over the top when the game asks.
  auto one=ds_place(DSL_ONE,1920,1080,false,false);assert(one.top.shown() && !one.touch.shown());
  assert(one.top.w==1280 && one.top.h==960);  // a whole 5x
  assert(ds_place(DSL_ONE,1920,1080,true,false).touch.w==1280);
  assert(ds_place(DSL_ONE,1920,1080,false,true).touchFloats);
  // Fallbacks: no wing on a 4:3 window, SIDE BY SIDE upright is STACKED.
  assert(ds_place(DSL_WING,1024,768,false,false).used==DSL_PIP);
  assert(ds_place(DSL_WING,1920,1080,false,false).used==DSL_WING);
  assert(ds_place(DSL_SIDE,800,1280,false,false).used==DSL_STACKED);
  assert(ds_place(DSL_SIDE,1920,1080,false,false).used==DSL_SIDE);
  assert(ds_place(DSL_WINGS,640,480,false,false).used==DSL_ONE);
  // TOUCH IN WING: the top screen centred at a whole scale, the touch screen in a wing level with its bottom, the
  // party in the other wing; the touch side swaps.
  auto wing=ds_place(DSL_WING,1920,1080,false,false);
  assert(wing.top.w==1280 && wing.top.x==320 && wing.touch.x==wing.top.x+wing.top.w && wing.touch.y+wing.touch.h==wing.top.y+wing.top.h);
  assert(wing.panel[0].x==0 && wing.panel[0].w==320 && wing.panel[1].x==1600);
  auto wingLeft=ds_place(DSL_WING,1920,1080,false,false,true);
  assert(wingLeft.touch.x==0 && wingLeft.panel[0].x==1600);
  assert(ds_place(DSL_WING,1280,720,false,false).top.w==768);   // 3x
  assert(ds_place(DSL_WING,1280,800,false,false).top.w==768);   // 4x leaves no wing: 3x
  // WINGS: the top screen alone in the middle, panels in both wings, the touch screen floating in when wanted.
  auto wings=ds_place(DSL_WINGS,1920,1080,false,false);
  assert(wings.used==DSL_WINGS && wings.top.w==1280 && !wings.touch.shown() && wings.panel[0].shown() && wings.panel[1].shown());
  assert(ds_place(DSL_WINGS,1920,1080,false,true).touchFloats && ds_place(DSL_WINGS,1920,1080,true,false).touchFloats);
  assert(ds_place(DSL_WINGS,1024,768,false,false).used==DSL_WINGS);
  // Touch points map back to DS pixels, a drag off the edge stays on it.
  int x=0,y=0;DsBox t{100,50,512,384};
  assert(ds_touch_at(t,100,50,false,x,y) && x==0 && y==0);
  assert(ds_touch_at(t,100+511,50+383,false,x,y) && x==255 && y==191);
  assert(!ds_touch_at(t,99,50,false,x,y));
  assert(ds_touch_at(t,2000,-5,true,x,y) && x==255 && y==0);
  assert(!ds_touch_at(DsBox{},10,10,true,x,y));
  for(int i=0;i<DSL_COUNT;i++)std::printf("%s ",ds_layout_name(i));
  std::puts("\nPASS DS LAYOUT placement, fallbacks and touch mapping");
}
