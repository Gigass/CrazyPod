#ifndef CRAZYPOD_HOME_ACTIONS_H
#define CRAZYPOD_HOME_ACTIONS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_home_actions_callbacks {
    void (*open_queue)(void);
    void (*toggle_playback)(void);
};

void crazypod_home_actions_configure(
    lv_obj_t *parent,
    const struct crazypod_home_actions_callbacks *callbacks);
void crazypod_home_actions_show(void);
void crazypod_home_actions_dismiss(bool restore_desktop);
bool crazypod_home_actions_visible(void);
bool crazypod_home_actions_handle_button(
    long base, bool repeated, intptr_t data);

#endif
