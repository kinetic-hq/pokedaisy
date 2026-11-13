#!/bin/sh
# TrimUI launcher (an app folder in CrossMix's Apps, or a NextUI Tools pak). No firmware power, library or save
# migration. On CrossMix the CPU's lowest speed goes up while the game runs (its own cpufreq.sh: ondemand, 1.4 to
# 2.0 GHz) and the earlier settings come back after: at ondemand's 1.2 GHz a frame of the game plus EXPAND came
# out longer than the screen's refresh. POCKET_CPU=keep leaves the CPU alone.
set -eu
PAK_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
cd "$PAK_DIR"
export POCKET_FULLSCREEN=1
export POCKET_RAW_TRIMUI="${POCKET_RAW_TRIMUI:-1}"
mkdir -p roms saves states
if [ -f launch.log ] && [ "$(wc -c < launch.log)" -gt 1048576 ]; then
  mv -f launch.log launch.previous.log
fi
CPUFREQ=/mnt/SDCARD/System/usr/trimui/scripts/cpufreq.sh
CPU=/sys/devices/system/cpu/cpu0/cpufreq
restore=""
if [ "${POCKET_CPU:-}" != keep ] && [ -x "$CPUFREQ" ] && [ -r "$CPU/scaling_governor" ]; then
  restore="$(cat "$CPU/scaling_governor") $(cat "$CPU/scaling_min_freq") $(cat "$CPU/scaling_max_freq")"
  "$CPUFREQ" ondemand 5 8 >> launch.log 2>&1 || restore=""
fi
status=0
# The program: PokeDaisy since 1.18.2, pokedaisy-linux in older folders.
program=./PokeDaisy
[ -x "$program" ] || program=./pokedaisy-linux
"$program" --data "$PAK_DIR" "$@" >> "$PAK_DIR/launch.log" 2>&1 || status=$?
if [ -n "$restore" ]; then
  # shellcheck disable=SC2086 # three words, split on purpose
  set -- $restore
  echo "$1" > "$CPU/scaling_governor" 2>/dev/null || true
  # The order the kernel takes them in: whichever way the range moves.
  echo "$2" > "$CPU/scaling_min_freq" 2>/dev/null || true
  echo "$3" > "$CPU/scaling_max_freq" 2>/dev/null || true
  echo "$2" > "$CPU/scaling_min_freq" 2>/dev/null || true
fi
exit "$status"
