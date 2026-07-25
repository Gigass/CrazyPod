#ifndef CRAZYPOD_LYRICS_H
#define CRAZYPOD_LYRICS_H

#include <stdbool.h>
#include <stdint.h>

bool crazypod_lyrics_load(const char *track_path);
bool crazypod_lyrics_available(void);
void crazypod_lyrics_window(uint32_t elapsed_ms,
                            const char **previous,
                            const char **current,
                            const char **next);

#endif
