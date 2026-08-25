#include "config.h"

#ifdef IPOD_6G

#include "buflib.h"
#include "core_alloc.h"

#include "crazypod_audio_memory_policy.h"
#include "crazypod_audio_reserve.h"

static int audio_reserve_handle;

bool crazypod_audio_reserve_acquire(void)
{
    if(audio_reserve_handle > 0)
        return true;
    audio_reserve_handle = core_alloc_ex(
        CRAZYPOD_AUDIO_BUFFER_FLOOR, &buflib_ops_locked);
    return audio_reserve_handle > 0;
}

void crazypod_audio_reserve_release(void)
{
    if(audio_reserve_handle > 0)
        audio_reserve_handle = core_free(audio_reserve_handle);
}

bool crazypod_audio_reserve_is_held(void)
{
    return audio_reserve_handle > 0;
}

#endif
