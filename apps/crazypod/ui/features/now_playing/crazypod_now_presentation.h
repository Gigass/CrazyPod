#ifndef CRAZYPOD_NOW_PLAYING_PRESENTATION_H
#define CRAZYPOD_NOW_PLAYING_PRESENTATION_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

bool crazypod_now_presentation_matches(const char *track_path);
bool crazypod_now_presentation_prepare(
    const lv_image_dsc_t *artwork, const char *track_path);
bool crazypod_now_presentation_get(
    const char *track_path,
    const lv_image_dsc_t **cover,
    const lv_image_dsc_t **backdrop,
    uint32_t *text_color);
void crazypod_now_presentation_discard(void);

#endif
