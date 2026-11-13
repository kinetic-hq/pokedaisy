// SETTINGS: what the companion knows about the game, then the options - and
// the black-out overlays they open: a pick-list for a multi-choice setting
// (the app's OptionSelector: never a row cycling through more than two
// values) and a YES / NO confirm (OptionConfirm).
#include <stdio.h>
#include <string.h>

#include "pd_layout_pick.h"
#include "pd_ui_internal.h"

#define TITLE_H 24
#define LIST_Y (CONTENT_Y + TITLE_H + 4)

static const char* const SCREEN_NAMES[PD_SCREEN_MODES] = { "PIXEL 1x", "SHARP 1.25x", "SHARP 1.5x", "STRETCH" };
static const char* const ASPECT_NAMES[PD_ASPECTS] = { "ORIGINAL", "EXPAND", "ZOOM OUT" };
const char* const PD_GAME_SPEEDS[PD_GAME_SPEED_COUNT] = { "1x", "1.5x", "2x", "3x", "4x", "0.5x", "0.25x" };
const char* const PD_SHINY_LABELS[PD_SHINY_CHOICES] = { "NORMAL", "1 IN 4096", "1 IN 1024", "1 IN 512", "1 IN 128" };
const char* const PD_SCENE_SPEEDS[PD_SCENE_SPEED_COUNT] = { "SAME", "1x", "1.5x", "2x", "3x", "4x" };
static const char* const SCENE_NAMES[PD_SCENE_COUNT] = { "BATTLE SPEED", "TEXT SPEED", "MENU SPEED", "BIKE SPEED", "SURF SPEED" };
static const char* const VOLUMES[5] = { "20%", "40%", "60%", "80%", "100%" };

const char* pd_aspect_name(int aspect) {
    return aspect >= 0 && aspect < PD_ASPECTS ? ASPECT_NAMES[aspect] : "";
}

static const char* const VIEW_NAMES[PD_COMPANION_VIEWS] = {
    "BATTLE WINGS", "WINDOW", "SIDE PANEL", "WINGS", "GLASS DRAWER", "STEP BACK",
};
const char* pd_companion_view_name(int view) {
    return view >= 0 && view < PD_COMPANION_VIEWS ? VIEW_NAMES[view] : VIEW_NAMES[0];
}

const char* pd_screen_mode_name(int mode) {
    return mode >= 0 && mode < PD_SCREEN_MODES ? SCREEN_NAMES[mode] : "";
}

// The list's visible band (rows outside it are drawn clipped and take no taps) and its rows' height last time
// (the scroll is clamped to it before drawing, so a D-pad's step past the end never draws an empty list).
static int listTop, listBottom, lastTotal = -1;

int pd_ui_list_step(int selected, int count, int step) {
    if (count <= 0) return -1;
    int next = selected + step;
    return next < 0 ? 0 : next >= count ? count - 1 : next;
}

static void settings_nav_row(struct pd_ui* ui, int id, int y) {
    if (!ui->shoulderTabs || ui->settingsNavCount >= PD_UI_MAX_HITS) return;
    int i = ui->settingsNavCount++;
    ui->settingsNav[i].id = id;
    ui->settingsNav[i].top = y;
    if (ui->settingsNavId < 0) ui->settingsNavId = id;
}

int ui_settings_follow(int scroll, int y, int height) {
    // At the first setting keep the game's context (place, money and host) visible.
    return y <= 0 ? 0 : ui_scroll_to(scroll, y, ROW_H, height);
}

static void settings_outline(struct pd_canvas* c, int x, int y, int w, int h) {
    pd_fill(c, x, y, w, 2, PD_VALUE);
    pd_fill(c, x, y+h-2, w, 2, PD_VALUE);
    pd_fill(c, x, y, 2, h, PD_VALUE);
    pd_fill(c, x+w-2, y, 2, h, PD_VALUE);
}

static void setting(struct pd_ui* ui, struct pd_canvas* c, int x, int* y, int w, enum setting_row row,
                    const char* label, const char* value) {
    if (ui->shoulderTabs) {
        int top = *y + ui->settingsScroll - listTop;
        settings_nav_row(ui, HIT_SETTING + row, ui->settingsNavCount == 0 ? 0 : top);
    }
    ui_row(c, x, *y, w, label, value, ui->pressedId == HIT_SETTING + (int) row);
    // A scroll hit: a tap is the row, a drag on it still scrolls the list.
    if (*y >= listTop && *y + ROW_H <= listBottom) {
        ui_add_scroll_hit(ui, x - 4, *y, w + 8, ROW_H, HIT_SETTING + (int) row);
        if (ui->shoulderTabs && ui->overlay == PD_OVERLAY_NONE && ui->settingsNavId == HIT_SETTING + (int) row)
            settings_outline(c, x - 4, *y, w + 8, ROW_H);
    }
    *y += ROW_H;
}

// A row shown, not tappable (LOAD STATE with none yet, ASPECT N/A).
static void muted_row(struct pd_canvas* c, int x, int* y, int w, const char* label, const char* value) {
    pd_text(c, x, pd_text_y(*y, ROW_H), label, PD_MUTED, PD_MUTED_SHADOW);
    int vx = x + w * 42 / 100, past = x + pd_text_width(label) + 8;
    pd_text(c, vx < past ? past : vx, pd_text_y(*y, ROW_H), value, PD_MUTED, PD_MUTED_SHADOW);
    *y += ROW_H;
}

static void group(struct pd_canvas* c, int x, int* y, const char* title) {
    *y += 3;
    // Gen 1's value colour is the rows' black: its titles are a black band (groupTitleFill).
    if (pd_cur->groupTitleFill != PD_NO_SHADOW) pd_fill(c, x - 4, *y + 1, pd_text_width(title) + 8, ROW_H - 2, pd_cur->groupTitleFill);
    pd_text(c, x, pd_text_y(*y, ROW_H), title, pd_cur->groupTitleText, pd_cur->groupTitleShadow);
    *y += ROW_H;
}

// A row that opens a page of its own: the app's chevron at its right end.
static void page_row(struct pd_ui* ui, struct pd_canvas* c, int x, int* y, int w, enum setting_row row, const char* label) {
    int ry = *y;
    setting(ui, c, x, y, w, row, label, NULL);
    pd_triangle(c, x + w - 8, ry + 5, 4, PD_RIGHT, PD_VALUE);
}

static const char* on_off(bool on) { return on ? "ON" : "OFF"; }

// fullSettings (the Linux / Windows host): the app's companion SETTINGS, group by group in its order, the rows it has
// and this host's own in the group they fit; sub-pages for the set-once ones.
static void full_rows(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                      const struct pd_host_info* host, int ix, int* yp, int iw) {
    struct pd_settings* st = ui->settings;
    char buf[40];
    int y = *yp;
    static const char* const SHADERS[PD_SHADERS] = {"OFF", "SHARP", "xBRZ"};
    static const char* const COLORS[PD_COLORS] = {"ORIGINAL", "HANDHELD", "OLED"};
    static const char* const BUTTONS[PD_BUTTONS] = {"X", "Y", "L2", "R2", "NONE"};
    static const char* const ORIENT[PD_ORIENTATIONS] = {"LANDSCAPE", "PORTRAIT", "LANDSCAPE FLIPPED", "PORTRAIT FLIPPED"};
    switch (ui->settingsPage) {
    case PAGE_SPEED:
        if (host->hasSceneSpeeds) {
            group(c, ix, &y, "SCENE SPEEDS");
            for (int k = 0; k < PD_SCENE_COUNT; k++) {
                if (host->sceneKnown[k]) setting(ui, c, ix, &y, iw, SETTING_SCENE + k, SCENE_NAMES[k], PD_SCENE_SPEEDS[st->sceneSpeed[k]]);
                else muted_row(c, ix, &y, iw, SCENE_NAMES[k], "N/A");
            }
            group(c, ix, &y, "FAST-FORWARD");
        }
        if (host->hasFfMode) setting(ui, c, ix, &y, iw, SETTING_FF_MODE, "FF MODE", st->ffNormal ? "NORMAL" : "SMART");
        if (host->hasFfMusic) {
            ui->steadyOffered = host->hasSteady;
            setting(ui, c, ix, &y, iw, SETTING_FF_MUSIC, "FF MUSIC",
                    !st->ffAudio ? "OFF" : st->ffSteady && host->hasSteady ? "STEADY" : "SPED-UP");
        }
        *yp = y;
        return;
    case PAGE_DISPLAY:
        if (!host->hideScreenMode) setting(ui, c, ix, &y, iw, SETTING_SCREEN, "SCREEN", pd_screen_mode_name(st->screenMode));
        if (host->hasShaders) {
            setting(ui, c, ix, &y, iw, SETTING_SHADER, "SHADER", SHADERS[st->shader]);
            setting(ui, c, ix, &y, iw, SETTING_COLORS, "COLORS", COLORS[st->colors]);
        }
        if (host->hasDisplayFilter) setting(ui, c, ix, &y, iw, SETTING_FILTER, "FILTER", st->lcdFilter ? "LCD" : "OFF");
        if (host->hasShaders) setting(ui, c, ix, &y, iw, SETTING_DIM, "DIM COMPANION", on_off(st->dimCompanion));
        if (host->hasFullscreen) setting(ui, c, ix, &y, iw, SETTING_FULLSCREEN, "FULLSCREEN", on_off(st->fullscreen == 1));
        if (host->hasOrientation)
            setting(ui, c, ix, &y, iw, SETTING_ORIENTATION, "ORIENTATION",
                    ORIENT[st->orientation >= 0 && st->orientation < PD_ORIENTATIONS ? st->orientation : 0]);
        if (host->hasGameTouch)
            setting(ui, c, ix, &y, iw, SETTING_GAME_TOUCH, "GAME TOUCH",
                    st->gameTouch == PD_GAME_TOUCH_OFF ? "OFF" : "DS SCREEN ONLY");
        *yp = y;
        return;
    case PAGE_BUTTONS:
        setting(ui, c, ix, &y, iw, SETTING_BUTTON_LAYOUT, "BUTTON LAYOUT", st->buttonLayout == PD_LAYOUT_XBOX ? "XBOX" : "NINTENDO");
        *yp = y;
        return;
    case PAGE_HOTKEYS:
        setting(ui, c, ix, &y, iw, SETTING_QUICK_BTN, "QUICK MENU", BUTTONS[st->quickBtn]);
        setting(ui, c, ix, &y, iw, SETTING_COMPANION_BTN, "COMPANION", BUTTONS[st->companionBtn]);
        if (host->hasHud) setting(ui, c, ix, &y, iw, SETTING_HUD_BTN, "HUD CURSOR", BUTTONS[st->hudBtn]);
        *yp = y;
        return;
    case PAGE_QUICK:
        setting(ui, c, ix, &y, iw, SETTING_QUICK_MENU, "STYLE", st->quickGrid ? "GRID" : "RING");
        *yp = y;
        return;
    default:
        break;
    }
    if (s->valid) {
        const char* place = pd_mapsec_name(g, s->mapsec);
        ui_row(c, ix, y, iw, "PLACE", *place ? place : "-", false);
        y += ROW_H;
        pd_ui_format_money(s->money, buf, sizeof(buf));
        ui_row(c, ix, y, iw, "MONEY", buf, false);
        y += ROW_H;
    }
    for (int i = 0; i < host->count && i < PD_UI_HOST_LINES; i++) {
        ui_row(c, ix, y, iw, host->labels[i], host->values[i], false);
        y += ROW_H;
    }
    pd_fill(c, ix, y + 1, iw, 1, PD_DIVIDER);
    y += 4;

    group(c, ix, &y, "FF / SPEED");
    setting(ui, c, ix, &y, iw, SETTING_FF_TOGGLE, "FF", on_off(host->ffOn));
    if (st->ffSpeed) snprintf(buf, sizeof(buf), "%dx", st->ffSpeed);
    else snprintf(buf, sizeof(buf), "MAX");
    setting(ui, c, ix, &y, iw, SETTING_FF, "FF SPEED", buf);
    if (host->hasGameSpeed) setting(ui, c, ix, &y, iw, SETTING_GAME_SPEED, "GAME SPEED", PD_GAME_SPEEDS[st->gameSpeed]);
    if (host->hasRewind) setting(ui, c, ix, &y, iw, SETTING_REWIND, "REWIND", on_off(st->rewind));
    if (host->hasFfMode || host->hasFfMusic || host->hasSceneSpeeds) page_row(ui, c, ix, &y, iw, SETTING_PAGE_SPEED, "SPEED OPTIONS");

    if (host->hasSound) {
        group(c, ix, &y, "SOUND");
        snprintf(buf, sizeof(buf), "%d%%", st->gameVolume);
        setting(ui, c, ix, &y, iw, SETTING_VOLUME, "GAME VOLUME", buf);
        setting(ui, c, ix, &y, iw, SETTING_MUTE, "MUTE GAME", on_off(st->gameMuted));
    }

    group(c, ix, &y, "GAMEPLAY");
    if (host->wildSupported) setting(ui, c, ix, &y, iw, SETTING_WILD, "WILD ENCOUNTERS", on_off(!st->wildOff));
    else muted_row(c, ix, &y, iw, "WILD ENCOUNTERS", "N/A");
    if (host->fieldSupported) {
        if (host->hasAutoRun) setting(ui, c, ix, &y, iw, SETTING_AUTO_RUN, "AUTO-RUN", on_off(st->autoRun));
        setting(ui, c, ix, &y, iw, SETTING_RUN_INDOORS, "RUN INDOORS", on_off(st->runIndoors));
    } else {
        if (host->hasAutoRun) muted_row(c, ix, &y, iw, "AUTO-RUN", "N/A");
        muted_row(c, ix, &y, iw, "RUN INDOORS", "N/A");
    }
    if (host->hasRomTweaks) {
        if (host->instantTextFits) setting(ui, c, ix, &y, iw, SETTING_INSTANT_TEXT, "INSTANT TEXT", on_off(st->instantText));
        else muted_row(c, ix, &y, iw, "INSTANT TEXT", "N/A");
        if (host->reusableTmsBuiltIn) muted_row(c, ix, &y, iw, "REUSABLE TMS", "ON");
        else if (host->reusableTmsFits) setting(ui, c, ix, &y, iw, SETTING_REUSABLE_TMS, "REUSABLE TMS", on_off(st->reusableTms));
        else muted_row(c, ix, &y, iw, "REUSABLE TMS", "N/A");
    }
    if (host->hasTradeEvos) {
        if (host->tradeEvoFits) setting(ui, c, ix, &y, iw, SETTING_TRADE_EVOS, "TRADE EVOS", on_off(st->tradeEvolutions));
        else muted_row(c, ix, &y, iw, "TRADE EVOS", "N/A");
    }
    if (host->hasShinyOdds) {
        if (host->shinyOddsFits) setting(ui, c, ix, &y, iw, SETTING_SHINY_ODDS, "SHINY ODDS", PD_SHINY_LABELS[st->shinyOdds]);
        else muted_row(c, ix, &y, iw, "SHINY ODDS", "N/A");
    }
    setting(ui, c, ix, &y, iw, SETTING_LEVEL_CAP, "LEVEL CAP", on_off(st->levelCap));
    if (host->hasCheats) setting(ui, c, ix, &y, iw, SETTING_CHEATS, "CHEATS", on_off(st->cheats));

    group(c, ix, &y, "SCREEN");
    ui->pixelOff = host->expandGame && st->aspect != PD_ASPECT_ORIGINAL;
    if (host->hasExpand) {
        if (host->expandGame) {
            setting(ui, c, ix, &y, iw, SETTING_ASPECT, "ASPECT", pd_aspect_name(st->aspect));
            if (st->aspect == PD_ASPECT_ZOOM_OUT)
                setting(ui, c, ix, &y, iw, SETTING_AUTO_ZOOM, "AUTO ZOOM IN", on_off(st->autoZoom));
        } else {
            muted_row(c, ix, &y, iw, "ASPECT", "N/A");
        }
    }
    if (host->hasHud) setting(ui, c, ix, &y, iw, SETTING_HUD, "HUD", on_off(st->hud));
    page_row(ui, c, ix, &y, iw, SETTING_PAGE_DISPLAY, "DISPLAY OPTIONS");
    if (host->hasCompanionView)
        setting(ui, c, ix, &y, iw, SETTING_VIEW, "COMPANION VIEW", pd_companion_view_name(st->companionView));
    const char* themeName = pd_themes[0].label;
    for (int i = 0; i < pd_theme_count; i++)
        if (pd_themes[i].id == st->theme) themeName = pd_themes[i].label;
    setting(ui, c, ix, &y, iw, SETTING_THEME, "THEME", themeName);
    setting(ui, c, ix, &y, iw, SETTING_DARK, "DARK MODE", on_off(st->darkMode));
    if (host->hasPerf) setting(ui, c, ix, &y, iw, SETTING_PERF, "FPS / CPU", on_off(st->showPerf));

    group(c, ix, &y, "COMPANION");
    setting(ui, c, ix, &y, iw, SETTING_HINTS, "BATTLE HINTS", on_off(!st->hideHints));
    setting(ui, c, ix, &y, iw, SETTING_ONE_TAP, "ONE-TAP MOVES", on_off(st->oneTapMoves));
    snprintf(buf, sizeof(buf), "%d TABS", ui_bar_count(ui));
    setting(ui, c, ix, &y, iw, SETTING_TAB_BAR, "TAB BAR", buf);

    if (host->hasButtonLayout || host->hasButtonRows || host->hasQuickMenu || host->hasRumble) {
        group(c, ix, &y, "CONTROLS");
        if (host->hasButtonLayout) page_row(ui, c, ix, &y, iw, SETTING_PAGE_BUTTONS, "GAME BUTTONS");
        if (host->hasButtonRows) page_row(ui, c, ix, &y, iw, SETTING_PAGE_HOTKEYS, "HOTKEYS");
        if (host->hasQuickMenu) page_row(ui, c, ix, &y, iw, SETTING_PAGE_QUICK, "QUICK MENU");
        if (host->hasRumble) setting(ui, c, ix, &y, iw, SETTING_RUMBLE, "RUMBLE", on_off(st->rumble));
    }
    if (host->hasUpdateCheck || host->hasResume || host->hasPerGame || host->hasSaver) {
        group(c, ix, &y, "APP");
        if (host->hasSaver) {
            static const char* const SAVER[PD_SAVER_COUNT] = {"AUTO", "ON", "OFF"};
            int v = st->batterySaver >= 0 && st->batterySaver < PD_SAVER_COUNT ? st->batterySaver : 0;
            setting(ui, c, ix, &y, iw, SETTING_SAVER, "BATTERY SAVER",
                    v == PD_SAVER_AUTO && host->saving ? "AUTO, SAVING" : SAVER[v]);
        }
        if (host->hasPerGame)
            setting(ui, c, ix, &y, iw, SETTING_PER_GAME, "THIS GAME ONLY", on_off(host->perGame));
        if (host->hasResume) setting(ui, c, ix, &y, iw, SETTING_RESUME, "RESUME GAMES", on_off(st->resumeGames));
        if (host->hasUpdateCheck)
            setting(ui, c, ix, &y, iw, SETTING_UPDATES, "CHECK FOR UPDATES", on_off(st->updateCheck));
    }

    y += 3;
    pd_fill(c, ix, y, iw, 1, PD_DIVIDER);
    y += 5;
    int bw = (iw - 6) / 2;
    if (y >= listTop && y + 22 <= listBottom) {
        ui_button(ui, c, ix, y, bw, 22, "CLOSE GAME", HIT_SETTING + SETTING_LEAVE);
        ui_button(ui, c, ix + bw + 6, y, bw, 22, "RESTART GAME", HIT_SETTING + SETTING_RESTART);
    } else if (!host->frontendOwnsPlayback) {
        pd_title_box(c, ix, y, bw, 22, PD_TITLE_FILL);
        pd_title_box(c, ix + bw + 6, y, bw, 22, PD_TITLE_FILL);
    }
    y += 26;
    *yp = y;
}

void ui_settings_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                     const struct pd_host_info* host) {
    ui->settingsNavCount = 0;
    ui->layoutHost = host ? host->layouts : NULL;
    ui->layoutHostCount = host ? host->layoutCount : 0;
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, CONTENT_Y, w, TITLE_H, PD_TITLE_FILL);
    int ty = pd_text_y(CONTENT_Y, TITLE_H);
    static const char* const PAGE_TITLES[] = {"SETTINGS", "SPEED OPTIONS", "DISPLAY OPTIONS", "GAME BUTTONS", "HOTKEYS",
                                              "QUICK MENU"};
    const bool full = host && host->fullSettings;
    if (!full || !host->androidMenus || ui->settingsPage < 0 || ui->settingsPage > PAGE_QUICK) ui->settingsPage = PAGE_HOME;
    const char* pageTitle = ui->optionSource ? ui->optionSource->title : PAGE_TITLES[ui->settingsPage];
    pd_text(c, x + f + 6, ty, pageTitle, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    int right = x + w - f - 6;
    // The TRAINER CARD opens from here (the list below is full): a grey chip at
    // the title window's right, white while pressed, like the tab bar's.
    if (ui->settingsPage != PAGE_HOME) {
        // A sub-page: its title alone (BACK returns to SETTINGS).
    } else if (host && host->batteryPct >= 0 && host->fullSettings) {
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", host->batteryPct);
        right -= pd_text_width(pct);
        pd_text(c, right, ty, pct, PD_TITLE_TEXT, PD_TITLE_SHADOW);
        right -= 26;
        pd_fill(c, right + 2, ty + 6, 18, 8, PD_TITLE_TEXT);
        pd_fill(c, right + 3, ty + 7, 16, 6, PD_TITLE_FILL);
        pd_fill(c, right + 4, ty + 8, 14 * host->batteryPct / 100, 4, PD_TITLE_TEXT);
        pd_fill(c, right + 20, ty + 8, 2, 4, PD_TITLE_TEXT);
        right -= 2;
    } else if (g->cfg && g->cfg->cardStyle && g->rom) {
        const char* label = "TRAINER CARD";
        int bw = pd_text_width(label) + 12, bh = TITLE_H - 6;
        int bx = x + w - f - 2 - bw, by = CONTENT_Y + 3;
        pd_title_box(c, bx, by, bw, bh, ui->pressedId == HIT_OPEN_CARD ? PD_TITLE_FILL : PD_LIST_FILL);
        pd_text(c, bx + 6, pd_text_y(by, bh), label, PD_TITLE_TEXT, PD_TITLE_SHADOW);
        ui_add_hit(ui, bx, by, bw, bh, HIT_OPEN_CARD);
        settings_nav_row(ui, HIT_OPEN_CARD, 0);
        if (ui->shoulderTabs && ui->overlay == PD_OVERLAY_NONE && ui->settingsNavId == HIT_OPEN_CARD)
            settings_outline(c, bx, by, bw, bh);
        right = bx - 8;
    }
    if (g->title && ui->settingsPage == PAGE_HOME) {
        int tx = x + f + 6 + pd_text_width("SETTINGS") + 12;
        int tw = pd_text_width(g->title);
        if (tw > right - tx) tw = right - tx;
        pd_text_fit(c, right - tw, ty, tw, g->title, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    }

    int lh = c->h - LIST_Y - MARGIN;
    int lf = pd_list_box(c, x, LIST_Y, w, lh);
    // The bar takes its column off the rows while there's more than fits.
    const int infoCount = ui->optionSource ? ui->optionSource->infoCount : 0;
    const int separator = infoCount ? 4 : 0;
    const int totalOptions = ui->optionSource ? ui->optionSource->count * ROW_H + separator : lastTotal;
    bool hasBar = totalOptions > lh - 2 * lf;
    int ix = x + lf + 4, iw = w - 2 * (lf + 4) - (hasBar ? SCROLL_BAR_W + 2 : 0);
    // The rows scroll where they don't all fit (a drag, or the D-pad past the last one shown).
    listTop = LIST_Y + lf;
    listBottom = LIST_Y + lh - lf;
    ui->settingsNavHeight = listBottom - listTop;
    ui_add_scroll_hit(ui, x, listTop, w, listBottom - listTop, HIT_SETTINGS_LIST);
    if (ui->optionSource) {
        const int n = ui->optionSource->count, view = listBottom - listTop;
        const int selected = ui->optionSelected;
        if (selected >= infoCount && selected < n)
            ui->settingsScroll = ui_settings_follow(ui->settingsScroll,
                selected == infoCount ? 0 : selected * ROW_H + separator, view);
        ui->settingsScroll = ui_clamp_scroll(ui->settingsScroll, totalOptions, view);
        pd_clip(c, x + lf, listTop, w - 2 * lf, view);
        if (separator) pd_fill(c, ix, listTop + infoCount * ROW_H - ui->settingsScroll + 1, iw, 1, PD_DIVIDER);
        for (int i = 0; i < n; i++) {
            const int ry = listTop + i * ROW_H + (i >= infoCount ? separator : 0) - ui->settingsScroll;
            if (ry + ROW_H <= listTop || ry >= listBottom) continue;
            const struct pd_option_row* row = &ui->optionSource->rows[i];
            ui_row(c, ix, ry, iw, row->label, row->value, false);
            if (i == selected) settings_outline(c, ix - 4, ry, iw + 8, ROW_H);
        }
        pd_unclip(c);
        if (hasBar)
            ui_scroll_rail(ui,c,x+w-lf-SCROLL_BAR_W-2,listTop+2,SCROLL_BAR_W,view-4,ui->settingsScroll,totalOptions,view);
        return;
    }
    pd_clip(c, x + lf, listTop, w - 2 * lf, listBottom - listTop);
    if (lastTotal >= 0) ui->settingsScroll = ui_clamp_scroll(ui->settingsScroll, lastTotal, listBottom - listTop);
    int y = listTop - ui->settingsScroll;
    char buf[40];
    if (full && host->androidMenus) {
        full_rows(ui, c, g, s, host, ix, &y, iw);
        goto done;
    }
    if (s->valid) {
        const char* place = pd_mapsec_name(g, s->mapsec);
        ui_row(c, ix, y, iw, "PLACE", *place ? place : "-", false);
        y += ROW_H;
        pd_ui_format_money(s->money, buf, sizeof(buf));
        ui_row(c, ix, y, iw, "MONEY", buf, false);
        y += ROW_H;
    }
    for (int i = 0; host && i < host->count && i < PD_UI_HOST_LINES; i++) {
        ui_row(c, ix, y, iw, host->labels[i], host->values[i], false);
        y += ROW_H;
    }
    pd_fill(c, ix, y + 1, iw, 1, PD_DIVIDER);
    y += 4;

    struct pd_settings* st = ui->settings;
    if (full) {
        group(c, ix, &y, "FF / SPEED");
        setting(ui, c, ix, &y, iw, SETTING_FF_TOGGLE, "FF", host->ffOn ? "ON" : "OFF");
    }
    if (st->ffSpeed) snprintf(buf, sizeof(buf), "%dx", st->ffSpeed);
    else snprintf(buf, sizeof(buf), "MAX");
    if (full || !host || !host->frontendOwnsPlayback) setting(ui, c, ix, &y, iw, SETTING_FF, full ? "FF SPEED" : "FAST-FORWARD", buf);
    if (full) {
        setting(ui, c, ix, &y, iw, SETTING_FF_MODE, "FF MODE", st->ffNormal ? "NORMAL" : "SMART");
        setting(ui, c, ix, &y, iw, SETTING_GAME_SPEED, "GAME SPEED", PD_GAME_SPEEDS[st->gameSpeed]);
        group(c, ix, &y, "SOUND");
        snprintf(buf, sizeof(buf), "%d%%", st->gameVolume);
        setting(ui, c, ix, &y, iw, SETTING_VOLUME, "GAME VOLUME", buf);
        setting(ui, c, ix, &y, iw, SETTING_MUTE, "MUTE GAME", st->gameMuted ? "ON" : "OFF");
        group(c, ix, &y, "GAMEPLAY");
        if (host->wildSupported) setting(ui, c, ix, &y, iw, SETTING_WILD, "WILD ENCOUNTERS", st->wildOff ? "OFF" : "ON");
        else muted_row(c, ix, &y, iw, "WILD ENCOUNTERS", "N/A");
        if (host->fieldSupported) {
            setting(ui, c, ix, &y, iw, SETTING_AUTO_RUN, "AUTO-RUN", st->autoRun ? "ON" : "OFF");
            setting(ui, c, ix, &y, iw, SETTING_RUN_INDOORS, "RUN INDOORS", st->runIndoors ? "ON" : "OFF");
        } else {
            muted_row(c, ix, &y, iw, "AUTO-RUN", "N/A");
            muted_row(c, ix, &y, iw, "RUN INDOORS", "N/A");
        }
        if (host->hasRomTweaks) {
            if (host->instantTextFits) setting(ui, c, ix, &y, iw, SETTING_INSTANT_TEXT, "INSTANT TEXT", st->instantText ? "ON" : "OFF");
            else muted_row(c, ix, &y, iw, "INSTANT TEXT", "N/A");
            if (host->reusableTmsBuiltIn) muted_row(c, ix, &y, iw, "REUSABLE TMS", "ON");
            else if (host->reusableTmsFits) setting(ui, c, ix, &y, iw, SETTING_REUSABLE_TMS, "REUSABLE TMS", st->reusableTms ? "ON" : "OFF");
            else muted_row(c, ix, &y, iw, "REUSABLE TMS", "N/A");
        }
        if (host->hasTradeEvos) {
            if (host->tradeEvoFits) setting(ui, c, ix, &y, iw, SETTING_TRADE_EVOS, "TRADE EVOS", st->tradeEvolutions ? "ON" : "OFF");
            else muted_row(c, ix, &y, iw, "TRADE EVOS", "N/A");
        }
        if (host->hasShinyOdds) {
            if (host->shinyOddsFits) setting(ui, c, ix, &y, iw, SETTING_SHINY_ODDS, "SHINY ODDS", PD_SHINY_LABELS[st->shinyOdds]);
            else muted_row(c, ix, &y, iw, "SHINY ODDS", "N/A");
        }
        setting(ui, c, ix, &y, iw, SETTING_LEVEL_CAP, "LEVEL CAP", st->levelCap ? "ON" : "OFF");
        if (host->hasCheats) setting(ui, c, ix, &y, iw, SETTING_CHEATS, "CHEATS", st->cheats ? "ON" : "OFF");
        else muted_row(c, ix, &y, iw, "CHEATS", "N/A");
        group(c, ix, &y, "SCREEN");
    }
    // PIXEL 1x is ORIGINAL's: EXPAND that wide can't keep up, so there it reads as the next size (pd_session_screen).
    ui->pixelOff = host && host->expandGame && st->aspect != PD_ASPECT_ORIGINAL;
    if (!host || !host->hideScreenMode)
        setting(ui, c, ix, &y, iw, SETTING_SCREEN, "SCREEN",
                pd_screen_mode_name(ui->pixelOff && st->screenMode == PD_SCREEN_PIXEL ? PD_SCREEN_SHARP_125 : st->screenMode));
    if (host && host->hasExpand) {
        if (host->expandGame) {
            setting(ui, c, ix, &y, iw, SETTING_ASPECT, "ASPECT", pd_aspect_name(st->aspect));
            // Two values: flips in place (the app's rule for pick-lists is more than two). Only
            // ZOOM OUT has anything to step back from.
            if (st->aspect == PD_ASPECT_ZOOM_OUT)
                setting(ui, c, ix, &y, iw, SETTING_AUTO_ZOOM, "AUTO ZOOM IN", st->autoZoom ? "ON" : "OFF");
        } else {
            // This game's overworld can't be rebuilt: shown, not tappable.
            muted_row(c, ix, &y, iw, "ASPECT", "N/A");
        }
    }
    if (host && host->hasCompanionView)
        setting(ui, c, ix, &y, iw, SETTING_VIEW, "COMPANION VIEW", pd_companion_view_name(st->companionView));
    if (host && host->hasHud) setting(ui, c, ix, &y, iw, SETTING_HUD, "HUD", st->hud ? "ON" : "OFF");
    if (host && host->hasPerf) setting(ui, c, ix, &y, iw, SETTING_PERF, "FPS / CPU", st->showPerf ? "ON" : "OFF");
    if (host && host->hasUpdateCheck)
        setting(ui, c, ix, &y, iw, SETTING_UPDATES, "CHECK FOR UPDATES", st->updateCheck ? "ON" : "OFF");
    if (host && host->hasShaders) {
        // Three values each, stepped on a tap so each shows on the game at once (the app's SHADERS rows do the same).
        static const char* const SHADERS[PD_SHADERS] = {"OFF", "SHARP", "xBRZ"};
        static const char* const COLORS[PD_COLORS] = {"ORIGINAL", "HANDHELD", "OLED"};
        setting(ui, c, ix, &y, iw, SETTING_SHADER, "SHADER", SHADERS[st->shader]);
        setting(ui, c, ix, &y, iw, SETTING_COLORS, "COLORS", COLORS[st->colors]);
        setting(ui, c, ix, &y, iw, SETTING_DIM, "DIM COMPANION", st->dimCompanion ? "ON" : "OFF");
    }
    if (host && host->hasFullscreen)
        setting(ui, c, ix, &y, iw, SETTING_FULLSCREEN, "FULLSCREEN", st->fullscreen == 1 ? "ON" : "OFF");
    if (host && host->hasOrientation) {
        static const char* const names[PD_ORIENTATIONS] = {"LANDSCAPE", "PORTRAIT", "LANDSCAPE FLIPPED",
                                                           "PORTRAIT FLIPPED"};
        setting(ui, c, ix, &y, iw, SETTING_ORIENTATION, "ORIENTATION",
                names[st->orientation >= 0 && st->orientation < PD_ORIENTATIONS ? st->orientation : 0]);
    }
    if (host && host->hasGameTouch)
        setting(ui, c, ix, &y, iw, SETTING_GAME_TOUCH, "GAME TOUCH",
                st->gameTouch == PD_GAME_TOUCH_OFF ? "OFF" : "DS SCREEN ONLY");
    if (host && host->hasButtonLayout)
        setting(ui, c, ix, &y, iw, SETTING_BUTTON_LAYOUT, "BUTTON LAYOUT",
                st->buttonLayout == PD_LAYOUT_XBOX ? "XBOX" : "NINTENDO");
    if (host && host->hasSavesBeside)
        setting(ui, c, ix, &y, iw, SETTING_SAVES_BESIDE, "SAVES BESIDE GAMES", st->savesBeside ? "ON" : "OFF");
    if (host && host->hasDisplayFilter)
        setting(ui, c, ix, &y, iw, SETTING_FILTER, "FILTER", st->lcdFilter ? "LCD" : "OFF");
    if (host && host->hasQuickMenu)
        setting(ui, c, ix, &y, iw, SETTING_QUICK_MENU, "QUICK MENU", st->quickGrid ? "GRID" : "RING");
    if (host && host->hasButtonRows) {
        static const char* const NAMES[PD_BUTTONS] = {"X", "Y", "L2", "R2", "NONE"};
        setting(ui, c, ix, &y, iw, SETTING_QUICK_BTN, "QUICK MENU BUTTON", NAMES[st->quickBtn]);
        setting(ui, c, ix, &y, iw, SETTING_COMPANION_BTN, "COMPANION BUTTON", NAMES[st->companionBtn]);
        if (host->hasHud) setting(ui, c, ix, &y, iw, SETTING_HUD_BTN, "HUD CURSOR BUTTON", NAMES[st->hudBtn]);
    }
    if (full) {
        setting(ui, c, ix, &y, iw, SETTING_DARK, "DARK MODE", st->darkMode ? "ON" : "OFF");
        const char* themeName = pd_themes[0].label;
        for (int i = 0; i < pd_theme_count; i++)
            if (pd_themes[i].id == st->theme) themeName = pd_themes[i].label;
        setting(ui, c, ix, &y, iw, SETTING_THEME, "THEME", themeName);
        group(c, ix, &y, "COMPANION");
        setting(ui, c, ix, &y, iw, SETTING_HINTS, "BATTLE HINTS", st->hideHints ? "OFF" : "ON");
        setting(ui, c, ix, &y, iw, SETTING_ONE_TAP, "ONE-TAP MOVES", st->oneTapMoves ? "ON" : "OFF");
    }
    snprintf(buf, sizeof(buf), "%d TABS", ui_bar_count(ui));
    setting(ui, c, ix, &y, iw, SETTING_TAB_BAR, "TAB BAR", buf);
    if (full) {
        y += 3;
        pd_fill(c, ix, y, iw, 1, PD_DIVIDER);
        y += 5;
        // CLOSE GAME / RESTART GAME, under a line.
        int bw = (iw - 6) / 2;
        if (y >= listTop && y + 22 <= listBottom) {
            ui_button(ui, c, ix, y, bw, 22, "CLOSE GAME", HIT_SETTING + SETTING_LEAVE);
            ui_button(ui, c, ix + bw + 6, y, bw, 22, "RESTART GAME", HIT_SETTING + SETTING_RESTART);
        } else if (!host || !host->frontendOwnsPlayback) {
            pd_title_box(c, ix, y, bw, 22, PD_TITLE_FILL);
            pd_title_box(c, ix + bw + 6, y, bw, 22, PD_TITLE_FILL);
        }
        y += 26;
    } else if (!host || !host->frontendOwnsPlayback) {
        setting(ui, c, ix, &y, iw, SETTING_SAVE_STATE, "SAVE STATE", NULL);
        if (host && host->hasState) {
            setting(ui, c, ix, &y, iw, SETTING_LOAD_STATE, "LOAD STATE", host->stateWhen);
        } else {
            // Nothing to load: shown, not tappable.
            muted_row(c, ix, &y, iw, "LOAD STATE", "none yet");
        }
        setting(ui, c, ix, &y, iw, SETTING_LEAVE, "LEAVE GAME", NULL);
    }
done:
    pd_unclip(c);
    int total = y + ui->settingsScroll - listTop, view = listBottom - listTop;
    lastTotal = total;
    ui->settingsScroll = ui_clamp_scroll(ui->settingsScroll, total, view);
    if (total > view) {
        // Its own column: a thumb to drag, or a tap on the rail to jump there.
        int bx = x + w - lf - SCROLL_BAR_W - 2;
        ui_scroll_rail(ui, c, bx, listTop + 2, SCROLL_BAR_W, view - 4, ui->settingsScroll, total, view);
    }
}

// --- overlays ---

static void title_window(struct pd_canvas* c, int y, const char* title) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, y, w, TITLE_H, PD_TITLE_FILL);
    pd_text(c, x + f + 6, pd_text_y(y, TITLE_H), title, PD_TITLE_TEXT, PD_TITLE_SHADOW);
}

static void pick_list(struct pd_ui* ui, struct pd_canvas* c, const char* title, const char* const* options, int n,
                      int current) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int y = CONTENT_Y + 8;
    title_window(c, y, title);
    y += TITLE_H + 4;
    int lf = pd_list_box(c, x, y, w, n * ROW_H + 2 * 7 + 4);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    int ry = y + lf + 2;
    ui->startId = HIT_OPTION + current; // the D-pad starts on the current choice
    for (int i = 0; i < n; i++) {
        bool on = i == current || ui->pressedId == HIT_OPTION + i;
        if (on) pd_fill(c, ix - 4, ry, iw + 8, ROW_H, PD_ROW_SELECTED);
        pd_text(c, ix + 10, pd_text_y(ry, ROW_H), options[i], on ? PD_VALUE : PD_LABEL,
                on ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
        if (i == current) pd_triangle(c, ix, ry + 4, 5, PD_RIGHT, PD_VALUE);
        ui_add_hit(ui, ix - 4, ry, iw + 8, ROW_H, HIT_OPTION + i);
        ry += ROW_H;
    }
}

// A page of ON / OFF rows (TAB BAR's, HUD's), each flipped by A / a tap, or a plain list (MORE's: values NULL);
// a grey note under them. B / a tap beside it closes it.
static void toggle_page(struct pd_ui* ui, struct pd_canvas* c, const char* title, const char* const* labels,
                        const bool* on, const bool* muted, int n, const char* note, const char* const* values) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int y = CONTENT_Y + 4;
    title_window(c, y, title);
    y += TITLE_H + 4;
    int lf = pd_list_box(c, x, y, w, n * ROW_H + 2 * 7 + 4 + (note ? 16 : 0));
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    int ry = y + lf + 2;
    if (ui->startId < 0) ui->startId = HIT_OPTION;
    for (int i = 0; i < n; i++) {
        bool sel = ui->pressedId == HIT_OPTION + i;
        if (on) {
            // A value of its own (HUD's LAYOUT / LOOK / SIZE), else ON / OFF.
            const char* v = values && values[i] ? values[i] : on[i] ? "ON" : "OFF";
            ui_row(c, ix, ry, iw, labels[i], v, sel);
            if (muted && muted[i]) {
                // Its value while the whole thing is off: shown, greyed.
                pd_fill(c, ix + iw * 42 / 100, ry + 1, iw - iw * 42 / 100, ROW_H - 2, sel ? PD_ROW_SELECTED : PD_LIST_FILL);
                pd_text(c, ix + iw * 42 / 100, pd_text_y(ry, ROW_H), v, PD_MUTED, PD_MUTED_SHADOW);
            }
        } else {
            if (sel) pd_fill(c, ix - 4, ry, iw + 8, ROW_H, PD_ROW_SELECTED);
            pd_text(c, ix + 10, pd_text_y(ry, ROW_H), labels[i], sel ? PD_VALUE : PD_LABEL,
                    sel ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
        }
        ui_add_hit(ui, ix - 4, ry, iw + 8, ROW_H, HIT_OPTION + i);
        ry += ROW_H;
    }
    if (note) pd_text_fit(c, ix, ry + 2, iw, note, PD_MUTED, PD_MUTED_SHADOW);
}

static void tab_bar_page(struct pd_ui* ui, struct pd_canvas* c) {
    const char* labels[UI_BAR_CHOICES];
    bool on[UI_BAR_CHOICES];
    enum pd_tab tabs[UI_BAR_CHOICES];
    const int n = ui_configurable_tabs(ui, tabs);
    for (int i = 0; i < n; i++) {
        labels[i] = ui_tab_label(tabs[i]);
        on[i] = (ui->settings->tabBar >> tabs[i]) & 1;
    }
    toggle_page(ui, c, "TAB BAR", labels, on, NULL, n, "At most 5 on the bar; the rest open from MORE.", NULL);
}

// FieldHud.kt's fieldHudRows: the switch, LAYOUT / LOOK / SIZE (a press cycles them), then which parts show.
static void hud_page(struct pd_ui* ui, struct pd_canvas* c) {
    enum { N = 4 + PD_HUD_PARTS };
    const struct pd_settings* s = ui->settings;
    const char* labels[N] = { "HUD", "LAYOUT", "LOOK", "SIZE" };
    const char* values[N] = { NULL, pd_hud_layout_name(s->hudLayout), pd_hud_look_name(s->hudLook),
                              pd_hud_size_name(s->hudSize) };
    bool on[N] = { s->hud, true, true, true }, muted[N] = { false, !s->hud, !s->hud, !s->hud };
    for (int i = 0; i < PD_HUD_PARTS; i++) {
        labels[4 + i] = pd_hud_part_name(i);
        on[4 + i] = !((s->hudHidden >> i) & 1);
        muted[4 + i] = !s->hud;
    }
    toggle_page(ui, c, "HUD", labels, on, muted, N, "Shown over the overworld, off in menus and battles.", values);
}

// MoreScreen: every tab this game has off the bar as an app tile, a line, and the way to SETTINGS' page for it.
void ui_more_page(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g) {
    enum pd_tab more[UI_BAR_CHOICES];
    int n = ui_more_tabs(ui, g, more);
    int x = MARGIN, w = c->w - 2 * MARGIN;
    title_window(c, CONTENT_Y, "MORE");
    int y = CONTENT_Y + TITLE_H + 3, h = c->h - y - 2;
    int f = pd_list_box(c, x, y, w, h);
    int ix = x + f + 4, iy = y + f + 4, iw = w - 2 * (f + 4);
    int tile = 52, gap = 6;
    for (int i = 0; i < n; i++) {
        int tx = ix + (i % 5) * (tile + gap), ty = iy + (i / 5) * (tile + gap);
        pd_title_box(c, tx, ty, tile, tile, ui->pressedId == HIT_OPTION + i ? PD_LIST_FILL : 0xF0F0F0);
        const struct pd_art* art = ui_tab_art(more[i]);
        if (art) ui_art(c, art, tx + (tile - 32) / 2, ty + 4, 2);
        const char* label = ui_tab_label(more[i]);
        pd_text(c, tx + (tile - pd_text_width(label)) / 2, ty + tile - 16, label, PD_TITLE_TEXT, PD_TITLE_SHADOW);
        ui_add_hit(ui, tx, ty, tile, tile, HIT_OPTION + i);
    }
    int by = y + h - f - 23;
    pd_text_wrap(c, ix, by - 33, iw, 15, 2, "Tabs that aren't on the tab bar are here. Pick the bar's tabs in TAB BAR.",
                 PD_LABEL, PD_LABEL_SHADOW);
    ui_button(ui, c, x + w - f - 86, by, 80, 20, "TAB BAR", HIT_MORE_TAB_BAR);
}

static void confirm(struct pd_ui* ui, struct pd_canvas* c, const char* title, const char* text, const char* yes) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int y = CONTENT_Y + 8;
    title_window(c, y, title);
    y += TITLE_H + 4;
    int h = 3 * 16 + 2 * 7 + 6;
    int lf = pd_list_box(c, x, y, w, h);
    pd_text_wrap(c, x + lf + 6, y + lf + 2, w - 2 * (lf + 6), 16, 3, text, PD_LABEL, PD_LABEL_SHADOW);
    y += h + 6;
    int bw = 96;
    ui->startId = HIT_NO; // cancel first
    ui_button(ui, c, c->w / 2 - bw - 6, y, bw, 24, yes, HIT_YES);
    ui_button(ui, c, c->w / 2 + 6, y, bw, 24, "CANCEL", HIT_NO);
}

void ui_overlay(struct pd_ui* ui, struct pd_canvas* c) {
    if (ui->overlay == PD_OVERLAY_NONE) return;
    pd_dim(c, 0xE6);
    // Swallows taps beside the windows (they close the overlay).
    ui_add_hit(ui, 0, 0, c->w, c->h, HIT_SCRIM);
    static const char* const FF[PD_FF_CHOICES] = { "2x", "3x", "4x", "5x", "6x", "8x", "10x", "MAX" };
    switch (ui->overlay) {
    case PD_OVERLAY_PICK_SCREEN:
    {
        int skip = ui->pixelOff ? 1 : 0, mode = ui->settings->screenMode - skip;
        pick_list(ui, c, "SCREEN", SCREEN_NAMES + skip, PD_SCREEN_MODES - skip, mode < 0 ? 0 : mode);
        break;
    }
    case PD_OVERLAY_PICK_ASPECT:
        pick_list(ui, c, "ASPECT", ASPECT_NAMES, PD_ASPECTS, ui->settings->aspect);
        break;
    case PD_OVERLAY_PICK_FF:
        pick_list(ui, c, "FAST-FORWARD", FF, PD_FF_CHOICES, pd_ff_index(ui->settings->ffSpeed));
        break;
    case PD_OVERLAY_PICK_SPEED:
        pick_list(ui, c, "GAME SPEED", PD_GAME_SPEEDS, PD_GAME_SPEED_COUNT, ui->settings->gameSpeed);
        break;
    case PD_OVERLAY_PICK_SHINY:
        pick_list(ui, c, "SHINY ODDS", PD_SHINY_LABELS, PD_SHINY_CHOICES, ui->settings->shinyOdds);
        break;
    case PD_OVERLAY_PICK_SCENE:
        pick_list(ui, c, SCENE_NAMES[ui->sceneRow], PD_SCENE_SPEEDS, PD_SCENE_SPEED_COUNT, ui->settings->sceneSpeed[ui->sceneRow]);
        break;
    case PD_OVERLAY_PICK_BOX:
        ui_boxes_overlay(ui, c);
        break;
    case PD_OVERLAY_PICK_THEME: {
        // Limited-time themes are offered only around their holidays on the app: not here yet.
        static const char* names[16];
        int n = 0, current = 0;
        for (int i = 0; i < pd_theme_count && n < 16; i++) {
            if (pd_themes[i].seasonal) continue;
            if (pd_themes[i].id == ui->settings->theme) current = n;
            names[n++] = pd_themes[i].label;
        }
        pick_list(ui, c, "THEME", names, n, current);
        break;
    }
    case PD_OVERLAY_PICK_VIEW: {
        static const char* names[PD_COMPANION_VIEWS];
        for (int i = 0; i < PD_COMPANION_VIEWS; i++) names[i] = pd_companion_view_name(i);
        pick_list(ui, c, "COMPANION VIEW", names, PD_COMPANION_VIEWS, ui->settings->companionView);
        break;
    }
    case PD_OVERLAY_LAYOUT:
        if (ui->layoutHost && ui->layoutHostCount > 0) {
            static int tiles[PD_LAYOUT_OPTIONS][4];
            int n = ui->layoutHostCount > PD_LAYOUT_OPTIONS ? PD_LAYOUT_OPTIONS : ui->layoutHostCount;
            int sel = ui->pressedId >= HIT_OPTION && ui->pressedId < HIT_OPTION + n ? ui->pressedId - HIT_OPTION : -1;
            int focus = ui->focusId >= HIT_OPTION && ui->focusId < HIT_OPTION + n ? ui->focusId - HIT_OPTION
                : ui->settings->companionView;
            pd_layout_picker_draw(c, "COMPANION VIEW", ui->layoutHost, n, ui->settings->companionView, focus, sel,
                                  &ui->layoutScroll, tiles);
            ui->startId = HIT_OPTION + ui->settings->companionView;
            for (int i = 0; i < n; i++)
                if (tiles[i][2]) ui_add_hit(ui, tiles[i][0], tiles[i][1], tiles[i][2], tiles[i][3], HIT_OPTION + i);
        }
        break;
    case PD_OVERLAY_PICK_VOLUME:
        pick_list(ui, c, "GAME VOLUME", VOLUMES, 5, ui->settings->gameVolume / 20 - 1);
        break;
    case PD_OVERLAY_TAB_BAR:
        tab_bar_page(ui, c);
        break;
    case PD_OVERLAY_HUD:
        hud_page(ui, c);
        break;
    case PD_OVERLAY_CONFIRM_SAVE:
        confirm(ui, c, "SAVE STATE",
                "Save a state of the game as it is now? It replaces the last one. The save file isn't touched.",
                "SAVE");
        break;
    case PD_OVERLAY_CONFIRM_LOAD:
        confirm(ui, c, "LOAD STATE",
                "Go back to the saved state? Play since then is lost; the save file isn't touched.", "LOAD");
        break;
    case PD_OVERLAY_CONFIRM_LEAVE:
        confirm(ui, c, "CLOSE GAME",
                "Back to the game list? What the game saved stays saved; anything since is lost.", "CLOSE");
        break;
    case PD_OVERLAY_CONFIRM_RESTART:
        confirm(ui, c, "RESTART GAME", "Start the game again from its save? Anything since the last save is lost.", "RESTART");
        break;
    default:
        break;
    }
}

void pd_ui_options_view(struct pd_ui* ui,struct pd_canvas* c,const struct pd_game* g,const struct pd_snapshot* s){
    ui->hitCount=0;pd_backdrop(c);ui_settings_tab(ui,c,g,s,NULL);
}

void pd_ui_choices_view(struct pd_canvas* c,const char* title,const char* const* choices,int count,int selected){
    if(count<1 || count>8)return;
    struct pd_ui ui={0};ui.pressedId=-1;
    pd_dim(c,230);
    pick_list(&ui,c,title,choices,count,selected);
}
