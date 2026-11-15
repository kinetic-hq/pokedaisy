// The player's settings in a small text file (key=value lines), read at start
// and written on each change. Unknown keys and bad values fall back to the
// defaults, so an old or hand-edited file never breaks a start.
#ifndef PD_SETTINGS_H
#define PD_SETTINGS_H

#include <stdbool.h>

#include "pd_ui.h"

// The desktop host's ASPECT default: EXPAND (so AUTO ZOOM IN works from the start) where the app's is ORIGINAL. Set
// before any load; a settings file's ORIGINAL then counts only if the player picked it (aspectpicked=1).
extern bool pd_settings_expand_default;
// DARK MODE's default where the file doesn't say (the desktop host: on).
extern bool pd_settings_dark_default;
// DARK MODE AUTO: the host's look at the system's theme (1 dark, 0 light, -1 can't tell). NULL (the 3DS): no AUTO.
extern int (*pd_settings_system_dark)(void);
// With AUTO on, darkMode takes the system's choice; true when that changed it.
bool pd_settings_follow_system(struct pd_settings* s);
// SAVE BACKUPS' choices (Android's KEEP PER GAME); 10 to begin with.
#define PD_BACKUP_KEEP_COUNT 6
extern const int PD_BACKUP_KEEPS[PD_BACKUP_KEEP_COUNT];
void pd_settings_defaults(struct pd_settings* s);
// Missing file = defaults (and false).
bool pd_settings_load(struct pd_settings* s, const char* path);
bool pd_settings_save(const struct pd_settings* s, const char* path);

#endif
