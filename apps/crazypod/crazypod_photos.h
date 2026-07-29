#ifndef CRAZYPOD_PHOTOS_H
#define CRAZYPOD_PHOTOS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#define CRAZYPOD_PHOTO_MAX_FILES 256
#define CRAZYPOD_PHOTO_THUMB_SLOTS 16
#define CRAZYPOD_PHOTO_THUMB_SIZE 64
#define CRAZYPOD_PHOTO_VIEW_WIDTH 640
#define CRAZYPOD_PHOTO_VIEW_HEIGHT 400
#define CRAZYPOD_PHOTO_VIEWPORT_WIDTH 320
#define CRAZYPOD_PHOTO_VIEWPORT_HEIGHT 240

void crazypod_photos_init(void);
void crazypod_photos_refresh(void);
void crazypod_photos_ensure_catalog(void);
void crazypod_photos_suspend(void);
void crazypod_photos_resume(void);
void crazypod_photos_set_lock_suspended(bool suspended);
void crazypod_photos_set_route_suspended(bool suspended);
void crazypod_photos_invalidate_catalog(void);

int crazypod_photo_count(void);
int crazypod_photo_favorite_count(void);
int crazypod_photo_favorite_index(int favorite_index);
const char *crazypod_photo_path(int index);
const char *crazypod_photo_name(int index);
bool crazypod_photo_is_favorite(int index);
bool crazypod_photo_toggle_favorite(int index);

const lv_image_dsc_t *crazypod_photo_thumbnail(int slot, int index);
const lv_image_dsc_t *crazypod_photo_view(int index);
const lv_image_dsc_t *crazypod_photo_render_viewport(
    int index, int zoom_percent, int *pan_x, int *pan_y);
const lv_image_dsc_t *crazypod_photo_render_crop_preview(
    int index, int center_y);
int crazypod_photo_view_progress(int index);
unsigned crazypod_photo_generation(void);
unsigned crazypod_photo_view_generation(void);
bool crazypod_photos_busy(void);

#endif
