#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "crazypod_audio_memory_policy.h"

#define MIB (1024u * 1024u)

int main(void)
{
    assert(CRAZYPOD_RUNTIME_HANDLE_HEADROOM >= 512u * 3u + 64u);
    assert(crazypod_audio_runtime_headroom(64u * MIB) == 16u * MIB);
    assert(crazypod_audio_runtime_headroom(20u * MIB) == 16u * MIB);
    assert(crazypod_audio_runtime_headroom(12u * MIB) == 8u * MIB);
    assert(crazypod_audio_runtime_headroom(4u * MIB) == 0);
    assert(crazypod_audio_runtime_headroom(4u * MIB + 4096u) == 4096u);
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
