#ifndef CRAZYPOD_VIDEO_POSTER_H
#define CRAZYPOD_VIDEO_POSTER_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_video_poster_init(void);
void crazypod_video_poster_reset(void);
void crazypod_video_poster_suspend(void);
void crazypod_video_poster_resume(void);
const lv_image_dsc_t *crazypod_video_poster_get(int index);
unsigned crazypod_video_poster_generation(void);
bool crazypod_video_poster_busy(void);
void crazypod_video_poster_mark_changed(void);

#endif
