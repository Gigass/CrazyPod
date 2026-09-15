#ifndef CRAZYPOD_GAMEBOY_H
#define CRAZYPOD_GAMEBOY_H

#include "crazypod_gameboy_core.h"

enum crazypod_gameboy_result {
    CRAZYPOD_GAMEBOY_OK,
    CRAZYPOD_GAMEBOY_BAD_ROM,
    CRAZYPOD_GAMEBOY_NO_MEMORY,
    CRAZYPOD_GAMEBOY_IO_ERROR,
    CRAZYPOD_GAMEBOY_BAD_SAVE,
    CRAZYPOD_GAMEBOY_CORE_ERROR
};

/* Scan /MiniApps/Games and its GB/GBC subdirectories; at most 128 entries. */
void crazypod_gameboy_scan(void);
int crazypod_gameboy_count(void);
const char *crazypod_gameboy_title(int index);
enum crazypod_gameboy_result crazypod_gameboy_open(
    int index, void (*audio)(const int16_t *, size_t));
bool crazypod_gameboy_save(void);
void crazypod_gameboy_close(void);
/* False when the loaded cartridge has no RAM or battery to persist, so
 * the menu can say so instead of a save silently doing nothing. */
bool crazypod_gameboy_saves_progress(void);

/* What happened to this game's save file when it was opened. Reported in
 * the game menu: a save that is silently not written and silently not
 * read looks exactly like one that is, so say which it was. */
enum crazypod_gameboy_save_state {
    CRAZYPOD_GAMEBOY_SAVE_UNSUPPORTED = 0,
    CRAZYPOD_GAMEBOY_SAVE_ABSENT,
    CRAZYPOD_GAMEBOY_SAVE_LOADED,
    CRAZYPOD_GAMEBOY_SAVE_WRITTEN,
};
enum crazypod_gameboy_save_state crazypod_gameboy_save_state(void);

#endif
