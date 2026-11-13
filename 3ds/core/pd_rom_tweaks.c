// SPDX-License-Identifier: GPL-3.0-or-later
// INSTANT TEXT and REUSABLE TMS, ported from the Android app (InstantText.kt, ReusableTms.kt): the notes there say how
// each patch works and which games it fits.
#include "pd_rom_tweaks.h"

#include <string.h>

#define SEARCH 0x10000
#define ANY (-1)
#define ANY_BL_HIGH (-2) // the first half of a `bl`: 0xF0-0xF7
#define ANY_BL_LOW (-3)  // the second half: 0xF8-0xFF

// RenderFont as agbcc built it, through its `bx r1`; 40 bytes, the gFonts literal straight after.
static const int FONT[] = {
    0x10, 0xB5, 0x04, 0x1C, 0x61, 0x79, 0x08, 0x48, 0x02, 0x68, 0x48, 0x00, 0x40, 0x18, 0x80, 0x00, 0x80, 0x18, 0x01, 0x68,
    0x20, 0x1C, ANY, ANY_BL_HIGH, ANY, ANY_BL_LOW, 0x00, 0x04, 0x00, 0x0C, 0x02, 0x28, 0xF0, 0xD0, 0x10, 0xBC, 0x02, 0xBC,
    0x08, 0x47,
};
#define FONT_CALL 0x16
#define NEW_CALL 0x12

// The new RenderFont: it prints on to the next control code in one frame (InstantText.kt has it line by line).
static const uint8_t FONT_CODE[40] = {
    0x10, 0xB5, 0x04, 0x1C, 0x61, 0x79, 0x0C, 0x20, 0x41, 0x43, 0x07, 0x48, 0x00, 0x68, 0x41, 0x58, 0x20, 0x1C, 0, 0, 0, 0,
    0x41, 0x08, 0x05, 0xD2, 0xF3, 0xD1, 0x60, 0x77, 0x21, 0x68, 0x09, 0x78, 0xF7, 0x29, 0xEE, 0xD3, 0x10, 0xBD,
};

// Ruby / Sapphire's UpdateWindowText: its PrintNextChar(win) and the switch after it ...
static const int RS_PRINT[] = {0x20, 0x1C, ANY, ANY_BL_HIGH, ANY, ANY_BL_LOW, 0xE0, 0x8A, 0x0A, 0x28, 0x25, 0xD8, 0x80, 0x00};
// ... and its `state = WIN_STATE_CHAR_DELAY; delayCounter = GetTextDelay(win)` with the `return 0` after it.
static const int RS_DELAY[] = {
    0x03, 0x20, 0xE0, 0x82, 0x20, 0x1C, 0x00, 0xF0, ANY, ANY_BL_LOW, 0x60, 0x73, 0x00, 0x20, 0x10, 0xBC, 0x02, 0xBC, 0x08, 0x47,
};
#define RS_GAP 0x58

#define COUNT(a) ((int) (sizeof(a) / sizeof((a)[0])))

static bool sig_at(const uint8_t* rom, size_t o, const int* sig, int n) {
    for (int i = 0; i < n; i++) {
        int b = rom[o + i];
        if (sig[i] == ANY) continue;
        if (sig[i] == ANY_BL_HIGH ? b < 0xF0 || b > 0xF7 : sig[i] == ANY_BL_LOW ? b < 0xF8 : b != sig[i]) return false;
    }
    return true;
}

// Where `sig` sits in the first 64 KB: up to two places (enough to tell one from many).
static int matches(const uint8_t* rom, size_t size, const int* sig, int n, size_t found[2]) {
    size_t limit = size < SEARCH ? size : SEARCH;
    if (limit < (size_t) n + 4) return 0;
    int count = 0;
    for (size_t o = 0; o + (size_t) n + 4 <= limit; o += 2) {
        if (sig_at(rom, o, sig, n)) {
            if (count < 2) found[count] = o;
            count++;
        }
    }
    return count;
}

static unsigned u16(const uint8_t* p) { return (unsigned) p[0] | ((unsigned) p[1] << 8); }

// Where the `bl` at ROM offset `at` goes.
static int64_t bl_target(const uint8_t* rom, size_t at) {
    int hi = (int) (u16(rom + at) & 0x7FF);
    if (hi & 0x400) hi -= 0x800;
    return (int64_t) at + 4 + ((int64_t) hi * 4096) + ((int64_t) (u16(rom + at + 2) & 0x7FF) << 1);
}

static void bl(uint8_t out[4], int64_t at, int64_t target) {
    int64_t off = (target - (at + 4)) >> 1;
    unsigned h1 = 0xF000 | (unsigned) ((off >> 11) & 0x7FF), h2 = 0xF800 | (unsigned) (off & 0x7FF);
    out[0] = (uint8_t) h1;
    out[1] = (uint8_t) (h1 >> 8);
    out[2] = (uint8_t) h2;
    out[3] = (uint8_t) (h2 >> 8);
}

bool pd_instant_text_find(const uint8_t* rom, size_t size, struct pd_tweak_patch* out) {
    memset(out, 0, sizeof(*out));
    if (!rom) return false;
    size_t font[2], print[2], delay[2];
    int nFont = matches(rom, size, FONT, COUNT(FONT), font);
    int nPrint = matches(rom, size, RS_PRINT, COUNT(RS_PRINT), print);
    int nDelay = matches(rom, size, RS_DELAY, COUNT(RS_DELAY), delay);
    if (nFont == 1 && nPrint == 0) {
        size_t at = font[0];
        if (at % 4) return false; // FONT_CODE's `ldr` counts on it
        uint8_t code[40];
        memcpy(code, FONT_CODE, sizeof(code));
        bl(code + NEW_CALL, (int64_t) (at + NEW_CALL), bl_target(rom, at + FONT_CALL));
        for (int i = 0; i < 40; i += 4) {
            if (!memcmp(rom + at + i, code + i, 4)) continue;
            struct pd_tweak_write* w = &out->writes[out->count++];
            w->at = (uint32_t) (at + i);
            w->len = 4;
            memcpy(w->value, code + i, 4);
        }
        return out->count > 0;
    }
    if (nFont == 0 && nPrint == 1 && nDelay == 1 && delay[0] == print[0] + RS_GAP) {
        unsigned branch = 0xE000 | (unsigned) ((((int64_t) print[0] - (int64_t) (delay[0] + 4)) >> 1) & 0x7FF);
        struct pd_tweak_write* w = &out->writes[out->count++];
        w->at = (uint32_t) delay[0];
        w->len = 2;
        w->value[0] = (uint8_t) branch;
        w->value[1] = (uint8_t) (branch >> 8);
        return true;
    }
    return false;
}

// How many sites the engine behind the header's game code has: the FireRed engine three, Emerald / Ruby / Sapphire one.
static int expected_sites(const uint8_t* rom, size_t size) {
    if (size < 0xC0) return 0;
    const char* code = (const char*) rom + 0xAC;
    if (!memcmp(code, "BPR", 3) || !memcmp(code, "BPG", 3) || !memcmp(code, "PUV", 3)) return 3;
    if (!memcmp(code, "BPE", 3) || !memcmp(code, "AXV", 3) || !memcmp(code, "AXP", 3)) return 1;
    return 0;
}

bool pd_reusable_tms_find(const uint8_t* rom, size_t size, struct pd_tweak_patch* out) {
    memset(out, 0, sizeof(*out));
    int want = rom ? expected_sites(rom, size) : 0;
    if (!want) return false;
    int found = 0;
    int64_t calls = -1;
    for (size_t o = 0; o + 16 <= size; o += 2) {
        // movs r0, #0xA9; lsls r0, r0, #1; cmp rN, r0; bhi past the call; adds r0, rN, #0; movs r1, #1; bl RemoveBagItem
        if (rom[o] != 0xA9 || rom[o + 1] != 0x20 || u16(rom + o + 2) != 0x0040) continue;
        unsigned cmp = u16(rom + o + 4), n = cmp & 7;
        if (cmp != (0x4280 | n) || u16(rom + o + 6) != 0xD803 || u16(rom + o + 8) != (0x1C00 | (n << 3)) ||
            u16(rom + o + 10) != 0x2101 || (u16(rom + o + 12) & 0xF800) != 0xF000 || (u16(rom + o + 14) & 0xF800) != 0xF800)
            continue;
        int64_t target = bl_target(rom, o + 12);
        if (calls >= 0 && target != calls) return memset(out, 0, sizeof(*out)), false; // every site calls the same function
        calls = target;
        if (++found > want) return memset(out, 0, sizeof(*out)), false;
        struct pd_tweak_write* w = &out->writes[out->count++];
        w->at = (uint32_t) (o + 6);
        w->len = 2;
        w->value[0] = 0x03; // `b` to where the `bhi` went
        w->value[1] = 0xE0;
    }
    if (found != want) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    return true;
}
