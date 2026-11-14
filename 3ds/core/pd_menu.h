// The game list on the bottom screen (an OPTION-style pick-list, D-pad or
// stylus) and the top screen's splash while no game runs.
#ifndef PD_MENU_H
#define PD_MENU_H

#include <stdbool.h>

#include "pd_canvas.h"

// An item starting with PD_MENU_GROUP is a group title (the app's SETTINGS groups): drawn as a title, never selected.
// "LABEL\tVALUE" draws the label grey and the value red, as the app's settings rows; a value starting with
// PD_MENU_MUTED is greyed and the row can't be picked (N/A). A trailing PD_MENU_PAGE draws the app's chevron: the row
// opens a page of its own.
#define PD_MENU_GROUP "\x01"
#define PD_MENU_MUTED "\x02"
#define PD_MENU_PAGE "\x03"

enum pd_menu_focus { PD_MENU_FOCUS_LIST, PD_MENU_FOCUS_NAV, PD_MENU_FOCUS_ACTIONS };

struct pd_menu {
    const char* title;
    const char* const* items;
    int count;
    int selected;
    int scroll;
    bool touching;
    int pressed; // row the stylus went down on, -1 = none
    bool overlay; // just the windows, over whatever is behind (a host's menu over the game)
    const char* emptyPath; // where the games go, shown while there are none (NULL: the 3DS's SD card)
    // The library's rail (the app's GAMES / TOOLS / SETTINGS, LibraryNav) down the left, and buttons at the title
    // window's right (the library header's +): none unless the host sets them after pd_menu_init.
    const char* const* nav;
    int navCount, navSelected;
    const char* const* actions;
    int actionCount, actionSelected;
    int focus;      // enum pd_menu_focus
    int navPicked;  // a rail item / header button a tap picked, -1 = none (the host clears them)
    int actionPicked;
    int pressedNav, pressedAction;
    const char* hint; // a line under the list window (the app's page hints), NULL = none
    const char* keys; // the controller's help bar along the bottom ("A PICK  B BACK"), NULL = none
    int drawnW, drawnH; // the canvas the menu was last drawn on (for taps)
};

void pd_menu_init(struct pd_menu* m, const char* title, const char* const* items, int count);
void pd_menu_draw(struct pd_menu* m, struct pd_canvas* c);
// Up / down the list, past group titles and N/A rows.
void pd_menu_move(struct pd_menu* m, int delta);
// Stylus down / up; returns the row a tap picked (on release), else -1. A tap on the rail or a header button sets
// navPicked / actionPicked instead.
int pd_menu_touch(struct pd_menu* m, int x, int y, bool down);
// The selected row can be picked (not a group title or an N/A row).
bool pd_menu_pickable(const struct pd_menu* m, int row);

// The top screen: the name, then up to three lines of help.
void pd_splash_draw(struct pd_canvas* c, const char* line1, const char* line2, const char* line3);

#endif
