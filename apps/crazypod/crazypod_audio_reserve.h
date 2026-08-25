#ifndef CRAZYPOD_AUDIO_RESERVE_H
#define CRAZYPOD_AUDIO_RESERVE_H

#include <stdbool.h>

bool crazypod_audio_reserve_acquire(void);
void crazypod_audio_reserve_release(void);
bool crazypod_audio_reserve_is_held(void);

#endif
