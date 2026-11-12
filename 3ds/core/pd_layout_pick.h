// LAYOUT: a pick-list of screen layouts (COMPANION VIEW, DS LAYOUT), each tile a small drawing of where the game,
// the companion and the panels go, worked out by the host from its real layout code and handed over as boxes.
#ifndef PD_LAYOUT_PICK_H
#define PD_LAYOUT_PICK_H

#include <stdbool.h>

#include "pd_canvas.h"

// What a box in a drawing is.
enum pd_layout_part {
    PD_PART_GAME,      // the game's picture (a DS game's top screen)
    PD_PART_TOUCH,     // a DS game's touch screen
    PD_PART_COMPANION, // the companion
    PD_PART_PANEL,     // a side panel / strip (party, battle, map)
    PD_PART_DIM,       // the game dimmed under something
    PD_PART_GLASS,     // a see-through sheet
};

// Boxes in 0..1000 of the drawing's width and height, drawn in order.
struct pd_layout_box {
    short x, y, w, h;
    unsigned char part; // enum pd_layout_part
};

#define PD_LAYOUT_BOXES 8
#define PD_LAYOUT_OPTIONS 16
struct pd_layout_option {
    const char* name;
    bool original; // PokeDaisy's own layout (the Android app has it), else one added in this build
    int boxCount;
    struct pd_layout_box box[PD_LAYOUT_BOXES];
};

// The picker: a title window, then the options as tiles in rows; CURRENT carries the red marker, SELECTED the
// cursor, PRESSED is white while a finger is on it (-1 none). SCROLL (in rows) is kept so SELECTED shows. TILES gets
// each drawn tile's place (x, y, w, h; w = 0 off screen) for taps. A note under it explains the marks.
void pd_layout_picker_draw(struct pd_canvas* c, const char* title, const struct pd_layout_option* options, int n,
                           int current, int selected, int pressed, int* scroll, int tiles[][4]);
// The picker's columns on a canvas this wide (the D-pad's LEFT / RIGHT step one, UP / DOWN a row).
int pd_layout_picker_columns(int canvasW);
// The option whose tile holds (x, y), else -1.
int pd_layout_picker_hit(const int tiles[][4], int n, int x, int y);

// One drawing, on its own (the GET STARTED and Settings pages put one beside the rows).
void pd_layout_preview(struct pd_canvas* c, int x, int y, int w, int h, const struct pd_layout_option* option);

#endif
