// SPDX-License-Identifier: GPL-3.0-or-later
// INSTANT TEXT and REUSABLE TMS for a GBA ROM: where to patch, found by the code itself (no table per game).
// Ported from the Android app's InstantText.kt and ReusableTms.kt; the host writes the bytes (see linux/main.c).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PD_TWEAK_WRITES_MAX 16

// One piece: `len` bytes of `value` at ROM offset `at` (bus address 0x08000000 + at).
struct pd_tweak_write {
    uint32_t at;
    uint8_t len;
    uint8_t value[4];
};

struct pd_tweak_patch {
    int count; // 0 = the ROM's code isn't known
    struct pd_tweak_write writes[PD_TWEAK_WRITES_MAX];
};

// INSTANT TEXT: a text box's words appear at once. Reads the ROM's first 64 KB.
bool pd_instant_text_find(const uint8_t* rom, size_t size, struct pd_tweak_patch* out);

// REUSABLE TMS: a TM stays in the bag after it teaches a move. All the engine's sites or none.
bool pd_reusable_tms_find(const uint8_t* rom, size_t size, struct pd_tweak_patch* out);
