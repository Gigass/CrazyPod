#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "crazypod_audio_memory_policy.h"

#define MIB (1024u * 1024u)

int main(void)
{
    assert(CRAZYPOD_RUNTIME_HANDLE_HEADROOM >= 512u * 3u + 64u);
    /* A large arena keeps the fixed floor; the reservation now also leaves a
     * margin above it, so the headroom is that much smaller. */
    assert(crazypod_audio_buffer_floor_for(64u * MIB) == 4u * MIB);
    assert(crazypod_audio_runtime_headroom(64u * MIB) == 16u * MIB);
    assert(crazypod_audio_runtime_headroom(20u * MIB) ==
           20u * MIB - 4u * MIB - 512u * 1024u);
    assert(crazypod_audio_runtime_headroom(12u * MIB) ==
           12u * MIB - 4u * MIB - 512u * 1024u);

    /* A 32 MiB iPod Video's arena: the floor scales down rather than being
     * unobtainable, and the reservation always leaves room for it. */
    assert(crazypod_audio_buffer_floor_for(9u * MIB) == 3u * MIB);
    assert(crazypod_audio_buffer_floor_for(6u * MIB) == 2u * MIB);
    assert(crazypod_audio_buffer_floor_for(2u * MIB) == 1u * MIB);
    assert(crazypod_audio_runtime_headroom(6u * MIB) ==
           6u * MIB - 2u * MIB - 512u * 1024u);
    assert(crazypod_audio_runtime_headroom(1u * MIB) == 0);

    crazypod_audio_buffer_set_arena(6u * MIB);
    assert(crazypod_audio_buffer_floor() == 2u * MIB);
    assert(crazypod_audio_buffer_meets_floor(2u * MIB));
    assert(!crazypod_audio_buffer_meets_floor(2u * MIB - 1));
    crazypod_audio_buffer_set_arena(64u * MIB);
    assert(crazypod_audio_buffer_floor() == 4u * MIB);
    assert(!crazypod_audio_buffer_may_shrink(
        true, CRAZYPOD_AUDIO_BUFFER_FLOOR));
    assert(!crazypod_audio_buffer_may_shrink(
        false, CRAZYPOD_AUDIO_BUFFER_FLOOR - 1));
    assert(crazypod_audio_buffer_may_shrink(
        false, CRAZYPOD_AUDIO_BUFFER_FLOOR));
    assert(crazypod_audio_buffer_meets_floor(
        CRAZYPOD_AUDIO_BUFFER_FLOOR));
    puts("crazypod audio memory policy host tests passed");
    return 0;
}
