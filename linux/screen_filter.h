/* GPL-3.0, see ../NOTICE. FILTER on the desktop host (SETTINGS > DISPLAY OPTIONS, and the companion's DISPLAY): the
 * app's LCD, LCD PAPER, SCANLINES and CRT (ScreenShaders.kt) as a mask multiplied over the game's picture
 * (SDL_BLENDMODE_MOD), lined up with the game's own pixels, so they work on every SDL renderer. The colour half of
 * LCD PAPER (15% less saturation) goes through COLORS' table (shader.h). SUBPIXEL, the window-wide mask, stays in
 * display_filter.h. */
#ifndef POCKET_SCREEN_FILTER_H
#define POCKET_SCREEN_FILTER_H
#include <stdbool.h>
#include <stdint.h>

/* True for the kinds drawn here (LCD, LCD PAPER, SCANLINES, CRT). */
bool screen_filter_on_game(int filter);
/* The mask's pixel (x, y) of a dw x dh picture of a sw x sh game: 0xFFRRGGBB, each channel what that colour is
 * multiplied by (255 = as it is). grid: GRID (0 SOFT, 1 MEDIUM, 2 STRONG). */
uint32_t screen_filter_pixel(int filter, int grid, int x, int y, int dw, int dh, int sw, int sh);
#endif
