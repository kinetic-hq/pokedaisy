/* Fault injection around upstream's real VFile fallback; no game data. */
#include "host_io.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pocket_os.h"
#include "pd_settings.h"
struct fault_file {
  struct VFile d;
  struct VFile *inner;
  bool noMap, failRead, failWrite, failSync, failTruncate, failClose;
  int writes, unmaps, closes;
};
static bool fc(struct VFile *v) {
  struct fault_file *f = (void *)v;
  f->closes++;
  bool ok = f->inner->close(f->inner);
  return !f->failClose && ok;
}
static off_t fs(struct VFile *v, off_t o, int w) {
  struct fault_file *f = (void *)v;
  return f->inner->seek(f->inner, o, w);
}
static ssize_t fr(struct VFile *v, void *p, size_t n) {
  struct fault_file *f = (void *)v;
  return f->failRead ? -1 : f->inner->read(f->inner, p, n);
}
static ssize_t fl(struct VFile *v, char *p, size_t n) {
  struct fault_file *f = (void *)v;
  return f->inner->readline(f->inner, p, n);
}
static ssize_t fw(struct VFile *v, const void *p, size_t n) {
  struct fault_file *f = (void *)v;
  f->writes++;
  return f->failWrite ? -1 : f->inner->write(f->inner, p, n);
}
static void *fm(struct VFile *v, size_t n, int mode) {
  struct fault_file *f = (void *)v;
  return f->noMap ? NULL : f->inner->map(f->inner, n, mode);
}
static void fu(struct VFile *v, void *p, size_t n) {
  struct fault_file *f = (void *)v;
  f->unmaps++;
  f->inner->unmap(f->inner, p, n);
}
static void ft(struct VFile *v, size_t n) {
  struct fault_file *f = (void *)v;
  if (!f->failTruncate)
    f->inner->truncate(f->inner, n);
}
static ssize_t fz(struct VFile *v) {
  struct fault_file *f = (void *)v;
  return f->inner->size(f->inner);
}
static bool fy(struct VFile *v, void *p, size_t n) {
  struct fault_file *f = (void *)v;
  return !f->failSync && f->inner->sync(f->inner, p, n);
}
static struct fault_file file(bool failMap) {
  unsigned char seed[] = {1, 2, 3, 4};
  struct fault_file f = {.inner = VFileMemChunk(seed, 4), .noMap = failMap};
  assert(f.inner);
  f.d = (struct VFile){.close = fc,
                       .seek = fs,
                       .read = fr,
                       .readline = fl,
                       .write = fw,
                       .map = fm,
                       .unmap = fu,
                       .truncate = ft,
                       .size = fz,
                       .sync = fy};
  return f;
}
static void count_backup(const char *name, void *user) {
  *(int *)user += !strncmp(name, "game.gba.backup-", 16) && strstr(name, ".sav") && !strstr(name, ".tmp");
}
static int backups(const char *dir) {
  char path[600];
  snprintf(path, sizeof path, "%s/" POCKET_BACKUP_DIR, dir);
  int n = 0;
  pocket_list_dir(path, count_backup, &n);
  return n;
}
static void check_backup(const char *name, void *user) {
  char path[700];
  snprintf(path, sizeof path, "%s/" POCKET_BACKUP_DIR "/%s", (const char *)user, name);
  FILE *f = fopen(path, "rb");
  assert(f && fgetc(f) >= 20 - POCKET_BACKUP_KEEP); // the newest are kept
  fclose(f);
  assert(pocket_remove(path));
}
static void write_save(const char *path, unsigned char fill, unsigned char clock) {
  unsigned char data[512 + 16];
  memset(data, fill, 512);
  memset(data + 512, clock, 16);
  FILE *f = fopen(path, "wb");
  assert(f && fwrite(data, 1, sizeof data, f) == sizeof data && !fclose(f));
}
static void test_backups(void) {
  char dir[500], save[600];
  snprintf(dir, sizeof dir, "%s/pd-linux-backup-XXXXXX", pocket_temp_root());
  assert(pocket_mkdtemp(dir));
  snprintf(save, sizeof save, "%s/game.gba.sav", dir);
  time_t t = 1760000000;
  assert(pocket_save_backup(save, t) && backups(dir) == 0); // no save yet
  write_save(save, 1, 0);
  assert(pocket_save_backup(save, t++) && backups(dir) == 1);
  assert(pocket_save_backup(save, t++) && backups(dir) == 1); // unchanged
  write_save(save, 1, 9);
  assert(pocket_save_backup(save, t++) && backups(dir) == 1); // only the clock moved
  for (int i = 2; i < 20; i++) {
    write_save(save, (unsigned char)i, 0);
    assert(pocket_save_backup(save, t++));
    assert(backups(dir) == (i < POCKET_BACKUP_KEEP ? i : POCKET_BACKUP_KEEP));
  }
  char path[700];
  snprintf(path, sizeof path, "%s/" POCKET_BACKUP_DIR, dir);
  assert(pocket_list_dir(path, check_backup, dir));
  assert(pocket_rmdir(path));
  assert(pocket_remove(save));
  assert(pocket_rmdir(dir));
}
/* State writes go to NAME.tmp, then over NAME in one step, also where NAME
 * exists already (Windows' rename() refuses that; MoveFileEx doesn't). */
static void test_replace(void) {
  char dir[500], a[600], b[600];
  snprintf(dir, sizeof dir, "%s/pd-linux-replace-XXXXXX", pocket_temp_root());
  assert(pocket_mkdtemp(dir) && pocket_exists(dir));
  snprintf(a, sizeof a, "%s/state.ss", dir);
  snprintf(b, sizeof b, "%s/state.ss.tmp", dir);
  write_save(a, 1, 0);
  write_save(b, 2, 0);
  assert(pocket_readable(a) && pocket_replace(b, a));
  assert(!pocket_exists(b));
  FILE *f = fopen(a, "rb");
  assert(f && fgetc(f) == 2);
  fclose(f);
  assert(!pocket_replace(b, a)); // nothing to move
  // settings.ini is written over itself the same way (pd_settings_save): the second save must replace the first.
  char ini[600];
  snprintf(ini, sizeof ini, "%s/settings.ini", dir);
  struct pd_settings set, back;
  pd_settings_defaults(&set);
  set.ffSpeed = 3;
  assert(pd_settings_save(&set, ini));
  set.ffSpeed = 4;
  assert(pd_settings_save(&set, ini));
  pd_settings_defaults(&back);
  assert(pd_settings_load(&back, ini) && back.ffSpeed == 4);
  assert(pocket_remove(ini));
  assert(pocket_remove(a) && !pocket_exists(a));
  assert(pocket_mkdir(dir) && pocket_rmdir(dir)); // mkdir of an existing one is fine
  char list[] = "640x480,,1280x720", *next = list;
  assert(!strcmp(pocket_strsep(&next, ","), "640x480") && !*pocket_strsep(&next, ","));
  assert(!strcmp(pocket_strsep(&next, ","), "1280x720") && !next);
}
int main(void) {
  test_backups();
  test_replace();
  struct pocket_save_status status;
  struct fault_file f = file(true);
  struct VFile *v = pocket_save_wrap(&f.d, &status);
  assert(v);
  assert(v->seek(v, 2, SEEK_SET) == 2);
  unsigned char *p = v->map(v, 8, MAP_WRITE);
  assert(p && p[0] == 1 && p[3] == 4 && p[7] == 0);
  assert(v->seek(v, 0, SEEK_CUR) == 2);
  p[0] = 9;
  assert(v->sync(v, p, 8));
  assert(v->seek(v, 0, SEEK_CUR) == 2);
  unsigned char out[8];
  assert(f.inner->seek(f.inner, 0, SEEK_SET) == 0);
  assert(f.inner->read(f.inner, out, 8) == 8 && out[0] == 9);
  v->unmap(v, p, 8);
  assert(!f.unmaps);
  assert(v->close(v));
  assert(status.closed && !status.failed && f.closes == 1);
  f = file(false);
  v = pocket_save_wrap(&f.d, &status);
  p = v->map(v, 4, MAP_WRITE);
  assert(p && p[0] == 1);
  v->unmap(v, p, 4);
  assert(f.unmaps == 1);
  assert(v->close(v) && !status.failed);
  f = file(true);
  f.failRead = true;
  v = pocket_save_wrap(&f.d, &status);
  p = v->map(v, 4, MAP_WRITE);
  assert(p && status.failed);
  p[0] = 77;
  assert(!v->sync(v, p, 4));
  v->unmap(v, p, 4);
  assert(f.writes == 0);
  v->close(v);
  f = file(true);
  v = pocket_save_wrap(&f.d, &status);
  p = v->map(v, 4, MAP_WRITE);
  assert(p);
  f.failWrite = true;
  assert(!v->sync(v, p, 4) && status.failed);
  v->unmap(v, p, 4);
  assert(f.writes == 1);
  v->close(v);
  f = file(true);
  v = pocket_save_wrap(&f.d, &status);
  f.failSync = true;
  assert(!v->sync(v, NULL, 0) && status.failed);
  v->close(v);
  f = file(true);
  v = pocket_save_wrap(&f.d, &status);
  f.failTruncate = true;
  v->truncate(v, 8);
  assert(status.failed);
  v->close(v);
  f = file(false);
  v = pocket_save_wrap(&f.d, &status);
  f.failClose = true;
  assert(!v->close(v) && status.failed && status.closed);
  assert(!pocket_save_wrap(NULL, &status) && status.failed);
  puts("PASS save mapping/fallback, latched I/O failures, ownership, bounded backups and replacing writes");
}
