#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stddef.h>

#include "kernel.h"
#include "timefuncs.h"
#include "../../../../miniapps/sdk/crazypod_miniapp_native.h"
#include "crazypod_miniapp_host_system.h"

static struct {
    size_t external_limit;
    size_t external_used;
    size_t external_high_water;
    bool active;
} session_memory;

uint32_t crazypod_miniapp_host_monotonic_ms(void)
{
    return (uint32_t)(
        ((uint64_t)(uint32_t)current_tick * 1000u) / HZ);
}

uint32_t crazypod_miniapp_host_epoch_seconds(void)
{
    time_t value = mktime(get_time());

    return value > 0 && (uint64_t)value <= UINT32_MAX
        ? (uint32_t)value : 0;
}

bool crazypod_miniapp_host_session_begin(size_t reserved_size)
{
    if(session_memory.active ||
       session_memory.external_used != 0 ||
       reserved_size > CP_NATIVE_SESSION_MAX)
        return false;
    session_memory.external_limit =
        CP_NATIVE_SESSION_MAX - reserved_size;
    session_memory.active = true;
    return true;
}

bool crazypod_miniapp_host_memory_reserve(size_t size)
{
    if(!session_memory.active ||
       size > session_memory.external_limit -
           session_memory.external_used)
        return false;
    session_memory.external_used += size;
    if(session_memory.external_used >
       session_memory.external_high_water)
        session_memory.external_high_water =
            session_memory.external_used;
    return true;
}

bool crazypod_miniapp_host_memory_replace(
    size_t old_size, size_t new_size)
{
    size_t retained;

    if(!session_memory.active ||
       old_size > session_memory.external_used)
        return false;
    retained = session_memory.external_used - old_size;
    if(new_size > session_memory.external_limit - retained)
        return false;
    session_memory.external_used = retained + new_size;
    if(session_memory.external_used >
       session_memory.external_high_water)
        session_memory.external_high_water =
            session_memory.external_used;
    return true;
}

void crazypod_miniapp_host_memory_release(size_t size)
{
    if(size <= session_memory.external_used)
        session_memory.external_used -= size;
    else
        session_memory.external_used = 0;
}

size_t crazypod_miniapp_host_memory_used(void)
{
    return session_memory.external_used;
}

size_t crazypod_miniapp_host_memory_high_water(void)
{
    return session_memory.external_high_water;
}

size_t crazypod_miniapp_host_memory_limit(void)
{
    return session_memory.external_limit;
}

void crazypod_miniapp_host_memory_reset_high_water(void)
{
    session_memory.external_high_water =
        session_memory.external_used;
}

void crazypod_miniapp_host_session_finish(void)
{
    session_memory.active = false;
    session_memory.external_limit = 0;
    session_memory.external_used = 0;
    session_memory.external_high_water = 0;
}

#endif
