/* PATCH A ROM (bps.c): a patch built here with each of the four actions, applied, and its checks. */
#include "bps.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t buf[4096];
static size_t len;

static void num(uint64_t v) {
  for (;;) {
    uint8_t x = v & 0x7F;
    v >>= 7;
    if (!v) {
      buf[len++] = 0x80 | x;
      return;
    }
    buf[len++] = x;
    v--;
  }
}
static void u32(uint32_t v) {
  for (int i = 0; i < 4; i++)
    buf[len++] = (uint8_t)(v >> (8 * i));
}
static void action(int kind, uint64_t n) { num((n - 1) << 2 | (uint64_t)kind); }
static void rel(int64_t d) { num(d < 0 ? ((uint64_t)-d << 1) | 1 : (uint64_t)d << 1); }

int main(void) {
  // The CRC itself: zlib's.
  assert(bps_crc32(0, (const uint8_t *)"123456789", 9) == 0xCBF43926u);

  uint8_t source[64], want[64 + 8];
  for (int i = 0; i < 64; i++)
    source[i] = (uint8_t)(i * 7 + 3);
  // The target: source 0-15 as is, "HACK", source 40-51 (copied from elsewhere), then a run of 'Z' x 12 from the
  // target itself, then source 0-27.
  size_t t = 0;
  memcpy(want + t, source, 16), t += 16;
  memcpy(want + t, "HACK", 4), t += 4;
  memcpy(want + t, source + 40, 12), t += 12;
  want[t++] = 'Z';
  for (int i = 0; i < 11; i++)
    want[t + i] = 'Z';
  t += 11;
  memcpy(want + t, source, 28), t += 28;
  assert(t == 72);

  memcpy(buf, "BPS1", 4), len = 4;
  num(64), num(72);
  num(4), memcpy(buf + len, "meta", 4), len += 4;
  action(0, 16);                                        // SOURCE_READ 0-15
  action(1, 4), memcpy(buf + len, "HACK", 4), len += 4; // TARGET_READ
  action(2, 12), rel(40);                               // SOURCE_COPY from 40
  action(1, 1), buf[len++] = 'Z';                       // TARGET_READ 'Z' (at 32)
  action(3, 11), rel(32);                               // TARGET_COPY from 32: a run
  action(2, 28), rel(-52);                              // SOURCE_COPY back to 0
  u32(bps_crc32(0, source, 64));
  u32(bps_crc32(0, want, 72));
  u32(bps_crc32(0, buf, len));

  struct bps p;
  assert(bps_parse(&p, buf, len) == BPS_OK);
  assert(p.sourceSize == 64 && p.targetSize == 72 && p.sourceCrc == bps_crc32(0, source, 64));
  uint8_t *out;
  size_t outLen;
  assert(bps_apply(&p, source, 64, &out, &outLen) == BPS_OK);
  assert(outLen == 72 && !memcmp(out, want, 72));
  free(out);

  // Another game: refused before anything is written.
  uint8_t other[64];
  memcpy(other, source, 64);
  other[5] ^= 1;
  assert(bps_apply(&p, other, 64, &out, &outLen) == BPS_WRONG_SOURCE && !out);
  assert(bps_apply(&p, source, 63, &out, &outLen) == BPS_WRONG_SOURCE);

  // A damaged or cut-off download: its own CRC fails.
  uint8_t bad[4096];
  memcpy(bad, buf, len);
  bad[12] ^= 0x40;
  assert(bps_parse(&p, bad, len) == BPS_DAMAGED);
  assert(bps_parse(&p, buf, len - 3) == BPS_DAMAGED);
  assert(bps_parse(&p, (const uint8_t *)"UPS1xxxxxxxxxxxxxxxxx", 21) == BPS_NOT_BPS);

  // Header kinds and names.
  uint8_t rom[0x300] = {0};
  assert(bps_platform_of(rom, sizeof rom) == BPS_GBA);
  rom[0x104] = 0xCE, rom[0x105] = 0xED, rom[0x106] = 0x66, rom[0x107] = 0x66;
  assert(bps_platform_of(rom, sizeof rom) == BPS_GB);
  rom[0x143] = 0x80;
  assert(bps_platform_of(rom, sizeof rom) == BPS_GBC);
  char name[300];
  bps_output_name(name, sizeof name, "C:\\Patches\\ Unbound v2.1.1.bps", "gba");
  assert(!strcmp(name, "Unbound v2.1.1.gba"));
  bps_output_name(name, sizeof name, "/x/What?.bps", "gbc");
  assert(!strcmp(name, "What_.gbc"));
  bps_output_name(name, sizeof name, ".bps", "gba");
  assert(!strcmp(name, "patched.gba"));
  assert(bps_known_base(0x1F1C08FB) && !bps_known_base(1));
  printf("bps ok\n");
  return 0;
}
