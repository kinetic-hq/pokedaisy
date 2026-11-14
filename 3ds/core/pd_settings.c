#include "pd_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

const char* const pd_filter_names[PD_FILTERS] = {"OFF", "LCD", "LCD PAPER", "SCANLINES", "CRT", "SUBPIXEL"};
const char* const pd_grid_names[PD_GRIDS] = {"SOFT", "MEDIUM", "STRONG"};
bool pd_settings_expand_default = false;
bool pd_settings_dark_default = false;

void pd_settings_defaults(struct pd_settings* s) {
    // Everything not set below starts at 0 / off (SHINY ODDS NORMAL, SCENE SPEEDS SAME, INSTANT TEXT, REUSABLE TMS, DAY
    // AND NIGHT off...): a host's settings live on its stack, so nothing may be left as it was.
    memset(s, 0, sizeof(*s));
    s->screenMode = PD_SCREEN_SHARP;
    s->ffSpeed = PD_FF_MIN;
    s->guideNotice = 0;
    s->aspect = pd_settings_expand_default ? PD_ASPECT_EXPAND : PD_ASPECT_ORIGINAL;
    s->aspectPicked = false;
    s->autoZoom = true;
    s->lookAround = true;
    s->wildOff = false;
    s->tabBar = PD_TAB_BAR_DEFAULT;
    s->hud = true;
    s->hudHidden = 0;
    s->hudLayout = PD_HUD_CORNERS;
    s->hudLook = PD_HUD_GLASS;
    s->hudSize = PD_HUD_NORMAL;
    s->scaling = PD_SCALE_INTEGER;
    s->ffAudio = true;
    s->ffSteady = true;
    s->rewind = false;
    s->runIndoors = false;
    s->levelCap = false;
    s->resumeGames = true;
    s->rumble = false;
    s->batterySaver = PD_SAVER_AUTO;
    s->quickGrid = false;
    s->lcdFilter = false;
    s->filter = PD_FILTER_OFF;
    s->grid = 1;
    s->quickBtn = PD_BTN_Y;
    s->companionBtn = PD_BTN_X;
    s->hudBtn = PD_BTN_NONE;
    s->hideHints = false;
    s->oneTapMoves = false;
    s->foeIvs = false;
    s->battleInfo = true;
    s->battleViewInfo = false;
    s->battleFocus = false;
    s->gameSpeed = 0;
    s->gameVolume = 100;
    s->gameMuted = false;
    s->autoRun = false;
    s->darkMode = pd_settings_dark_default;
    s->ffNormal = false;
    s->cheats = false;
    s->theme = 8; // POKéDAISY
    s->companionView = PD_VIEW_BATTLE_WINGS;
    s->panelLocked = false;
    s->updateCheck = true;
    s->shader = 0;
    s->colors = 0;
    s->dimCompanion = false;
    s->reduceMotion = false;
    s->boxartAuto = true;
    s->savesBeside = true;
    s->fullscreen = -1;
    s->buttonLayout = PD_LAYOUT_NINTENDO;
    for (int i = 0; i < PD_HOTKEY_BUTTONS; i++) s->hotkeys[i] = 0;
    // The desktop host's defaults: L3 fast-forward on and off, the triggers zoom (a DS game keeps its own L2 / R2).
    s->hotkeys[PD_HOTKEY_L3] = PD_HOTKEY_DO_FF;
    s->hotkeys[PD_HOTKEY_L2] = PD_HOTKEY_DO_ZOOM_OUT;
    s->hotkeys[PD_HOTKEY_R2] = PD_HOTKEY_DO_ZOOM_IN;
    s->orientation = PD_ORIENT_LANDSCAPE;
    s->gameTouch = PD_GAME_TOUCH_DS_SCREEN;
    s->background = -1;
    s->brightness = 110;
    s->bezelStyle = -1;
    s->wingLook = 1;
    s->uiSize = 1;
    s->wingRoom = 3; // AUTO
    s->stretch = false;
    // linux/bezel.h's BEZEL_DEFAULT_SLOTS: LOCATION, CLOCK AND BATTERY, NEXT BOSS AND CAP, BADGES, PLAY TIME,
    // REPEL OR MONEY.
    static const int BEZEL_SLOTS[6] = {1, 2, 12, 6, 7, 10};
    for (int i = 0; i < 6; i++) s->bezelSlots[i] = BEZEL_SLOTS[i];
    s->barCount[0] = s->barCount[1] = s->barCount[2] = s->barCount[3] = -1;
    s->wingCount = -1;
    s->battleWingCount = -1;
    s->partyTypes = true;
}

// "a,b,c" into list (each 0 ... max - 1, the rest skipped); the count, -1 for an unreadable list.
static int int_list(const char* p, int* list, int cap, int max) {
    int n = 0;
    while (*p && n < cap) {
        if (*p < '0' || *p > '9') return -1;
        int v = atoi(p);
        bool dup = false;
        for (int k = 0; k < n; k++) dup |= list[k] == v;
        if (v < max && !dup) list[n++] = v;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    return n;
}

const int PD_GAME_SPEED_PCT[PD_GAME_SPEED_COUNT] = {100, 115, 125, 150, 175, 200, 250, 300, 400, 50, 25};

int pd_game_speed_index(int pct) {
    for (int i = 0; i < PD_GAME_SPEED_COUNT; i++)
        if (PD_GAME_SPEED_PCT[i] == pct) return i;
    return 0;
}

const int PD_FF_SPEEDS[PD_FF_CHOICES] = {2, 3, 4, 5, 6, 8, 10, 0};

int pd_ff_index(int speed) {
    for (int i = 0; i < PD_FF_CHOICES; i++)
        if (PD_FF_SPEEDS[i] == speed) return i;
    return 0;
}

static bool int_in(const char* v, int lo, int hi, int* out) {
    char* end;
    long n = strtol(v, &end, 10);
    if (end == v || n < lo || n > hi) return false;
    *out = (int) n;
    return true;
}

bool pd_settings_load(struct pd_settings* s, const char* path) {
    pd_settings_defaults(s);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[128];
    bool filterSeen = false;
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* value = eq + 1;
        if (!strcmp(key, "screen")) int_in(value, 0, PD_SCREEN_MODES - 1, &s->screenMode);
        else if (!strcmp(key, "ff")) {
            int n;
            if (int_in(value, 0, 10, &n) && PD_FF_SPEEDS[pd_ff_index(n)] == n) s->ffSpeed = n;
        }
        else if (!strcmp(key, "guide")) int_in(value, 0, 0x7FFFFFFF, &s->guideNotice);
        else if (!strcmp(key, "dark")) s->darkMode = atoi(value) != 0;
        else if (!strcmp(key, "ffnormal")) s->ffNormal = atoi(value) != 0;
        else if (!strcmp(key, "cheats")) s->cheats = atoi(value) != 0;
        else if (!strcmp(key, "theme")) int_in(value, 0, 99, &s->theme);
        else if (!strcmp(key, "batterysaver")) int_in(value, 0, PD_SAVER_COUNT - 1, &s->batterySaver);
        else if (!strcmp(key, "aspect")) int_in(value, 0, PD_ASPECTS - 1, &s->aspect);
        else if (!strcmp(key, "view")) int_in(value, 0, PD_COMPANION_VIEWS - 1, &s->companionView);
        else if (!strcmp(key, "panellock")) s->panelLocked = atoi(value) != 0;
        else if (!strcmp(key, "autozoom")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->autoZoom = on;
        } else if (!strcmp(key, "perf")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->showPerf = on;
        } else if (!strcmp(key, "wildoff")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->wildOff = on;
        } else if (!strcmp(key, "tabs")) {
            int bits;
            if (int_in(value, 1, (1 << PD_TAB_COUNT) - 1, &bits)) s->tabBar = bits;
        } else if (!strcmp(key, "hud")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->hud = on;
        } else if (!strcmp(key, "lcd")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->lcdFilter = on;
        } else if (!strcmp(key, "filter")) {
            filterSeen = int_in(value, 0, PD_FILTERS - 1, &s->filter);
        } else if (!strcmp(key, "grid")) {
            int_in(value, 0, PD_GRIDS - 1, &s->grid);
        } else if (!strcmp(key, "quickgrid")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->quickGrid = on;
        } else if (!strcmp(key, "hudhide")) {
            int bits;
            if (int_in(value, 0, (1 << PD_HUD_PARTS) - 1, &bits)) s->hudHidden = bits;
        } else if (!strcmp(key, "hudlayout")) int_in(value, 0, PD_HUD_LAYOUTS - 1, &s->hudLayout);
        else if (!strcmp(key, "hudlook")) int_in(value, 0, PD_HUD_LOOKS - 1, &s->hudLook);
        else if (!strcmp(key, "hudsize")) int_in(value, 0, PD_HUD_SIZES - 1, &s->hudSize);
        else if (!strcmp(key, "scaling")) int_in(value, 0, PD_SCALINGS - 1, &s->scaling);
        else if (!strcmp(key, "quickbtn")) int_in(value, 0, PD_BTN_L2 + 1, &s->quickBtn);
        else if (!strcmp(key, "companionbtn")) int_in(value, 0, PD_BTN_L2 + 1, &s->companionBtn);
        else if (!strcmp(key, "hudbtn")) int_in(value, 0, PD_BUTTONS - 1, &s->hudBtn);
        else if (!strcmp(key, "speed")) {
            // Before the small steps: an index into 1x, 1.5x, 2x, 3x, 4x, 0.5x, 0.25x.
            static const int OLD_PCT[] = {100, 150, 200, 300, 400, 50, 25};
            int n;
            if (int_in(value, 0, 6, &n)) s->gameSpeed = pd_game_speed_index(OLD_PCT[n]);
        } else if (!strcmp(key, "gamespeed")) {
            int n;
            if (int_in(value, 1, 1000, &n) && PD_GAME_SPEED_PCT[pd_game_speed_index(n)] == n) s->gameSpeed = pd_game_speed_index(n);
        }
        else if (!strcmp(key, "shinyodds")) int_in(value, 0, PD_SHINY_CHOICES - 1, &s->shinyOdds);
        else if (!strcmp(key, "battlespeed")) int_in(value, 0, PD_SCENE_SPEED_COUNT - 1, &s->sceneSpeed[PD_SCENE_BATTLE]);
        else if (!strcmp(key, "textspeed")) int_in(value, 0, PD_SCENE_SPEED_COUNT - 1, &s->sceneSpeed[PD_SCENE_TEXT]);
        else if (!strcmp(key, "menuspeed")) int_in(value, 0, PD_SCENE_SPEED_COUNT - 1, &s->sceneSpeed[PD_SCENE_MENU]);
        else if (!strcmp(key, "bikespeed")) int_in(value, 0, PD_SCENE_SPEED_COUNT - 1, &s->sceneSpeed[PD_SCENE_BIKE]);
        else if (!strcmp(key, "surfspeed")) int_in(value, 0, PD_SCENE_SPEED_COUNT - 1, &s->sceneSpeed[PD_SCENE_SURF]);
        else if (!strcmp(key, "volume")) int_in(value, 20, 100, &s->gameVolume);
        else if (!strcmp(key, "shader")) int_in(value, 0, PD_SHADERS - 1, &s->shader);
        else if (!strcmp(key, "colors")) int_in(value, 0, PD_COLORS - 1, &s->colors);
        else if (!strcmp(key, "fullscreen")) int_in(value, -1, 1, &s->fullscreen);
        else if (!strcmp(key, "buttons")) int_in(value, 0, PD_BUTTON_LAYOUTS - 1, &s->buttonLayout);
        else if (!strcmp(key, "orientation")) int_in(value, 0, PD_ORIENTATIONS - 1, &s->orientation);
        else if (!strcmp(key, "gametouch")) int_in(value, 0, PD_GAME_TOUCHES - 1, &s->gameTouch);
        else if (!strcmp(key, "background")) int_in(value, -1, 3, &s->background);
        else if (!strcmp(key, "winglook")) int_in(value, 0, 2, &s->wingLook);
        else if (!strcmp(key, "uisize")) int_in(value, 0, 2, &s->uiSize);
        else if (!strcmp(key, "wingroom")) {
            // 1.12 / 1.13's key: BALANCED was its default, so it becomes AUTO; a choice of the other two stays.
            int r;
            if (int_in(value, 0, 2, &r) && r != 1) s->wingRoom = r;
        } else if (!strcmp(key, "wingroom2")) int_in(value, 0, 3, &s->wingRoom);
        else if (!strcmp(key, "stretch")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->stretch = on;
        }
        else if (!strcmp(key, "brightness")) int_in(value, 80, 150, &s->brightness);
        else if (!strcmp(key, "bezel")) int_in(value, -1, 2, &s->bezelStyle);
        else if (!strcmp(key, "bezelslots")) {
            const char* p = value;
            for (int i = 0; i < 6 && *p; i++) {
                int v = atoi(p);
                if (v >= 0 && v <= 15) s->bezelSlots[i] = v;
                while (*p && *p != ',') p++;
                if (*p == ',') p++;
            }
            // The old defaults (LEAD POKEMON and NEXT BOSS below, where a filler on top said it again): the new ones.
            static const int OLD[6] = {1, 2, 5, 14, 12, 10}, NEW[6] = {1, 2, 12, 6, 7, 10};
            if (!memcmp(s->bezelSlots, OLD, sizeof(OLD))) memcpy(s->bezelSlots, NEW, sizeof(NEW));
        }
        else if (!strcmp(key, "bartop")) s->barCount[0] = int_list(value, s->barList[0], PD_BAR_LIST_MAX, 64);
        else if (!strcmp(key, "barbottom")) s->barCount[1] = int_list(value, s->barList[1], PD_BAR_LIST_MAX, 64);
        else if (!strcmp(key, "wing")) s->wingCount = int_list(value, s->wingList, PD_WING_LIST_MAX, PD_WING_SECTIONS);
    else if (!strcmp(key, "battlebartop")) s->barCount[2] = int_list(value, s->barList[2], PD_BAR_LIST_MAX, 64);
    else if (!strcmp(key, "battlebarbottom")) s->barCount[3] = int_list(value, s->barList[3], PD_BAR_LIST_MAX, 64);
    else if (!strcmp(key, "battlewing"))
        s->battleWingCount = int_list(value, s->battleWingList, PD_WING_LIST_MAX, PD_BATTLE_SECTIONS);
        else if (!strcmp(key, "partytypes")) {
            int on;
            if (int_in(value, 0, 1, &on)) s->partyTypes = on;
        }
        else if (!strcmp(key, "hotkeys")) {
            // One action a button, comma-separated; an unknown one is the button's usual use.
            const char* p = value;
            int count = 0;
            for (int i = 0; i < PD_HOTKEY_BUTTONS && *p && *p != '\n'; i++) {
                char* end;
                long n = strtol(p, &end, 10);
                if (end == p) break;
                s->hotkeys[i] = n >= 0 && n < PD_HOTKEY_ACTIONS ? (unsigned char) n : 0;
                count++;
                p = *end == ',' ? end + 1 : end;
            }
            // A file from before the triggers were on the list: L3 left on its usual use takes its new default.
            if (count == 8 && !s->hotkeys[PD_HOTKEY_L3]) s->hotkeys[PD_HOTKEY_L3] = PD_HOTKEY_DO_FF;
        }
        else {
            int on;
            bool* b = !strcmp(key, "ffaudio") ? &s->ffAudio : !strcmp(key, "ffsteady") ? &s->ffSteady : !strcmp(key, "rewind") ? &s->rewind
                    : !strcmp(key, "runindoors") ? &s->runIndoors : !strcmp(key, "nohints") ? &s->hideHints
                    : !strcmp(key, "onetap") ? &s->oneTapMoves : !strcmp(key, "foeivs") ? &s->foeIvs : !strcmp(key, "battleinfo") ? &s->battleInfo
                    : !strcmp(key, "battleview") ? &s->battleViewInfo : !strcmp(key, "battlefocus") ? &s->battleFocus : !strcmp(key, "lookaround") ? &s->lookAround : !strcmp(key, "daynight") ? &s->dayNight
                    : !strcmp(key, "mute") ? &s->gameMuted
                    : !strcmp(key, "aspectpicked") ? &s->aspectPicked : !strcmp(key, "autorun") ? &s->autoRun : !strcmp(key, "updatecheck") ? &s->updateCheck
                    : !strcmp(key, "dim") ? &s->dimCompanion : !strcmp(key, "savesbeside") ? &s->savesBeside
                    : !strcmp(key, "reducemotion") ? &s->reduceMotion : !strcmp(key, "levelcap") ? &s->levelCap : !strcmp(key, "instanttext") ? &s->instantText : !strcmp(key, "reusabletms") ? &s->reusableTms : !strcmp(key, "tradeevos") ? &s->tradeEvolutions : !strcmp(key, "resume") ? &s->resumeGames : !strcmp(key, "rumble") ? &s->rumble
                    : !strcmp(key, "boxart") ? &s->boxartAuto : NULL;
            if (b && int_in(value, 0, 1, &on)) *b = on;
        }
    }
    fclose(f);
    // FILTER: a settings.ini from before it had kinds keeps its LCD mask as SUBPIXEL.
    if (!filterSeen && s->lcdFilter) s->filter = PD_FILTER_SUBPIXEL;
    s->lcdFilter = s->filter == PD_FILTER_SUBPIXEL;
    // An ORIGINAL the player never picked is the old default: the desktop's EXPAND replaces it.
    if (pd_settings_expand_default && !s->aspectPicked && s->aspect == PD_ASPECT_ORIGINAL) s->aspect = PD_ASPECT_EXPAND;
    return true;
}

// plorpOS: through a temporary file and a rename, as plorpOS writes it, so neither reads half a file.
bool pd_settings_save(const struct pd_settings* s, const char* path) {
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE* f = fopen(tmp, "w");
    if (!f) return false;
    fprintf(f, "screen=%d\nff=%d\nguide=%d\naspect=%d\nautozoom=%d\nwildoff=%d\ntabs=%d\nhud=%d\nhudhide=%d\nquickgrid=%d\nlcd=%d\n",
            s->screenMode, s->ffSpeed, s->guideNotice, s->aspect, s->autoZoom ? 1 : 0, s->wildOff ? 1 : 0, s->tabBar,
            s->hud ? 1 : 0, s->hudHidden, s->quickGrid ? 1 : 0, s->filter == PD_FILTER_SUBPIXEL ? 1 : 0);
    fprintf(f, "filter=%d\ngrid=%d\n", s->filter, s->grid);
    fprintf(f, "hudlayout=%d\nhudlook=%d\nhudsize=%d\n", s->hudLayout, s->hudLook, s->hudSize);
    fprintf(f, "view=%d\npanellock=%d\n", s->companionView, s->panelLocked ? 1 : 0);
    fprintf(f, "shinyodds=%d\n", s->shinyOdds);
    fprintf(f, "ffsteady=%d\n", s->ffSteady ? 1 : 0);
    fprintf(f, "scaling=%d\nffaudio=%d\nrewind=%d\nrunindoors=%d\n", s->scaling, s->ffAudio ? 1 : 0,
            s->rewind ? 1 : 0, s->runIndoors ? 1 : 0);
    fprintf(f, "battlespeed=%d\ntextspeed=%d\nmenuspeed=%d\nbikespeed=%d\nsurfspeed=%d\n", s->sceneSpeed[0], s->sceneSpeed[1],
            s->sceneSpeed[2], s->sceneSpeed[3], s->sceneSpeed[4]);
    fprintf(f, "instanttext=%d\nreusabletms=%d\ntradeevos=%d\n", s->instantText ? 1 : 0, s->reusableTms ? 1 : 0,
            s->tradeEvolutions ? 1 : 0);
    fprintf(f, "quickbtn=%d\ncompanionbtn=%d\nhudbtn=%d\n", s->quickBtn, s->companionBtn, s->hudBtn);
    fprintf(f, "levelcap=%d\nresume=%d\nrumble=%d\nbatterysaver=%d\n", s->levelCap ? 1 : 0, s->resumeGames ? 1 : 0,
            s->rumble ? 1 : 0, s->batterySaver);
    fprintf(f, "dark=%d\ntheme=%d\nffnormal=%d\ncheats=%d\n", s->darkMode, s->theme, s->ffNormal, s->cheats);
    fprintf(f, "gamespeed=%d\nvolume=%d\nnohints=%d\nonetap=%d\nmute=%d\nautorun=%d\nperf=%d\n", PD_GAME_SPEED_PCT[s->gameSpeed],
            s->gameVolume, s->hideHints, s->oneTapMoves, s->gameMuted, s->autoRun, s->showPerf ? 1 : 0);
    fprintf(f, "updatecheck=%d\nshader=%d\ncolors=%d\ndim=%d\nsavesbeside=%d\n", s->updateCheck ? 1 : 0, s->shader,
            s->colors, s->dimCompanion ? 1 : 0, s->savesBeside ? 1 : 0);
    fprintf(f, "aspectpicked=%d\nfoeivs=%d\n", s->aspectPicked ? 1 : 0, s->foeIvs ? 1 : 0);
    fprintf(f, "battleinfo=%d\nbattleview=%d\n", s->battleInfo ? 1 : 0, s->battleViewInfo ? 1 : 0);
    fprintf(f, "battlefocus=%d\nlookaround=%d\ndaynight=%d\n", s->battleFocus ? 1 : 0, s->lookAround ? 1 : 0,
            s->dayNight ? 1 : 0);
    fprintf(f, "fullscreen=%d\nreducemotion=%d\nboxart=%d\n", s->fullscreen, s->reduceMotion ? 1 : 0,
            s->boxartAuto ? 1 : 0);
    fprintf(f, "buttons=%d\nhotkeys=", s->buttonLayout);
    for (int i = 0; i < PD_HOTKEY_BUTTONS; i++) fprintf(f, i ? ",%d" : "%d", s->hotkeys[i]);
    fprintf(f, "\norientation=%d\ngametouch=%d\nbackground=%d\nbrightness=%d\n", s->orientation, s->gameTouch,
            s->background, s->brightness);
    fprintf(f, "winglook=%d\nuisize=%d\nwingroom2=%d\nstretch=%d\n", s->wingLook, s->uiSize, s->wingRoom,
            s->stretch ? 1 : 0);
    fprintf(f, "bezel=%d\nbezelslots=%d,%d,%d,%d,%d,%d\n", s->bezelStyle, s->bezelSlots[0], s->bezelSlots[1],
            s->bezelSlots[2], s->bezelSlots[3], s->bezelSlots[4], s->bezelSlots[5]);
    static const char* const LISTS[4] = {"bartop", "barbottom", "battlebartop", "battlebarbottom"};
    for (int b = 0; b < 4; b++) {
        if (s->barCount[b] < 0) continue;
        fprintf(f, "%s=", LISTS[b]);
        for (int i = 0; i < s->barCount[b] && i < PD_BAR_LIST_MAX; i++) fprintf(f, i ? ",%d" : "%d", s->barList[b][i]);
        fprintf(f, "\n");
    }
    if (s->wingCount >= 0) {
        fprintf(f, "wing=");
        for (int i = 0; i < s->wingCount && i < PD_WING_LIST_MAX; i++) fprintf(f, i ? ",%d" : "%d", s->wingList[i]);
        fprintf(f, "\n");
    }
    if (s->battleWingCount >= 0) {
        fprintf(f, "battlewing=");
        for (int i = 0; i < s->battleWingCount && i < PD_WING_LIST_MAX; i++)
            fprintf(f, i ? ",%d" : "%d", s->battleWingList[i]);
        fprintf(f, "\n");
    }
    fprintf(f, "partytypes=%d\n", s->partyTypes ? 1 : 0);
    if (fclose(f) != 0) return false;
#ifdef _WIN32
    // rename() won't replace a file on Windows; MoveFileEx does, in one step (paths are UTF-8: the host's manifest).
    return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmp, path) == 0;
#endif
}
