#include "crazypod_audio_memory_policy.h"

#define CRAZYPOD_AUDIO_BUFFER_FLOOR (4u * 1024u * 1024u)
#define CRAZYPOD_RUNTIME_HEADROOM_MAX (16u * 1024u * 1024u)

size_t crazypod_audio_runtime_headroom(size_t allocatable)
{
    size_t headroom;

    if (allocatable <= CRAZYPOD_AUDIO_BUFFER_FLOOR)
        return 0;
    headroom = allocatable - CRAZYPOD_AUDIO_BUFFER_FLOOR;
    if (headroom > CRAZYPOD_RUNTIME_HEADROOM_MAX)
        headroom = CRAZYPOD_RUNTIME_HEADROOM_MAX;
    return headroom;
}

bool crazypod_audio_buffer_may_shrink(bool playback_running)
{
    return !playback_running;
}
