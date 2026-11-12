// GPL-3.0. TOUCH PIECES on the windows players have: every known screen recognised from its outlines, every piece
// inside its wing at one scale, whole, clear of the others, the stacks centred and the two wings balanced.
#include "touch_pieces.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static bool overlap(const DsBox& a,const DsBox& b){
  return a.shown() && b.shown() && a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}
static bool inside(const DsBox& b,const DsBox& room){
  return b.x>=room.x && b.y>=room.y && b.x+b.w<=room.x+room.w && b.y+b.h<=room.y+room.h;
}
// A touch screen with only the outlines of a screen's buttons drawn, on the games' pale background.
static std::vector<uint32_t> outlines(const DsPieceScreen& sc){
  std::vector<uint32_t> px(256*192,0xFFF7FFEBu);
  for(int i=0;i<sc.count;i++){const DsPiece& p=sc.pieces[i];if(p.own && p.w>60)continue;
    for(int x=p.x;x<p.x+p.w;x++){px[p.y*256+x]=0xFF414141u;if(p.y+p.h<=192)px[(p.y+p.h-1)*256+x]=0xFF414141u;}
    for(int y=p.y;y<std::min(192,p.y+p.h);y++){px[y*256+p.x]=0xFF414141u;px[y*256+p.x+p.w-1]=0xFF414141u;}}
  for(int i=0;i<sc.checkCount;i++){const DsPieceCheck& c=sc.checks[i];
    for(int x=c.x;x<c.x+c.w;x++)px[c.y*256+x]=0xFF414141u;
    for(int y=c.y;y<std::min(192,c.y+c.h);y++){px[y*256+c.x]=0xFF414141u;px[y*256+c.x+c.w-1]=0xFF414141u;}}
  return px;
}
static void check(const DsPieceGame& game,const char* name){
  std::printf("%s\n",name);
  // Recognition: each screen's own outlines and nothing else; a blank screen is none.
  for(int s=0;s<game.count;s++){auto px=outlines(game.screens[s]);assert(ds_pieces_detect(game,px.data())==s);}
  {std::vector<uint32_t> blank(256*192,0xFFF7FFEBu);assert(ds_pieces_detect(game,blank.data())==-1);}
  // A rounded corner goes see-through up to the outline, never past it, and never at a corner left square.
  {auto px=outlines(game.screens[0]);const DsPiece& run=game.screens[0].pieces[3];
   std::vector<uint32_t> art(run.w*run.h);ds_piece_art(game,run,px.data(),art.data());
   assert(art[0]>>24 && art[(run.h-1)*run.w]>>24);   // its outline's corner pixel stays; the bottom is open, kept
   const DsPiece& fight=game.screens[0].pieces[0];std::vector<uint32_t> a2(fight.w*fight.h);ds_piece_art(game,fight,px.data(),a2.data());
   assert(a2[fight.w+1]>>24);}  // inside the outline stays
  const int sizes[][2]={{1920,1080},{1920,1200},{1280,800},{1280,720}};
  for(auto& sz:sizes){
    int ow=sz[0],oh=sz[1];
    DsBox top=ds_pieces_top(game,ow,oh);
    assert(top.shown() && top.w*3==top.h*4 && top.w%256==0 && top.x==(ow-top.w)/2);
    DsBox wing[2];ds_pieces_wings(ow,oh,top,wing);
    // The layout's placement: that top screen and wings for a game with pieces, plain SPLIT TOUCH halves without.
    DsPlacement pl=ds_place_with_pieces(&game,DSL_SPLIT,ow,oh,false,false,false);
    assert(pl.pieces && pl.top.w==top.w && pl.wing[1].x==top.x+top.w && pl.split());
    assert(!ds_place_with_pieces(nullptr,DSL_SPLIT,ow,oh,false,false,false).pieces);
    assert(!ds_place_with_pieces(&game,DSL_SPLIT,ow,oh,true,false,false).pieces);   // R2: the touch screen whole
    assert(!ds_place_with_pieces(&game,DSL_WINGS,ow,oh,false,false,false).pieces);
    DsBox half[2];assert(ds_pieces_halves(ow,oh,top,half,false));
    for(auto& h:half)assert(!overlap(h,top) && inside(h,{0,0,ow,oh}));
    std::printf("%dx%d: top %dx (%d px wings)",ow,oh,top.w/256,wing[0].w);
    for(int s=0;s<game.count;s++){
      const DsPieceScreen& sc=game.screens[s];
      DsPieceLayout l=ds_pieces_arrange(game,s,wing);
      assert(l.shown() && l.scale>=1 && (l.scale<2?l.scale*4==std::floor(l.scale*4):l.scale==std::floor(l.scale)));
      std::printf("  %s %gx",sc.name,l.scale);
      int bx0[2]={1<<30,1<<30},by0[2]={1<<30,1<<30},bx1[2]={0,0},by1[2]={0,0};
      for(int i=0;i<sc.count;i++){
        const DsPiece& p=sc.pieces[i];const DsPiecePlaced& pl=l.placed[i];
        // Whole, at the one scale, inside its card, its card inside its wing, clear of the top screen.
        if(!p.own)assert(std::abs(pl.art.w-p.w*l.scale)<=1 && std::abs(pl.art.h-p.h*l.scale)<=1);
        assert(inside(pl.art,pl.card) && inside(pl.card,wing[int(p.wing)]) && !overlap(pl.card,top));
        if(pl.label.shown())assert(inside(pl.label,pl.card) && !overlap(pl.label,pl.art));
        for(int j=0;j<i;j++)assert(!overlap(pl.card,l.placed[j].card));
        int w=p.wing;bx0[w]=std::min(bx0[w],pl.card.x);by0[w]=std::min(by0[w],pl.card.y);
        bx1[w]=std::max(bx1[w],pl.card.x+pl.card.w);by1[w]=std::max(by1[w],pl.card.y+pl.card.h);
        // A tap on the piece's middle presses its button's middle.
        int x,y;assert(ds_pieces_hit(game,l,pl.art.x+pl.art.w/2,pl.art.y+pl.art.h/2,x,y)==i);
        assert(std::abs(x-(p.x+p.w/2))<=1 && std::abs(y-(p.y+p.h/2))<=1);
      }
      // Each wing's stack centred down it (to a pixel or two of rounding) and the wings balanced: their stacks'
      // areas within a quarter of each other.
      long area[2];
      for(int w=0;w<2;w++){
        assert(std::abs((by0[w]-wing[w].y)-(wing[w].y+wing[w].h-by1[w]))<=2*int(l.scale)+2);
        area[w]=long(bx1[w]-bx0[w])*(by1[w]-by0[w]);
      }
      double diff=double(std::abs(area[0]-area[1]))/std::max(area[0],area[1]);
      std::printf(" (balance %.0f%%)",diff*100);
      assert(diff<0.25);
      // Off the pieces is nothing: the top screen and the gaps press no button.
      int x,y;assert(ds_pieces_hit(game,l,top.x+top.w/2,top.y+top.h/2,x,y)==-1);
    }
    std::printf("\n");
  }
  // The FIGHT button keeps the BAG / POKEMON / RUN buttons together in the other wing.
  const DsPieceScreen& cmd=game.screens[0];
  assert(cmd.pieces[1].wing==cmd.pieces[2].wing && cmd.pieces[2].wing==cmd.pieces[3].wing && cmd.pieces[0].wing!=cmd.pieces[1].wing);
}
int main(){
  const DsPieceGame& hgss=*ds_pieces_game("IPGE");
  const DsPieceGame& platinum=*ds_pieces_game("CPUE");
  assert(ds_pieces_game("IPKE")==&hgss && &platinum!=&hgss && ds_pieces_game("ADAE")==nullptr && ds_pieces_game(nullptr)==nullptr);
  check(hgss,"HeartGold / SoulSilver");
  check(platinum,"Platinum");
  // Neither game's screens pass for the other's.
  for(int s=0;s<platinum.count;s++){auto px=outlines(platinum.screens[s]);int h=ds_pieces_detect(hgss,px.data());assert(h<0 || h==s);}
  std::printf("touch pieces: ok\n");
}
