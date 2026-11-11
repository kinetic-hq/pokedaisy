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
int main(){
  const int sizes[][2]={{1920,1080},{1280,800},{1024,768},{800,1280},{640,480},{2560,1080},{1280,720},{512,384}};
  for(auto& s:sizes)for(int layout=0;layout<DSL_COUNT;layout++)for(int swapped=0;swapped<2;swapped++)for(int want=0;want<2;want++){
    DsPlacement p=ds_place(layout,s[0],s[1],swapped,want);
    inside(p.top,s[0],s[1]);inside(p.touch,s[0],s[1]);
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
  // TOUCH IN WING: a full-height top screen, the touch screen beside it.
  auto wing=ds_place(DSL_WING,1920,1080,false,false);
  assert(wing.top.h==1080 && wing.touch.x==wing.top.x+wing.top.w && wing.touch.y+wing.touch.h==1080);
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
