#include "config.h"

#ifdef IPOD_6G

#define CRAZYPOD_VIDEO_CORE 1

#include <stdio.h>

#include "../crazypod_video_plugin.h"
#include "../../plugins/mpegplayer/mpegplayer.h"
#include "crazypod_video_engine_internal.h"

static bool mpeg_initialized;
static bool mpeg_opened;

static enum crazypod_video_engine_result mpeg_open(
    const char *path, const struct crazypod_video_engine_host *host)
{
    int result;

    (void)host;
    if(stream_init() < STREAM_OK) {
        /* stream_init() is transactional; keep this idempotent call as a
         * second ownership barrier if a future stage is added incorrectly. */
        stream_exit();
        return CRAZYPOD_VIDEO_ENGINE_NO_MEMORY;
    }
    mpeg_initialized = true;
    result = stream_open(path);
    if(result < STREAM_OK) {
        stream_exit();
        mpeg_initialized = false;
        return result == STREAM_UNSUPPORTED
            ? CRAZYPOD_VIDEO_ENGINE_UNSUPPORTED
            : CRAZYPOD_VIDEO_ENGINE_OPEN_FAILED;
    }
    mpeg_opened = true;
    stream_vo_set_clip(NULL);
    stream_show_vo(true);
    return CRAZYPOD_VIDEO_ENGINE_OK;
}

static enum crazypod_video_engine_result mpeg_play(void)
{
    return stream_play() >= STREAM_OK
        ? CRAZYPOD_VIDEO_ENGINE_OK : CRAZYPOD_VIDEO_ENGINE_ERROR;
}

static void mpeg_service(void)
{
}

static void mpeg_pause(void)
{
    (void)stream_pause();
}

static void mpeg_resume(void)
{
    (void)stream_resume();
}

static void mpeg_seek(uint32_t position_ms)
{
    uint32_t ticks = (uint32_t)(
        (uint64_t)position_ms * TS_SECOND / 1000u);

    (void)stream_seek(ticks, SEEK_SET);
}

static void mpeg_redraw(void)
{
    vo_lock();
    vo_unlock();
    (void)stream_draw_frame(false);
}

static void mpeg_close(void)
{
    if(mpeg_opened) {
        (void)stream_stop();
        (void)stream_close();
        mpeg_opened = false;
    }
    if(mpeg_initialized) {
        stream_exit();
        mpeg_initialized = false;
    }
}

static enum crazypod_video_engine_status mpeg_status(void)
{
    switch(stream_status()) {
    case STREAM_PLAYING:
        return CRAZYPOD_VIDEO_ENGINE_PLAYING;
    case STREAM_PAUSED:
        return CRAZYPOD_VIDEO_ENGINE_PAUSED;
    case STREAM_STOPPED:
    default:
        return CRAZYPOD_VIDEO_ENGINE_STOPPED;
    }
}

static uint32_t mpeg_position_ms(void)
{
    return (uint32_t)((uint64_t)stream_get_time() * 1000u / TS_SECOND);
}

static uint32_t mpeg_duration_ms(void)
{
    return (uint32_t)(
        (uint64_t)stream_get_duration() * 1000u / TS_SECOND);
}

const struct crazypod_video_engine_ops *
crazypod_video_engine_mpeg_ops(void)
{
    static const struct crazypod_video_engine_ops ops = {
        .open = mpeg_open,
        .play = mpeg_play,
        .service = mpeg_service,
        .pause = mpeg_pause,
        .resume = mpeg_resume,
        .seek = mpeg_seek,
        .redraw = mpeg_redraw,
        .close = mpeg_close,
        .status = mpeg_status,
        .position_ms = mpeg_position_ms,
        .duration_ms = mpeg_duration_ms,
    };

    return &ops;
}

#endif
