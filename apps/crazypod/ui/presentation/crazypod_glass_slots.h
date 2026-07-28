#ifndef CRAZYPOD_GLASS_SLOTS_H
#define CRAZYPOD_GLASS_SLOTS_H

#include "lcd.h"
#include "lvgl.h"

#include "crazypod_glass_sampler.h"

enum crazypod_glass_slot {
    CRAZYPOD_GLASS_SLOT_MENU_TOPBAR = 0,
    CRAZYPOD_GLASS_SLOT_MENU_PANEL,
    CRAZYPOD_GLASS_SLOT_SEARCH_QUERY,
    CRAZYPOD_GLASS_SLOT_SEARCH_RESULTS,
    CRAZYPOD_GLASS_SLOT_INFO_TOAST,
    CRAZYPOD_GLASS_SLOT_INFO_BAR,
    CRAZYPOD_GLASS_SLOT_INFO_BAR_ALT,
};

fb_data *crazypod_glass_slot_pixels(enum crazypod_glass_slot slot);
lv_image_dsc_t *crazypod_glass_slot_descriptor(
    enum crazypod_glass_slot slot);
void crazypod_glass_slots_configure(
    crazypod_glass_boost_callback boost);
bool crazypod_glass_slot_prepare_frame(
    enum crazypod_glass_slot slot,
    int x, int y, int width, int height,
    enum crazypod_glass_material material);
bool crazypod_glass_slot_prepare_menu(
    enum crazypod_glass_slot slot,
    int x, int y, int width, int height,
    enum crazypod_glass_material material);
lv_obj_t *crazypod_glass_slot_panel(
    enum crazypod_glass_slot slot, bool prepared,
    lv_obj_t *parent, int x, int y,
    int width, int height, int radius,
    enum crazypod_glass_material material);

#endif
