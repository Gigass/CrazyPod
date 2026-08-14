#ifndef CRAZYPOD_RUNTIME_SERVICES_H
#define CRAZYPOD_RUNTIME_SERVICES_H

#include <stdbool.h>

void crazypod_runtime_services_configure(
    void (*render)(bool transition));
void crazypod_runtime_services_start(void);
int crazypod_runtime_services_prepare_wait(
    long now, bool *screen_off);
bool crazypod_runtime_services_screen_off_tick(void);
void crazypod_runtime_services_tick(
    long now, bool frame_due, bool locked);

#endif
