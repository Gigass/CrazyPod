#ifndef CRAZYPOD_VIDEO_CATALOG_H
#define CRAZYPOD_VIDEO_CATALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

struct crazypod_video_catalog_entry {
    char path[MAX_PATH];
    char poster_path[MAX_PATH];
    char name[MAX_PATH];
    uint32_t size;
    uint32_t mtime;
    uint32_t resume_ticks;
    uint32_t duration_ticks;
};

void crazypod_video_catalog_init(void);
void crazypod_video_catalog_refresh(void);
int crazypod_video_catalog_count(void);
const struct crazypod_video_catalog_entry *
crazypod_video_catalog_get(int index);
bool crazypod_video_catalog_path_supported(const char *path);
bool crazypod_video_catalog_update_playback(
    int index, uint32_t resume_ticks, uint32_t duration_ticks);

#endif
