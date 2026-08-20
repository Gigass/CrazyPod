#ifndef CRAZYPOD_HEADPHONE_POPUP_H
#define CRAZYPOD_HEADPHONE_POPUP_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_headphone_popup_callbacks {
    bool (*can_show)(void);
    void (*before_show)(void);
    lv_obj_t *(*create_panel)(
        lv_obj_t *parent, int x, int y, int width, int height);
    void (*dismissed)(void);
};

void crazypod_headphone_popup_configure(
    lv_obj_t *parent,
    const struct crazypod_headphone_popup_callbacks *callbacks);
void crazypod_headphone_popup_set_ui_ready(bool inserted);
void crazypod_headphone_popup_connection_changed(bool inserted);
bool crazypod_headphone_popup_visible(void);
bool crazypod_headphone_popup_handle_button(
    long base, bool repeated, intptr_t data);
void crazypod_headphone_popup_dismiss(bool animated);
#ifdef SIMULATOR
void crazypod_headphone_popup_simulator_show(void);
#endif

#endif
