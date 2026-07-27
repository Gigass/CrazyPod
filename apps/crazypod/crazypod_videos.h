#ifndef CRAZYPOD_VIDEOS_H
#define CRAZYPOD_VIDEOS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#define CRAZYPOD_VIDEO_MAX_FILES 256
#define CRAZYPOD_VIDEO_POSTER_WIDTH 128
#define CRAZYPOD_VIDEO_POSTER_HEIGHT 96

enum crazypod_video_result {
    CRAZYPOD_VIDEO_OK = 0,
    CRAZYPOD_VIDEO_INVALID_FILE = -1,
    CRAZYPOD_VIDEO_NO_MEMORY = -2,
    CRAZYPOD_VIDEO_UNSUPPORTED = -3,
    CRAZYPOD_VIDEO_OPEN_FAILED = -4,
    CRAZYPOD_VIDEO_ENGINE_FAILED = -5,
};

void crazypod_videos_init(void);
void crazypod_videos_refresh(void);
void crazypod_videos_suspend(void);
void crazypod_videos_resume(void);

int crazypod_video_count(void);
const char *crazypod_video_path(int index);
const char *crazypod_video_name(int index);
uint32_t crazypod_video_resume_seconds(int index);
uint32_t crazypod_video_duration_seconds(int index);
const lv_image_dsc_t *crazypod_video_poster(int index);
unsigned crazypod_video_generation(void);
bool crazypod_videos_busy(void);

enum crazypod_video_result crazypod_video_play(int index);
enum crazypod_video_result crazypod_video_last_result(void);
const char *crazypod_video_result_message(
    enum crazypod_video_result result);

#endif
