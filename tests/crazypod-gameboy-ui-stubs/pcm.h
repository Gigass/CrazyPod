#ifndef GAMEBOY_UI_TEST_PCM_H
#define GAMEBOY_UI_TEST_PCM_H
#include <stddef.h>
typedef void (*pcm_play_callback_type)(const void **, size_t *);
void pcm_play_lock(void);
void pcm_play_unlock(void);
#endif
