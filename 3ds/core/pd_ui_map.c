// MAP: the game's region map rebuilt from the ROM (pd_map.c), filling the tab like the app's, the
// player's head on the tile the game puts it, FireRed's corner cursor, the
// place name in the game's own label (the app's MapScreen.kt), a tap naming
// what's under it, Kanto / Sevii pages, ME and the PLACES list.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pd_icon.h"
#include "pd_map.h"
#include "pd_games.h"
#include "pd_gb.h"
#include "pd_dex.h"
#include "pd_guide.h"
#include "pd_nuzlocke.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define FRAME 5
#define MAX_RECTS 8
// The most MapScreen stretches the map to fill the tab (MAX_MAP_STRETCH); past it the map keeps its shape.
#define MAX_STRETCH_NUM 6
#define MAX_STRETCH_DEN 5
#define PLACE_ROW 18
#define CURSOR 0xFFFFFF
#define CURSOR_ARM 5
#define CURSOR_THICK 2
// FireRed's label: a darkening strip, white text over grey.
#define FR_LABEL_SHADOW 0x636363

static const struct pd_game* lastGame;

// Where the map is drawn this time: (x, y) w x h canvas pixels for its mapW x mapH (240 x 160, a Game Boy's 160 x 144).
static struct { int x, y, w, h; } view = { MARGIN + FRAME, MARGIN + FRAME, PD_MAP_W, PD_MAP_H };
// The page's size (240 x 160, a Game Boy's 160 x 144: its pixels' row stride) and what of it is shown: its map, without
// the border around it (pd_map_art's crop).
static int mapW = PD_MAP_W, mapH = PD_MAP_H;
static struct { int l, t, r, b; } crop = { 0, 0, PD_MAP_W, PD_MAP_H };
static int sx(int mx) { return view.x + (mx - crop.l) * view.w / (crop.r - crop.l); }
static int sy(int my) { return view.y + (my - crop.t) * view.h / (crop.b - crop.t); }
static int lastPlayerPage = -1, lastPlayerTx, lastPlayerTy;

// --- the PLACES list (RegionMapModel.places) ---

struct place {
    int mapsec; // -1 = a group title
    const char* name;
};
#define MAX_PLACES 200
static struct place places[MAX_PLACES];
static int placeCount;
static unsigned placesRom;

static bool starts_route(const char* n) {
    return !strncasecmp(n, "route ", 6);
}

static bool is_town(const char* n) {
    const char* last = strrchr(n, ' ');
    last = last ? last + 1 : n;
    static const char* const WORDS[] = { "town", "city", "village", "island", "plateau" };
    for (size_t i = 0; i < sizeof(WORDS) / sizeof(WORDS[0]); i++) {
        if (!strcasecmp(last, WORDS[i])) return true;
    }
    return false;
}

static int cmp_place(const void* a, const void* b) {
    const struct place *x = a, *y = b;
    if (starts_route(x->name) && starts_route(y->name)) {
        int nx = atoi(x->name + 6), ny = atoi(y->name + 6);
        if (nx != ny) return nx - ny;
    }
    return strcasecmp(x->name, y->name);
}

static void build_places(const struct pd_game* g) {
    if (placesRom == g->loadId && placeCount) return;
    placesRom = g->loadId;
    placeCount = 0;
    static struct place towns[MAX_PLACES], routes[MAX_PLACES], other[MAX_PLACES];
    int nt = 0, nr = 0, no = 0;
    for (int id = 0; id < 256; id++) {
        const char* name = pd_mapsec_name(g, id);
        int page;
        struct pd_tile_rect r;
        if (!*name || pd_map_tiles(g, id, &page, &r, 1) < 1) continue;
        bool dup = false;
        for (int i = 0; i < nt && !dup; i++) dup = !strcasecmp(towns[i].name, name);
        for (int i = 0; i < nr && !dup; i++) dup = !strcasecmp(routes[i].name, name);
        for (int i = 0; i < no && !dup; i++) dup = !strcasecmp(other[i].name, name);
        if (dup) continue;
        struct place p = { id, name };
        if (starts_route(name)) routes[nr++] = p;
        else if (is_town(name)) towns[nt++] = p;
        else other[no++] = p;
    }
    qsort(routes, (size_t) nr, sizeof(struct place), cmp_place);
    qsort(other, (size_t) no, sizeof(struct place), cmp_place);
    struct { const char* title; struct place* list; int n; } groups[] = {
        { "TOWNS & CITIES", towns, nt }, { "ROUTES", routes, nr }, { "OTHER PLACES", other, no },
    };
    for (int gi = 0; gi < 3; gi++) {
        if (!groups[gi].n) continue;
        places[placeCount++] = (struct place) { -1, groups[gi].title };
        for (int i = 0; i < groups[gi].n && placeCount < MAX_PLACES; i++) places[placeCount++] = groups[gi].list[i];
    }
}

// --- drawing ---

static void frame(struct pd_canvas* c, int x, int y, int w, int h) {
    static const struct pd_layer LAYERS[] = { { 0x293131, 2 }, { 0x8C8CCE, 1 }, { 0xFFFFFF, 2 } };
    pd_layered_box(c, x, y, w, h, 3, LAYERS, 3, 0x000000);
}

// The page scaled to the view, each canvas pixel its nearest map pixel.
static void draw_page(struct pd_canvas* c, const uint32_t* px) {
    static int col[1024];
    for (int x = 0; x < view.w && x < 1024; x++) col[x] = crop.l + x * (crop.r - crop.l) / view.w;
    for (int y = 0; y < view.h; y++) {
        int cy = view.y + y;
        if (cy < c->cy0 || cy >= c->cy1) continue;
        const uint32_t* src = px + (crop.t + y * (crop.b - crop.t) / view.h) * mapW;
        uint32_t* dst = c->px + cy * c->w + view.x;
        for (int x = 0; x < view.w && x < 1024; x++) {
            int cx = view.x + x;
            if (cx >= c->cx0 && cx < c->cx1) dst[x] = src[col[x]];
        }
    }
}

static void draw_head(struct pd_canvas* c, const uint32_t* head, int tx, int ty) {
    // The head's 16 x 16 centred on its tile, scaled like the map.
    int x0 = sx(tx * 8 + 4 - 8), y0 = sy(ty * 8 + 4 - 8), x1 = sx(tx * 8 + 4 + 8), y1 = sy(ty * 8 + 4 + 8);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            uint32_t p = head[((y - y0) * 16 / (y1 - y0)) * 16 + (x - x0) * 16 / (x1 - x0)];
            if (p != PD_MAP_CLEAR) pd_fill(c, x, y, 1, 1, p);
        }
    }
}

// Four white L corners around a tile rect, out by 2 or 3 px (the blink); black on a Game Boy's light town map
// (MapLabelStyle.GEN1's cursorColor).
static void cursor_pixels(struct pd_canvas* c, int x, int y, int w, int h, bool big, uint32_t colour) {
    int out = big ? 3 : 2;
    int l = sx(x) - out, t = sy(y) - out;
    int rr = sx(x + w) + out, b = sy(y + h) + out;
    pd_clip(c, view.x, view.y, view.w, view.h);
    pd_fill(c, l, t, CURSOR_ARM, CURSOR_THICK, colour);
    pd_fill(c, l, t, CURSOR_THICK, CURSOR_ARM, colour);
    pd_fill(c, rr - CURSOR_ARM, t, CURSOR_ARM, CURSOR_THICK, colour);
    pd_fill(c, rr - CURSOR_THICK, t, CURSOR_THICK, CURSOR_ARM, colour);
    pd_fill(c, l, b - CURSOR_THICK, CURSOR_ARM, CURSOR_THICK, colour);
    pd_fill(c, l, b - CURSOR_ARM, CURSOR_THICK, CURSOR_ARM, colour);
    pd_fill(c, rr - CURSOR_ARM, b - CURSOR_THICK, CURSOR_ARM, CURSOR_THICK, colour);
    pd_fill(c, rr - CURSOR_THICK, b - CURSOR_ARM, CURSOR_THICK, CURSOR_ARM, colour);
    pd_unclip(c);
}

// Darkens a strip of the map (FireRed's label: black at 6/16).
static void darken(struct pd_canvas* c, int x, int y, int w, int h) {
    for (int yy = y < c->cy0 ? c->cy0 : y; yy < y + h && yy < c->cy1; yy++) {
        for (int xx = x < c->cx0 ? c->cx0 : x; xx < x + w && xx < c->cx1; xx++) {
            uint32_t p = c->px[yy * c->w + xx];
            uint32_t r = (p >> 16 & 0xFF) * 10 / 16, g = (p >> 8 & 0xFF) * 10 / 16, b = (p & 0xFF) * 10 / 16;
            c->px[yy * c->w + xx] = r << 16 | g << 8 | b;
        }
    }
}

// Returns how far down the map the label reaches where the buttons must stay clear of it (0: they don't).
static int label(struct pd_canvas* c, const struct pd_game* g, const char* name, const char* dungeon) {
    if (g->kind == PD_GAME_GB) {
        // Gen 1 / Gen 2 (MapLabelStyle.GEN1): the name in black on a white strip over the map's top row, 128 map
        // pixels wide, as tall as a line of the game's font needs; hidden when blank. The strip reaches under the
        // buttons on a small canvas, so they sit below it (blank or not: they don't jump as the label comes and goes).
        int w = sx(128) - view.x, h = sy(8) - view.y;
        if (h < 14) h = 14;
        if (!name || !*name) return h;
        pd_fill(c, view.x, view.y, w, h, PD_GB_PAPER);
        pd_text_fit(c, view.x + 3, view.y + (h - 14) / 2, w - 5, name, PD_GB_INK, PD_NO_SHADOW);
        return h;
    }
    if (g->kind == PD_GAME_EMERALD) {
        // Emerald: a framed window bottom-right, always shown.
        int w = 130, h = 30;
        int x = view.x + view.w - 2 - w, y = view.y + view.h - 2 - h;
        int f = pd_list_box(c, x, y, w, h);
        pd_fill(c, x + f, y + f, w - 2 * f, h - 2 * f, 0xFFFFFF);
        pd_text_fit(c, x + 7, pd_text_y(y, h), w - 10, name, PD_TITLE_TEXT, PD_TITLE_SHADOW);
        return 0;
    }
    // FireRed: a strip top-left per line, hidden when blank.
    const char* lines[2] = { name, dungeon };
    for (int i = 0; i < 2; i++) {
        if (!lines[i] || !*lines[i]) continue;
        int lw = view.w / 2;
        darken(c, view.x, view.y + i * 18, lw, 18);
        pd_text_fit(c, view.x + 5, view.y + i * 18 + 1, lw - 8, lines[i], 0xFFFFFF, FR_LABEL_SHADOW);
    }
    return 0;
}

// The map's buttons, a row over its top right (MapScreen's), right to left.
static int buttonRight, buttonTop;
static void map_button(struct pd_ui* ui, struct pd_canvas* c, const char* text, int id) {
    int w = pd_text_width(text) + 16;
    buttonRight -= w;
    ui_button(ui, c, buttonRight, buttonTop, w, 22, text, id);
    buttonRight -= 4;
}

// The map's view: a w x h page of which [l, r) x [t, b) is shown.
static void layout_box(struct pd_canvas* c, int width, int height, int l, int t, int r, int b) {
    mapW = width;
    mapH = height;
    crop.l = l;
    crop.t = t;
    crop.r = r;
    crop.b = b;
    int mw = crop.r - crop.l, mh = crop.b - crop.t;
    // The map fills the tab: stretched to it while that's within 1.2x of its shape, else as big as fits,
    // centred (MapScreen's MAX_MAP_STRETCH).
    int aw = c->w - 2 * MARGIN - 2 * FRAME, ah = c->h - CONTENT_Y - MARGIN - 2 * FRAME;
    int w = aw, h = ah;
    if ((long) aw * mh * MAX_STRETCH_DEN > (long) ah * mw * MAX_STRETCH_NUM) w = ah * mw / mh;
    else if ((long) ah * mw * MAX_STRETCH_DEN > (long) aw * mh * MAX_STRETCH_NUM) h = aw * mh / mw;
    view.w = w > 0 ? w : 1;
    view.h = h > 0 ? h : 1;
    view.x = MARGIN + FRAME + (aw - w) / 2;
    view.y = CONTENT_Y + FRAME + (ah - h) / 2;
}

// A page of the game's map, without the border its crop leaves out.
static void layout(struct pd_canvas* c, const struct pd_map_art* art, int page) {
    const int16_t* k = art->crop[page];
    if (k[2] > k[0] && k[3] > k[1]) layout_box(c, art->w, art->h, k[0], k[1], k[2], k[3]);
    else layout_box(c, art->w, art->h, 0, 0, art->w, art->h);
}

void ui_map_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                const struct pd_host_info* host) {
    lastGame = g;
    const struct pd_map_art* art = pd_map_art(g);
    if (!art) {
        ui_notice(c, "NO MAP", "The region map's art wasn't found", "in this ROM.");
        return;
    }
    int playerPage = -1, ptx = 0, pty = 0;
    bool hasPlayer = pd_map_player_tile(g, s, &playerPage, &ptx, &pty);
    lastPlayerPage = hasPlayer ? playerPage : -1;
    lastPlayerTx = ptx;
    lastPlayerTy = pty;
    int page = ui->mapPage >= 0 ? ui->mapPage : hasPlayer ? playerPage : 0;
    if (page >= art->pages) page = 0;
    layout(c, art, page);
    frame(c, view.x - FRAME, view.y - FRAME, view.w + 2 * FRAME, view.h + 2 * FRAME);
    draw_page(c, art->page[page]);

    // What the cursor and label show: the tapped place, else where the player is.
    struct pd_tile_rect rects[MAX_RECTS];
    int n = 0;
    const char* name = "";
    const char* dungeon = NULL;
    if (ui->mapSel >= 0 || ui->mapDungeon >= 0 || ui->mapPage >= 0) {
        int main = ui->mapSel >= 0 ? ui->mapSel : ui->mapDungeon;
        name = pd_mapsec_name(g, main);
        if (ui->mapDungeon >= 0) {
            rects[n++] = (struct pd_tile_rect) { ui->mapSelTx, ui->mapSelTy, 1, 1 };
            if (ui->mapSel >= 0) dungeon = pd_mapsec_name(g, ui->mapDungeon);
        } else {
            int p;
            n = pd_map_tiles(g, main, &p, rects, MAX_RECTS);
        }
    } else if (hasPlayer && page == playerPage) {
        int p;
        n = pd_map_tiles(g, pd_map_player_section(g, s), &p, rects, MAX_RECTS);
        name = pd_mapsec_name(g, s->mapsec);
    } else if (g->kind == PD_GAME_GB && !hasPlayer && s->valid && s->posOk) {
        name = pd_mapsec_name(g, s->mapsec);  // off the map (FAST SHIP): its name, no cursor
    }
    int gender = s->gender == 1 ? 1 : 0;
    pd_clip(c, view.x, view.y, view.w, view.h);
    if (hasPlayer && page == playerPage && art->hasHead[gender]) draw_head(c, art->head[gender], ptx, pty);
    pd_unclip(c);
    for (int i = 0; i < n; i++) cursor_pixels(c, rects[i].x * 8, rects[i].y * 8, rects[i].w * 8, rects[i].h * 8, ui->mapBlink, g->kind == PD_GAME_GB ? PD_GB_INK : CURSOR);
    int below = label(c, g, name, dungeon);
    ui_add_hit(ui, view.x, view.y, view.w, view.h, HIT_MAP);

    // Over the map's top right: PLACES, ME, the other page(s) (MapScreen's order, left to right).
    buttonRight = view.x + view.w - 4;
    buttonTop = view.y + (below ? below + 2 : 4);
    if (ui->shoulderTabs) {
        if (art->pages > 1) map_button(ui, c, pd_map_page_name(g, (page + 1) % art->pages), HIT_MAP_REGION);
        if (host && host->flyReady) map_button(ui, c, "FLY", HIT_MAP_FLY);
        map_button(ui, c, "ME", HIT_MAP_ME);
        map_button(ui, c, "PLACES", HIT_MAP_PLACES);
    } else {
        map_button(ui, c, "PLACES", HIT_MAP_PLACES);
        if (host && host->flyReady) map_button(ui, c, "FLY", HIT_MAP_FLY);
        if (ui->mapSel >= 0 || ui->mapDungeon >= 0 || (ui->mapPage >= 0 && ui->mapPage != playerPage))
            map_button(ui, c, "ME", HIT_MAP_ME);
        if (art->pages > 1) map_button(ui, c, pd_map_page_name(g, (page + 1) % art->pages), HIT_MAP_REGION);
    }
}

// MAP's FLY: the towns the game would fly to.
void ui_fly_overlay(struct pd_ui* ui, struct pd_canvas* c) {
    pd_dim(c, 0xE6);
    ui_add_hit(ui, 0, 0, c->w, c->h, HIT_SCRIM);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = CONTENT_Y;
    int f = pd_title_box(c, x, y, w, 24, PD_TITLE_FILL);
    pd_text(c, x + f + 6, pd_text_y(y, 24), "FLY", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    y += 28;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf, iy = y + lf + 1, iw = w - 2 * lf, ih = c->h - y - MARGIN - 2 * lf - 2;
    ui_add_scroll_hit(ui, ix, iy, iw, ih, HIT_SCRIM);
    ui->placesScroll = ui_clamp_scroll(ui->placesScroll, ui->flyCount * PLACE_ROW, ih);
    pd_clip(c, ix, iy, iw, ih);
    for (int i = ui->placesScroll / PLACE_ROW; i < ui->flyCount; i++) {
        int ry = iy + i * PLACE_ROW - ui->placesScroll;
        if (ry >= iy + ih) break;
        if (ui->pressedId == HIT_FLY_TOWN + i) pd_fill(c, ix, ry, iw, PLACE_ROW, PD_ROW_SELECTED);
        pd_text_fit(c, ix + 14, pd_text_y(ry, PLACE_ROW), iw - 28, ui->flyNames[i], PD_LABEL, PD_LABEL_SHADOW);
        int top = ry < iy ? iy : ry;
        int bottom = ry + PLACE_ROW > iy + ih ? iy + ih : ry + PLACE_ROW;
        ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_FLY_TOWN + i);
    }
    pd_unclip(c);
    ui_scroll_bar(c, ix + iw - 4, iy, ih, ui->placesScroll, ui->flyCount * PLACE_ROW, PD_MUTED);
}

void ui_places_overlay(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g) {
    build_places(g);
    pd_dim(c, 0xE6);
    ui_add_hit(ui, 0, 0, c->w, c->h, HIT_SCRIM);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = CONTENT_Y;
    int f = pd_title_box(c, x, y, w, 24, PD_TITLE_FILL);
    pd_text(c, x + f + 6, pd_text_y(y, 24), "PLACES", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    y += 28;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf, iy = y + lf + 1, iw = w - 2 * lf, ih = c->h - y - MARGIN - 2 * lf - 2;
    ui_add_scroll_hit(ui, ix, iy, iw, ih, HIT_SCRIM);
    // The row a host's D-pad cursor moves onto comes wholly into view (a row cut at the window's edge).
    static int seenFocus = -1;
    if (ui->focusId != seenFocus && ui->focusId >= HIT_PLACE && ui->focusId < HIT_PLACE + placeCount)
        ui->placesScroll = ui_scroll_to(ui->placesScroll, (ui->focusId - HIT_PLACE) * PLACE_ROW, PLACE_ROW, ih);
    seenFocus = ui->focusId;
    ui->placesScroll = ui_clamp_scroll(ui->placesScroll, placeCount * PLACE_ROW, ih);
    pd_clip(c, ix, iy, iw, ih);
    for (int i = ui->placesScroll / PLACE_ROW; i < placeCount; i++) {
        int ry = iy + i * PLACE_ROW - ui->placesScroll;
        if (ry >= iy + ih) break;
        int ty = pd_text_y(ry, PLACE_ROW);
        if (places[i].mapsec < 0) {
            pd_text(c, ix + 6, ty, places[i].name, PD_VALUE, PD_VALUE_SHADOW);
            continue;
        }
        if (ui->pressedId == HIT_PLACE + i) pd_fill(c, ix, ry, iw, PLACE_ROW, PD_ROW_SELECTED);
        pd_text_fit(c, ix + 14, ty, iw - 100, places[i].name, PD_LABEL, PD_LABEL_SHADOW);
        int page;
        struct pd_tile_rect r;
        if (pd_map_tiles(g, places[i].mapsec, &page, &r, 1) && page > 0) {
            pd_text_right(c, ix + iw - 10, ty, pd_map_page_name(g, page), PD_MUTED, PD_MUTED_SHADOW);
        }
        int top = ry < iy ? iy : ry;
        int bottom = ry + PLACE_ROW > iy + ih ? iy + ih : ry + PLACE_ROW;
        ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_PLACE + i);
    }
    pd_unclip(c);
    ui_scroll_bar(c, ix + iw - 4, iy, ih, ui->placesScroll, placeCount * PLACE_ROW, PD_MUTED);
}

bool ui_map_act(struct pd_ui* ui, int id) {
    if (ui->overlay == PD_OVERLAY_FLY) {
        if (id >= HIT_FLY_TOWN && id < HIT_FLY_TOWN + ui->flyCount) {
            ui->flyPick = id - HIT_FLY_TOWN;
            ui->overlay = PD_OVERLAY_NONE;
            ui->action = PD_ACTION_FLY_TO;
            return true;
        }
        if (id == HIT_SCRIM) {
            ui->overlay = PD_OVERLAY_NONE;
            return true;
        }
        return false;
    }
    if (ui->overlay == PD_OVERLAY_PLACES) {
        if (id >= HIT_PLACE && id < HIT_PLACE + placeCount && places[id - HIT_PLACE].mapsec >= 0 && lastGame) {
            int mapsec = places[id - HIT_PLACE].mapsec, page;
            struct pd_tile_rect r;
            if (pd_map_tiles(lastGame, mapsec, &page, &r, 1)) {
                ui->mapPage = page;
                ui->mapSel = mapsec;
                ui->mapDungeon = -1;
            }
            ui->overlay = PD_OVERLAY_NONE;
            return true;
        }
        if (id == HIT_SCRIM) {
            ui->overlay = PD_OVERLAY_NONE;
            return true;
        }
        return false;
    }
    if (ui->tab != PD_TAB_MAP || ui->overlay != PD_OVERLAY_NONE || !lastGame) return false;
    const struct pd_map_art* art = pd_map_art(lastGame);
    int pages = art ? art->pages : 1;
    int shown = ui->mapPage >= 0 ? ui->mapPage : lastPlayerPage >= 0 ? lastPlayerPage : 0;
    switch (id) {
    case HIT_MAP: {
        int tx = (crop.l + (ui->touchX0 - view.x) * (crop.r - crop.l) / view.w) / 8;
        int ty = (crop.t + (ui->touchY0 - view.y) * (crop.b - crop.t) / view.h) / 8;
        // The player's own tile: back to following the player.
        if (shown == lastPlayerPage && tx == lastPlayerTx && ty == lastPlayerTy) {
            ui->mapSel = ui->mapDungeon = -1;
            ui->mapPage = -1;
            return true;
        }
        int mapsec, dungeon;
        pd_map_pick(lastGame, shown, tx, ty, &mapsec, &dungeon);
        ui->mapPage = shown;
        ui->mapSel = mapsec;
        ui->mapDungeon = dungeon;
        ui->mapSelTx = tx;
        ui->mapSelTy = ty;
        return true;
    }
    case HIT_MAP_REGION:
        ui->mapPage = (shown + 1) % pages;
        ui->mapSel = ui->mapDungeon = -1;
        return true;
    case HIT_MAP_ME:
        ui->mapPage = ui->mapSel = ui->mapDungeon = -1;
        return true;
    case HIT_MAP_PLACES:
        ui->overlay = PD_OVERLAY_PLACES;
        ui->placesScroll = 0;
        return true;
    case HIT_MAP_FLY:
        ui->action = PD_ACTION_FLY_ASK;
        return true;
    default:
        return false;
    }
}

// The same map frame, bounded stretch, nearest-neighbour scaling, label and cursor
// for another backend's already-composed art. The normal GBA path still builds its
// own page, head and selection above.
enum pd_map_input pd_ui_map_control(int* action, int count, int dx, int dy, bool accept, bool back, bool places) {
    if (*action >= count) *action = -1;
    if (back) {
        if (*action >= 0 || count == 0) return PD_MAP_CLOSE;
        *action = 0;
        return PD_MAP_IDLE;
    }
    if (*action >= 0) {
        *action = pd_ui_list_step(*action, count, dx);
        if (accept) return PD_MAP_CHOOSE;
        if (dy < 0) *action = -1;
        return PD_MAP_IDLE;
    }
    if (places && count) return PD_MAP_OPEN_PLACES;
    if (accept) return PD_MAP_CENTER;
    return dx || dy ? PD_MAP_MOVE : PD_MAP_IDLE;
}

struct pd_map_viewport pd_ui_map_view(struct pd_canvas* c, const struct pd_map_source* source) {
    if (!source || !source->pixels || source->width < 1 || source->height < 1 ||
        source->width > 4096 || source->height > 4096 || c->w < 32 || c->h < 32 || c->w > 1024) {
        ui_notice(c, "NO MAP", "The region map's art wasn't found", "in this ROM.");
        return (struct pd_map_viewport){0};
    }
    layout_box(c, source->width, source->height, 0, 0, source->width, source->height);
    frame(c, view.x - FRAME, view.y - FRAME, view.w + 2 * FRAME, view.h + 2 * FRAME);
    draw_page(c, source->pixels);
    if (source->cursorX >= 0 && source->cursorY >= 0 && source->cursorW > 0 && source->cursorH > 0 &&
        source->cursorX <= mapW - source->cursorW && source->cursorY <= mapH - source->cursorH)
        cursor_pixels(c, source->cursorX, source->cursorY, source->cursorW, source->cursorH, false, CURSOR);
    const struct pd_game game = {0};
    label(c, &game, source->name, NULL);
    return (struct pd_map_viewport){view.x, view.y, view.w, view.h};
}

// The right wing's sections (LAYOUT > RIGHT WING): under the map, each fact a label over its value (the badges as
// pips), the HERE list, the QUICK BAG, the next boss's team, what's left to pick up here, the NUZLOCKE area, the
// GUIDE's tip, the party's shared weaknesses and who's ready to evolve; the SAVE STATE / CLOSE GAME buttons at the
// foot. What the save doesn't give, and what the host's bars already show (pd_ui_bar_facts), is left out.
const char* const pd_wing_section_names[PD_WING_SECTIONS] = {
    "MAP", "BADGES", "PLAY TIME", "BOSS AND CAP", "CAUGHT HERE", "CAUGHT / SEEN", "MONEY", "HERE", "QUICK BAG",
    "STATE BUTTONS", "NEXT BOSS", "ITEMS HERE", "NUZLOCKE", "TIP", "PARTY WEAKNESSES", "READY TO EVOLVE", "TMS HERE", "TIP OR WEAKNESSES"};
const int pd_wing_default[] = {PD_WS_MAP,       PD_WS_BOSS_TEAM,   PD_WS_HERE,
                               PD_WS_QUICK_BAG, PD_WS_TIP_OR_WEAK, PD_WS_BUTTONS};
const int pd_wing_default_count = sizeof pd_wing_default / sizeof pd_wing_default[0];
const int* pd_ui_wing_list;
int pd_ui_wing_count;
const struct pd_nuzlocke* pd_ui_wing_nuzlocke;

struct wing_fact { char label[24], value[48]; int pips; }; // pips >= 0: that many of 8 badges, drawn as dots;
                                                           // WILD_ICONS: the value beside the label, the icons under
enum { WILD_ICONS = -2, WILD_MAX = 24, ITEMS_SHOWN = 4 };
static int wildSpecies[WILD_MAX], wildMin[WILD_MAX], wildMax[WILD_MAX], wildCount, wildHere, wildKinds;
static bool wildCaught[WILD_MAX];
// HERE lists the area's kinds under the facts: CAUGHT HERE's icon row is left out then.
static bool hereAsList;

bool pd_ui_wing_buttons_on;
struct pd_ui_rect pd_ui_wing_buttons[PD_WING_BUTTONS];
int pd_ui_repel_steps = -1;

// The guide's (a walk over the boss teams, this area's wild sets and things): worked out twice a second at most.
static char boss[40], itemNames[ITEMS_SHOWN][32], tmNames[ITEMS_SHOWN][32], tip[160];
static int cap, bossN, itemsLeft, tmsLeft;
static bool tipOk;
static struct pd_team_mon bossTeam[PD_TEAM_MAX];

static void wing_refresh(const struct pd_game* g, const struct pd_snapshot* s) {
    static uint32_t at;
    static const struct pd_snapshot* atSave;
    if (pd_ui_now_ms && pd_ui_now_ms - at < 500u && atSave == s) return;
    at = pd_ui_now_ms;
    atSave = s;
    bossN = pd_ui_next_boss_team(g, s, boss, sizeof(boss), bossTeam);
    cap = 0;
    for (int i = 0; i < bossN; i++) cap = bossTeam[i].level > cap ? bossTeam[i].level : cap;
    int here, kinds;
    if (!pd_ui_caught_here(g, s, &here, &kinds)) kinds = 0;
    wildCount = kinds > 0 ? pd_ui_wild_levels_here(g, s, wildSpecies, wildCaught, wildMin, wildMax, WILD_MAX) : 0;
    wildHere = here;
    wildKinds = kinds;
    itemsLeft = pd_ui_area_items(g, s, false, itemNames, ITEMS_SHOWN);
    tmsLeft = pd_ui_area_items(g, s, true, tmNames, ITEMS_SHOWN);
    tipOk = pd_ui_area_tip(g, s, tip, sizeof(tip));
}

// A fact section's row: false when the save doesn't give it or a bar shows it already.
static bool wing_fact(const struct pd_game* g, const struct pd_snapshot* s, int sec, struct wing_fact* row) {
    uint32_t bars = pd_ui_bar_facts;
    row->pips = -1;
    switch (sec) {
    case PD_WS_BADGES: {
        if (!g || !g->cfg || g->cfg->badgeFlag <= 0 || (bars & PD_BAR_BADGES)) return false;
        int badges = 0;
        for (int i = 0; i < 8; i++) badges += pd_flag(s, g->cfg->badgeFlag + i);
        snprintf(row->label, sizeof(row->label), "BADGES");
        snprintf(row->value, sizeof(row->value), "%d OF 8", badges);
        row->pips = badges;
        return true;
    }
    case PD_WS_PLAY_TIME:
        if (!(s->playHours || s->playMinutes) || (bars & PD_BAR_TIME)) return false;
        snprintf(row->label, sizeof(row->label), "PLAY TIME");
        snprintf(row->value, sizeof(row->value), "%d:%02d", s->playHours, s->playMinutes);
        return true;
    case PD_WS_NEXT_BOSS:
        if (cap <= 0 || !boss[0] || (bars & PD_BAR_BOSS)) return false;
        snprintf(row->label, sizeof(row->label), "NEXT BOSS");
        snprintf(row->value, sizeof(row->value), "%s Lv%d", boss, cap);
        return true;
    case PD_WS_CAUGHT_HERE:
        if (wildKinds <= 0 || (bars & PD_BAR_HERE) || (hereAsList && wildCount > 0)) return false;
        snprintf(row->label, sizeof(row->label), "CAUGHT HERE");
        snprintf(row->value, sizeof(row->value), "%d / %d", wildHere, wildKinds);
        // The area's kinds as icons under it, the ones not caught yet faded.
        row->pips = wildCount > 0 && pd_mon_icon_pixels(g, wildSpecies[0], false) ? WILD_ICONS : -1;
        return true;
    case PD_WS_CAUGHT_SEEN: {
        if (!s->dexOk || s->dexCount <= 0 || (bars & PD_BAR_DEX)) return false;
        int seen = 0, caught = 0;
        for (int i = 0; i < s->dexCount && i / 8 < PD_DEX_BYTES; i++) {
            seen += s->dexSeen[i / 8] >> (i % 8) & 1;
            caught += s->dexCaught[i / 8] >> (i % 8) & 1;
        }
        snprintf(row->label, sizeof(row->label), "CAUGHT / SEEN");
        snprintf(row->value, sizeof(row->value), "%d / %d", caught, seen);
        return true;
    }
    case PD_WS_MONEY: {
        if (s->money < 0 || (bars & PD_BAR_MONEY)) return false;
        // A change rolls to the new sum over ROLL_MS rather than jumping.
        static long from = -1, to = -1;
        static uint32_t since;
        enum { ROLL_MS = 700 };
        long shownMoney = s->money;
        if (to < 0 || pd_ui_reduce_motion || !pd_ui_now_ms) {
            from = to = s->money;
        } else {
            uint32_t t = pd_ui_now_ms - since;
            long now = t >= ROLL_MS ? to : from + (to - from) * (long)t / ROLL_MS;
            if (s->money != to) {
                from = now;
                to = s->money;
                since = pd_ui_now_ms;
                t = 0;
            }
            shownMoney = t >= ROLL_MS ? to : from + (to - from) * (long)t / ROLL_MS;
            if (t < ROLL_MS) pd_ui_panel_moving = true;
        }
        snprintf(row->label, sizeof(row->label), "MONEY");
        snprintf(row->value, sizeof(row->value), "$%ld", shownMoney);
        return true;
    }
    default:
        return false;
    }
}
#define WILD_ROW_H (16 + PD_ICON + 4)

static int fact_height(const struct wing_fact* f) { return f->pips == WILD_ICONS ? WILD_ROW_H : 2 * 16 + 4; }

// The area's icons in one row across w: a step apart (overlapping when they don't fit), as many as fit at 12 px.
static void wild_icons(struct pd_canvas* c, const struct pd_game* g, int x, int y, int w) {
    int n = wildCount, step = PD_ICON;
    if (n * PD_ICON > w) {
        step = n > 1 ? (w - PD_ICON) / (n - 1) : PD_ICON;
        if (step < 12) {
            step = 12;
            n = (w - PD_ICON) / step + 1;
        }
    }
    for (int i = 0; i < n; i++) pd_draw_mon_icon(c, g, wildSpecies[i], x + i * step, y, !wildCaught[i]);
}

static void draw_fact(struct pd_canvas* c, const struct wing_fact* row, int x, int y, int w) {
    if (row->pips == WILD_ICONS) {
        int vw = pd_number_width(row->value);
        pd_heading_fit(c, x, y, w - vw - 8, row->label, PD_MUTED, PD_MUTED_SHADOW);
        pd_number_right(c, x + w, y, row->value, PD_LABEL, PD_LABEL_SHADOW);
        wild_icons(c, ui_game, x, y + 16, w);
        return;
    }
    pd_heading_fit(c, x, y, w, row->label, PD_MUTED, PD_MUTED_SHADOW);
    if (row->pips >= 0 && w - 4 >= 8 * 10) {
        // A new badge's dot pops: drawn bigger for POP_MS, then settles.
        static int badgesSeen = -1;
        static uint32_t popSince;
        enum { POP_MS = 900 };
        if (badgesSeen >= 0 && row->pips > badgesSeen && pd_ui_now_ms && !pd_ui_reduce_motion) popSince = pd_ui_now_ms;
        badgesSeen = row->pips;
        bool pop = popSince && pd_ui_now_ms - popSince < POP_MS;
        if (pop) pd_ui_panel_moving = true;
        // Eight dots, the badges earned filled gold, the rest as rings.
        for (int k = 0; k < 8; k++) {
            int px = x + 4 + k * 10, py = y + 16 + 4;
            if (pop && k == row->pips - 1) {
                pd_round_rect(c, px - 2, py - 2, 12, 12, 6, 0xB08820);
                pd_round_rect(c, px - 1, py - 1, 10, 10, 5, 0xF8C830);
                pd_fill(c, px + 1, py + 1, 3, 2, 0xFFFFFF);
            } else if (k < row->pips) {
                pd_round_rect(c, px, py, 8, 8, 4, 0xB08820);
                pd_round_rect(c, px + 1, py + 1, 6, 6, 3, 0xF8C830);
                pd_fill(c, px + 2, py + 2, 2, 1, 0xFFF0B0);
            } else {
                pd_round_rect(c, px, py, 8, 8, 4, PD_MUTED);
                pd_round_rect(c, px + 1, py + 1, 6, 6, 3, PD_TITLE_FILL);
            }
        }
        return;
    }
    if (pd_number_width(row->value) <= w - 4) pd_number(c, x + 4, y + 16, row->value, PD_LABEL, PD_LABEL_SHADOW);
    else pd_text_fit(c, x + 4, y + 16, w - 4, row->value, PD_LABEL, PD_LABEL_SHADOW);
}

// HERE: this area's wild kinds, a row each: its icon, its levels here and a tick once caught. QUICK BAG: the balls,
// the healing items and the repellents in the bag, and the steps left while one works. BUTTONS: SAVE STATE and CLOSE
// GAME (the desktop host's), side by side where they fit, else one over the other.
#define HERE_ROW 22
#define BAG_ROW 16
#define BUTTON_H 22
#define GAP 6 // under each section but the facts (theirs is in their height)

static bool name_has(const char* name, const char* part) {
    size_t n = strlen(part);
    for (; name && *name; name++)
        if (!strncasecmp(name, part, n)) return true;
    return false;
}

// The bag's balls, healing items and repellents, each -1 when the bag isn't read.
static void quick_bag(const struct pd_game* g, const struct pd_snapshot* s, int* balls, int* heal, int* repel) {
    *balls = *heal = *repel = -1;
    if (!s->valid || !s->bagOk) return;
    *balls = *heal = *repel = 0;
    static const char* const HEALING[] = {"POTION", "RESTORE", "FRESH WATER", "SODA POP", "LEMONADE", "MOOMOO",
                                          "BERRY JUICE", "ENERGYPOWDER", "ENERGY POWDER", "ENERGY ROOT"};
    for (int p = 0; p < PD_POCKET_COUNT; p++) {
        const struct pd_pocket_items* pk = &s->bag[p];
        for (int i = 0; i < pk->count && i < PD_POCKET_SLOTS; i++) {
            int id = pk->items[i].id, q = pk->items[i].quantity;
            if (!id || !q) continue;
            if (p == PD_POCKET_BALLS) {
                *balls += q;
                continue;
            }
            if (p == PD_POCKET_KEY || p == PD_POCKET_TMHM || p == PD_POCKET_BERRIES) continue;
            const char* name = pd_item_name(g, id);
            if (!name || !*name) continue;
            if (name_has(name, "REPEL")) {
                *repel += q;
                continue;
            }
            bool healing = p == PD_POCKET_MEDICINE && !name_has(name, "ETHER") && !name_has(name, "ELIXIR");
            for (size_t k = 0; !healing && k < sizeof HEALING / sizeof HEALING[0]; k++) healing = name_has(name, HEALING[k]);
            if (healing && !name_has(name, "PP ")) *heal += q;
        }
    }
}

// Whether the bag holds item ID (any pocket).
static bool bag_has(const struct pd_snapshot* s, int id) {
    if (!s->bagOk || id <= 0) return false;
    for (int p = 0; p < PD_POCKET_COUNT; p++)
        for (int i = 0; i < s->bag[p].count && i < PD_POCKET_SLOTS; i++)
            if (s->bag[p].items[i].id == id && s->bag[p].items[i].quantity) return true;
    return false;
}

static int quick_bag_height(const struct pd_snapshot* s) {
    if (!s->valid) return 0;
    if (!s->bagOk) return 2 * BAG_ROW;
    return 4 * BAG_ROW + (pd_ui_repel_steps > 0 ? BAG_ROW : 0);
}

static void quick_bag_draw(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s, int x, int y,
                           int w) {
    pd_heading_fit(c, x, y, w, "QUICK BAG", PD_MUTED, PD_MUTED_SHADOW);
    y += BAG_ROW;
    if (!s->bagOk) {
        pd_text_fit(c, x + 4, y, w - 4, "N/A", PD_LABEL, PD_LABEL_SHADOW);
        return;
    }
    int counts[3];
    quick_bag(g, s, &counts[0], &counts[1], &counts[2]);
    static const char* const NAMES[3] = {"BALLS", "HEALING", "REPELS"};
    for (int i = 0; i < 3; i++, y += BAG_ROW) {
        char v[16];
        snprintf(v, sizeof(v), "%d", counts[i]);
        int vw = pd_number_width(v);
        pd_text_fit(c, x + 4, y, w - vw - 10, NAMES[i], PD_LABEL, PD_LABEL_SHADOW);
        pd_number_right(c, x + w, y, v, PD_LABEL, PD_LABEL_SHADOW);
    }
    if (pd_ui_repel_steps > 0) {
        char line[40];
        snprintf(line, sizeof(line), "REPEL: %d STEPS", pd_ui_repel_steps);
        pd_text_fit(c, x + 4, y, w - 4, line, PD_VALUE, PD_VALUE_SHADOW);
    }
}

// A tick: two strokes, 7 x 6.
static void tick(struct pd_canvas* c, int x, int y, uint32_t ink) {
    for (int i = 0; i < 3; i++) pd_fill(c, x + i, y + 3 + i, 2, 1, ink);
    for (int i = 0; i < 5; i++) pd_fill(c, x + 3 + i, y + 4 - i, 2, 1, ink);
}

// A heading with a count (or other short text) at its right.
static void heading_count(struct pd_canvas* c, int x, int y, int w, const char* label, const char* count) {
    int cw = count && *count ? pd_number_width(count) : 0;
    pd_heading_fit(c, x, y, cw ? w - cw - 8 : w, label, PD_MUTED, PD_MUTED_SHADOW);
    if (cw) pd_number_right(c, x + w, y, count, PD_LABEL, PD_LABEL_SHADOW);
}

// An icon cut to a HERE_ROW-high row at (x, y).
static void row_icon(struct pd_canvas* c, const struct pd_game* g, int species, int x, int y, bool faded) {
    pd_clip(c, x, y, PD_ICON, HERE_ROW);
    pd_draw_mon_icon(c, g, species, x, y + HERE_ROW - PD_ICON + 2, faded);
    pd_unclip(c);
}

// The HERE list from Y down to BOTTOM: as many rows as fit.
static void here_list(struct pd_canvas* c, const struct pd_game* g, int x, int y, int w, int bottom) {
    if (y + 16 + HERE_ROW > bottom) return;
    char count[16];
    snprintf(count, sizeof(count), "%d/%d", wildHere, wildKinds);
    heading_count(c, x, y, w, "HERE", count);
    y += 16;
    int fit = (bottom - y) / HERE_ROW;
    int shown = wildCount < fit ? wildCount : fit;
    for (int i = 0; i < shown; i++, y += HERE_ROW) {
        row_icon(c, g, wildSpecies[i], x, y, !wildCaught[i]);
        char lv[32];
        if (wildMin[i] > 0 && wildMax[i] > wildMin[i]) snprintf(lv, sizeof(lv), "Lv%d-%d", wildMin[i] % 1000, wildMax[i] % 1000);
        else if (wildMin[i] > 0) snprintf(lv, sizeof(lv), "Lv%d", wildMin[i] % 1000);
        else lv[0] = 0;
        int ty = pd_text_y(y, HERE_ROW);
        pd_text_fit(c, x + PD_ICON + 2, ty, w - PD_ICON - 14, lv, wildCaught[i] ? PD_LABEL : PD_MUTED,
                    wildCaught[i] ? PD_LABEL_SHADOW : PD_MUTED_SHADOW);
        if (wildCaught[i]) tick(c, x + w - 8, y + HERE_ROW / 2 - 3, 0x30A050);
    }
}

// BOSS TEAM: the next boss's name and the type most of its team shares (a gym's), then a row a member: its icon, its
// level and its types.
static int boss_team(struct pd_canvas* c, const struct pd_game* g, int x, int y, int w) {
    if (bossN <= 0 || !boss[0]) return 0;
    int h = 16 + bossN * HERE_ROW + GAP;
    if (!c) return h;
    int counts[32] = {0}, best = -1;
    for (int i = 0; i < bossN; i++) {
        int t1, t2;
        pd_species_type_pair(g, bossTeam[i].species, &t1, &t2);
        if (t1 != PD_TYPE_NONE && t1 < 32) counts[t1]++;
        if (t2 != PD_TYPE_NONE && t2 != t1 && t2 < 32) counts[t2]++;
    }
    for (int t = 0; t < 32; t++)
        if (counts[t] * 2 > bossN && (best < 0 || counts[t] > counts[best])) best = t;
    int pw = best >= 0 ? ui_type_pills(NULL, 0, 0, best, PD_TYPE_NONE, w / 2, false) : 0;
    pd_heading_fit(c, x, y, w - (pw ? pw + 6 : 0), boss, PD_MUTED, PD_MUTED_SHADOW);
    if (pw) ui_type_pills(c, x + w, y + 2, best, PD_TYPE_NONE, w / 2, true);
    y += 16;
    for (int i = 0; i < bossN; i++, y += HERE_ROW) {
        row_icon(c, g, bossTeam[i].species, x, y, false);
        char lv[16];
        snprintf(lv, sizeof(lv), "Lv%d", bossTeam[i].level % 1000);
        int ty = pd_text_y(y, HERE_ROW), lx = x + PD_ICON + 2;
        int lw = pd_text_fit(c, lx, ty, w - PD_ICON - 2, lv, PD_LABEL, PD_LABEL_SHADOW);
        int t1, t2;
        pd_species_type_pair(g, bossTeam[i].species, &t1, &t2);
        ui_type_pills(c, x + w, y + (HERE_ROW - 12) / 2, t1, t2, w - PD_ICON - lw - 8, true);
    }
    return h;
}

// ITEMS HERE / TMS HERE: what's left to pick up in the area, a name a row (the first few).
static int area_items(struct pd_canvas* c, int x, int y, int w, bool tms) {
    int left = tms ? tmsLeft : itemsLeft;
    if (left < 0 || (tms && left == 0)) return 0; // no area data; no TMs left here: nothing to say
    int rows = left == 0 ? 1 : left < ITEMS_SHOWN ? left : ITEMS_SHOWN;
    bool more = left > ITEMS_SHOWN;
    int h = 16 + (rows + more) * BAG_ROW + GAP;
    if (!c) return h;
    char count[12];
    snprintf(count, sizeof(count), "%d", left);
    heading_count(c, x, y, w, tms ? "TMS HERE" : "ITEMS HERE", count);
    y += 16;
    if (!left) {
        pd_text_fit(c, x + 4, y, w - 4, "NONE LEFT", PD_MUTED, PD_MUTED_SHADOW);
        return h;
    }
    for (int i = 0; i < rows; i++, y += BAG_ROW)
        pd_text_fit(c, x + 4, y, w - 4, tms ? tmNames[i] : itemNames[i], PD_LABEL, PD_LABEL_SHADOW);
    if (more) {
        char line[24];
        snprintf(line, sizeof(line), "AND %d MORE", left - ITEMS_SHOWN);
        pd_text_fit(c, x + 4, y, w - 4, line, PD_MUTED, PD_MUTED_SHADOW);
    }
    return h;
}

// NUZLOCKE (while a run is on): this area's encounter, free or what became of it, and the run's living and dead.
static int nuzlocke_section(struct pd_canvas* c, const struct pd_snapshot* s, int x, int y, int w) {
    const struct pd_nuzlocke* n = pd_ui_wing_nuzlocke;
    if (!n || !n->on || !n->supported) return 0;
    int h = 16 + 2 * BAG_ROW + GAP;
    if (!c) return h;
    pd_heading_fit(c, x, y, w, "NUZLOCKE", PD_MUTED, PD_MUTED_SHADOW);
    y += 16;
    const struct pd_nuz_encounter* e = s->posOk ? pd_nuz_encounter_at(&n->run, s->mapsec) : NULL;
    const char* state = !s->posOk ? "?" : !e ? "FREE" : e->outcome == PD_NUZ_CAUGHT ? "CAUGHT"
                                         : e->outcome == PD_NUZ_MISSED ? "MISSED" : "NOW";
    char line[48];
    snprintf(line, sizeof(line), "AREA: %s", state);
    pd_text_fit(c, x + 4, y, w - 4, line, e ? PD_LABEL : PD_VALUE, e ? PD_LABEL_SHADOW : PD_VALUE_SHADOW);
    y += BAG_ROW;
    snprintf(line, sizeof(line), "ALIVE %d  DEAD %d", pd_nuz_alive(&n->run), pd_nuz_dead(&n->run));
    pd_text_fit(c, x + 4, y, w - 4, line, PD_LABEL, PD_LABEL_SHADOW);
    return h;
}

// TIP: the GUIDE's next thing to do here (hidden where it has none).
static int tip_section(struct pd_canvas* c, int x, int y, int w) {
    if (!tipOk || !tip[0]) return 0;
    int lines = pd_text_wrap(NULL, x + 4, 0, w - 4, BAG_ROW, 4, tip, 0, 0);
    if (lines <= 0) return 0;
    int h = 16 + lines * BAG_ROW + GAP;
    if (!c) return h;
    pd_heading_fit(c, x, y, w, "TIP", PD_MUTED, PD_MUTED_SHADOW);
    pd_text_wrap(c, x + 4, y + 16, w - 4, BAG_ROW, 4, tip, PD_LABEL, PD_LABEL_SHADOW);
    return h;
}

// PARTY WEAKNESSES: the attacking types more than one of the party (one, in a party of two or less) is weak to, most
// first, each a pill and how many.
static int party_weak(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s, int x, int y, int w) {
    if (!g || !s->valid || s->partyCount <= 0) return 0;
    int types[PD_PARTY_SIZE][2], n = 0;
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE; i++) {
        const struct pd_mon* m = &s->party[i];
        if (m->egg || m->species <= 0 || m->species == PD_SPECIES_EGG) continue;
        pd_species_type_pair(g, m->species, &types[n][0], &types[n][1]);
        if (types[n][0] != PD_TYPE_NONE) n++;
    }
    if (!n) return 0;
    int count = pd_type_count(g), weak[32] = {0}, order[32], k = 0, need = n >= 3 ? 2 : 1;
    if (count > 32) count = 32;
    for (int t = 0; t < count; t++) {
        const char* name = pd_type_name(g, t);
        if (!name || !*name || !strcmp(name, "???")) continue;
        for (int i = 0; i < n; i++) weak[t] += pd_type_multiplier_pct(g, t, types[i][0], types[i][1]) > 100;
        if (weak[t] >= need) order[k++] = t;
    }
    for (int i = 1; i < k; i++) // most first, ties in the game's order
        for (int j = i; j > 0 && weak[order[j]] > weak[order[j - 1]]; j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    if (k > 6) k = 6;
    // Laid out in rows across w: a pill and its count.
    int rows = 1, px = 0;
    int at[6][2];
    for (int i = 0; i < k; i++) {
        char v[8];
        snprintf(v, sizeof(v), "%d", weak[order[i]]);
        int pw = ui_type_pills(NULL, 0, 0, order[i], PD_TYPE_NONE, w - 4 - pd_number_width(v) - 2, false);
        int iw = pw + 2 + pd_number_width(v);
        if (px && px + iw > w - 4) {
            rows++;
            px = 0;
        }
        at[i][0] = px;
        at[i][1] = rows - 1;
        px += iw + 8;
    }
    int h = 16 + rows * BAG_ROW + GAP;
    if (!c) return h;
    pd_heading_fit(c, x, y, w, "PARTY WEAK TO", PD_MUTED, PD_MUTED_SHADOW);
    y += 16;
    if (!k) {
        pd_text_fit(c, x + 4, y, w - 4, "NO SHARED WEAKNESS", PD_LABEL, PD_LABEL_SHADOW);
        return h;
    }
    for (int i = 0; i < k; i++) {
        char v[8];
        snprintf(v, sizeof(v), "%d", weak[order[i]]);
        int rx = x + 4 + at[i][0], ry = y + at[i][1] * BAG_ROW;
        int pw = ui_type_pills(c, rx, ry + 2, order[i], PD_TYPE_NONE, w - 4 - pd_number_width(v) - 2, false);
        pd_number(c, rx + pw + 2, ry, v, PD_LABEL, PD_LABEL_SHADOW);
    }
    return h;
}

// READY TO EVOLVE: the party members whose evolution is met now (a level, enough friendship, the stone in the bag);
// hidden where the game's evolution table isn't known.
static int ready_to_evolve(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s, int x, int y,
                           int w) {
    if (!g || !s->valid || s->partyCount <= 0) return 0;
    int slot[PD_PARTY_SIZE], into[PD_PARTY_SIZE], n = 0;
    bool known = false;
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE; i++) {
        const struct pd_mon* m = &s->party[i];
        if (m->egg || m->species <= 0 || m->species == PD_SPECIES_EGG) continue;
        struct pd_dex_evolution evo[16];
        int k = pd_dex_evolutions(g, m->species, evo, 16);
        if (k < 0) continue;
        known = true;
        for (int e = 0; e < k; e++) {
            if (evo[e].from != m->species) continue;
            int how = evo[e].method, p = evo[e].parameter;
            bool ready = (how == 4 || (how >= 8 && how <= 14)) ? m->level >= p
                         : (how >= 1 && how <= 3) ? m->hasFriendship && m->friendship >= 220
                         : how == 7 ? bag_has(s, p) : false;
            if (ready) {
                slot[n] = i;
                into[n++] = evo[e].to;
                break;
            }
        }
    }
    if (!known) return 0;
    int h = 16 + (n ? n * HERE_ROW : BAG_ROW) + GAP;
    if (!c) return h;
    pd_heading_fit(c, x, y, w, "READY TO EVOLVE", PD_MUTED, PD_MUTED_SHADOW);
    y += 16;
    if (!n) {
        pd_text_fit(c, x + 4, y, w - 4, "NONE RIGHT NOW", PD_MUTED, PD_MUTED_SHADOW);
        return h;
    }
    for (int i = 0; i < n; i++, y += HERE_ROW) {
        const struct pd_mon* m = &s->party[slot[i]];
        row_icon(c, g, m->species, x, y, false);
        char name[40], line[48];
        pd_species_name(g, into[i], name, sizeof(name));
        snprintf(line, sizeof(line), "> %s", name);
        pd_text_fit(c, x + PD_ICON + 2, pd_text_y(y, HERE_ROW), w - PD_ICON - 2, line, PD_LABEL, PD_LABEL_SHADOW);
    }
    return h;
}

// SAVE STATE and CLOSE GAME at the wing's foot, BOTTOM their lowest pixel.
static int wing_buttons_height(int w) {
    if (!pd_ui_wing_buttons_on) return 0;
    bool side = w >= pd_text_width("SAVE STATE") + pd_text_width("CLOSE GAME") + 28;
    return side ? BUTTON_H : 2 * BUTTON_H + 4;
}

static void wing_buttons(struct pd_canvas* c, int x, int w, int bottom, int top) {
    memset(pd_ui_wing_buttons, 0, sizeof(pd_ui_wing_buttons));
    int h = wing_buttons_height(w);
    if (!h || bottom - h < top) return;
    static const char* const LABELS[PD_WING_BUTTONS] = {"SAVE STATE", "CLOSE GAME"};
    bool side = h == BUTTON_H;
    for (int i = 0; i < PD_WING_BUTTONS; i++) {
        struct pd_ui_rect r;
        if (side) r = (struct pd_ui_rect){x + i * (w + 4) / 2, bottom - BUTTON_H, (w - 4) / 2, BUTTON_H};
        else r = (struct pd_ui_rect){x, bottom - h + i * (BUTTON_H + 4), w, BUTTON_H};
        pd_title_box(c, r.x, r.y, r.w, r.h, PD_TITLE_FILL);
        int tw = pd_text_width(LABELS[i]);
        if (tw <= r.w - 6) pd_text(c, r.x + (r.w - tw) / 2, pd_text_y(r.y, r.h), LABELS[i], PD_TITLE_TEXT, PD_TITLE_SHADOW);
        else pd_text_fit(c, r.x + 3, pd_text_y(r.y, r.h), r.w - 6, LABELS[i], PD_TITLE_TEXT, PD_TITLE_SHADOW);
        pd_ui_wing_buttons[i] = r;
    }
}

// A section's height at width w (0: nothing to show), drawn at (x, y) when c is set; HERE's is its least (a heading and
// a row), drawn into h.
static int wing_section(struct pd_canvas* c, int sec, const struct pd_game* g, const struct pd_snapshot* s, int x, int y,
                        int w, int h) {
    struct wing_fact row;
    switch (sec) {
    case PD_WS_BADGES: case PD_WS_PLAY_TIME: case PD_WS_NEXT_BOSS: case PD_WS_CAUGHT_HERE: case PD_WS_CAUGHT_SEEN:
    case PD_WS_MONEY:
        if (!wing_fact(g, s, sec, &row)) return 0;
        if (c) draw_fact(c, &row, x, y, w);
        return fact_height(&row);
    case PD_WS_HERE:
        if (wildCount <= 0) return 0;
        if (c) here_list(c, g, x, y, w, y + h - GAP);
        return 16 + HERE_ROW + GAP;
    case PD_WS_QUICK_BAG: {
        int bh = quick_bag_height(s);
        if (!bh) return 0;
        if (c) quick_bag_draw(c, g, s, x, y, w);
        return bh + GAP;
    }
    case PD_WS_BOSS_TEAM: return boss_team(c, g, x, y, w);
    case PD_WS_ITEMS_HERE: return area_items(c, x, y, w, false);
    case PD_WS_TMS_HERE: return area_items(c, x, y, w, true);
    case PD_WS_NUZLOCKE: return nuzlocke_section(c, s, x, y, w);
    case PD_WS_TIP: return tip_section(c, x, y, w);
    case PD_WS_PARTY_TYPES: return party_weak(c, g, s, x, y, w);
    case PD_WS_EVOLVE: return ready_to_evolve(c, g, s, x, y, w);
    case PD_WS_TIP_OR_WEAK: // the tip where there's one, else the party's weaknesses
        return tipOk && tip[0] ? tip_section(c, x, y, w) : party_weak(c, g, s, x, y, w);
    default: return 0;
    }
}

// The sections from TOP down to FOOT, in the list's order: each that has something to say while it fits, the rest
// dropped; HERE takes what's left over, so what follows it sits at the foot.
static void wing_sections(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s, const int* list,
                          int count, int x, int top, int w, int foot) {
    int avail = foot - top + GAP, used = 0, n = 0, here = -1;
    int sec[PD_WING_LIST_MAX], h[PD_WING_LIST_MAX];
    for (int i = 0; i < count && n < PD_WING_LIST_MAX; i++) {
        if (list[i] == PD_WS_MAP || list[i] == PD_WS_BUTTONS) continue;
        int sh = wing_section(NULL, list[i], g, s, x, 0, w, 0);
        if (!sh) continue;
        if (used + sh > avail) break;
        if (list[i] == PD_WS_HERE) here = n;
        sec[n] = list[i];
        h[n++] = sh;
        used += sh;
    }
    if (here >= 0) h[here] += avail - used;
    for (int i = 0, y = top; i < n; y += h[i], i++) wing_section(c, sec[i], g, s, x, y, w, h[i]);
}

// COMPANION VIEW's map panel (the Linux / Windows host's right wing off a battle): the place's name over the map, as
// wide as the panel, the player's head and the cursor on where they are. Nothing to tap.
void pd_ui_map_panel(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s, bool blink) {
    ui_game = g;
    int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
    int x = f + 4, y = f + 3, w = c->w - 2 * (f + 4);
    memset(pd_ui_wing_buttons, 0, sizeof(pd_ui_wing_buttons));
    const int* list = pd_ui_wing_list ? pd_ui_wing_list : pd_wing_default;
    int count = pd_ui_wing_list ? pd_ui_wing_count : pd_wing_default_count;
    bool mapOn = false, buttonsOn = false, sectionsOn = false;
    for (int i = 0; i < count; i++) {
        mapOn |= list[i] == PD_WS_MAP;
        buttonsOn |= list[i] == PD_WS_BUTTONS;
        sectionsOn |= list[i] != PD_WS_MAP && list[i] != PD_WS_BUTTONS;
    }
    if (s->valid) wing_refresh(g, s);
    int foot = c->h - f - 3, buttonsH = buttonsOn ? wing_buttons_height(w) : 0;
    if (buttonsH && foot - buttonsH - 6 > y + 2 * 16 + 40) foot -= buttonsH + 6;  // the buttons' room
    else buttonsH = 0;
    const char* here = s->valid && s->posOk ? pd_mapsec_name(g, s->mapsec) : "";
    pd_text_wrap(c, x, y, w, 16, 2, *here ? here : "MAP", PD_LABEL, PD_LABEL_SHADOW);
    y += 2 * 16 + 4;
    const struct pd_map_art* art = mapOn ? pd_map_art(g) : NULL;
    if (buttonsH) wing_buttons(c, x, w, c->h - f - 3, y);
    if (!mapOn) { // no map: the sections from the top
        hereAsList = true;
        if (s->valid) wing_sections(c, g, s, list, count, x, y, w, foot);
        return;
    }
    if (!art) {
        pd_text_wrap(c, x, y, w, 16, 2, "NO MAP IN THIS ROM", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    int playerPage = -1, ptx = 0, pty = 0;
    bool hasPlayer = pd_map_player_tile(g, s, &playerPage, &ptx, &pty);
    int page = hasPlayer && playerPage < art->pages ? playerPage : 0;
    const int16_t* k = art->crop[page];
    bool cropped = k[2] > k[0] && k[3] > k[1];
    mapW = art->w;
    mapH = art->h;
    crop.l = cropped ? k[0] : 0;
    crop.t = cropped ? k[1] : 0;
    crop.r = cropped ? k[2] : art->w;
    crop.b = cropped ? k[3] : art->h;
    // As wide as the panel, its own shape (a short panel: as tall as fits). A tall, narrow wing (beside a game at
    // full height) gets the map at its own size instead, a window round the player, and the sections under it.
    int mw = crop.r - crop.l, mh = crop.b - crop.t;
    int vw = w - 2 * FRAME, vh = vw * mh / mw, room = foot - y - 2 * FRAME;
    int factsH = sectionsOn ? 3 * (2 * 16 + 4) : 0;  // room for a few sections at least: what doesn't fit is left out
    bool window = vw < mw && room - factsH >= vw;
    hereAsList = window;
    if (window) {
        vh = room - factsH - 2 * FRAME;
        if (vh > mh) vh = mh;
        if (vh > vw * 2) vh = vw * 2;
        int cx = crop.l + mw / 2, cy = crop.t + mh / 2;
        if (hasPlayer && page == playerPage) {
            cx = ptx * 8 + 4;
            cy = pty * 8 + 4;
        }
        int l = cx - vw / 2, t = cy - vh / 2;
        l = l < crop.l ? crop.l : l > crop.r - vw ? crop.r - vw : l;
        t = t < crop.t ? crop.t : t > crop.b - vh ? crop.b - vh : t;
        crop.l = l;
        crop.t = t;
        crop.r = l + vw;
        crop.b = t + vh;
    } else if (vh > room) {
        vh = room;
        vw = vh * mw / mh;
    }
    if (vw < 8 || vh < 8) return;
    view.w = vw;
    view.h = vh;
    view.x = (c->w - vw) / 2;
    view.y = y + FRAME;
    frame(c, view.x - FRAME, view.y - FRAME, view.w + 2 * FRAME, view.h + 2 * FRAME);
    draw_page(c, art->page[page]);
    if (window && s->valid) wing_sections(c, g, s, list, count, x, view.y + view.h + FRAME + 6, w, foot);
    if (!hasPlayer || page != playerPage) return;
    int gender = s->gender == 1 ? 1 : 0;
    pd_clip(c, view.x, view.y, view.w, view.h);
    if (art->hasHead[gender]) draw_head(c, art->head[gender], ptx, pty);
    pd_unclip(c);
    struct pd_tile_rect rects[MAX_RECTS];
    int p, n = pd_map_tiles(g, pd_map_player_section(g, s), &p, rects, MAX_RECTS);
    for (int i = 0; i < n; i++)
        cursor_pixels(c, rects[i].x * 8, rects[i].y * 8, rects[i].w * 8, rects[i].h * 8, blink,
                      g->kind == PD_GAME_GB ? PD_GB_INK : CURSOR);
}
