/* GPL-3.0. The update check: once per launch, in the background, the newest
 * desktop release of kinetic-hq/pokedaisy-desktop against the version built in. On
 * Windows UPDATE NOW downloads the release zip, checks it against the release's
 * SHA256SUMS.txt and installs it when PokeDaisy closes (a helper copies the new
 * files over the program's, never DATA: settings, saves, states, ROMs). On Linux
 * it opens the release page. Offline or failing, it says nothing. */
#ifndef POCKET_UPDATER_H
#define POCKET_UPDATER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum updater_state {
  UPDATER_IDLE,        // not started, turned off, or a local ("dev") build
  UPDATER_CHECKING,
  UPDATER_CURRENT,     // nothing newer (or the check failed: silent)
  UPDATER_AVAILABLE,   // a newer release; updater_latest() names it
  UPDATER_DOWNLOADING,
  UPDATER_READY,       // downloaded and checked: installed when the program closes
  UPDATER_FAILED,      // the download or its check failed
};

#define UPDATER_API "https://api.github.com/repos/kinetic-hq/pokedaisy-desktop/releases/latest"
/* The repo's name before it was renamed: asked when the new name doesn't answer (before the rename, or if GitHub's
 * redirect from the old name ever stops). Both fetches follow redirects. */
#define UPDATER_API_OLD "https://api.github.com/repos/kinetic-hq/pokedaisy/releases/latest"
#define UPDATER_TAG_PREFIX "desktop-v"
#define UPDATER_WINDOWS_ZIP "pokedaisy-windows-x86_64.zip"
#define UPDATER_SUMS "SHA256SUMS.txt"

/* Starts the check in a thread unless !enabled, current is "dev" or the release
 * is the one the player skipped (DATA/update.ini). data = the DATA folder. */
void updater_start(const char *data, const char *current, bool enabled);
enum updater_state updater_state(void);
const char *updater_latest(void);  // "1.2.0"
const char *updater_current(void); // the built-in version
bool updater_can_install(void);    // this platform downloads and installs (Windows)
void updater_download(void);       // UPDATE NOW on Windows (background)
void updater_open_page(void);      // UPDATE NOW elsewhere: the release page in the browser
void updater_later(void);          // asked again on the next launch
void updater_skip(void);           // never asked about this release again
/* At exit: starts the helper that installs a READY update once this process is gone. */
void updater_finish(void);

/* For the tests. */
int updater_compare(const char *a, const char *b); // <0, 0, >0 by semver x.y.z
bool updater_parse(const char *json, char *version, size_t versionSize, char *zip, size_t zipSize,
                   char *sums, size_t sumsSize, char *page, size_t pageSize);
bool updater_sum_for(const char *sums, const char *file, char hex[65]);
void updater_sha256(const uint8_t *data, size_t size, char hex[65]);
#endif
