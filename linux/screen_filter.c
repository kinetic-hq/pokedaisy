/* GPL-3.0, see ../NOTICE. FILTER's masks (screen_filter.h), worked out from the app's shaders (ScreenShaders.kt) as
 * what each one multiplies a pixel's colour by. A shader that depends on the colour itself (CRT's beams widen with it,
 * LCD PAPER's paper shows by its brightness) uses the middle of its range here. */
#include "screen_filter.h"
#include "pd_ui.h"
#include <math.h>

bool screen_filter_on_game(int filter) {
  return filter == PD_FILTER_LCD || filter == PD_FILTER_LCD_PAPER || filter == PD_FILTER_SCANLINES ||
         filter == PD_FILTER_CRT;
}

static double fract(double v) { return v - floor(v); }

static uint8_t byte_of(double v) {
  v = v < 0 ? 0 : v > 1 ? 1 : v;
  return (uint8_t)lround(v * 255.0);
}

static uint32_t grey(double v) {
  uint32_t b = byte_of(v);
  return 0xFF000000u | b << 16 | b << 8 | b;
}

/* LCD PAPER's paper (simpletex_lcd's look, the app's noise): faint blotches, grain and short fibres. */
static double hash(double x, double y) {
  x = fract(x * 0.1031), y = fract(y * 0.1030);
  double d = x * (y + 33.33) + y * (x + 33.33);
  x += d, y += d;
  return fract((x + y) * x);
}
static double noise(double x, double y) {
  double ix = floor(x), iy = floor(y), fx = x - ix, fy = y - iy;
  fx = fx * fx * (3 - 2 * fx), fy = fy * fy * (3 - 2 * fy);
  double a = hash(ix, iy), b = hash(ix + 1, iy), c = hash(ix, iy + 1), d = hash(ix + 1, iy + 1);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static double smooth(double e0, double e1, double v) {
  double t = (v - e0) / (e1 - e0);
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return t * t * (3 - 2 * t);
}

uint32_t screen_filter_pixel(int filter, int grid, int x, int y, int dw, int dh, int sw, int sh) {
  if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
    return 0xFFFFFFFFu;
  if (grid < 0 || grid > 2)
    grid = 1;
  // Where this window pixel falls in the game's: u, v in game pixels.
  double u = (x + 0.5) * sw / dw, v = (y + 0.5) * sh / dh;
  double cellW = (double)dw / sw, cellH = (double)dh / sh;
  switch (filter) {
  case PD_FILTER_LCD: {
    // The first third of every game pixel's column and row darkened by GRID (mGBA's lcd), once there's room for it.
    static const double STRENGTH[3] = {0.2, 0.35, 0.5};
    bool line = (cellW >= 3 && fract(u) * 3 < 1) || (cellH >= 3 && fract(v) * 3 < 1);
    return grey(line ? 1 - STRENGTH[grid] : 1);
  }
  case PD_FILTER_SCANLINES:
    // The top half of every game row at half brightness (mGBA's scanlines at 0.5).
    return grey(cellH >= 2 && fract(v) * 2 < 1 ? 0.5 : 1);
  case PD_FILTER_LCD_PAPER: {
    // Soft lines from the distance to the game pixel's centre, 48(x^4 - 8/3 x^6): squared for thin ones, the whole
    // curve at STRONG; then the paper, as for a mid-bright colour.
    static const double STRENGTH[3] = {0.3, 0.45, 0.6}, FULL[3] = {0, 0.5, 1};
    double dx = fabs(fract(u) - 0.5), dy = fabs(fract(v) - 0.5), x2 = dx > dy ? dx : dy;
    x2 *= x2;
    double w = 48.0 * (x2 * x2 - 8.0 / 3.0 * x2 * x2 * x2);
    double weight = (w * w + (w - w * w) * FULL[grid]) * 0.85;
    weight = weight < 0 ? 0 : weight > 1 ? 1 : weight;
    double m = cellW >= 2 && cellH >= 2 ? 1 - STRENGTH[grid] * weight : 1;
    double n = 0.2 * noise(x / 90.0, y / 90.0) + 0.3 * noise(x / 22.0, y / 22.0) + 0.3 * noise(x / 5.0, y / 5.0) +
               0.2 * hash(x, y);
    double fibre = fmax(smooth(0.7, 0.95, noise(x / 2.0, y / 14.0 + 17.0)),
                        smooth(0.7, 0.95, noise(x / 14.0 + 51.0, y / 2.0)));
    double paper = 0.88 + 0.12 * n - 0.025 * fibre, mix = 0.5;
    double r = m * (1 + (0.98 * paper - 1) * mix), g = m * (1 + (0.96 * paper - 1) * mix),
           b = m * (1 + (0.90 * paper - 1) * mix);
    return 0xFF000000u | (uint32_t)byte_of(r) << 16 | (uint32_t)byte_of(g) << 8 | byte_of(b);
  }
  case PD_FILTER_CRT: {
    // Each game row a gaussian beam (its middle width), the rows either side adding in; an R / G / B stripe a window
    // pixel wide; brightened back by 1.5, all in linear light (gamma 2.2).
    double pos = v - 0.5, f = fract(pos), wd = 0.27;
    double beam = exp(-f * f / (2 * wd * wd)) + exp(-(1 - f) * (1 - f) / (2 * wd * wd));
    if (cellH < 2)
      beam = 1;
    double mask[3] = {0.6, 0.6, 0.6};
    mask[x % 3] = 1;
    uint32_t out = 0xFF000000u;
    for (int i = 0; i < 3; i++)
      out |= (uint32_t)byte_of(pow(fmin(beam * mask[i] * 1.5, 1.0), 1 / 2.2)) << (16 - 8 * i);
    return out;
  }
  default:
    return 0xFFFFFFFFu;
  }
}
