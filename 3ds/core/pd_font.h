// Pixel Operator as a 1-bit bitmap font (pd_font_gen.c, from
// 3ds/tools/gen_font.py): 16-pixel cells, one screen pixel per font pixel.
#ifndef PD_FONT_H
#define PD_FONT_H

#include <stdint.h>

#define PD_FONT_HEIGHT 16

struct pd_glyph {
    uint16_t codepoint;
    uint8_t advance;
    uint16_t rows[PD_FONT_HEIGHT]; // bit 0 = leftmost pixel
};

extern const struct pd_glyph pd_font_glyphs[];
extern const int pd_font_glyph_count;

// Optional caller-owned overrides. Missing glyphs use Pixel Operator, then '?'.
// Keep the array alive until reset. NULL or count <= 0 restores the default.
// Process-wide: set/reset on the render thread, including before ROM unload.
void pd_font_set(const struct pd_glyph* glyphs, int count);
void pd_font_get(const struct pd_glyph** glyphs, int* count);

// The glyph for a codepoint ('?' when the font lacks it).
const struct pd_glyph* pd_font_glyph(uint32_t codepoint);

// The wings' faces (pd_font_faces_gen.c, 3ds/tools/gen_font_faces.py): Silkscreen Bold for headings, Atkinson
// Hyperlegible Bold's figures for numbers. pd_font_face puts one in front of everything (the game's font too) until
// it's reset with NULL; a codepoint it lacks falls through as before.
extern const struct pd_glyph pd_font_heading[];
extern const int pd_font_heading_count;
extern const struct pd_glyph pd_font_numbers[];
extern const int pd_font_numbers_count;
void pd_font_face(const struct pd_glyph* glyphs, int count);

#endif
