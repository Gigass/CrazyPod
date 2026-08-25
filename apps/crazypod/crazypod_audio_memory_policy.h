#ifndef CRAZYPOD_AUDIO_MEMORY_POLICY_H
#define CRAZYPOD_AUDIO_MEMORY_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#define CRAZYPOD_AUDIO_BUFFER_FLOOR (4u * 1024u * 1024u)
#define CRAZYPOD_RUNTIME_HANDLE_HEADROOM 1600u

size_t crazypod_audio_runtime_headroom(size_t allocatable);
bool crazypod_audio_buffer_may_shrink(bool playback_active,
                                      size_t resulting_size);
bool crazypod_audio_buffer_meets_floor(size_t size);

#endif
