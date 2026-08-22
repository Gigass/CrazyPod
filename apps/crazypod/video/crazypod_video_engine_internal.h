#ifndef CRAZYPOD_VIDEO_ENGINE_INTERNAL_H
#define CRAZYPOD_VIDEO_ENGINE_INTERNAL_H

#include "crazypod_video_engine.h"

struct crazypod_video_engine_ops {
    enum crazypod_video_engine_result (*open)(
        const char *path,
        const struct crazypod_video_engine_host *host);
    enum crazypod_video_engine_result (*play)(void);
    void (*service)(void);
    void (*pause)(void);
    void (*resume)(void);
    void (*seek)(uint32_t position_ms);
    void (*redraw)(void);
    void (*close)(void);
    enum crazypod_video_engine_status (*status)(void);
    uint32_t (*position_ms)(void);
    uint32_t (*duration_ms)(void);
};

const struct crazypod_video_engine_ops *
crazypod_video_engine_mpeg_ops(void);

#ifdef SIMULATOR
const struct crazypod_video_engine_ops *
crazypod_video_engine_ffmpeg_ops(void);
#endif

#endif
