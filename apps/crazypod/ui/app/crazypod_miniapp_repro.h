#ifndef CRAZYPOD_MINIAPP_REPRO_H
#define CRAZYPOD_MINIAPP_REPRO_H

#include "config.h"

#if defined(IPOD_6G) && \
    (defined(SIMULATOR) || \
     defined(CRAZYPOD_REPRO_DIAGNOSTICS))

#include <stdbool.h>

bool crazypod_miniapp_repro_start(long now);
int crazypod_miniapp_repro_wait_ticks(void);
void crazypod_miniapp_repro_service(long now);
bool crazypod_miniapp_repro_cpu_boost_requested(void);
void crazypod_miniapp_repro_trace_marker(
    const char *phase, long value);

#endif

#endif
