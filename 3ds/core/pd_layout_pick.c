// LAYOUT's pick-list: tiles of drawn layouts in the OPTION look (see pd_layout_pick.h).
#include "pd_layout_pick.h"

#include "pd_style.h"

#define MARGIN 4
#define TITLE_H 24
#define NOTE_H 16
#define TILE_GAP 4
#define NAME_H 16

int pd_layout_picker_columns(int canvasW) {
    return canvasW >= 380 ? 3 : 2;
}

static uint32_t part_color(int part) {
    switch (part) {
    case PD_PART_GAME: return 0x58A8F0;      // the game: a sky blue
    case PD_PART_TOUCH: return 0x68C878;     // the touch screen: green
    case PD_PART_COMPANION: return 0xF8F8F8; // the companion: the OPTION window's white
    case PD_PART_PANEL: return 0xF8D870;     // panels: a soft yellow
    case PD_PART_DIM: return 0x305878;       // the game, dimmed
    default: return 0xD8E8F8;                // glass
    }
}

void pd_layout_preview(struct pd_canvas* c, int x, int y, int w, int h, const struct pd_layout_option* o) {
    // The screen: a dark bezel round black.
    pd_fill(c, x, y, w, h, 0x202020);
    pd_fill(c, x + 1, y + 1, w - 2, h - 2, 0x000000);
    int ix = x + 2, iy = y + 2, iw = w - 4, ih = h - 4;
    for (int i = 0; o && i < o->boxCount && i < PD_LAYOUT_BOXES; i++) {
        const struct pd_layout_box* b = &o->box[i];
        int bx = ix + b->x * iw / 1000, by = iy + b->y * ih / 1000;
        int bw = (b->x + b->w) * iw / 1000 - b->x * iw / 1000, bh = (b->y + b->h) * ih / 1000 - b->y * ih / 1000;
        if (bw < 2 || bh < 2) continue;
        uint32_t col = part_color(b->part);
        if (b->part == PD_PART_GLASS) {
            // See-through: every other line, so the game shows between.
            for (int ry = 0; ry < bh; ry += 2) pd_fill(c, bx, by + ry, bw, 1, col);
            pd_fill(c, bx, by, bw, 1, 0xF8F8F8);
            continue;
        }
        pd_fill(c, bx, by, bw, bh, col);
        // A darker edge, so boxes that touch stay apart.
        uint32_t edge = (col >> 1) & 0x7F7F7F;
        pd_fill(c, bx, by, bw, 1, edge);
        pd_fill(c, bx, by + bh - 1, bw, 1, edge);
        pd_fill(c, bx, by, 1, bh, edge);
        pd_fill(c, bx + bw - 1, by, 1, bh, edge);
        if (b->part == PD_PART_COMPANION && bw > 10 && bh > 8) {
            // Rows, like the companion's list.
            for (int ry = by + 3; ry + 2 < by + bh - 2; ry += 4) pd_fill(c, bx + 3, ry, bw - 6 > 1 ? (bw - 6) * 2 / 3 : 1, 1, 0xA0A0A0);
        }
    }
}

int pd_layout_picker_hit(const int tiles[][4], int n, int x, int y) {
    for (int i = 0; i < n; i++)
        if (tiles[i][2] > 0 && x >= tiles[i][0] && y >= tiles[i][1] && x < tiles[i][0] + tiles[i][2] && y < tiles[i][1] + tiles[i][3])
            return i;
    return -1;
}

void pd_layout_picker_draw(struct pd_canvas* c, const char* title, const struct pd_layout_option* options, int n,
                           int current, int selected, int pressed, int* scroll, int tiles[][4]) {
    int w = c->w - 2 * MARGIN;
    // A page of its own: the backdrop's stripes over whatever was under it.
    for (int y = 0; y < c->h; y += 16) pd_fill(c, 0, y, c->w, y + 8 <= c->h ? 8 : c->h - y, PD_BACKDROP_A);
    for (int y = 8; y < c->h; y += 16) pd_fill(c, 0, y, c->w, y + 8 <= c->h ? 8 : c->h - y, PD_BACKDROP_B);
    int f = pd_title_box(c, MARGIN, MARGIN, w, TITLE_H, PD_TITLE_FILL);
    pd_text(c, MARGIN + f + 6, pd_text_y(MARGIN, TITLE_H), title, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    int ly = MARGIN + TITLE_H + 4, lh = c->h - ly - MARGIN - NOTE_H;
    int lf = pd_list_box(c, MARGIN, ly, w, lh);
    int cols = pd_layout_picker_columns(c->w);
    int ix = MARGIN + lf + 2, iy = ly + lf + 2, iw = w - 2 * (lf + 2), ih = lh - 2 * (lf + 2);
    int tw = (iw - (cols - 1) * TILE_GAP) / cols;
    // Previews 16:9, small enough that two rows fit (a short canvas would show one at a time).
    int pw = tw - 8, ph = pw * 9 / 16;
    int maxPh = (ih - 2 * TILE_GAP) / 2 - NAME_H - 8;
    if (maxPh > 16 && ph > maxPh) {
        ph = maxPh;
        pw = ph * 16 / 9;
    }
    int th = ph + NAME_H + 8;
    int visible = ih / (th + TILE_GAP);
    if (visible < 1) visible = 1;
    int rows = (n + cols - 1) / cols, selRow = selected >= 0 ? selected / cols : 0;
    if (selRow < *scroll) *scroll = selRow;
    if (selRow >= *scroll + visible) *scroll = selRow - visible + 1;
    if (*scroll > rows - visible) *scroll = rows - visible;
    if (*scroll < 0) *scroll = 0;
    pd_clip(c, ix, iy, iw, ih);
    for (int i = 0; i < n; i++) {
        int r = i / cols - *scroll, col = i % cols;
        tiles[i][0] = tiles[i][1] = tiles[i][2] = tiles[i][3] = 0;
        if (r < 0 || r >= visible) continue;
        int tx = ix + col * (tw + TILE_GAP), ty = iy + r * (th + TILE_GAP);
        bool on = i == selected || i == pressed;
        if (on) pd_fill(c, tx, ty, tw, th, PD_ROW_SELECTED);
        pd_layout_preview(c, tx + (tw - pw) / 2, ty + 4, pw, ph, &options[i]);
        int ny = ty + 4 + ph + 2;
        int nx = tx + 4;
        if (i == current) {
            pd_triangle(c, nx, ny + 3, 4, PD_RIGHT, PD_VALUE);
            nx += 8;
        }
        // The mark: a dot for PokeDaisy's own, a ring for one added here (explained in the note).
        int mx = tx + tw - 10, my = ny + 4;
        if (options[i].original) pd_fill(c, mx, my, 6, 6, PD_VALUE);
        else {
            pd_fill(c, mx, my, 6, 1, PD_MUTED);
            pd_fill(c, mx, my + 5, 6, 1, PD_MUTED);
            pd_fill(c, mx, my, 1, 6, PD_MUTED);
            pd_fill(c, mx + 5, my, 1, 6, PD_MUTED);
        }
        pd_text_fit(c, nx, ny, mx - nx - 4, options[i].name, on ? PD_VALUE : PD_LABEL, on ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
        tiles[i][0] = tx;
        tiles[i][1] = ty;
        tiles[i][2] = tw;
        tiles[i][3] = th;
    }
    pd_unclip(c);
    int ny = c->h - MARGIN - NOTE_H + 2;
    pd_fill(c, MARGIN + 4, ny + 4, 6, 6, PD_VALUE);
    int x = MARGIN + 14 + pd_text(c, MARGIN + 14, ny, "PokeDaisy's own", PD_ON_BACKDROP, PD_ON_BACKDROP_SHADOW) + 12;
    pd_fill(c, x, ny + 4, 6, 1, PD_ON_BACKDROP);
    pd_fill(c, x, ny + 9, 6, 1, PD_ON_BACKDROP);
    pd_fill(c, x, ny + 4, 1, 6, PD_ON_BACKDROP);
    pd_fill(c, x + 5, ny + 4, 1, 6, PD_ON_BACKDROP);
    pd_text(c, x + 10, ny, "added in this build", PD_ON_BACKDROP, PD_ON_BACKDROP_SHADOW);
}
