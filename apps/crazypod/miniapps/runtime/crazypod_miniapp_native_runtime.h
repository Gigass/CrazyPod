#ifndef CRAZYPOD_MINIAPP_NATIVE_RUNTIME_H
#define CRAZYPOD_MINIAPP_NATIVE_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

struct cp_input_event;
struct crazypod_miniapp_metadata;

int crazypod_miniapp_native_open(
    const struct crazypod_miniapp_metadata *metadata);
void crazypod_miniapp_native_close(void);
bool crazypod_miniapp_native_is_open(void);
bool crazypod_miniapp_native_event(
    const struct cp_input_event *event);
bool crazypod_miniapp_native_ui_event(
    uint32_t handler, uint8_t event_type,
    uint32_t target, int32_t value);
bool crazypod_miniapp_native_tick(void);
bool crazypod_miniapp_native_has_scheduled_work(void);

#endif
