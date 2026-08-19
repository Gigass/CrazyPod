#ifndef CRAZYPOD_PLAYLIST_H
#define CRAZYPOD_PLAYLIST_H

#include <stdbool.h>

#include "file.h"

bool crazypod_queue_replace(const char *const *paths, int count,
                            int start_index);
bool crazypod_queue_replace_shuffled(const char *const *paths, int count,
                                     unsigned int seed);
void crazypod_queue_restore_begin(void);
bool crazypod_queue_restore_add(const char *path);
void crazypod_queue_restore_finish(int selected_index, bool shuffled);
int crazypod_queue_count(void);
int crazypod_queue_index(void);
const char *crazypod_queue_path(int index);
void crazypod_queue_set_shuffle(bool enabled);
bool crazypod_queue_shuffle(void);
void crazypod_queue_set_repeat(int repeat_mode);
int crazypod_queue_repeat(void);
unsigned crazypod_queue_generation(void);

#endif
