/* GPL-3.0, see ../NOTICE. pocket_os.h's calls for POSIX and Windows. */
#include "pocket_os.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32
/* UTF-8 -> UTF-16 for the W calls, so a path means the same as in fopen under
 * the manifest's UTF-8 code page (and on a Windows that ignores it). */
static wchar_t *wide(const char *s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  wchar_t *w = n > 0 ? malloc(sizeof *w * (size_t)n) : NULL;
  if (w && !MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n)) {
    free(w);
    w = NULL;
  }
  return w;
}
static char *narrow(const wchar_t *w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
  char *s = n > 0 ? malloc((size_t)n) : NULL;
  if (s && !WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL)) {
    free(s);
    s = NULL;
  }
  return s;
}
bool pocket_mkdir(const char *path) {
  wchar_t *w = wide(path);
  bool ok = w && (CreateDirectoryW(w, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
  free(w);
  return ok;
}
bool pocket_replace(const char *from, const char *to) {
  wchar_t *a = wide(from), *b = wide(to);
  bool ok = a && b && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  free(a);
  free(b);
  return ok;
}
bool pocket_is_dir(const char *path) {
  wchar_t *w = wide(path);
  DWORD a = w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES;
  free(w);
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
void pocket_roots(void (*each)(const char *path, void *user), void *user) {
  DWORD drives = GetLogicalDrives();
  for (int i = 0; i < 26; i++)
    if (drives & (1u << i)) {
      char root[4] = {(char)('A' + i), ':', '\\', 0};
      UINT type = GetDriveTypeA(root);
      if (type != DRIVE_NO_ROOT_DIR && type != DRIVE_UNKNOWN)
        each(root, user);
    }
}
bool pocket_remove(const char *path) {
  wchar_t *w = wide(path);
  bool ok = w && DeleteFileW(w);
  free(w);
  return ok;
}
bool pocket_rmdir(const char *path) {
  wchar_t *w = wide(path);
  bool ok = w && RemoveDirectoryW(w);
  free(w);
  return ok;
}
static DWORD attributes(const char *path) {
  wchar_t *w = wide(path);
  DWORD a = w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES;
  free(w);
  return a;
}
bool pocket_exists(const char *path) {
  return attributes(path) != INVALID_FILE_ATTRIBUTES;
}
bool pocket_readable(const char *path) {
  wchar_t *w = wide(path);
  HANDLE h = w ? CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL)
               : INVALID_HANDLE_VALUE;
  free(w);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  CloseHandle(h);
  return true;
}
void pocket_localtime(const time_t *t, struct tm *out) {
  localtime_s(out, t);
}
char *pocket_mkdtemp(char *tmpl) {
  size_t n = strlen(tmpl);
  if (n < 6 || strcmp(tmpl + n - 6, "XXXXXX"))
    return NULL;
  for (int tries = 0; tries < 100; tries++) {
    unsigned v = (unsigned)GetTickCount() ^ ((unsigned)GetCurrentProcessId() << 8) ^ (unsigned)(tries * 7919);
    for (int i = 0; i < 6; i++, v /= 36)
      tmpl[n - 6 + i] = "abcdefghijklmnopqrstuvwxyz0123456789"[v % 36];
    wchar_t *w = wide(tmpl);
    bool made = w && CreateDirectoryW(w, NULL);
    free(w);
    if (made)
      return tmpl;
  }
  return NULL;
}
int pocket_setenv(const char *name, const char *value) {
  return _putenv_s(name, value) ? -1 : 0;
}
double pocket_now_ms(void) {
  static LARGE_INTEGER freq;
  LARGE_INTEGER now;
  if (!freq.QuadPart)
    QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}
void pocket_attach_log(const char *logFile) {
  HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
  if (err && err != INVALID_HANDLE_VALUE) {
    // Already redirected (a pipe, a file, ctest): leave it.
  } else if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
  } else if (logFile) {
    // Double-clicked: no console and no redirect; keep the log beside the data.
    freopen(logFile, "w", stderr);
  }
  setvbuf(stderr, NULL, _IONBF, 0);
}
bool pocket_list_dir(const char *path, pocket_dir_each each, void *user) {
  size_t n = strlen(path);
  char *pattern = malloc(n + 3);
  if (!pattern)
    return false;
  memcpy(pattern, path, n);
  memcpy(pattern + n, "/*", 3);
  wchar_t *w = wide(pattern);
  free(pattern);
  WIN32_FIND_DATAW e;
  HANDLE h = w ? FindFirstFileW(w, &e) : INVALID_HANDLE_VALUE;
  free(w);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  do {
    if (!wcscmp(e.cFileName, L".") || !wcscmp(e.cFileName, L".."))
      continue;
    char *name = narrow(e.cFileName);
    if (name)
      each(name, user);
    free(name);
  } while (FindNextFileW(h, &e));
  FindClose(h);
  return true;
}
#else
bool pocket_mkdir(const char *path) {
  return mkdir(path, 0755) == 0 || errno == EEXIST;
}
bool pocket_replace(const char *from, const char *to) {
  return rename(from, to) == 0;
}
bool pocket_remove(const char *path) {
  return unlink(path) == 0;
}
bool pocket_rmdir(const char *path) {
  return rmdir(path) == 0;
}
bool pocket_exists(const char *path) {
  return access(path, F_OK) == 0;
}
bool pocket_readable(const char *path) {
  return access(path, R_OK) == 0;
}
bool pocket_is_dir(const char *path) {
  struct stat s;
  return stat(path, &s) == 0 && S_ISDIR(s.st_mode);
}
void pocket_roots(void (*each)(const char *path, void *user), void *user) {
  const char *home = getenv("HOME");
  if (home && *home && strcmp(home, "/"))
    each(home, user);
  each("/", user);
}
void pocket_localtime(const time_t *t, struct tm *out) {
  localtime_r(t, out);
}
char *pocket_mkdtemp(char *tmpl) {
  return mkdtemp(tmpl);
}
int pocket_setenv(const char *name, const char *value) {
  return setenv(name, value, 1);
}
double pocket_now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}
void pocket_attach_log(const char *logFile) {
  (void)logFile;
}
bool pocket_list_dir(const char *path, pocket_dir_each each, void *user) {
  DIR *d = opendir(path);
  if (!d)
    return false;
  for (struct dirent *e; (e = readdir(d));)
    if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
      each(e->d_name, user);
  closedir(d);
  return true;
}
#endif

const char *pocket_temp_root(void) {
#ifdef _WIN32
  const char *t = getenv("TEMP");
  if (!t)
    t = getenv("TMP");
  return t ? t : ".";
#else
  const char *t = getenv("TMPDIR");
  return t && *t ? t : "/tmp";
#endif
}
char *pocket_strsep(char **next, const char *delims) {
  char *s = *next;
  if (!s)
    return NULL;
  char *end = s + strcspn(s, delims);
  if (*end) {
    *end = 0;
    *next = end + 1;
  } else
    *next = NULL;
  return s;
}
