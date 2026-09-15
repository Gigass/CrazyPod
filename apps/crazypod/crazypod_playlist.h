#ifndef CRAZYPOD_PLAYLIST_H
#define CRAZYPOD_PLAYLIST_H

#include <stdbool.h>

#include "file.h"

bool crazypod_queue_replace(const char *const *paths, int count,
                            int start_index);
bool crazypod_queue_replace_shuffled(const char *const *paths, int count,
                                     unsigned int seed);
/* Replaces the queue with one file and starts it at elapsed_ms. */
bool crazypod_queue_replace_resume(const char *path,
                                   unsigned long elapsed_ms);
void crazypod_queue_restore_begin(void);
bool crazypod_queue_restore_add(const char *path);
void crazypod_queue_restore_finish(int selected_index, bool shuffled);
int crazypod_queue_count(void);
int crazypod_queue_index(void);
bool crazypod_queue_copy_path(int index, char *buffer, size_t buffer_size);
void crazypod_queue_set_shuffle(bool enabled);
bool crazypod_queue_shuffle(void);
void crazypod_queue_set_repeat(int repeat_mode);
/* Tell the queue the next skip is the listener's, not a track ending --
 * repeat one steps for one and holds for the other. */
void crazypod_queue_note_manual_skip(void);
int crazypod_queue_repeat(void);
unsigned crazypod_queue_generation(void);

#endif
