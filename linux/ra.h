/* GPL-3.0, see ../NOTICE. RetroAchievements on the desktop host: rcheevos' rc_client over the running mGBA core,
 * softcore only, as the Android app (app/src/main/cpp/pokedaisy_ra.c). Every call is from the main thread; the
 * server's answers come back on threads of their own and wait for ra_pump. */
#ifndef POCKET_RA_H
#define POCKET_RA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mCore;

/* dataDir holds retroachievements.ini (the user name and the server's token, never the password). say: the host's
 * toast. A saved login is signed in again at once. */
void ra_init(const char *dataDir, const char *version, void (*say)(const char *));
void ra_shutdown(void);

enum ra_login { RA_LOGGED_OUT, RA_LOGGING_IN, RA_LOGGED_IN };
enum ra_login ra_login_state(void);
const char *ra_user(void); /* the signed-in name, "" when none */
/* Starts a password login; ra_login_state leaves RA_LOGGING_IN when the server has answered (ra_login_error then
 * says why it failed, "" when it didn't). The password is only sent, never kept. */
void ra_login(const char *user, const char *password);
const char *ra_login_error(void);
void ra_logout(void);

/* A GB / GBA game: rom is the ROM file's bytes (never the running ROM: mGBA writes into it). Nothing happens when
 * nobody is signed in. */
void ra_load_game(struct mCore *core, const uint8_t *rom, size_t size);
void ra_unload_game(void);
bool ra_game_active(void); /* a game the server knows, with achievements */
void ra_frame(void);       /* after every emulated frame */
/* SHINY ODDS raised on a game it works in: achievements pause (sets reward a shiny catch), as on Android. */
void ra_set_paused(bool paused);
void ra_pump(void);        /* the server's answers, and rc_client's idle work: every turn of any loop */
void ra_reset(void);       /* the game was reset */

/* Progress beside a state file: written with it, read back after it's loaded (none: reset). */
void ra_progress_save(const char *statePath);
void ra_progress_load(const char *statePath);

/* The loaded game's achievements as menu rows: group titles (PD_MENU_GROUP) and "TITLE\tN PTS" / "TITLE\tDONE".
 * desc[i] is row i's description ("" for a title). Returns the row count. */
int ra_rows(char rows[][96], char desc[][160], int max);
/* "N OF M UNLOCKED, P OF Q POINTS", or "" when no game is loaded. */
void ra_summary(char *out, size_t size);
#endif
