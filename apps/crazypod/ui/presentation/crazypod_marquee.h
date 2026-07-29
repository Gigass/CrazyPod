#ifndef CRAZYPOD_MARQUEE_H
#define CRAZYPOD_MARQUEE_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_marquee_configure(lv_obj_t *label, bool active);
void crazypod_marquee_set_text(
    lv_obj_t *label, const char *text, bool active);

#endif
