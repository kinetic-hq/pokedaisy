/* ADD TO DESKTOP: a Windows shortcut (.lnk) or a Linux desktop entry (.desktop) that starts PokeDaisy straight into one
 * game. GPL-3.0, see ../NOTICE. */
#include "shortcut.h"
#include "pocket_os.h"
#include "zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#else
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

void shortcut_file_name(const char *name, char *out, size_t size) {
  size_t n = 0;
  for (const unsigned char *s = (const unsigned char *)name; *s && n + 1 < size; s++)
    if (*s >= 32 && !strchr("\\/:*?\"<>|", *s))
      out[n++] = (char)*s;
  out[n] = 0;
  while (n && (out[n - 1] == ' ' || out[n - 1] == '.'))
    out[--n] = 0;
  size_t lead = strspn(out, " .");
  memmove(out, out + lead, strlen(out + lead) + 1);
  if (!*out)
    snprintf(out, size, "Game");
}

void shortcut_exec_line(const char *const *args, int count, char *out, size_t size) {
  size_t n = 0;
#define PUT(c)                                                                                                         \
  do {                                                                                                                 \
    if (n + 1 < size)                                                                                                  \
      out[n++] = (c);                                                                                                  \
  } while (0)
  for (int i = 0; i < count; i++) {
    if (i)
      PUT(' ');
    PUT('"');
    for (const char *s = args[i]; *s; s++) {
      // " ` $ \ get the quoting's backslash, and every backslash is doubled for the file's own escapes.
      int slashes = *s == '"' || *s == '`' || *s == '$' ? 2 : *s == '\\' ? 3 : 0;
      for (int k = 0; k < slashes; k++)
        PUT('\\');
      if (*s == '%')
        PUT('%');
      PUT(*s);
    }
    PUT('"');
  }
#undef PUT
  out[n < size ? n : size - 1] = 0;
}

static void be32(unsigned char *p, unsigned long v) {
  p[0] = (unsigned char)(v >> 24), p[1] = (unsigned char)(v >> 16), p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)v;
}
static unsigned char *chunk(unsigned char *p, const char *type, const unsigned char *data, unsigned long len) {
  be32(p, len);
  memcpy(p + 4, type, 4);
  if (len)
    memcpy(p + 8, data, len);
  be32(p + 8 + len, crc32(0, p + 4, (unsigned)(len + 4)));
  return p + 12 + len;
}
unsigned char *shortcut_png(const unsigned char *rgba, int size, size_t *len) {
  size_t raw = (size_t)size * (size * 4 + 1);
  unsigned char *rows = malloc(raw);
  uLongf zlen = compressBound((uLong)raw);
  unsigned char *z = malloc(zlen), *png = NULL;
  if (!rows || !z)
    goto done;
  for (int y = 0; y < size; y++) {
    rows[(size_t)y * (size * 4 + 1)] = 0; // no filter
    memcpy(rows + (size_t)y * (size * 4 + 1) + 1, rgba + (size_t)y * size * 4, (size_t)size * 4);
  }
  if (compress2(z, &zlen, rows, (uLong)raw, 9) != Z_OK)
    goto done;
  png = malloc(8 + 25 + 12 + zlen + 12);
  if (!png)
    goto done;
  memcpy(png, "\x89PNG\r\n\x1a\n", 8);
  unsigned char ihdr[13] = {0};
  be32(ihdr, (unsigned long)size), be32(ihdr + 4, (unsigned long)size);
  ihdr[8] = 8, ihdr[9] = 6; // 8 bits, RGBA
  unsigned char *p = chunk(png + 8, "IHDR", ihdr, 13);
  p = chunk(p, "IDAT", z, zlen);
  p = chunk(p, "IEND", NULL, 0);
  *len = (size_t)(p - png);
done:
  free(rows);
  free(z);
  return png;
}
unsigned char *shortcut_ico(const unsigned char *png, size_t pngLen, int size, size_t *len) {
  unsigned char *ico = malloc(22 + pngLen);
  if (!ico)
    return NULL;
  memset(ico, 0, 22);
  ico[2] = 1, ico[4] = 1; // an icon, one image
  ico[6] = ico[7] = (unsigned char)(size >= 256 ? 0 : size); // 0 means 256
  ico[10] = 1, ico[12] = 32;                                 // planes, bits
  for (int i = 0; i < 4; i++)
    ico[14 + i] = (unsigned char)(pngLen >> (8 * i)), ico[18 + i] = (unsigned char)(22u >> (8 * i));
  memcpy(ico + 22, png, pngLen);
  *len = 22 + pngLen;
  return ico;
}

static bool write_file(const char *path, const unsigned char *data, size_t len) {
  char tmp[1600];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return false;
  bool ok = fwrite(data, 1, len, f) == len;
  ok = fclose(f) == 0 && ok;
  return ok && pocket_replace(tmp, path);
}

/* The icon file in ROOT/shortcuts (ICO on Windows, PNG elsewhere); false with no icon or when it couldn't be kept. */
static bool icon_file(const char *root, const char *file, const unsigned char *icon, int size, char *out, size_t n) {
  if (!icon)
    return false;
  char dir[1400];
#ifdef _WIN32
  snprintf(dir, sizeof dir, "%.1300s\\shortcuts", root);
#else
  snprintf(dir, sizeof dir, "%.1300s/shortcuts", root);
#endif
  pocket_mkdir(dir);
  size_t pngLen = 0, len = 0;
  unsigned char *png = shortcut_png(icon, size, &pngLen), *data = png;
  if (!png)
    return false;
#ifdef _WIN32
  data = shortcut_ico(png, pngLen, size, &len);
  snprintf(out, n, "%s\\%s.ico", dir, file);
#else
  len = pngLen;
  snprintf(out, n, "%s/%s.png", dir, file);
#endif
  bool ok = data && write_file(out, data, len);
  if (data != png)
    free(data);
  free(png);
  return ok;
}

#ifdef _WIN32
static wchar_t *wide(const char *s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  wchar_t *w = n > 0 ? malloc((size_t)n * sizeof *w) : NULL;
  if (w && !MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n)) {
    free(w);
    w = NULL;
  }
  return w;
}
static void full_path(const char *path, char *out, size_t size) {
  wchar_t *w = wide(path), full[MAX_PATH * 4];
  if (w && GetFullPathNameW(w, MAX_PATH * 4, full, NULL))
    WideCharToMultiByte(CP_UTF8, 0, full, -1, out, (int)size, NULL, NULL);
  else
    snprintf(out, size, "%s", path);
  free(w);
}
int shortcut_create(const char *root, const char *rom, const char *name, const unsigned char *icon, int size,
                    const char *iconProgram, char *out, size_t outSize) {
  char file[256], data[1200], game[1600], ico[1800] = "", desk8[MAX_PATH * 4];
  wchar_t desk[MAX_PATH], exe[MAX_PATH * 4];
  shortcut_file_name(name, file, sizeof file);
  if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk) != S_OK ||
      !GetModuleFileNameW(NULL, exe, MAX_PATH * 4))
    return SHORTCUT_FAILED;
  WideCharToMultiByte(CP_UTF8, 0, desk, -1, desk8, sizeof desk8, NULL, NULL);
  snprintf(out, outSize, "%s\\%s.lnk", desk8, file);
  if (pocket_exists(out))
    return SHORTCUT_THERE;
  full_path(root, data, sizeof data);
  full_path(rom, game, sizeof game);
  bool hasIcon = icon_file(data, file, icon, size, ico, sizeof ico);
  if (!hasIcon && iconProgram && *iconProgram) {
    snprintf(ico, sizeof ico, "%s", iconProgram);
    for (char *p = ico; *p; p++)
      if (*p == '/')
        *p = '\\';
    hasIcon = true;
  }
  char args[3000];
  snprintf(args, sizeof args, "--data \"%s\" \"%s\"", data, game);
  HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  IShellLinkW *link = NULL;
  IPersistFile *pf = NULL;
  bool ok = false;
  wchar_t *wArgs = wide(args), *wName = wide(name), *wIco = wide(ico), *wOut = wide(out);
  wchar_t dir[MAX_PATH * 4];
  wcscpy(dir, exe);
  wchar_t *cut = wcsrchr(dir, L'\\');
  if (cut)
    *cut = 0;
  if (wArgs && wName && wIco && wOut &&
      SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link))) {
    IShellLinkW_SetPath(link, exe);
    IShellLinkW_SetArguments(link, wArgs);
    IShellLinkW_SetWorkingDirectory(link, dir);
    IShellLinkW_SetDescription(link, wName);
    if (hasIcon)
      IShellLinkW_SetIconLocation(link, wIco, 0);
    if (SUCCEEDED(IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&pf))) {
      ok = SUCCEEDED(IPersistFile_Save(pf, wOut, TRUE));
      IPersistFile_Release(pf);
    }
    IShellLinkW_Release(link);
  }
  free(wArgs), free(wName), free(wIco), free(wOut);
  if (SUCCEEDED(init))
    CoUninitialize();
  return ok ? SHORTCUT_MADE : SHORTCUT_FAILED;
}
#else
static void full_path(const char *path, char *out, size_t size) {
  char buf[PATH_MAX];
  snprintf(out, size, "%s", realpath(path, buf) ? buf : path);
}
/* XDG_DESKTOP_DIR from the environment or ~/.config/user-dirs.dirs, else ~/Desktop. */
static void desktop_dir(char *out, size_t size) {
  const char *home = getenv("HOME") ? getenv("HOME") : ".";
  const char *env = getenv("XDG_DESKTOP_DIR");
  if (env && *env) {
    snprintf(out, size, "%s", env);
    return;
  }
  snprintf(out, size, "%s/Desktop", home);
  char cfg[1200], line[1200];
  const char *xdg = getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg)
    snprintf(cfg, sizeof cfg, "%s/user-dirs.dirs", xdg);
  else
    snprintf(cfg, sizeof cfg, "%s/.config/user-dirs.dirs", home);
  FILE *f = fopen(cfg, "r");
  if (!f)
    return;
  while (fgets(line, sizeof line, f)) {
    char *v = strstr(line, "XDG_DESKTOP_DIR=\"");
    if (v != line)
      continue;
    v += strlen("XDG_DESKTOP_DIR=\"");
    char *end = strrchr(v, '"');
    if (!end)
      break;
    *end = 0;
    if (!strncmp(v, "$HOME", 5))
      snprintf(out, size, "%s%s", home, v + 5);
    else
      snprintf(out, size, "%s", v);
    break;
  }
  fclose(f);
}
int shortcut_create(const char *root, const char *rom, const char *name, const unsigned char *icon, int size,
                    const char *iconProgram, char *out, size_t outSize) {
  (void)iconProgram; // a desktop entry's Icon can't be a Windows program
  char file[256], desk[1100], data[PATH_MAX], game[PATH_MAX], exe[PATH_MAX], png[1800] = "";
  shortcut_file_name(name, file, sizeof file);
  desktop_dir(desk, sizeof desk);
  if (!pocket_is_dir(desk))
    return SHORTCUT_FAILED;
  snprintf(out, outSize, "%s/%s.desktop", desk, file);
  if (pocket_exists(out))
    return SHORTCUT_THERE;
  const char *appimage = getenv("APPIMAGE"); // an AppImage runs from a mount that goes away: the image itself
  ssize_t n = appimage ? -1 : readlink("/proc/self/exe", exe, sizeof exe - 1);
  if (appimage)
    snprintf(exe, sizeof exe, "%s", appimage);
  else if (n <= 0)
    return SHORTCUT_FAILED;
  else
    exe[n] = 0;
  full_path(root, data, sizeof data);
  full_path(rom, game, sizeof game);
  bool hasIcon = icon_file(data, file, icon, size, png, sizeof png);
  const char *args[] = {exe, "--data", data, game};
  char exec[6000], text[8000], shown[300];
  shortcut_exec_line(args, 4, exec, sizeof exec);
  size_t k = 0; // the name on one line
  for (const char *s = name; *s && k + 1 < sizeof shown; s++)
    shown[k++] = *s == '\n' || *s == '\r' ? ' ' : *s;
  shown[k] = 0;
  snprintf(text, sizeof text,
           "[Desktop Entry]\nType=Application\nName=%s\nComment=Play in PokeDaisy\nExec=%s\n%s%s%sTerminal=false\n"
           "Categories=Game;\n",
           shown, exec, hasIcon ? "Icon=" : "", hasIcon ? png : "", hasIcon ? "\n" : "");
  if (!write_file(out, (const unsigned char *)text, strlen(text)))
    return SHORTCUT_FAILED;
  chmod(out, 0755); // desktops start only an entry marked as a program
  return SHORTCUT_MADE;
}
#endif
