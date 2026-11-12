/* GPL-3.0. The update check (updater.h). */
#include "updater.h"
#include "http.h"
#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

static SDL_atomic_t state;
static char dataDir[1024], currentVersion[32], latestVersion[32];
static char zipUrl[1024], sumsUrl[1024], pageUrl[1024];

enum updater_state updater_state(void) { return (enum updater_state)SDL_AtomicGet(&state); }
const char *updater_latest(void) { return latestVersion; }
const char *updater_current(void) { return currentVersion; }

/* --- versions and the release's JSON --- */

static bool parse_version(const char *s, long v[3]) {
  for (int i = 0; i < 3; i++) {
    if (!isdigit((unsigned char)*s))
      return false;
    char *end;
    v[i] = strtol(s, &end, 10);
    s = end;
    if (i < 2 && *s++ != '.')
      return false;
  }
  return *s == 0;
}

int updater_compare(const char *a, const char *b) {
  long x[3], y[3];
  if (!parse_version(a, x) || !parse_version(b, y))
    return 0; // anything that isn't x.y.z is never offered
  for (int i = 0; i < 3; i++)
    if (x[i] != y[i])
      return x[i] < y[i] ? -1 : 1;
  return 0;
}

// The string value after "key": from json (first occurrence), unescaped of nothing (URLs and tags have no escapes).
static bool json_string(const char *json, const char *key, const char *from, char *out, size_t size) {
  char pattern[64];
  snprintf(pattern, sizeof pattern, "\"%s\"", key);
  const char *p = strstr(from ? from : json, pattern);
  if (!p)
    return false;
  p += strlen(pattern);
  while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r')
    p++;
  if (*p++ != '"')
    return false;
  const char *end = strchr(p, '"');
  if (!end || (size_t)(end - p) >= size)
    return false;
  memcpy(out, p, end - p);
  out[end - p] = 0;
  return true;
}

// Only plain https URLs on GitHub's hosts leave this file (they reach WinHTTP / the browser).
static bool safe_url(const char *u) {
  if (strncmp(u, "https://github.com/", 19) && strncmp(u, "https://api.github.com/", 23))
    return false;
  for (const char *p = u; *p; p++)
    if (!isalnum((unsigned char)*p) && !strchr(":/._-~%?=&+", *p))
      return false;
  return true;
}

static bool asset_url(const char *json, const char *name, char *out, size_t size) {
  for (const char *p = json; (p = strstr(p, "\"browser_download_url\"")); p++) {
    char url[1024];
    if (!json_string(json, "browser_download_url", p, url, sizeof url))
      return false;
    size_t n = strlen(url), m = strlen(name);
    if (n > m && url[n - m - 1] == '/' && !strcmp(url + n - m, name) && safe_url(url) && strlen(url) < size) {
      strcpy(out, url);
      return true;
    }
  }
  return false;
}

bool updater_parse(const char *json, char *version, size_t versionSize, char *zip, size_t zipSize,
                   char *sums, size_t sumsSize, char *page, size_t pageSize) {
  char tag[64];
  if (!json_string(json, "tag_name", NULL, tag, sizeof tag))
    return false;
  size_t prefix = strlen(UPDATER_TAG_PREFIX);
  long v[3];
  if (strncmp(tag, UPDATER_TAG_PREFIX, prefix) || !parse_version(tag + prefix, v) ||
      strlen(tag + prefix) >= versionSize)
    return false;
  strcpy(version, tag + prefix);
  if (!json_string(json, "html_url", NULL, page, pageSize) || !safe_url(page))
    return false;
  if (!asset_url(json, UPDATER_WINDOWS_ZIP, zip, zipSize))
    zip[0] = 0;
  if (!asset_url(json, UPDATER_SUMS, sums, sumsSize))
    sums[0] = 0;
  return true;
}

/* The end of the JSON object starting at p ('{'), past its '}', strings (and their escapes) skipped. NULL if cut. */
static const char *object_end(const char *p) {
  int depth = 0;
  bool inString = false;
  for (; *p; p++) {
    if (inString) {
      if (*p == '\\' && p[1])
        p++;
      else if (*p == '"')
        inString = false;
    } else if (*p == '"')
      inString = true;
    else if (*p == '{')
      depth++;
    else if (*p == '}' && --depth == 0)
      return p + 1;
  }
  return NULL;
}

bool updater_parse_list(const char *json, char *version, size_t versionSize, char *zip, size_t zipSize,
                        char *sums, size_t sumsSize, char *page, size_t pageSize) {
  const char *p = json;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
    p++;
  if (*p != '[')
    return updater_parse(json, version, versionSize, zip, zipSize, sums, sumsSize, page, pageSize); // one release
  bool found = false;
  while (*++p) {
    if (*p == ']')
      break;
    if (*p != '{')
      continue;
    const char *end = object_end(p);
    if (!end)
      break;
    size_t n = (size_t)(end - p);
    char *release = malloc(n + 1);
    if (!release)
      break;
    memcpy(release, p, n);
    release[n] = 0;
    // A draft never reaches an unsigned-in request, but leave one out anyway.
    char v[32], z[1024], s[1024], g[1024];
    bool draft = strstr(release, "\"draft\":true") || strstr(release, "\"draft\": true");
    if (!draft && updater_parse(release, v, sizeof v, z, sizeof z, s, sizeof s, g, sizeof g) &&
        strlen(v) < versionSize && strlen(z) < zipSize && strlen(s) < sumsSize && strlen(g) < pageSize &&
        (!found || updater_compare(v, version) > 0)) {
      strcpy(version, v);
      strcpy(zip, z);
      strcpy(sums, s);
      strcpy(page, g);
      found = true;
    }
    free(release);
    p = end - 1;
  }
  return found;
}

bool updater_sum_for(const char *sums, const char *file, char hex[65]) {
  for (const char *line = sums; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
    const char *name = line + 64;
    size_t n = strlen(file);
    if (strspn(line, "0123456789abcdefABCDEF") == 64 && (name[0] == ' ' || name[0] == '\t')) {
      while (*name == ' ' || *name == '\t' || *name == '*')
        name++;
      if (!strncmp(name, file, n) && (name[n] == '\n' || name[n] == '\r' || !name[n])) {
        for (int i = 0; i < 64; i++)
          hex[i] = (char)tolower((unsigned char)line[i]);
        hex[64] = 0;
        return true;
      }
    }
  }
  return false;
}

/* --- SHA-256 (FIPS 180-4) --- */

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(uint32_t h[8], const uint8_t *p) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++)
    w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = k + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
    uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    k = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
  }
  h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += k;
}
void updater_sha256(const uint8_t *data, size_t size, char hex[65]) {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  size_t full = size / 64 * 64;
  for (size_t i = 0; i < full; i += 64)
    sha_block(h, data + i);
  uint8_t tail[128] = {0};
  size_t rest = size - full;
  memcpy(tail, data + full, rest);
  tail[rest] = 0x80;
  size_t len = rest + 9 <= 64 ? 64 : 128;
  uint64_t bits = (uint64_t)size * 8;
  for (int i = 0; i < 8; i++)
    tail[len - 1 - i] = (uint8_t)(bits >> (8 * i));
  for (size_t i = 0; i < len; i += 64)
    sha_block(h, tail + i);
  for (int i = 0; i < 8; i++)
    snprintf(hex + 8 * i, 9, "%08x", (unsigned)h[i]);
}

/* --- fetching --- */

#define MAX_DOWNLOAD (128u << 20)

// GET url into a malloc'd, NUL-ended buffer (size without the NUL); NULL on any failure (http.c).
static char *fetch(const char *url, size_t *size) {
  if (!safe_url(url))
    return NULL;
  return http_get(url, "Accept: application/vnd.github+json", MAX_DOWNLOAD, size);
}

/* --- update.ini: the release the player skipped --- */

static void skipped(char *out, size_t size) {
  out[0] = 0;
  char path[1100], line[128];
  snprintf(path, sizeof path, "%s/update.ini", dataDir);
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  while (fgets(line, sizeof line, f))
    if (!strncmp(line, "skip=", 5)) {
      snprintf(out, size, "%.31s", line + 5);
      out[strcspn(out, "\r\n")] = 0;
    }
  fclose(f);
}

void updater_skip(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/update.ini", dataDir);
  FILE *f = fopen(path, "w");
  if (f) {
    fprintf(f, "skip=%s\n", latestVersion);
    fclose(f);
  }
  SDL_AtomicSet(&state, UPDATER_CURRENT);
}

void updater_later(void) { SDL_AtomicSet(&state, UPDATER_CURRENT); }

/* --- the check and the download --- */

static int check_thread(void *unused) {
  (void)unused;
  size_t size;
  char *json = fetch(UPDATER_API, &size);
  if (!json)
    json = fetch(UPDATER_API_OLD, &size);
  char version[32], skip[32];
  bool newer = json && updater_parse_list(json, version, sizeof version, zipUrl, sizeof zipUrl, sumsUrl, sizeof sumsUrl,
                                          pageUrl, sizeof pageUrl) &&
               updater_compare(version, currentVersion) > 0;
  free(json);
  if (newer) {
    skipped(skip, sizeof skip);
    newer = strcmp(skip, version) != 0;
  }
  if (newer) {
    snprintf(latestVersion, sizeof latestVersion, "%s", version);
    fprintf(stderr, "Update: PokeDaisy %s is out (this is %s)\n", latestVersion, currentVersion);
  }
  SDL_AtomicSet(&state, newer ? UPDATER_AVAILABLE : UPDATER_CURRENT);
  return 0;
}

void updater_start(const char *data, const char *current, bool enabled) {
  snprintf(dataDir, sizeof dataDir, "%s", data);
  snprintf(currentVersion, sizeof currentVersion, "%s", current);
  long v[3];
  if (!enabled || !parse_version(current, v) || SDL_AtomicGet(&state) != UPDATER_IDLE)
    return;
  SDL_AtomicSet(&state, UPDATER_CHECKING);
  SDL_Thread *t = SDL_CreateThread(check_thread, "update-check", NULL);
  if (t)
    SDL_DetachThread(t);
  else
    SDL_AtomicSet(&state, UPDATER_CURRENT);
}

bool updater_can_install(void) {
#ifdef _WIN32
  return zipUrl[0] && sumsUrl[0];
#else
  return false;
#endif
}

void updater_open_page(void) {
  if (pageUrl[0])
    SDL_OpenURL(pageUrl);
  SDL_AtomicSet(&state, UPDATER_CURRENT);
}

static char zipPath[1200];

static int download_thread(void *unused) {
  (void)unused;
  size_t zipSize = 0, sumsSize = 0;
  char *sums = fetch(sumsUrl, &sumsSize);
  char *zip = sums ? fetch(zipUrl, &zipSize) : NULL;
  char want[65], got[65];
  bool ok = zip && zipSize > 4 && !memcmp(zip, "PK\3\4", 4) && updater_sum_for(sums, UPDATER_WINDOWS_ZIP, want);
  if (ok) {
    updater_sha256((const uint8_t *)zip, zipSize, got);
    ok = !strcmp(want, got);
    if (!ok)
      fprintf(stderr, "Update: the download doesn't match SHA256SUMS.txt\n");
  }
  if (ok) {
    char dir[1100];
    snprintf(dir, sizeof dir, "%s/update", dataDir);
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#endif
    snprintf(zipPath, sizeof zipPath, "%s/" UPDATER_WINDOWS_ZIP, dir);
    FILE *f = fopen(zipPath, "wb");
    ok = f && fwrite(zip, 1, zipSize, f) == zipSize;
    if (f)
      ok = fclose(f) == 0 && ok;
  }
  free(sums);
  free(zip);
  fprintf(stderr, ok ? "Update: %s downloaded; installed when PokeDaisy closes\n" : "Update: %s download failed\n",
          latestVersion);
  SDL_AtomicSet(&state, ok ? UPDATER_READY : UPDATER_FAILED);
  return 0;
}

void updater_download(void) {
  if (!updater_can_install() || updater_state() != UPDATER_AVAILABLE)
    return;
  SDL_AtomicSet(&state, UPDATER_DOWNLOADING);
  SDL_Thread *t = SDL_CreateThread(download_thread, "update-download", NULL);
  if (t)
    SDL_DetachThread(t);
  else
    SDL_AtomicSet(&state, UPDATER_FAILED);
}

void updater_finish(void) {
#ifdef _WIN32
  if (updater_state() != UPDATER_READY)
    return;
  char *base = SDL_GetBasePath();
  if (!base)
    return;
  char app[1024];
  snprintf(app, sizeof app, "%s", base);
  SDL_free(base);
  size_t n = strlen(app);
  if (n && (app[n - 1] == '\\' || app[n - 1] == '/'))
    app[n - 1] = 0;
  char self[1024];
  DWORD selfLen = GetModuleFileNameA(NULL, self, sizeof self);
  if (selfLen == 0 || selfLen >= sizeof self)
    return;
  // The helper: waits for this process to end, unpacks the zip (Windows' own tar reads zips) and copies the
  // program's files over, leaving DATA alone (robocopy without /MIR deletes nothing).
  char script[1100];
  snprintf(script, sizeof script, "%s/update/apply.cmd", dataDir);
  FILE *f = fopen(script, "w");
  if (!f)
    return;
  fprintf(f,
          "@echo off\r\n"
          "setlocal\r\n"
          "cd /d \"%%~dp0\"\r\n"
          // Windows' own tools by their full path: a PATH with Git's or MSYS2's in front finds their tar (which
          // can't read a zip) and find first.
          "set \"SYS=%%SystemRoot%%\\System32\"\r\n"
          "echo waiting for %%1> apply.log\r\n"
          // Until PokeDaisy has closed: a running program's file can't be opened for writing. (Not tasklist piped
          // into find: with no console the pipe never ended and the wait with it.) At most about two minutes.
          "set /a tries=0\r\n"
          ":wait\r\n"
          "2>nul (>>\"%s\" call ) && goto closed\r\n"
          "set /a tries+=1\r\n"
          "if %%tries%% geq 120 goto failed\r\n"
          "\"%%SYS%%\\PING.EXE\" -n 2 127.0.0.1 >nul\r\n"
          "goto wait\r\n"
          ":closed\r\n"
          "if exist staging rmdir /s /q staging\r\n"
          "mkdir staging\r\n"
          "\"%%SYS%%\\tar.exe\" -xf " UPDATER_WINDOWS_ZIP " -C staging >>apply.log 2>&1 || goto failed\r\n"
          "\"%%SYS%%\\Robocopy.exe\" \"staging\\pokedaisy-windows-x86_64\" \"%s\" /E /XD DATA /R:5 /W:1 /NFL /NDL /NJH /NJS /NP >>apply.log 2>&1\r\n"
          "if errorlevel 8 goto failed\r\n"
          "rmdir /s /q staging\r\n"
          "del " UPDATER_WINDOWS_ZIP "\r\n"
          "echo %s> installed.txt\r\n"
          "exit /b 0\r\n"
          ":failed\r\n"
          "echo %s> failed.txt\r\n"
          "exit /b 1\r\n",
          self, app, latestVersion, latestVersion);
  fclose(f);
  char cmd[1400];
  snprintf(cmd, sizeof cmd, "cmd.exe /c \"\"%s\" %lu\"", script, (unsigned long)GetCurrentProcessId());
  STARTUPINFOA si;
  memset(&si, 0, sizeof si);
  si.cb = sizeof si;
  PROCESS_INFORMATION pi;
  // Out of this process's job when it may: Steam and some frontends start games in a job that ends every process
  // in it when the game closes, which would end the helper before it installs. A job that forbids it: start inside.
  DWORD flags = CREATE_NO_WINDOW | DETACHED_PROCESS;
  BOOL started = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags | CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &si, &pi);
  if (!started) {
    fprintf(stderr, "Update: can't leave this process's job (%lu), starting the installer inside it\n",
            (unsigned long)GetLastError());
    started = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi);
  }
  if (started) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    fprintf(stderr, "Update: installing %s after PokeDaisy closes\n", latestVersion);
  }
#endif
}
