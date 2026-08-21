#ifndef CRAZYPOD_AUDIO_MEMORY_POLICY_H
#define CRAZYPOD_AUDIO_MEMORY_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#define CRAZYPOD_RUNTIME_HANDLE_HEADROOM 1600u

size_t crazypod_audio_runtime_headroom(size_t allocatable);
bool crazypod_audio_buffer_may_shrink(bool playback_running);

#endif
