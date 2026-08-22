#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_video_engine.h"
#include "crazypod_video_engine_internal.h"

static const struct crazypod_video_engine_host *engine_host;
static const struct crazypod_video_engine_ops *engine_ops;

static const char *path_extension(const char *path)
{
    return path != NULL ? strrchr(path, '.') : NULL;
}

static bool extension_is(const char *extension, const char *expected)
{
    return extension != NULL && strcasecmp(extension, expected) == 0;
}

bool crazypod_video_engine_path_supported(const char *path)
{
    const char *extension = path_extension(path);

    if(extension_is(extension, ".mpg") ||
       extension_is(extension, ".mpeg"))
        return true;
#ifdef SIMULATOR
    return extension_is(extension, ".mp4") ||
        extension_is(extension, ".m4v") ||
        extension_is(extension, ".mov");
#else
    return false;
#endif
}

void crazypod_video_engine_set_host(
    const struct crazypod_video_engine_host *host)
{
    engine_host = host;
}

enum crazypod_video_engine_result
crazypod_video_engine_open(const char *path)
{
    const char *extension = path_extension(path);

    crazypod_video_engine_close();
    if(extension_is(extension, ".mpg") ||
       extension_is(extension, ".mpeg"))
        engine_ops = crazypod_video_engine_mpeg_ops();
#ifdef SIMULATOR
    else if(extension_is(extension, ".mp4") ||
            extension_is(extension, ".m4v") ||
            extension_is(extension, ".mov"))
        engine_ops = crazypod_video_engine_ffmpeg_ops();
#endif
    else
        return CRAZYPOD_VIDEO_ENGINE_UNSUPPORTED;

    return engine_ops->open(path, engine_host);
}

enum crazypod_video_engine_result crazypod_video_engine_play(void)
{
    return engine_ops != NULL
        ? engine_ops->play() : CRAZYPOD_VIDEO_ENGINE_ERROR;
}

void crazypod_video_engine_service(void)
{
    if(engine_ops != NULL && engine_ops->service != NULL)
        engine_ops->service();
}

void crazypod_video_engine_pause(void)
{
    if(engine_ops != NULL)
        engine_ops->pause();
}

void crazypod_video_engine_resume(void)
{
    if(engine_ops != NULL)
        engine_ops->resume();
}

void crazypod_video_engine_seek(uint32_t position_ms)
{
    if(engine_ops != NULL)
        engine_ops->seek(position_ms);
}

void crazypod_video_engine_redraw(void)
{
    if(engine_ops != NULL)
        engine_ops->redraw();
}

void crazypod_video_engine_close(void)
{
    if(engine_ops != NULL)
        engine_ops->close();
    engine_ops = NULL;
}

enum crazypod_video_engine_status crazypod_video_engine_status(void)
{
    return engine_ops != NULL
        ? engine_ops->status() : CRAZYPOD_VIDEO_ENGINE_STOPPED;
}

uint32_t crazypod_video_engine_position_ms(void)
{
    return engine_ops != NULL ? engine_ops->position_ms() : 0;
}

uint32_t crazypod_video_engine_duration_ms(void)
{
    return engine_ops != NULL ? engine_ops->duration_ms() : 0;
}

#endif
