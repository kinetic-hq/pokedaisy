#include "pd_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

void pd_settings_defaults(struct pd_settings* s) {
    s->screenMode = PD_SCREEN_SHARP;
    s->ffSpeed = PD_FF_MIN;
    s->guideNotice = 0;
    s->aspect = PD_ASPECT_ORIGINAL;
    s->autoZoom = true;
    s->wildOff = false;
    s->tabBar = PD_TAB_BAR_DEFAULT;
    s->hud = true;
    s->hudHidden = 0;
    s->hudLayout = PD_HUD_CORNERS;
    s->hudLook = PD_HUD_GLASS;
    s->hudSize = PD_HUD_NORMAL;
    s->scaling = PD_SCALE_INTEGER;
    s->ffAudio = false;
    s->rewind = false;
    s->runIndoors = false;
    s->levelCap = false;
    s->resumeGames = true;
    s->rumble = false;
    s->batterySaver = PD_SAVER_AUTO;
    s->quickGrid = false;
    s->lcdFilter = false;
    s->quickBtn = PD_BTN_Y;
    s->companionBtn = PD_BTN_X;
    s->hudBtn = PD_BTN_NONE;
    s->hideHints = false;
    s->oneTapMoves = false;
    s->gameSpeed = 0;
    s->gameVolume = 100;
    s->gameMuted = false;
    s->autoRun = false;
    s->darkMode = false;
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
    // linux/bezel.h's BEZEL_DEFAULT_SLOTS: LOCATION, CLOCK AND BATTERY, BADGES AND PLAY TIME, LEAD POKEMON,
    // NEXT BOSS AND CAP, REPEL OR MONEY.
    static const int BEZEL_SLOTS[6] = {1, 2, 5, 14, 12, 10};
    for (int i = 0; i < 6; i++) s->bezelSlots[i] = BEZEL_SLOTS[i];
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
        else if (!strcmp(key, "speed")) int_in(value, 0, PD_GAME_SPEED_COUNT - 1, &s->gameSpeed);
        else if (!strcmp(key, "volume")) int_in(value, 20, 100, &s->gameVolume);
        else if (!strcmp(key, "shader")) int_in(value, 0, PD_SHADERS - 1, &s->shader);
        else if (!strcmp(key, "colors")) int_in(value, 0, PD_COLORS - 1, &s->colors);
        else if (!strcmp(key, "fullscreen")) int_in(value, -1, 1, &s->fullscreen);
        else if (!strcmp(key, "buttons")) int_in(value, 0, PD_BUTTON_LAYOUTS - 1, &s->buttonLayout);
        else if (!strcmp(key, "orientation")) int_in(value, 0, PD_ORIENTATIONS - 1, &s->orientation);
        else if (!strcmp(key, "gametouch")) int_in(value, 0, PD_GAME_TOUCHES - 1, &s->gameTouch);
        else if (!strcmp(key, "background")) int_in(value, -1, 3, &s->background);
        else if (!strcmp(key, "winglook")) int_in(value, 0, 2, &s->wingLook);
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
            bool* b = !strcmp(key, "ffaudio") ? &s->ffAudio : !strcmp(key, "rewind") ? &s->rewind
                    : !strcmp(key, "runindoors") ? &s->runIndoors : !strcmp(key, "nohints") ? &s->hideHints
                    : !strcmp(key, "onetap") ? &s->oneTapMoves : !strcmp(key, "mute") ? &s->gameMuted
                    : !strcmp(key, "autorun") ? &s->autoRun : !strcmp(key, "updatecheck") ? &s->updateCheck
                    : !strcmp(key, "dim") ? &s->dimCompanion : !strcmp(key, "savesbeside") ? &s->savesBeside
                    : !strcmp(key, "reducemotion") ? &s->reduceMotion : !strcmp(key, "levelcap") ? &s->levelCap : !strcmp(key, "resume") ? &s->resumeGames : !strcmp(key, "rumble") ? &s->rumble
                    : !strcmp(key, "boxart") ? &s->boxartAuto : NULL;
            if (b && int_in(value, 0, 1, &on)) *b = on;
        }
    }
    fclose(f);
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
            s->hud ? 1 : 0, s->hudHidden, s->quickGrid ? 1 : 0, s->lcdFilter ? 1 : 0);
    fprintf(f, "hudlayout=%d\nhudlook=%d\nhudsize=%d\n", s->hudLayout, s->hudLook, s->hudSize);
    fprintf(f, "view=%d\npanellock=%d\n", s->companionView, s->panelLocked ? 1 : 0);
    fprintf(f, "scaling=%d\nffaudio=%d\nrewind=%d\nrunindoors=%d\n", s->scaling, s->ffAudio ? 1 : 0,
            s->rewind ? 1 : 0, s->runIndoors ? 1 : 0);
    fprintf(f, "quickbtn=%d\ncompanionbtn=%d\nhudbtn=%d\n", s->quickBtn, s->companionBtn, s->hudBtn);
    fprintf(f, "levelcap=%d\nresume=%d\nrumble=%d\nbatterysaver=%d\n", s->levelCap ? 1 : 0, s->resumeGames ? 1 : 0,
            s->rumble ? 1 : 0, s->batterySaver);
    fprintf(f, "dark=%d\ntheme=%d\nffnormal=%d\ncheats=%d\n", s->darkMode, s->theme, s->ffNormal, s->cheats);
    fprintf(f, "speed=%d\nvolume=%d\nnohints=%d\nonetap=%d\nmute=%d\nautorun=%d\nperf=%d\n", s->gameSpeed,
            s->gameVolume, s->hideHints, s->oneTapMoves, s->gameMuted, s->autoRun, s->showPerf ? 1 : 0);
    fprintf(f, "updatecheck=%d\nshader=%d\ncolors=%d\ndim=%d\nsavesbeside=%d\n", s->updateCheck ? 1 : 0, s->shader,
            s->colors, s->dimCompanion ? 1 : 0, s->savesBeside ? 1 : 0);
    fprintf(f, "fullscreen=%d\nreducemotion=%d\nboxart=%d\n", s->fullscreen, s->reduceMotion ? 1 : 0,
            s->boxartAuto ? 1 : 0);
    fprintf(f, "buttons=%d\nhotkeys=", s->buttonLayout);
    for (int i = 0; i < PD_HOTKEY_BUTTONS; i++) fprintf(f, i ? ",%d" : "%d", s->hotkeys[i]);
    fprintf(f, "\norientation=%d\ngametouch=%d\nbackground=%d\nbrightness=%d\n", s->orientation, s->gameTouch,
            s->background, s->brightness);
    fprintf(f, "winglook=%d\n", s->wingLook);
    fprintf(f, "bezel=%d\nbezelslots=%d,%d,%d,%d,%d,%d\n", s->bezelStyle, s->bezelSlots[0], s->bezelSlots[1],
            s->bezelSlots[2], s->bezelSlots[3], s->bezelSlots[4], s->bezelSlots[5]);
    if (fclose(f) != 0) return false;
#ifdef _WIN32
    // rename() won't replace a file on Windows; MoveFileEx does, in one step (paths are UTF-8: the host's manifest).
    return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmp, path) == 0;
#endif
}
