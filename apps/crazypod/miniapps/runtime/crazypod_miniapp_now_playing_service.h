#ifndef CRAZYPOD_MINIAPP_NOW_PLAYING_SERVICE_H
#define CRAZYPOD_MINIAPP_NOW_PLAYING_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct crazypod_track;

bool crazypod_miniapp_now_playing_copy_track(struct crazypod_track *track);
int crazypod_miniapp_now_playing_service_call(
    uint32_t operation,
    const void *request, size_t request_size,
    void *response, size_t response_capacity);

#endif
