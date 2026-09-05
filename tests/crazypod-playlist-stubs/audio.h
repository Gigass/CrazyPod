#ifndef TEST_CRAZYPOD_PLAYLIST_AUDIO_H
#define TEST_CRAZYPOD_PLAYLIST_AUDIO_H

#include "metadata.h"

#define AUDIO_STATUS_PLAY 0x01
#define AUDIO_STATUS_PAUSE 0x02

int audio_status(void);
struct mp3entry *audio_current_track(void);
void audio_stop(void);
void audio_pause(void);
void audio_play(unsigned long elapsed, unsigned long offset);
void audio_resume(void);

#endif
