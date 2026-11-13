/* FF MUSIC's STEADY on the desktop: each song the game plays is recorded on its own, on a second mGBA core booted with
 * the same ROM (never the player's game or save), cut where the sound engine itself loops, and kept as a WAV. While
 * the game races, the song's recording plays at its normal speed instead of the game's sped-up sound. Ported from the
 * Android app (FfMusicRenderer.kt, M4aSongs.kt, FfMusicKey.kt, M4aLoop.kt, FfMusicCache.kt). GBA games only.
 * GPL-3.0, see ../NOTICE. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mCore;
struct ffm;

/* The recorder for this ROM (its own copy of [rom]), its clips under ROOT/ffmusic. Its thread finds the ROM's sound
 * engine first; until then, and on a ROM whose engine it doesn't know, nothing gets recorded. */
struct ffm *ffm_start(const uint8_t *rom, size_t len, uint32_t crc, const char *root);
void ffm_stop(struct ffm *f);

/* The song [c]'s background-music player is playing: its header's ROM offset, 0 for none. Between frames. */
uint32_t ffm_key(struct mCore *c);

/* Record [key]'s song soon, unless it's kept already. Marks it played (the songs played longest ago make room). */
void ffm_request(struct ffm *f, uint32_t key);

enum ffm_state { FFM_NEVER, FFM_COMING, FFM_READY };
/* Whether [key] has a recording (READY), is getting one (COMING) or never will (NEVER: play it sped up). */
enum ffm_state ffm_state(struct ffm *f, uint32_t key);

/* A recording loaded to play: the song's intro once, then its loop over and over. */
struct ffm_clip {
  uint32_t key;
  int16_t *intro, *loop; /* interleaved stereo at FFM_RATE */
  size_t introFrames, loopFrames;
  uint64_t at; /* where playback is, in frames from the song's start */
};
#define FFM_RATE 32768

bool ffm_load(struct ffm *f, uint32_t key, struct ffm_clip *out);
void ffm_clip_free(struct ffm_clip *clip);
/* [frames] stereo frames of the clip from where it is, moving on. */
void ffm_clip_fill(struct ffm_clip *clip, int16_t *out, size_t frames);

/* Exposed for the tests. */
bool ffm_locate_table(const uint8_t *rom, size_t len, uint32_t *songNumStart, uint32_t *table);
bool ffm_splice(const int16_t *pcm, size_t len, size_t loopStart, size_t loopEnd, int rate, size_t *from,
                size_t *clipLen);
