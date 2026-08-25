#include "crazypod_audio_memory_policy.h"

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

bool crazypod_audio_buffer_may_shrink(bool playback_active,
                                      size_t resulting_size)
{
    return !playback_active &&
        crazypod_audio_buffer_meets_floor(resulting_size);
}

bool crazypod_audio_buffer_meets_floor(size_t size)
{
    return size >= CRAZYPOD_AUDIO_BUFFER_FLOOR;
}
