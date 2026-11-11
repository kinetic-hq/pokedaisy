/* The game list's games: DATA/roms and the folders named in DATA/roms-folders.txt, each with its subfolders.
 * GPL-3.0, see ../NOTICE. */
#ifndef POCKET_ROM_LIST_H
#define POCKET_ROM_LIST_H
#include <stdbool.h>
#include <stddef.h>

#define POCKET_ROM_LIST_MAX 4096
#define POCKET_ROM_FOLDERS_FILE "roms-folders.txt"

struct rom_list {
  char **paths;  // full paths, for opening
  char **labels; // the file names, as the list shows them
  int count, cap;
};

/* A file name the host plays: .gba / .gbc / .gb, and .nds when NDS. */
bool rom_list_playable(const char *name, bool nds);
/* Adds every game in FOLDER and in its folders one level down. */
void rom_list_scan(struct rom_list *l, const char *folder, bool nds);
#define POCKET_FOLDERS_MAX 32
#define POCKET_FOLDER_MAX 1024
/* The added game folders, DATA/roms-folders.txt's lines (blank lines and # comments skipped, surrounding spaces and
 * quotes and a trailing separator dropped): how many, at most MAX. */
int rom_folders_read(const char *root, char folders[][POCKET_FOLDER_MAX], int max);
/* Writes the list back (through a temporary file and a rename). */
bool rom_folders_write(const char *root, char folders[][POCKET_FOLDER_MAX], int count);
/* ROM sits in one of the added folders (or below it). */
bool rom_in_added_folder(const char *root, const char *rom);
/* DATA/roms, then each line of DATA/roms-folders.txt (blank lines and # comments skipped, surrounding spaces and
 * quotes dropped); sorted by label, a path listed twice kept once. */
void rom_list_load(struct rom_list *l, const char *root, bool nds);
void rom_list_free(struct rom_list *l);

/* SAVES BESIDE GAMES: the battery save beside ROM, named as RetroArch / the Android app name it: the ROM's file
 * name with its extension swapped for .sav ("Game.gba" -> "Game.sav"). */
void rom_save_beside(char *out, size_t size, const char *rom);
/* Copies FROM to TO through TO.tmp and a rename; false when it couldn't. FROM is left alone. */
bool rom_copy_file(const char *from, const char *to);
#endif
