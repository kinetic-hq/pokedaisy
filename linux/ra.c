/* GPL-3.0, see ../NOTICE. RetroAchievements (ra.h): rc_client, the server calls on threads, the GB / GBA memory maps
 * as the Android app reads them (app/src/main/cpp/pokedaisy_ra.c). */
#include "ra.h"
#include "http.h"
#include "pd_menu.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mgba/core/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/memory.h>

#include "rc_client.h"
#include "rc_consoles.h"
#include "../third_party/rcheevos/src/rhash/md5.h"

/* rc_client.c's RC_CLIENT_ACHIEVEMENT_WARNING_ID: ids from here up are its own placeholders. */
#define RA_WARNING_ID 101000001u
#define RA_MAX_CALLS 32
#define RA_MAX_BODY (8u << 20)

static rc_client_t *client;
static struct mCore *core;
static void (*toast)(const char *);
static char iniPath[1100], agent[200], userName[64], loginError[160];
static enum ra_login loginState = RA_LOGGED_OUT;
static bool gameActive, paused;

/* The server's calls: a thread each; the answer waits here for ra_pump (main thread). */
static struct call {
  int id; /* 0 = free */
  rc_client_server_callback_t callback;
  void *data;
  bool ready;
  int status;
  char *body;
  size_t len;
} calls[RA_MAX_CALLS];
static int nextId = 1;
static SDL_mutex *lock;

struct job {
  int id;
  char *url, *post, *type;
};

static char *dup_or_null(const char *s) {
  if (!s)
    return NULL;
  size_t n = strlen(s) + 1;
  char *d = malloc(n);
  if (d)
    memcpy(d, s, n);
  return d;
}

static int fetch(void *arg) {
  struct job *j = arg;
  size_t len = 0;
  int status = 0;
  char *body = http_post(j->url, j->post, j->type, agent, RA_MAX_BODY, &len, &status);
  SDL_LockMutex(lock);
  bool kept = false;
  for (int i = 0; i < RA_MAX_CALLS; i++)
    if (calls[i].id == j->id && !calls[i].ready) {
      calls[i].ready = true;
      calls[i].status = body ? status : RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
      calls[i].body = body;
      calls[i].len = len;
      kept = true;
      break;
    }
  SDL_UnlockMutex(lock);
  if (!kept)
    free(body);
  free(j->url);
  free(j->post);
  free(j->type);
  free(j);
  return 0;
}

static void server_call(const rc_api_request_t *request, rc_client_server_callback_t callback, void *data,
                        rc_client_t *c) {
  (void)c;
  SDL_LockMutex(lock);
  int slot = -1;
  for (int i = 0; i < RA_MAX_CALLS && slot < 0; i++)
    if (!calls[i].id)
      slot = i;
  int id = 0;
  if (slot >= 0) {
    id = nextId++;
    if (nextId <= 0)
      nextId = 1;
    calls[slot] = (struct call){.id = id, .callback = callback, .data = data};
  }
  SDL_UnlockMutex(lock);
  struct job *j = id ? calloc(1, sizeof *j) : NULL;
  if (j) {
    j->id = id;
    j->url = dup_or_null(request->url);
    j->post = dup_or_null(request->post_data);
    j->type = dup_or_null(request->content_type);
  }
  SDL_Thread *t = j && j->url ? SDL_CreateThread(fetch, "ra-http", j) : NULL;
  if (t) {
    SDL_DetachThread(t);
    return;
  }
  // No slot or no thread: fail it now (retryable, so rc_client asks again later).
  if (j) {
    free(j->url);
    free(j->post);
    free(j->type);
    free(j);
  }
  if (id) {
    SDL_LockMutex(lock);
    for (int i = 0; i < RA_MAX_CALLS; i++)
      if (calls[i].id == id)
        calls[i].id = 0;
    SDL_UnlockMutex(lock);
  }
  rc_api_server_response_t r = {NULL, 0, RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR};
  callback(&r, data);
}

/* RA's Game Boy / Color map: $0000-$FFFF the CPU bus (WRAM bank 1 fixed at $D000), then $10000-$15FFF the Color's
 * WRAM banks 2-7. */
static uint32_t read_gb(uint32_t address, uint8_t *buffer, uint32_t num) {
  struct GB *gb = core->board;
  uint32_t done = 0;
  for (; done < num; done++) {
    uint32_t a = address + done;
    if (a >= 0xD000 && a < 0xE000)
      buffer[done] = gb->memory.wram[0x1000 + (a - 0xD000)];
    else if (a < 0x10000)
      buffer[done] = (uint8_t)core->busRead8(core, a);
    else if (a < 0x16000)
      buffer[done] = gb->memory.wram[0x2000 + (a - 0x10000)];
    else
      break;
  }
  return done;
}

/* RA's GBA map: $000000 IWRAM, $008000 EWRAM, $048000 the cart's save RAM. */
static uint32_t read_memory(uint32_t address, uint8_t *buffer, uint32_t num, rc_client_t *c) {
  (void)c;
  if (!core)
    return 0;
  if (core->platform(core) == mPLATFORM_GB)
    return read_gb(address, buffer, num);
  uint32_t done = 0;
  while (done < num) {
    uint32_t a = address + done;
    size_t id, base;
    if (a < 0x8000)
      id = REGION_WORKING_IRAM, base = 0;
    else if (a < 0x48000)
      id = REGION_WORKING_RAM, base = 0x8000;
    else if (a < 0x58000)
      id = REGION_CART_SRAM, base = 0x48000;
    else
      break;
    size_t size = 0;
    const uint8_t *block = core->getMemoryBlock(core, id, &size);
    size_t off = a - base;
    if (!block || off >= size)
      break;
    size_t n = size - off;
    if (n > num - done)
      n = num - done;
    memcpy(buffer + done, block + off, n);
    done += (uint32_t)n;
  }
  return done;
}

static void say(const char *s) {
  if (toast)
    toast(s);
}

static void event(const rc_client_event_t *e, rc_client_t *c) {
  (void)c;
  char line[200];
  switch (e->type) {
  case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
    if (e->achievement && e->achievement->id < RA_WARNING_ID) {
      snprintf(line, sizeof line, "Achievement unlocked: %s (%u)", e->achievement->title ? e->achievement->title : "",
               (unsigned)e->achievement->points);
      say(line);
    }
    break;
  case RC_CLIENT_EVENT_GAME_COMPLETED: {
    const rc_client_game_t *g = rc_client_get_game_info(client);
    snprintf(line, sizeof line, "Every achievement in %s unlocked", g && g->title ? g->title : "this game");
    say(line);
    break;
  }
  case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
    if (e->leaderboard) {
      snprintf(line, sizeof line, "Leaderboard: %s, %s", e->leaderboard->title ? e->leaderboard->title : "",
               e->leaderboard->tracker_value ? e->leaderboard->tracker_value : "");
      say(line);
    }
    break;
  case RC_CLIENT_EVENT_SERVER_ERROR:
    if (e->server_error) {
      snprintf(line, sizeof line, "RetroAchievements: %s",
               e->server_error->error_message ? e->server_error->error_message : "server error");
      say(line);
    }
    break;
  case RC_CLIENT_EVENT_DISCONNECTED:
    say("RetroAchievements: offline, unlocks will be sent later");
    break;
  case RC_CLIENT_EVENT_RECONNECTED:
    say("RetroAchievements: back online, unlocks sent");
    break;
  case RC_CLIENT_EVENT_RESET:
    if (core)
      core->reset(core);
    break;
  default:
    break;
  }
}

static void rc_log(const char *message, const rc_client_t *c) {
  (void)c;
  fprintf(stderr, "ra: %s\n", message);
}

static void ini_write(const char *user, const char *token) {
  if (!*iniPath)
    return;
  if (!user) {
    remove(iniPath);
    return;
  }
  FILE *f = fopen(iniPath, "w");
  if (!f)
    return;
  fprintf(f, "user=%s\ntoken=%s\n", user, token ? token : "");
  fclose(f);
}

static void logged_in(int result, const char *error, rc_client_t *c, void *data) {
  bool fromToken = data != NULL;
  if (result == RC_OK) {
    const rc_client_user_t *u = rc_client_get_user_info(c);
    snprintf(userName, sizeof userName, "%s", u && u->display_name ? u->display_name : u ? u->username : "");
    loginError[0] = 0;
    loginState = RA_LOGGED_IN;
    if (u)
      ini_write(u->username, u->token);
    if (!fromToken) {
      char line[120];
      snprintf(line, sizeof line, "Signed in to RetroAchievements as %s", userName);
      say(line);
    }
    return;
  }
  loginState = RA_LOGGED_OUT;
  snprintf(loginError, sizeof loginError, "%s", error && *error ? error : "The server didn't answer");
  // A token the server turned down is gone for good; a missing network keeps it for next time.
  if (fromToken && (result == RC_INVALID_CREDENTIALS || result == RC_EXPIRED_TOKEN || result == RC_ACCESS_DENIED)) {
    ini_write(NULL, NULL);
    say("RetroAchievements: sign in again in SETTINGS");
  }
}

void ra_init(const char *dataDir, const char *version, void (*sayFn)(const char *)) {
  if (client)
    return;
  toast = sayFn;
  lock = SDL_CreateMutex();
  if (!lock)
    return;
  client = rc_client_create(read_memory, server_call);
  if (!client)
    return;
  rc_client_enable_logging(client, RC_CLIENT_LOG_LEVEL_WARN, rc_log);
  rc_client_set_event_handler(client, event);
  // Softcore, as the Android app: states, rewind and the companion's helpers aren't blocked.
  rc_client_set_hardcore_enabled(client, 0);
  char clause[64] = "";
  rc_client_get_user_agent_clause(client, clause, sizeof clause);
#ifdef _WIN32
  const char *os = "Windows";
#else
  const char *os = "Linux";
#endif
  snprintf(agent, sizeof agent, "PokeDaisy/%s (%s) %s", version && *version ? version : "dev", os, clause);
  snprintf(iniPath, sizeof iniPath, "%s/retroachievements.ini", dataDir);
  FILE *f = fopen(iniPath, "r");
  if (!f)
    return;
  char line[256], user[64] = "", token[128] = "";
  while (fgets(line, sizeof line, f)) {
    line[strcspn(line, "\r\n")] = 0;
    if (!strncmp(line, "user=", 5))
      snprintf(user, sizeof user, "%.63s", line + 5);
    else if (!strncmp(line, "token=", 6))
      snprintf(token, sizeof token, "%.127s", line + 6);
  }
  fclose(f);
  if (*user && *token) {
    loginState = RA_LOGGING_IN;
    rc_client_begin_login_with_token(client, user, token, logged_in, (void *)1);
  }
}

void ra_shutdown(void) {
  if (!client)
    return;
  rc_client_destroy(client);
  client = NULL;
}

enum ra_login ra_login_state(void) { return loginState; }
const char *ra_user(void) { return loginState == RA_LOGGED_IN ? userName : ""; }
const char *ra_login_error(void) { return loginError; }

void ra_login(const char *user, const char *password) {
  if (!client || loginState == RA_LOGGING_IN)
    return;
  loginError[0] = 0;
  loginState = RA_LOGGING_IN;
  rc_client_begin_login_with_password(client, user, password, logged_in, NULL);
}

void ra_logout(void) {
  if (!client)
    return;
  rc_client_logout(client);
  gameActive = false;
  loginState = RA_LOGGED_OUT;
  userName[0] = 0;
  ini_write(NULL, NULL);
}

static void game_loaded(int result, const char *error, rc_client_t *c, void *data) {
  (void)data;
  if (result != RC_OK) {
    gameActive = false;
    if (result == RC_NO_GAME_LOADED || result == RC_NOT_FOUND)
      say("RetroAchievements: no achievements for this ROM");
    else if (result != RC_ABORTED) {
      char line[200];
      snprintf(line, sizeof line, "RetroAchievements: %s", error && *error ? error : "couldn't load the game");
      say(line);
    }
    return;
  }
  rc_client_user_game_summary_t s;
  memset(&s, 0, sizeof s);
  rc_client_get_user_game_summary(c, &s);
  gameActive = s.num_core_achievements > 0;
  char line[200];
  if (gameActive)
    snprintf(line, sizeof line, "RetroAchievements: %u of %u unlocked", (unsigned)s.num_unlocked_achievements,
             (unsigned)s.num_core_achievements);
  else
    snprintf(line, sizeof line, "RetroAchievements: no achievements for this ROM yet");
  say(line);
  if (gameActive && paused)
    say("Achievements are paused while SHINY ODDS is raised");
}

void ra_load_game(struct mCore *c, const uint8_t *rom, size_t size) {
  ra_unload_game();
  core = c;
  if (!client || loginState == RA_LOGGED_OUT || !rom || !size || size > 0x7FFFFFFF)
    return;
  // RetroAchievements' GB / GBA hash: the whole file's MD5.
  md5_state_t md5;
  md5_byte_t digest[16];
  md5_init(&md5);
  md5_append(&md5, rom, (int)size);
  md5_finish(&md5, digest);
  char hash[33];
  for (int i = 0; i < 16; i++)
    snprintf(hash + 2 * i, 3, "%02x", digest[i]);
  rc_client_begin_load_game(client, hash, game_loaded, NULL);
}

void ra_unload_game(void) {
  if (client)
    rc_client_unload_game(client);
  gameActive = false;
  core = NULL;
}

bool ra_game_active(void) { return client && gameActive && rc_client_is_game_loaded(client); }

void ra_set_paused(bool on) {
  if (on == paused)
    return;
  paused = on;
  if (ra_game_active())
    say(on ? "Achievements are paused while SHINY ODDS is raised" : "Achievements are back on");
}

void ra_frame(void) {
  if (client && core && gameActive && !paused)
    rc_client_do_frame(client);
}

void ra_pump(void) {
  if (!client)
    return;
  for (;;) {
    rc_client_server_callback_t callback = NULL;
    void *data = NULL;
    rc_api_server_response_t r = {0};
    char *body = NULL;
    SDL_LockMutex(lock);
    for (int i = 0; i < RA_MAX_CALLS; i++)
      if (calls[i].id && calls[i].ready) {
        callback = calls[i].callback;
        data = calls[i].data;
        body = calls[i].body;
        r.body = body;
        r.body_length = calls[i].len;
        r.http_status_code = calls[i].status;
        calls[i] = (struct call){0};
        break;
      }
    SDL_UnlockMutex(lock);
    if (!callback)
      break;
    callback(&r, data); // outside the lock: it may start the next call
    free(body);
  }
  rc_client_idle(client);
}

void ra_reset(void) {
  if (client && gameActive)
    rc_client_reset(client);
}

static void side_path(char *out, size_t size, const char *statePath) { snprintf(out, size, "%s.ra", statePath); }

void ra_progress_save(const char *statePath) {
  if (!ra_game_active())
    return;
  char p[1200];
  side_path(p, sizeof p, statePath);
  size_t size = rc_client_progress_size(client);
  uint8_t *buf = size ? malloc(size) : NULL;
  if (buf && rc_client_serialize_progress_sized(client, buf, size) == RC_OK) {
    FILE *f = fopen(p, "wb");
    if (f) {
      fwrite(buf, 1, size, f);
      fclose(f);
    }
  }
  free(buf);
}

void ra_progress_load(const char *statePath) {
  if (!ra_game_active())
    return;
  char p[1200];
  side_path(p, sizeof p, statePath);
  FILE *f = fopen(p, "rb");
  uint8_t *buf = NULL;
  long n = 0;
  if (f) {
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    rewind(f);
    if (n > 0 && n < (8 << 20)) {
      buf = malloc((size_t)n);
      if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
      }
    }
    fclose(f);
  }
  // A state from before (no progress beside it): every achievement starts over from that point.
  rc_client_deserialize_progress_sized(client, buf, buf ? (size_t)n : 0);
  free(buf);
}

/* Text the menu can draw: control bytes (its own markers, tabs) become spaces. */
static void clean(char *out, size_t size, const char *s) {
  size_t n = 0;
  for (; s && *s && n + 1 < size; s++)
    out[n++] = (unsigned char)*s < 0x20 ? ' ' : *s;
  out[n] = 0;
}

int ra_rows(char rows[][96], char desc[][160], int max) {
  if (!ra_game_active())
    return 0;
  rc_client_achievement_list_t *list =
      rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  if (!list)
    return 0;
  int n = 0;
  for (uint32_t b = 0; b < list->num_buckets && n < max; b++) {
    const rc_client_achievement_bucket_t *bucket = &list->buckets[b];
    bool any = false;
    for (uint32_t i = 0; i < bucket->num_achievements; i++)
      any |= bucket->achievements[i]->id < RA_WARNING_ID;
    if (!any)
      continue;
    char label[64];
    clean(label, sizeof label, bucket->label ? bucket->label : "");
    for (char *p = label; *p; p++)
      if (*p >= 'a' && *p <= 'z')
        *p = (char)(*p - 'a' + 'A');
    snprintf(rows[n], 96, PD_MENU_GROUP "%s", label);
    desc[n++][0] = 0;
    for (uint32_t i = 0; i < bucket->num_achievements && n < max; i++) {
      const rc_client_achievement_t *a = bucket->achievements[i];
      if (a->id >= RA_WARNING_ID)
        continue;
      char title[72];
      clean(title, sizeof title, a->title);
      if (a->unlocked)
        snprintf(rows[n], 96, "%s\tDONE", title);
      else if (a->measured_progress[0])
        snprintf(rows[n], 96, "%s\t%s", title, a->measured_progress);
      else
        snprintf(rows[n], 96, "%s\t%u PTS", title, (unsigned)a->points);
      clean(desc[n], 160, a->description);
      n++;
    }
  }
  rc_client_destroy_achievement_list(list);
  return n;
}

void ra_summary(char *out, size_t size) {
  if (!size)
    return;
  out[0] = 0;
  if (!ra_game_active())
    return;
  rc_client_user_game_summary_t s;
  memset(&s, 0, sizeof s);
  rc_client_get_user_game_summary(client, &s);
  snprintf(out, size, "%u OF %u UNLOCKED, %u OF %u POINTS", (unsigned)s.num_unlocked_achievements,
           (unsigned)s.num_core_achievements, (unsigned)s.points_unlocked, (unsigned)s.points_core);
}
