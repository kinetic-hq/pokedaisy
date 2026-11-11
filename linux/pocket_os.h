/* The few OS calls the SDL host needs that POSIX and Windows spell differently.
 * GPL-3.0, see ../NOTICE.
 *
 * Linux keeps exactly what it did before (these are thin wrappers there). On
 * Windows the host is built with MinGW-w64 and runs with the UTF-8 code page
 * (pokedaisy.manifest), so fopen / dirent / mGBA's VFileOpen all take the same
 * UTF-8 paths; only what MinGW's C library lacks or does differently is here. */
#ifndef POCKET_OS_H
#define POCKET_OS_H
#include <stdbool.h>
#include <time.h>
#ifdef _WIN32
#include <string.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

/* mkdir DIR (0755 on POSIX); true when it exists afterwards. */
bool pocket_mkdir(const char *path);
/* Moves FROM over TO, replacing it in one step: rename() on POSIX, and on
 * Windows MoveFileEx(REPLACE_EXISTING | WRITE_THROUGH), since a plain rename()
 * there refuses to replace an existing file. Atomic state writes rely on it. */
bool pocket_replace(const char *from, const char *to);
bool pocket_remove(const char *path);
bool pocket_rmdir(const char *path);
/* access(path, F_OK) / R_OK. */
bool pocket_exists(const char *path);
bool pocket_readable(const char *path);
/* PATH is a folder (a drive's root included). */
bool pocket_is_dir(const char *path);
/* The places a folder browser starts from: each drive's root ("C:\\") on Windows, "/" and HOME elsewhere. Calls
 * EACH for each. */
void pocket_roots(void (*each)(const char *path, void *user), void *user);
void pocket_localtime(const time_t *t, struct tm *out);
/* Makes a new empty directory from TEMPLATE ending in XXXXXX (mkdtemp). */
char *pocket_mkdtemp(char *tmpl);
/* Where temporary files go: TMPDIR or /tmp; TEMP on Windows. */
const char *pocket_temp_root(void);
int pocket_setenv(const char *name, const char *value);
char *pocket_strsep(char **next, const char *delims);
/* Milliseconds on a monotonic clock. */
double pocket_now_ms(void);
/* Windows: a GUI program has no console; logs go to the parent's console when
 * started from one, else to LOGFILE. Nothing elsewhere. */
void pocket_attach_log(const char *logFile);

/* Directory listing: calls EACH for every entry but "." and "..". */
typedef void (*pocket_dir_each)(const char *name, void *user);
bool pocket_list_dir(const char *path, pocket_dir_each each, void *user);
#endif
