#include "crazypod_audio_memory_policy.h"

#define CRAZYPOD_RUNTIME_HEADROOM_MAX (16u * 1024u * 1024u)

/*
 * The fixed floor was chosen for the 6G's arena, around 40 MiB, where 4 MiB of
 * file buffer is a tenth of it. A 32 MiB iPod Video runs in about 9.5 MiB once
 * .bss and the plugin buffer are taken out, so the same 4 MiB is nearly half
 * the arena and cannot be had as one contiguous block once a catalog of
 * several thousand tracks is pinned in the middle of it. audio_reset_buffer()
 * then panics with OOM.
 *
 * Scale the floor to the arena, and keep a margin so the runtime reservation
 * never leaves exactly the floor behind, where buflib overhead or a pinned
 * block is enough to miss it.
 */
#define CRAZYPOD_AUDIO_BUFFER_FLOOR_MIN (1u * 1024u * 1024u)
#define CRAZYPOD_AUDIO_BUFFER_FLOOR_SHARE 3u
#define CRAZYPOD_AUDIO_BUFFER_MARGIN (512u * 1024u)

static size_t effective_floor = CRAZYPOD_AUDIO_BUFFER_FLOOR;

size_t crazypod_audio_buffer_floor_for(size_t allocatable)
{
    size_t share = allocatable / CRAZYPOD_AUDIO_BUFFER_FLOOR_SHARE;

    if (share >= CRAZYPOD_AUDIO_BUFFER_FLOOR)
        return CRAZYPOD_AUDIO_BUFFER_FLOOR;
    if (share <= CRAZYPOD_AUDIO_BUFFER_FLOOR_MIN)
        return CRAZYPOD_AUDIO_BUFFER_FLOOR_MIN;
    return share;
}

void crazypod_audio_buffer_set_arena(size_t allocatable)
{
    effective_floor = crazypod_audio_buffer_floor_for(allocatable);
}

size_t crazypod_audio_buffer_floor(void)
{
    return effective_floor;
}

size_t crazypod_audio_runtime_headroom(size_t allocatable)
{
    size_t reserved =
        crazypod_audio_buffer_floor_for(allocatable) +
        CRAZYPOD_AUDIO_BUFFER_MARGIN;
    size_t headroom;

    if (allocatable <= reserved)
        return 0;
    headroom = allocatable - reserved;
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
    return size >= effective_floor;
}
