/* GPL-3.0. The game picture's SHADER and COLORS (SETTINGS > SCREEN on the Linux / Windows host), done on the CPU
 * so they work on every SDL renderer (Direct3D, OpenGL, software):
 * - SHADER: OFF (whole pixels, as before), SHARP (a whole-pixel prescale smoothed the last bit to the screen, like
 *   libretro's sharp-bilinear: even pixels at any size), xBRZ (Zenju's xBRZ, third_party/xbrz: rounded edges);
 * - COLORS: ORIGINAL, HANDHELD (the GBA's / DS's own LCD, libretro's gba-color / nds-color, public domain) and
 *   OLED (ours: true black, a little less saturation and peak white, for OLED panels).
 * Nothing here touches the emulator's frame: it reads it and draws a copy. */
#ifndef POCKET_SHADER_H
#define POCKET_SHADER_H
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum pocket_shader { POCKET_SHADER_OFF, POCKET_SHADER_SHARP, POCKET_SHADER_XBRZ, POCKET_SHADERS };
enum pocket_colors { POCKET_COLORS_ORIGINAL, POCKET_COLORS_HANDHELD, POCKET_COLORS_OLED, POCKET_COLORS_COUNT };
const char *pocket_shader_name(int shader);
const char *pocket_colors_name(int colors);
/* BRIGHTNESS (SETTINGS > SCREEN): a percent, 100 = the game's own colours. Above 100 the mid-tones are lifted (a gamma
 * curve: black stays black, white stays white, nothing clips), below they go down. */
#define POCKET_BRIGHTNESS_DEFAULT 110
#define POCKET_BRIGHTNESS_MIN 80
#define POCKET_BRIGHTNESS_MAX 150
#define POCKET_BRIGHTNESS_STEP 10
void pocket_shader_brightness(int percent);
/* FILTER's LCD PAPER: this many percent less saturation (0 = none). */
void pocket_shader_desaturate(int percent);

/* What to draw with; ds picks nds-color over gba-color for HANDHELD. */
void pocket_shader_set(int shader, int colors, bool ds);
/* True when the picture goes through here (anything but OFF + ORIGINAL at 100% brightness). */
bool pocket_shader_active(void);
/* Draws the w x h picture at px (pitch in pixels) into dst: XBGR words (mGBA's, SDL's BGR888), or XRGB ones
 * (melonDS's, SDL's RGB888 / ARGB8888) with xrgb. */
bool pocket_shader_draw(SDL_Renderer *r, const uint32_t *px, int pitch, int w, int h, bool xrgb, const SDL_Rect *dst);
/* The renderer is going away: its textures with it. */
void pocket_shader_free(void);

/* For the tests: one pixel through COLORS (XBGR in, XRGB out). */
uint32_t pocket_shader_color(uint32_t xbgr);
#ifdef __cplusplus
}
#endif
#endif
