#ifndef CRAZYPOD_LYRICS_H
#define CRAZYPOD_LYRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum crazypod_lyrics_status {
    CRAZYPOD_LYRICS_EMPTY = 0,
    CRAZYPOD_LYRICS_SYNCED,
    CRAZYPOD_LYRICS_PLAIN,
    CRAZYPOD_LYRICS_NOT_FOUND,
    CRAZYPOD_LYRICS_INVALID,
    CRAZYPOD_LYRICS_CAPACITY,
};

bool crazypod_lyrics_load(const char *track_path);
bool crazypod_lyrics_available(void);
bool crazypod_lyrics_synchronized(void);
enum crazypod_lyrics_status crazypod_lyrics_get_status(void);
int crazypod_lyrics_line_count(void);
const char *crazypod_lyrics_line_text(int index);
uint32_t crazypod_lyrics_line_time(int index);
const char *crazypod_lyrics_plain_text(void);
int crazypod_lyrics_display_page_count(size_t capacity);
bool crazypod_lyrics_copy_display_page(
    int index, char *destination, size_t capacity);
int crazypod_lyrics_current_line(uint32_t elapsed_ms);
void crazypod_lyrics_window(uint32_t elapsed_ms,
                            const char **previous,
                            const char **current,
                            const char **next);

#endif
