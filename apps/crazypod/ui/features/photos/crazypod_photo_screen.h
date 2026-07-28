#ifndef CRAZYPOD_PHOTO_SCREEN_H
#define CRAZYPOD_PHOTO_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

enum crazypod_photo_grid_mode {
    CRAZYPOD_PHOTO_GRID_LIBRARY = 0,
    CRAZYPOD_PHOTO_GRID_FAVORITES,
    CRAZYPOD_PHOTO_GRID_WALLPAPER,
};

typedef lv_obj_t *(*crazypod_photo_info_panel_factory)(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, void *context);

int crazypod_photo_screen_grid_count(enum crazypod_photo_grid_mode mode);
int crazypod_photo_screen_grid_index(
    enum crazypod_photo_grid_mode mode, int position);
void crazypod_photo_screen_render_grid(
    lv_obj_t *parent, enum crazypod_photo_grid_mode mode,
    int selected, const char *title,
    const lv_font_t *title_font,
    uint32_t primary_color, uint32_t panel_color);
void crazypod_photo_screen_render_detail(
    lv_obj_t *parent, int photo_index,
    uint32_t white_color,
    crazypod_photo_info_panel_factory info_panel_factory,
    void *factory_context);
void crazypod_photo_screen_render_favorite_status(
    lv_obj_t *parent, int photo_index, long now,
    uint32_t white_color, uint32_t muted_color);
void crazypod_photo_screen_reset_transient(void);
bool crazypod_photo_screen_favorite_progress_ready(void);
void crazypod_photo_screen_update_favorite_progress(int width);
lv_obj_t *crazypod_photo_screen_render_image(
    lv_obj_t *parent, const lv_image_dsc_t *descriptor,
    int x, int y, int width, int height);

#endif
