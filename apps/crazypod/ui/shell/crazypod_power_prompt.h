#ifndef CRAZYPOD_POWER_PROMPT_H
#define CRAZYPOD_POWER_PROMPT_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_power_prompt_callbacks {
    bool compact_hold_feedback;
    void (*before_hold_show)(void);
    void (*before_show)(void);
    lv_obj_t *(*create_panel)(
        lv_obj_t *parent, int x, int y, int width, int height);
    void (*animate_panel)(lv_obj_t *panel, int target_y);
    void (*dismissed)(void);
    void (*execute)(int selected_action);
};

void crazypod_power_prompt_configure(
    lv_obj_t *parent,
    const struct crazypod_power_prompt_callbacks *callbacks);
bool crazypod_power_prompt_visible(void);
bool crazypod_power_prompt_hold_feedback_visible(void);
void crazypod_power_prompt_show(void);
void crazypod_power_prompt_dismiss(void);
bool crazypod_power_prompt_handle_button(
    long base, bool repeated, intptr_t data);
bool crazypod_power_prompt_handle_play_hold(
    long button, long now, long hold_ticks);
void crazypod_power_prompt_begin_play_hold(long start_tick);
void crazypod_power_prompt_tick(long now, long hold_ticks);

#endif
