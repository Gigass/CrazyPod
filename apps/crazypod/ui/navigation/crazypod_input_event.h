#ifndef CRAZYPOD_INPUT_EVENT_H
#define CRAZYPOD_INPUT_EVENT_H

#include <stdbool.h>
#include <stdint.h>

struct crazypod_input_event {
    long raw;
    long base;
    intptr_t data;
    bool release;
    bool repeated;
};

struct crazypod_input_event crazypod_input_event_make(
    long button, intptr_t data);
bool crazypod_input_button_is_remote(long button);
long crazypod_input_translate_remote(long button);
int crazypod_input_wheel_steps(
    const struct crazypod_input_event *event, int maximum);

#endif
