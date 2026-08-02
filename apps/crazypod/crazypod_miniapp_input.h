#ifndef CRAZYPOD_MINIAPP_INPUT_H
#define CRAZYPOD_MINIAPP_INPUT_H

#include <stdbool.h>

#include "../../miniapps/sdk/crazypod_miniapp_native.h"

#define CRAZYPOD_MINIAPP_INPUT_CAPACITY 8

struct crazypod_miniapp_input_queue {
    struct cp_input_event events[CRAZYPOD_MINIAPP_INPUT_CAPACITY];
    unsigned head;
    unsigned count;
};

void crazypod_miniapp_input_reset(
    struct crazypod_miniapp_input_queue *queue);
bool crazypod_miniapp_input_push_wheel(
    struct crazypod_miniapp_input_queue *queue,
    const struct cp_input_event *event);
bool crazypod_miniapp_input_push_wheel_coalesced(
    struct crazypod_miniapp_input_queue *queue,
    const struct cp_input_event *event);
bool crazypod_miniapp_input_next(
    struct crazypod_miniapp_input_queue *queue, bool frame_due,
    struct cp_input_event *event);
unsigned crazypod_miniapp_input_count(
    const struct crazypod_miniapp_input_queue *queue);

#endif
