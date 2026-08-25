#ifndef TEST_CRAZYPOD_PLAYLIST_PLAYLIST_H
#define TEST_CRAZYPOD_PLAYLIST_PLAYLIST_H

#include <stdbool.h>
#include <stddef.h>

#include "file.h"
#include "metadata.h"

enum {
    PLAYLIST_PREPEND = -1,
    PLAYLIST_INSERT_FIRST = -4,
};

struct mutex {
    int unused;
};

static inline void mutex_init(struct mutex *mutex)
{
    mutex->unused = 0;
}

static inline void mutex_lock(struct mutex *mutex)
{
    (void)mutex;
}

static inline void mutex_unlock(struct mutex *mutex)
{
    (void)mutex;
}

struct playlist_info {
    int fd;
    int control_fd;
    int max_playlist_size;
    int index;
    int amount;
    bool started;
    struct mutex mutex;
};

struct playlist_track_info {
    char filename[MAX_PATH];
    int index;
    int display_index;
};

void playlist_init(void);
int playlist_insert_track(struct playlist_info *playlist,
                          const char *filename, int position,
                          bool queued, bool sync);
void playlist_start(int start_index, unsigned long elapsed,
                    unsigned long offset);

#endif
