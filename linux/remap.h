/* GAME BUTTONS: which keyboard key and which controller button press each of the game's buttons, kept in
 * DATA/controls.ini. GPL-3.0, see ../NOTICE. */
#ifndef POCKET_REMAP_H
#define POCKET_REMAP_H
#include <stdbool.h>

#define REMAP_MAX 32   // buttons, by the host's own numbers (main.c's A, B, SELECT ...)
#define REMAP_NONE -1  // nothing bound
#define REMAP_PAD_LT 100 // the triggers, which a controller reports as axes
#define REMAP_PAD_RT 101

struct remap {
  int key[REMAP_MAX]; // a keycode each (SDL_Keycode), or REMAP_NONE
  int pad[REMAP_MAX]; // a controller button each (SDL_GameControllerButton, REMAP_PAD_LT / RT), or REMAP_NONE
  bool keyOwn, padOwn; // the player changed them (else the defaults, which follow BUTTON LAYOUT)
};

/* BUTTON gets KEY (or PAD's button). A button that had it takes BUTTON's old one, so the two swap and nothing is lost. */
void remap_set_key(struct remap *r, int button, int key);
void remap_set_pad(struct remap *r, int button, int pad);
/* The buttons KEY (or PAD's button) presses, into OUT (at most MAX); how many. */
int remap_from_key(const struct remap *r, int key, int *out, int max);
int remap_from_pad(const struct remap *r, int pad, int *out, int max);

/* DATA/controls.ini: "key.<NAME>=<number>" and "pad.<NAME>=<number>" for BUTTONS (COUNT host numbers, NAMES theirs).
 * Only a side the player changed is in the file; the rest is left as R had it (the defaults). */
void remap_load(struct remap *r, const char *root, const int *buttons, const char *const *names, int count);
bool remap_save(const struct remap *r, const char *root, const int *buttons, const char *const *names, int count);
#endif
