/* GPL-3.0, see ../NOTICE. The HTTPS GET (http.h). */
#include "http.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

bool http_url_ok(const char *u) {
  if (strncmp(u, "https://", 8) || strlen(u) >= 1024)
    return false;
  for (const char *p = u; *p; p++)
    if (!isalnum((unsigned char)*p) && !strchr(":/._-~%?=&+,", *p))
      return false;
  return true;
}

static bool header_ok(const char *h) {
  if (!h)
    return true;
  if (strlen(h) >= 512)
    return false;
  for (const char *p = h; *p; p++)
    if (!isalnum((unsigned char)*p) && !strchr(" :/._-+=,;", *p))
      return false;
  return true;
}

void http_encode(const char *s, char *out, size_t size) {
  static const char HEX[] = "0123456789ABCDEF";
  size_t n = 0;
  for (; *s && n + 4 < size; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || strchr("-._~", c))
      out[n++] = (char)c;
    else {
      out[n++] = '%';
      out[n++] = HEX[c >> 4];
      out[n++] = HEX[c & 15];
    }
  }
  if (size)
    out[n < size ? n : size - 1] = 0;
}

char *http_get(const char *url, const char *header, size_t max, size_t *size) {
  if (!http_url_ok(url) || !header_ok(header))
    return NULL;
  char *buf = NULL;
  size_t len = 0;
#ifdef _WIN32
  wchar_t wurl[1024], wheader[512];
  if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024))
    return NULL;
  if (header && !MultiByteToWideChar(CP_UTF8, 0, header, -1, wheader, 512))
    return NULL;
  URL_COMPONENTS u;
  memset(&u, 0, sizeof u);
  u.dwStructSize = sizeof u;
  wchar_t host[256], path[1024];
  u.lpszHostName = host, u.dwHostNameLength = 256;
  u.lpszUrlPath = path, u.dwUrlPathLength = 1024;
  if (!WinHttpCrackUrl(wurl, 0, 0, &u))
    return NULL;
  HINTERNET s = WinHttpOpen(L"PokeDaisy", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
  HINTERNET c = s ? WinHttpConnect(s, host, u.nPort, 0) : NULL;
  HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE)
                  : NULL;
  bool ok = false;
  if (r) {
    WinHttpSetTimeouts(r, 5000, 5000, 10000, 30000);
    DWORD status = 0, statusSize = sizeof status;
    if (WinHttpSendRequest(r, header ? wheader : WINHTTP_NO_ADDITIONAL_HEADERS, header ? (DWORD)-1L : 0, NULL, 0, 0,
                           0) &&
        WinHttpReceiveResponse(r, NULL) &&
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &statusSize,
                            NULL) &&
        status == 200) {
      ok = true;
      for (;;) {
        DWORD avail = 0, got = 0;
        if (!WinHttpQueryDataAvailable(r, &avail)) {
          ok = false;
          break;
        }
        if (!avail)
          break;
        if (len + avail > max) {
          ok = false;
          break;
        }
        char *grown = realloc(buf, len + avail + 1);
        if (!grown) {
          ok = false;
          break;
        }
        buf = grown;
        if (!WinHttpReadData(r, buf + len, avail, &got)) {
          ok = false;
          break;
        }
        len += got;
      }
    }
    WinHttpCloseHandle(r);
  }
  if (c)
    WinHttpCloseHandle(c);
  if (s)
    WinHttpCloseHandle(s);
  if (!ok) {
    free(buf);
    return NULL;
  }
#else
  // curl, if there is one (the URL and header were checked above: no quotes in them).
  char cmd[1700];
  snprintf(cmd, sizeof cmd, "curl -fsSL -m 20 -A PokeDaisy %s%s%s '%s' 2>/dev/null", header ? "-H '" : "",
           header ? header : "", header ? "'" : "", url);
  FILE *p = popen(cmd, "r");
  if (!p)
    return NULL;
  char chunk[4096];
  size_t got;
  bool over = false;
  while ((got = fread(chunk, 1, sizeof chunk, p)) > 0) {
    if (len + got > max) {
      over = true;
      break;
    }
    char *grown = realloc(buf, len + got + 1);
    if (!grown) {
      over = true;
      break;
    }
    buf = grown;
    memcpy(buf + len, chunk, got);
    len += got;
  }
  if (pclose(p) != 0 || over) {
    free(buf);
    return NULL;
  }
#endif
  if (!buf)
    buf = calloc(1, 1);
  if (buf)
    buf[len] = 0;
  *size = len;
  return buf;
}
