#ifndef CRAZYPOD_VIDEO_ENGINE_H
#define CRAZYPOD_VIDEO_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

enum crazypod_video_engine_result {
    CRAZYPOD_VIDEO_ENGINE_OK = 0,
    CRAZYPOD_VIDEO_ENGINE_UNSUPPORTED = -1,
    CRAZYPOD_VIDEO_ENGINE_OPEN_FAILED = -2,
    CRAZYPOD_VIDEO_ENGINE_NO_MEMORY = -3,
    CRAZYPOD_VIDEO_ENGINE_ERROR = -4,
};

enum crazypod_video_engine_status {
    CRAZYPOD_VIDEO_ENGINE_STOPPED = 0,
    CRAZYPOD_VIDEO_ENGINE_PLAYING,
    CRAZYPOD_VIDEO_ENGINE_PAUSED,
    CRAZYPOD_VIDEO_ENGINE_ENDED,
    CRAZYPOD_VIDEO_ENGINE_STATUS_FAILED,
};

struct crazypod_video_engine_host {
    void (*present_yuv)(
        unsigned char * const source[3],
        int source_x, int source_y, int stride,
        int destination_x, int destination_y,
        int width, int height);
    void (*set_error)(const char *message);
};

bool crazypod_video_engine_path_supported(const char *path);
void crazypod_video_engine_set_host(
    const struct crazypod_video_engine_host *host);
enum crazypod_video_engine_result
crazypod_video_engine_open(const char *path);
enum crazypod_video_engine_result crazypod_video_engine_play(void);
void crazypod_video_engine_service(void);
void crazypod_video_engine_pause(void);
void crazypod_video_engine_resume(void);
void crazypod_video_engine_seek(uint32_t position_ms);
void crazypod_video_engine_redraw(void);
void crazypod_video_engine_close(void);
enum crazypod_video_engine_status crazypod_video_engine_status(void);
uint32_t crazypod_video_engine_position_ms(void);
uint32_t crazypod_video_engine_duration_ms(void);

#endif
