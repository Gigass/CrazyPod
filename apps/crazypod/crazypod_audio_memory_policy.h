#ifndef CRAZYPOD_AUDIO_MEMORY_POLICY_H
#define CRAZYPOD_AUDIO_MEMORY_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#define CRAZYPOD_AUDIO_BUFFER_FLOOR (4u * 1024u * 1024u)
#define CRAZYPOD_RUNTIME_HANDLE_HEADROOM 1600u

/*
 * CRAZYPOD_AUDIO_BUFFER_FLOOR is the floor for an arena the size of the 6G's.
 * It is nearly half of a 32 MiB iPod Video's, and unobtainable there once a
 * large catalog is pinned, so the effective floor scales with the arena.
 * Set it from the allocatable size before reserving, then query it.
 */
size_t crazypod_audio_buffer_floor_for(size_t allocatable);
void crazypod_audio_buffer_set_arena(size_t allocatable);
size_t crazypod_audio_buffer_floor(void);
size_t crazypod_audio_runtime_headroom(size_t allocatable);
bool crazypod_audio_buffer_may_shrink(bool playback_active,
                                      size_t resulting_size);
bool crazypod_audio_buffer_meets_floor(size_t size);

#endif
