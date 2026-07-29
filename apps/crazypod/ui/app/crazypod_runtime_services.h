#ifndef CRAZYPOD_RUNTIME_SERVICES_H
#define CRAZYPOD_RUNTIME_SERVICES_H

#include <stdbool.h>

void crazypod_runtime_services_configure(
    void (*render)(bool transition));
int crazypod_runtime_services_wait_ticks(void);
void crazypod_runtime_services_tick(
    long now, bool frame_due, bool locked);

#endif
