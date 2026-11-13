// SPDX-License-Identifier: GPL-3.0-or-later
// INSTANT TEXT and REUSABLE TMS (pd_rom_tweaks.c) on hand-made code: no ROM bytes in the repo.
#include "pd_rom_tweaks.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 0x20000

static void put_bl(uint8_t* rom, size_t at, size_t target) {
    long off = ((long) target - (long) (at + 4)) >> 1;
    unsigned h1 = 0xF000 | (unsigned) ((off >> 11) & 0x7FF), h2 = 0xF800 | (unsigned) (off & 0x7FF);
    rom[at] = (uint8_t) h1;
    rom[at + 1] = (uint8_t) (h1 >> 8);
    rom[at + 2] = (uint8_t) h2;
    rom[at + 3] = (uint8_t) (h2 >> 8);
}

// RenderFont as agbcc built it (InstantText.kt's FONT), its `bl _call_via_r1` at +0x16 going to `callee`.
static void put_font(uint8_t* rom, size_t at, size_t callee) {
    static const uint8_t font[40] = {
        0x10, 0xB5, 0x04, 0x1C, 0x61, 0x79, 0x08, 0x48, 0x02, 0x68, 0x48, 0x00, 0x40, 0x18, 0x80, 0x00, 0x80, 0x18, 0x01, 0x68,
        0x20, 0x1C, 0, 0, 0, 0, 0x00, 0x04, 0x00, 0x0C, 0x02, 0x28, 0xF0, 0xD0, 0x10, 0xBC, 0x02, 0xBC, 0x08, 0x47,
    };
    memcpy(rom + at, font, sizeof(font));
    put_bl(rom, at + 0x16, callee);
}

// The party menu's `if (item < ITEM_HM01) RemoveBagItem(item, 1)` with the item in r4.
static void put_tm_site(uint8_t* rom, size_t at, size_t removeBagItem) {
    static const uint8_t site[12] = {0xA9, 0x20, 0x40, 0x00, 0x84, 0x42, 0x03, 0xD8, 0x20, 0x1C, 0x01, 0x21};
    memcpy(rom + at, site, sizeof(site));
    put_bl(rom, at + 12, removeBagItem);
}

static size_t bl_target(const uint8_t* b, size_t at) {
    int hi = (int) ((b[at] | (b[at + 1] << 8)) & 0x7FF);
    if (hi & 0x400) hi -= 0x800;
    return (size_t) ((long) at + 4 + (long) hi * 4096 + (long) (((b[at + 2] | (b[at + 3] << 8)) & 0x7FF) << 1));
}

int main(void) {
    uint8_t* rom = calloc(1, SIZE);
    struct pd_tweak_patch p;

    // The FireRed engine: one RenderFont, three TM sites calling one RemoveBagItem.
    memcpy(rom + 0xAC, "BPRE", 4);
    put_font(rom, 0x1000, 0x1800);
    put_tm_site(rom, 0x4000, 0x9000);
    put_tm_site(rom, 0x6000, 0x9000);
    put_tm_site(rom, 0x12000, 0x9000);

    assert(pd_instant_text_find(rom, SIZE, &p));
    uint8_t patched[40];
    memcpy(patched, rom + 0x1000, sizeof(patched));
    for (int i = 0; i < p.count; i++) {
        assert(p.writes[i].len == 4 && p.writes[i].at >= 0x1000 && p.writes[i].at + 4 <= 0x1028);
        memcpy(patched + (p.writes[i].at - 0x1000), p.writes[i].value, 4);
    }
    // The new code keeps the font's call, moved to +0x12, and ends in `pop {r4, pc}`.
    uint8_t whole[0x1028];
    memcpy(whole, rom, sizeof(whole));
    memcpy(whole + 0x1000, patched, sizeof(patched));
    assert(bl_target(whole, 0x1012) == 0x1800);
    assert(patched[38] == 0x10 && patched[39] == 0xBD);

    assert(pd_reusable_tms_find(rom, SIZE, &p) && p.count == 3);
    assert(p.writes[0].at == 0x4006 && p.writes[1].at == 0x6006 && p.writes[2].at == 0x12006);
    for (int i = 0; i < 3; i++) assert(p.writes[i].len == 2 && p.writes[i].value[0] == 0x03 && p.writes[i].value[1] == 0xE0);

    // A site calling another function: a hack changed that path, so none.
    put_bl(rom, 0x6000 + 12, 0xA000);
    assert(!pd_reusable_tms_find(rom, SIZE, &p) && p.count == 0);
    put_bl(rom, 0x6000 + 12, 0x9000);
    // Emerald's engine has one site, so three is none.
    memcpy(rom + 0xAC, "BPEE", 4);
    assert(!pd_reusable_tms_find(rom, SIZE, &p));
    // A second RenderFont: the code isn't known.
    put_font(rom, 0x2000, 0x1800);
    assert(!pd_instant_text_find(rom, SIZE, &p));

    // Ruby / Sapphire: PrintNextChar's call and, 0x58 on, the delay a `b` back to it replaces.
    memset(rom, 0, SIZE);
    memcpy(rom + 0xAC, "AXVE", 4);
    static const uint8_t print[14] = {0x20, 0x1C, 0, 0, 0, 0, 0xE0, 0x8A, 0x0A, 0x28, 0x25, 0xD8, 0x80, 0x00};
    static const uint8_t delay[20] = {0x03, 0x20, 0xE0, 0x82, 0x20, 0x1C, 0x00, 0xF0, 0x10, 0xF8,
                                      0x60, 0x73, 0x00, 0x20, 0x10, 0xBC, 0x02, 0xBC, 0x08, 0x47};
    memcpy(rom + 0x3000, print, sizeof(print));
    put_bl(rom, 0x3002, 0x3800);
    memcpy(rom + 0x3058, delay, sizeof(delay));
    assert(pd_instant_text_find(rom, SIZE, &p) && p.count == 1);
    assert(p.writes[0].at == 0x3058 && p.writes[0].len == 2);
    unsigned branch = p.writes[0].value[0] | (p.writes[0].value[1] << 8);
    assert((branch & 0xF800) == 0xE000);
    int back = (int) (branch & 0x7FF);
    if (back & 0x400) back -= 0x800;
    assert(0x3058 + 4 + back * 2 == 0x3000);

    free(rom);
    puts("rom tweaks ok");
    return 0;
}
