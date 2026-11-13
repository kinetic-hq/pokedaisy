/* PATCH A ROM's BPS reader (bps.h). GPL-3.0, see ../NOTICE. */
#include "bps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOOTER 12

uint32_t bps_crc32(uint32_t crc, const uint8_t *data, size_t len) {
  static uint32_t table[256];
  static bool ready;
  if (!ready) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++)
        c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
    ready = true;
  }
  crc = ~crc;
  for (size_t i = 0; i < len; i++)
    crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

bool bps_looks_like(const uint8_t *head, size_t len) {
  return len >= 4 && head[0] == 'B' && head[1] == 'P' && head[2] == 'S' && head[3] == '1';
}

static uint32_t u32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

/* The patch's numbers: 7 bits a byte, the last one's top bit set, each byte past the first adding one more step. */
static bool number(const uint8_t *b, size_t *at, size_t end, uint64_t *v) {
  uint64_t data = 0, shift = 1;
  while (*at < end) {
    uint8_t x = b[(*at)++];
    data += (uint64_t)(x & 0x7F) * shift;
    if (x & 0x80) {
      *v = data;
      return true;
    }
    shift <<= 7;
    data += shift;
    if (shift > (1ull << 56))
      return false;
  }
  return false;
}

enum bps_result bps_parse(struct bps *p, const uint8_t *bytes, size_t size) {
  memset(p, 0, sizeof *p);
  if (!bps_looks_like(bytes, size))
    return BPS_NOT_BPS;
  if (size < 4 + 3 + FOOTER)
    return BPS_DAMAGED;
  size_t end = size - FOOTER, at = 4;
  if (bps_crc32(0, bytes, size - 4) != u32(bytes + size - 4))
    return BPS_DAMAGED;
  uint64_t meta;
  if (!number(bytes, &at, end, &p->sourceSize) || !number(bytes, &at, end, &p->targetSize) ||
      !number(bytes, &at, end, &meta) || meta > end - at)
    return BPS_DAMAGED;
  at += (size_t)meta;
  p->bytes = bytes;
  p->size = size;
  p->actions = at;
  p->sourceCrc = u32(bytes + end);
  p->targetCrc = u32(bytes + end + 4);
  return BPS_OK;
}

enum bps_result bps_apply(const struct bps *p, const uint8_t *source, size_t sourceLen, uint8_t **out, size_t *outLen) {
  *out = NULL;
  *outLen = 0;
  if ((uint64_t)sourceLen != p->sourceSize || bps_crc32(0, source, sourceLen) != p->sourceCrc)
    return BPS_WRONG_SOURCE;
  if (p->sourceSize + p->targetSize > BPS_MAX_BYTES)
    return BPS_TOO_BIG;
  size_t tlen = (size_t)p->targetSize;
  uint8_t *target = malloc(tlen ? tlen : 1);
  if (!target)
    return BPS_NO_MEMORY;
  const uint8_t *b = p->bytes;
  size_t end = p->size - FOOTER, at = p->actions, o = 0;
  int64_t sourceRel = 0, targetRel = 0;
  enum bps_result r = BPS_OK;
  while (at < end) {
    uint64_t data, d;
    if (!number(b, &at, end, &data)) {
      r = BPS_DAMAGED;
      break;
    }
    uint64_t length = (data >> 2) + 1;
    if (o + length > tlen) {
      r = BPS_DAMAGED;
      break;
    }
    size_t n = (size_t)length;
    switch (data & 3) {
    case 0: // SOURCE_READ
      if (o + n > sourceLen)
        r = BPS_DAMAGED;
      else
        memcpy(target + o, source + o, n);
      break;
    case 1: // TARGET_READ
      if (at + n > end)
        r = BPS_DAMAGED;
      else {
        memcpy(target + o, b + at, n);
        at += n;
      }
      break;
    case 2: // SOURCE_COPY
      if (!number(b, &at, end, &d)) {
        r = BPS_DAMAGED;
        break;
      }
      sourceRel += d & 1 ? -(int64_t)(d >> 1) : (int64_t)(d >> 1);
      if (sourceRel < 0 || (uint64_t)sourceRel + n > sourceLen)
        r = BPS_DAMAGED;
      else {
        memcpy(target + o, source + sourceRel, n);
        sourceRel += (int64_t)n;
      }
      break;
    default: // TARGET_COPY: may overlap what it writes (a run), so byte by byte
      if (!number(b, &at, end, &d)) {
        r = BPS_DAMAGED;
        break;
      }
      targetRel += d & 1 ? -(int64_t)(d >> 1) : (int64_t)(d >> 1);
      if (targetRel < 0 || (uint64_t)targetRel >= o)
        r = BPS_DAMAGED;
      else {
        for (size_t i = 0; i < n; i++)
          target[o + i] = target[targetRel + (int64_t)i];
        targetRel += (int64_t)n;
      }
      break;
    }
    if (r != BPS_OK)
      break;
    o += n;
  }
  if (r == BPS_OK && (o != tlen || bps_crc32(0, target, tlen) != p->targetCrc))
    r = BPS_BAD_RESULT;
  if (r != BPS_OK) {
    free(target);
    return r;
  }
  *out = target;
  *outLen = tlen;
  return BPS_OK;
}

const char *bps_reason(enum bps_result r) {
  switch (r) {
  case BPS_OK: return "Done";
  case BPS_NOT_BPS: return "That isn't a .bps patch";
  case BPS_DAMAGED: return "The patch is damaged or cut off: download it again";
  case BPS_WRONG_SOURCE: return "That game isn't the one the patch is for";
  case BPS_TOO_BIG: return "The patch is too big to apply here";
  case BPS_BAD_RESULT: return "The patched game came out wrong: nothing was saved";
  case BPS_NO_MEMORY: return "Not enough memory to apply the patch";
  }
  return "Couldn't apply the patch";
}

const char *bps_known_base(uint32_t crc) {
  static const struct {
    uint32_t crc;
    const char *name;
  } KNOWN[] = {
      {0xDD88761C, "Pokemon FireRed (USA)"},
      {0x84EE4776, "Pokemon FireRed (USA, Europe) (Rev 1)"},
      {0xD69C96CC, "Pokemon LeafGreen (USA)"},
      {0xDAFFECEC, "Pokemon LeafGreen (USA, Europe) (Rev 1)"},
      {0x1F1C08FB, "Pokemon Emerald (USA, Europe)"},
      {0x61641576, "Pokemon Ruby (USA, Europe) (Rev 1)"},
      {0xAEAC73E6, "Pokemon Ruby (USA, Europe) (Rev 2)"},
      {0xBAFEDAE5, "Pokemon Sapphire (USA, Europe) (Rev 1)"},
      {0x9CC4410E, "Pokemon Sapphire (USA, Europe) (Rev 2)"},
      {0x9F7FDD53, "Pokemon Red (USA, Europe)"},
      {0xD6DA8A1A, "Pokemon Blue (USA, Europe)"},
      {0x7D527D62, "Pokemon Yellow (USA, Europe)"},
      {0xEE6F5188, "Pokemon Crystal (USA)"},
      {0x3358E30A, "Pokemon Crystal (USA, Europe) (Rev 1)"},
      {0xBB6DD80C, "Pokemon Crystal (Australia)"},
      {0x6BDE3C3E, "Pokemon Gold (USA, Europe)"},
      {0x8AD48636, "Pokemon Silver (USA, Europe)"},
  };
  for (size_t i = 0; i < sizeof KNOWN / sizeof KNOWN[0]; i++)
    if (KNOWN[i].crc == crc)
      return KNOWN[i].name;
  return NULL;
}

enum bps_platform bps_platform_of(const uint8_t *rom, size_t len) {
  if (len > 0x150 && rom[0x104] == 0xCE && rom[0x105] == 0xED && rom[0x106] == 0x66 && rom[0x107] == 0x66)
    return rom[0x143] & 0x80 ? BPS_GBC : BPS_GB;
  if (len > 0x200 && rom[0x15C] == 0x56 && rom[0x15D] == 0xCF)
    return BPS_NDS;
  return BPS_GBA;
}

void bps_output_name(char *out, size_t size, const char *patchName, const char *ext) {
  const char *base = patchName;
  for (const char *p = patchName; *p; p++)
    if (*p == '/' || *p == '\\')
      base = p + 1;
  char stem[512];
  snprintf(stem, sizeof stem, "%s", base);
  char *dot = strrchr(stem, '.');
  if (dot)
    *dot = 0;
  // Trim spaces, and keep only what a file name can hold.
  char *s = stem;
  while (*s == ' ')
    s++;
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ')
    s[--n] = 0;
  for (char *p = s; *p; p++)
    if (strchr("<>:\"/\\|?*", *p) || (unsigned char)*p < 32)
      *p = '_';
  snprintf(out, size, "%s.%s", *s ? s : "patched", ext);
}
