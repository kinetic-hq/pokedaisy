/* PATCH A ROM: BPS patches (byuu's format, what most ROM hacks ship as), as the Android app's BpsPatch.kt and
 * RomPatcher.kt. `BPS1`, the source / target sizes, a metadata string, the actions, then the source, target and patch
 * CRC32s. The source's size and CRC find the ROM a patch is for; the target's CRC checks the result.
 * GPL-3.0, see ../NOTICE. */
#ifndef POCKET_BPS_H
#define POCKET_BPS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* In memory, source and result together: a GBA ROM (32 MB) with room to spare, not a big DS cart. */
#define BPS_MAX_BYTES (128u << 20)

enum bps_result { BPS_OK, BPS_NOT_BPS, BPS_DAMAGED, BPS_WRONG_SOURCE, BPS_TOO_BIG, BPS_BAD_RESULT, BPS_NO_MEMORY };

struct bps {
  const uint8_t *bytes; // the patch file (not copied: keep it while the patch is used)
  size_t size;
  uint64_t sourceSize, targetSize;
  size_t actions; // where the actions start
  uint32_t sourceCrc, targetCrc;
};

/* CRC32 (zlib's) of LEN bytes, going on from CRC (0 to start). */
uint32_t bps_crc32(uint32_t crc, const uint8_t *data, size_t len);
/* "BPS1" at the start. */
bool bps_looks_like(const uint8_t *head, size_t len);
/* BYTES read as a patch, its own CRC checked (a cut-off download fails here, not halfway). */
enum bps_result bps_parse(struct bps *p, const uint8_t *bytes, size_t size);
/* SOURCE patched into a new buffer (*OUT, freed by the caller): SOURCE checked against the patch's size and CRC
 * first, the result against the target's after. */
enum bps_result bps_apply(const struct bps *p, const uint8_t *source, size_t sourceLen, uint8_t **out, size_t *outLen);
/* A short line for each result, for the notice. */
const char *bps_reason(enum bps_result r);
/* The game a patch is often for, by its CRC32 ("Pokemon FireRed (USA)"), or NULL. */
const char *bps_known_base(uint32_t crc);

/* What a ROM is, by its header: a Game Boy cart's logo at 0x104 (Color when 0x143 says so), a DS cart's logo CRC at
 * 0x15C, else GBA. */
enum bps_platform { BPS_GBA, BPS_GB, BPS_GBC, BPS_NDS };
enum bps_platform bps_platform_of(const uint8_t *rom, size_t len);
/* The patched ROM's file name: the patch's name with EXT, characters a file name can't hold swapped for '_'
 * ("Unbound.bps" -> "Unbound.gba"). */
void bps_output_name(char *out, size_t size, const char *patchName, const char *ext);

#endif
