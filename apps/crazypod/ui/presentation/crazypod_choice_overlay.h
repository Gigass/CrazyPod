#ifndef CRAZYPOD_CHOICE_OVERLAY_H
#define CRAZYPOD_CHOICE_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_choice_overlay_callbacks {
    int (*count)(int kind, int id, void *context);
    int (*current_index)(int kind, int id, void *context);
    const char *(*title)(int kind, int id, void *context);
    const char *(*item_title)(
        int kind, int id, int index, void *context);
    bool (*item_color)(
        int kind, int id, int index, uint32_t *color,
        void *context);
    bool (*action_layout)(
        int kind, int id, void *context);
    lv_obj_t *(*create_underlay)(
        lv_obj_t *parent, void *context);
    lv_obj_t *(*create_panel)(
        lv_obj_t *parent, int x, int y,
        int width, int height, void *context);
    void (*animate_panel)(
        lv_obj_t *panel, int target_y, void *context);
    void *context;
};

void crazypod_choice_overlay_show(
    lv_obj_t *parent, int kind, int id, int selected,
    const lv_font_t *metadata_font,
    const struct crazypod_choice_overlay_callbacks *callbacks);
void crazypod_choice_overlay_reset(void);
void crazypod_choice_overlay_dismiss(void);
void crazypod_choice_overlay_move(int direction);
void crazypod_choice_overlay_begin_hold_feedback(int duration_ms);
void crazypod_choice_overlay_cancel_hold_feedback(void);
void crazypod_choice_overlay_show_receipt(
    const char *label, bool success, bool animated);
bool crazypod_choice_overlay_visible(void);
bool crazypod_choice_overlay_receipt_visible(void);
int crazypod_choice_overlay_kind(void);
int crazypod_choice_overlay_id(void);
int crazypod_choice_overlay_selected(void);

#endif
