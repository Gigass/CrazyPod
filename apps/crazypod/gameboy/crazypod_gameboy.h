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

#endif
