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
    // SPLIT TOUCH's halves: inside, clear of the top screen and of each other, the same size, only there.
    for(auto& half:p.half){within(half,s[0],s[1]);assert(!overlap(half,p.top));}
    assert(!overlap(p.half[0],p.half[1]));
    if(p.split())assert(p.used==DSL_SPLIT && !p.touch.shown() && p.half[0].w==p.half[1].w && p.half[0].h==p.half[1].h);
    if(p.used!=DSL_SPLIT)assert(!p.split());
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
  // SPLIT TOUCH: the top screen as WINGS places it, a half of the touch screen filling each wing, level with the top
  // screen's bottom, the left half (DS x 0-127) on the left; DS WING SIDE swaps them.
  struct Split{int w,h,top,half,halfH;} splits[]={
    {1920,1080,1280,320,480},  // 5x, halves at 2.5x
    {1920,1200,1536,192,288},  // 6x, halves at 1.5x
    {1280,800,1024,128,192},   // 4x, halves at 1x
    {1280,720,768,256,384},    // 3x, halves at 2x
  };
  for(auto& c:splits){
    auto sp=ds_place(DSL_SPLIT,c.w,c.h,false,false);
    assert(sp.used==DSL_SPLIT && sp.split() && sp.top.w==c.top && sp.top.x==(c.w-c.top)/2);
    assert(sp.half[0].w==c.half && sp.half[0].h==c.halfH && sp.half[0].x+sp.half[0].w==sp.top.x);
    assert(sp.half[1].x==sp.top.x+sp.top.w && sp.half[0].y+sp.half[0].h==sp.top.y+sp.top.h);
    assert(sp.panel[0].shown()==false && sp.panel[1].shown()==false);
    auto swapped=ds_place(DSL_SPLIT,c.w,c.h,false,false,true);
    assert(swapped.half[0].x==sp.half[1].x && swapped.half[1].x==sp.half[0].x);
    // Taps map back to their half; a stroke crossing the top screen stays on the nearer half's edge.
    int tx=0,ty=0;
    assert(ds_touch_at(sp,sp.half[0].x,sp.half[0].y,false,tx,ty) && tx==0 && ty==0);
    assert(ds_touch_at(sp,sp.half[1].x+sp.half[1].w-1,sp.half[1].y+sp.half[1].h-1,false,tx,ty) && tx==255 && ty==191);
    assert(ds_touch_at(sp,sp.half[1].x,sp.half[1].y,false,tx,ty) && tx==128);
    assert(ds_touch_at(swapped,swapped.half[1].x,swapped.half[1].y,false,tx,ty) && tx==128);
    assert(!ds_touch_at(sp,c.w/2,c.h/2,false,tx,ty));
    assert(ds_touch_at(sp,sp.top.x+10,sp.half[0].y,true,tx,ty) && tx==127);
    assert(ds_touch_at(sp,sp.top.x+sp.top.w-10,sp.half[0].y,true,tx,ty) && tx==128);
    int wx=0,wy=0;ds_touch_to_window(sp,128,0,wx,wy);assert(wx==sp.half[1].x && wy==sp.half[1].y);
    // R2: the touch screen whole in the middle.
    auto r2=ds_place(DSL_SPLIT,c.w,c.h,true,false);
    assert(!r2.split() && r2.touch.w==c.top && r2.top.shown());
  }
  // Too narrow for halves at 1x: TOUCH IN WING, then PICTURE IN PICTURE.
  assert(ds_place(DSL_SPLIT,1024,768,false,false).half[0].w==128);   // 3x, halves at 1x
  assert(ds_place(DSL_SPLIT,620,300,false,false).used==DSL_WING);
  assert(ds_place(DSL_SPLIT,640,480,false,false).used==DSL_PIP);
  // Touch points map back to DS pixels, a drag off the edge stays on it.
  int x=0,y=0;DsBox t{100,50,512,384};
  assert(ds_touch_at(t,100,50,false,x,y) && x==0 && y==0);
  assert(ds_touch_at(t,100+511,50+383,false,x,y) && x==255 && y==191);
  assert(!ds_touch_at(t,99,50,false,x,y));
  assert(ds_touch_at(t,2000,-5,true,x,y) && x==255 && y==0);
  assert(!ds_touch_at(DsBox{},10,10,true,x,y));
  // R3's cycle: every layout it lands on is its own (drawn unlike any before it), and it reaches all of those.
  for(auto wh:{std::pair<int,int>{1920,1080},{1920,1200},{1280,720},{1280,800},{720,1280}})for(int rules=0;rules<2;rules++){
    int first[DSL_COUNT];ds_layout_firsts(wh.first,wh.second,false,rules,first);
    int own=0;for(int l=0;l<DSL_COUNT;l++)own+=first[l]==l;
    int seen=0,at=0;bool visited[DSL_COUNT]={};
    for(int i=0;i<DSL_COUNT;i++){at=ds_layout_cycle(at,wh.first,wh.second,false,rules);if(first[at]!=at){std::printf("cycle lands on a copy %d\n",at);return 1;}if(!visited[at]){visited[at]=true;seen++;}}
    if(seen!=own){std::printf("cycle reaches %d of %d at %dx%d\n",seen,own,wh.first,wh.second);return 1;}
    std::printf("%dx%d%s: R3 skips",wh.first,wh.second,rules?"":" (no touch rules)");
    for(int l=0;l<DSL_COUNT;l++)if(first[l]!=l)std::printf(" [%s = %s]",ds_layout_name(l),ds_layout_name(first[l]));
    std::printf("\n");
  }
  // R3's tap: back to WINGS in one press from anywhere, then ONE SCREEN, then WINGS again.
  for(auto wh:{std::pair<int,int>{1920,1080},{1920,1200},{1280,720}})for(int rules=0;rules<2;rules++){
    int first[DSL_COUNT];ds_layout_firsts(wh.first,wh.second,false,rules,first);
    for(int l=0;l<DSL_COUNT;l++){
      int t=ds_layout_toggle(l,wh.first,wh.second,false,rules);
      if(first[l]!=first[DSL_WINGS] && t!=first[DSL_WINGS]){std::printf("toggle from %d misses WINGS\n",l);return 1;}
    }
    int a=ds_layout_toggle(DSL_WINGS,wh.first,wh.second,false,rules),b=ds_layout_toggle(a,wh.first,wh.second,false,rules);
    if(first[DSL_WINGS]!=first[DSL_ONE] && (a!=first[DSL_ONE] || b!=first[DSL_WINGS])){std::printf("toggle %d %d\n",a,b);return 1;}
  }
  for(int i=0;i<DSL_COUNT;i++)std::printf("%s ",ds_layout_name(i));
  std::puts("\nPASS DS LAYOUT placement, fallbacks and touch mapping");
}
