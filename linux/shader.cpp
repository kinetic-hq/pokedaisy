// GPL-3.0. SHADER and COLORS for the Linux / Windows host: see shader.h.
#include "shader.h"

#include "xbrz.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace {
int gShader = POCKET_SHADER_OFF, gColors = POCKET_COLORS_ORIGINAL;
bool gDs = false;
int gBrightness = 100;

// COLORS as a table of every colour at 6 bits a channel (the GBA's 5 and the DS's 6 both fit): XBGR in, XRGB out.
std::vector<uint32_t> lut;
int lutColors = -1;
bool lutDs = false;
int lutBrightness = -1;
int gDesat = 0, lutDesat = -1; // FILTER's LCD PAPER: percent less saturation

uint8_t to8(double v) {
  v = std::clamp(v, 0.0, 1.0);
  return uint8_t(std::lround(v * 255.0));
}

uint32_t mapColor(int colors, bool ds, double r, double g, double b) {
  if (colors == POCKET_COLORS_HANDHELD) {
    // libretro's gba-color / nds-color (hunterk, Pokefan531; public domain): the handhelds' own LCD. The GBA's
    // "darken screen" at 0.5, not 1.0: the full darkening made a backlit screen look unlit.
    double in = ds ? 1.91 : 2.2 + 0.5, out = ds ? 1.91 : 2.2, lum = ds ? 0.89 : 0.94;
    r = std::pow(r, in) * lum, g = std::pow(g, in) * lum, b = std::pow(b, in) * lum;
    double R, G, B;
    if (ds) {
      R = 0.87 * r + 0.255 * g - 0.125 * b;
      G = 0.10 * r + 0.645 * g + 0.255 * b;
      B = 0.10 * r + 0.17 * g + 0.73 * b;
    } else {
      R = 0.82 * r + 0.24 * g - 0.06 * b;
      G = 0.125 * r + 0.665 * g + 0.21 * b;
      B = 0.195 * r + 0.075 * g + 0.73 * b;
    }
    auto back = [&](double v) { return std::pow(std::clamp(v, 0.0, 1.0), 1.0 / out); };
    return uint32_t(to8(back(R))) << 16 | uint32_t(to8(back(G))) << 8 | to8(back(B));
  }
  if (colors == POCKET_COLORS_OLED) {
    // Ours: the games were made for a dim, washed-out LCD, so on an OLED they glare. In linear light a little less
    // saturation and peak white; black stays true black (it's off on an OLED).
    r = std::pow(r, 2.2), g = std::pow(g, 2.2), b = std::pow(b, 2.2);
    double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    auto fix = [&](double v) { return std::pow(std::clamp((y + (v - y) * 0.88) * 0.9, 0.0, 1.0), 1.0 / 2.2); };
    return uint32_t(to8(fix(r))) << 16 | uint32_t(to8(fix(g))) << 8 | to8(fix(b));
  }
  return uint32_t(to8(r)) << 16 | uint32_t(to8(g)) << 8 | to8(b);
}

// BRIGHTNESS as a gamma: v ^ (100 / percent).
double lift(double v) {
  return gBrightness == 100 ? v : std::pow(v, 100.0 / gBrightness);
}

void buildLut() {
  if (lutColors == gColors && lutDs == gDs && lutBrightness == gBrightness && lutDesat == gDesat && !lut.empty())
    return;
  lut.resize(1u << 18);
  for (uint32_t i = 0; i < lut.size(); i++) {
    auto ch = [&](int shift) {
      int v = (i >> shift) & 63;
      return lift(((v << 2) | (v >> 4)) / 255.0);
    };
    lut[i] = mapColor(gColors, gDs, ch(0), ch(6), ch(12));
    if (gDesat) { // toward the colour's own grey (Rec. 709 weights), as LCD PAPER's shader
      double r = (lut[i] >> 16 & 255) / 255.0, g = (lut[i] >> 8 & 255) / 255.0, b = (lut[i] & 255) / 255.0;
      double y = 0.2126 * r + 0.7152 * g + 0.0722 * b, k = gDesat / 100.0;
      lut[i] = uint32_t(to8(r + (y - r) * k)) << 16 | uint32_t(to8(g + (y - g) * k)) << 8 | to8(b + (y - b) * k);
    }
  }
  lutColors = gColors;
  lutDs = gDs;
  lutBrightness = gBrightness;
  lutDesat = gDesat;
}

inline uint32_t key(uint32_t xbgr) {
  return (xbgr >> 2 & 0x3F) | (xbgr >> 4 & 0xFC0) | (xbgr >> 6 & 0x3F000);
}

// ORIGINAL: the colour as it is, only XBGR -> XRGB (the table is 6 bits a channel, close but not exact).
inline uint32_t swap(uint32_t xbgr) {
  return (xbgr & 0xFF) << 16 | (xbgr & 0xFF00) | (xbgr >> 16 & 0xFF);
}

// A picture's textures, by where its pixels live: the DS's two screens (and EXPAND's window) each keep theirs.
struct Slot {
  const uint32_t *px = nullptr;
  SDL_Renderer *r = nullptr;
  SDL_Texture *src = nullptr, *big = nullptr;
  int sw = 0, sh = 0, bw = 0, bh = 0, bigAccess = 0;
  unsigned used = 0;
};
Slot slots[4];
unsigned tick;
std::vector<uint32_t> mapped, scaled;

void drop(Slot &s) {
  if (s.src)
    SDL_DestroyTexture(s.src);
  if (s.big)
    SDL_DestroyTexture(s.big);
  s = Slot{};
}

Slot &slotFor(SDL_Renderer *r, const uint32_t *px) {
  Slot *best = &slots[0];
  for (auto &s : slots) {
    if (s.px == px && s.r == r)
      return s;
    if (s.used < best->used)
      best = &s;
  }
  drop(*best);
  best->px = px;
  best->r = r;
  return *best;
}

SDL_Texture *texture(SDL_Renderer *r, SDL_Texture *&t, int &tw, int &th, int &access, int w, int h, int want,
                     SDL_ScaleMode mode) {
  if (!t || tw != w || th != h || access != want) {
    if (t)
      SDL_DestroyTexture(t);
    t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB888, want, w, h);
    tw = w, th = h, access = want;
  }
  if (t)
    SDL_SetTextureScaleMode(t, mode);
  return t;
}

void scaleXbrz(int factor, int w, int h) {
  scaled.resize(size_t(w) * factor * h * factor);
  // Slices of rows on a few threads (xBRZ allows it as long as they don't overlap).
  int parts = std::clamp(int(std::thread::hardware_concurrency()), 1, 4);
  if (h < 64)
    parts = 1;
  std::vector<std::thread> threads;
  for (int i = 1; i < parts; i++)
    threads.emplace_back([=] {
      xbrz::scale(factor, mapped.data(), scaled.data(), w, h, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(),
                  h * i / parts, h * (i + 1) / parts);
    });
  xbrz::scale(factor, mapped.data(), scaled.data(), w, h, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), 0, h / parts);
  for (auto &t : threads)
    t.join();
}
} // namespace

extern "C" {
const char *pocket_shader_name(int shader) {
  static const char *const NAMES[POCKET_SHADERS] = {"OFF", "SHARP", "xBRZ"};
  return shader >= 0 && shader < POCKET_SHADERS ? NAMES[shader] : NAMES[0];
}

const char *pocket_colors_name(int colors) {
  static const char *const NAMES[POCKET_COLORS_COUNT] = {"ORIGINAL", "HANDHELD", "OLED"};
  return colors >= 0 && colors < POCKET_COLORS_COUNT ? NAMES[colors] : NAMES[0];
}

void pocket_shader_set(int shader, int colors, bool ds) {
  gShader = shader >= 0 && shader < POCKET_SHADERS ? shader : POCKET_SHADER_OFF;
  gColors = colors >= 0 && colors < POCKET_COLORS_COUNT ? colors : POCKET_COLORS_ORIGINAL;
  gDs = ds;
}

void pocket_shader_desaturate(int percent) { gDesat = std::clamp(percent, 0, 100); }

void pocket_shader_brightness(int percent) {
  gBrightness = std::clamp(percent, POCKET_BRIGHTNESS_MIN, POCKET_BRIGHTNESS_MAX);
}

bool pocket_shader_active(void) {
  return gShader != POCKET_SHADER_OFF || gColors != POCKET_COLORS_ORIGINAL || gBrightness != 100 || gDesat;
}

uint32_t pocket_shader_color(uint32_t xbgr) {
  if (gColors == POCKET_COLORS_ORIGINAL && gBrightness == 100 && !gDesat)
    return swap(xbgr);
  buildLut();
  return lut[key(xbgr)];
}

bool pocket_shader_draw(SDL_Renderer *r, const uint32_t *px, int pitch, int w, int h, bool xrgb, const SDL_Rect *dst) {
  if (!r || !px || w <= 0 || h <= 0 || !dst || dst->w <= 0 || dst->h <= 0)
    return false;
  bool original = gColors == POCKET_COLORS_ORIGINAL && gBrightness == 100 && !gDesat;
  if (!original)
    buildLut();
  mapped.resize(size_t(w) * h);
  for (int y = 0; y < h; y++) {
    const uint32_t *row = px + size_t(y) * pitch;
    uint32_t *out = mapped.data() + size_t(y) * w;
    if (original && xrgb)
      std::copy(row, row + w, out);
    else if (original)
      for (int x = 0; x < w; x++)
        out[x] = swap(row[x]);
    else if (xrgb)
      for (int x = 0; x < w; x++)
        out[x] = lut[key(swap(row[x]))]; // swap() trades red and blue either way
    else
      for (int x = 0; x < w; x++)
        out[x] = lut[key(row[x])];
  }
  Slot &s = slotFor(r, px);
  s.used = ++tick;
  int ratio = std::max(1, std::min((dst->w + w - 1) / w, (dst->h + h - 1) / h));
  bool whole = dst->w % w == 0 && dst->h % h == 0 && dst->w / w == dst->h / h;
  int access = SDL_TEXTUREACCESS_STREAMING;
  if (gShader == POCKET_SHADER_XBRZ) {
    int factor = std::clamp(ratio, 2, xbrz::SCALE_FACTOR_MAX);
    scaleXbrz(factor, w, h);
    SDL_Texture *t = texture(r, s.big, s.bw, s.bh, s.bigAccess, w * factor, h * factor, access, SDL_ScaleModeLinear);
    if (!t || SDL_UpdateTexture(t, nullptr, scaled.data(), w * factor * 4) < 0)
      return false;
    return SDL_RenderCopy(r, t, nullptr, dst) == 0;
  }
  int srcAccess = SDL_TEXTUREACCESS_STREAMING;
  SDL_Texture *src = texture(r, s.src, s.sw, s.sh, srcAccess, w, h, SDL_TEXTUREACCESS_STREAMING, SDL_ScaleModeNearest);
  if (!src || SDL_UpdateTexture(src, nullptr, mapped.data(), w * 4) < 0)
    return false;
  if (gShader == POCKET_SHADER_OFF || whole || !SDL_RenderTargetSupported(r))
    return SDL_RenderCopy(r, src, nullptr, dst) == 0;
  // SHARP: whole pixels up to just past the box, then smoothed down to it: every pixel the same size, edges crisp.
  int k = std::min(ratio, 8);
  SDL_Texture *big = texture(r, s.big, s.bw, s.bh, s.bigAccess, w * k, h * k, SDL_TEXTUREACCESS_TARGET,
                             SDL_ScaleModeLinear);
  if (!big)
    return SDL_RenderCopy(r, src, nullptr, dst) == 0;
  SDL_Texture *before = SDL_GetRenderTarget(r);
  if (SDL_SetRenderTarget(r, big) < 0)
    return SDL_RenderCopy(r, src, nullptr, dst) == 0;
  SDL_RenderCopy(r, src, nullptr, nullptr);
  SDL_SetRenderTarget(r, before);
  return SDL_RenderCopy(r, big, nullptr, dst) == 0;
}

void pocket_shader_free(void) {
  for (auto &s : slots)
    drop(s);
}
}
