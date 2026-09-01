#ifndef CRAZYPOD_GAMEBOY_SCREEN_H
#define CRAZYPOD_GAMEBOY_SCREEN_H

#include "../../../gameboy/crazypod_gameboy.h"

enum crazypod_gameboy_result crazypod_gameboy_screen_run(int index);
const char *crazypod_gameboy_screen_error(
    enum crazypod_gameboy_result result);

#endif
