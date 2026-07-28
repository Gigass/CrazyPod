#include "config.h"

#ifdef IPOD_6G

#define CRAZYPOD_VIDEO_CORE 1

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef SIMULATOR
#include <stdlib.h>
#endif

#include "audio.h"
#include "bmp.h"
#include "buflib.h"
#include "button.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "pathfuncs.h"
#include "pcm_mixer.h"
#include "pcmbuf.h"
#include "powermgmt.h"
#include "settings.h"
#include "sound.h"
#include "storage.h"
#include "system.h"
#include "usb.h"
#ifdef SIMULATOR
#include "screendump.h"
#endif

#include "crazypod_video_plugin.h"
#include "../plugins/mpegplayer/mpegplayer.h"
#include "../plugins/mpegplayer/mpeg_settings.h"

#ifdef HAVE_SCHEDULER_BOOSTCTRL
#undef trigger_cpu_boost
#undef cancel_cpu_boost
#endif

#include "crazypod_image.h"
#include "crazypod_lcd.h"
#include "crazypod_state.h"
#include "crazypod_videos.h"
#include "video/crazypod_video_catalog.h"
#include "video/crazypod_video_poster.h"

struct mpeg_settings settings = {
    .limitfps = 1,
    .skipframes = 1,
};

#define CRAZYPOD_VIDEO_UI_HEIGHT 42
#define CRAZYPOD_VIDEO_SEEK_SECONDS 10u
#define CRAZYPOD_VIDEO_RESUME_MIN_SECONDS 5u
#define CRAZYPOD_VIDEO_RESUME_END_SECONDS 5u
#define CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS 2u

static enum crazypod_video_result last_video_result = CRAZYPOD_VIDEO_OK;
static int video_audio_buffer_handle;
static size_t video_audio_buffer_size;
static bool video_buffer_allocation_failed;
static char video_engine_message[96];

static void video_engine_splashf(int ticks, const char *format, ...)
{
    va_list arguments;

    (void)ticks;
    va_start(arguments, format);
    vsnprintf(video_engine_message, sizeof(video_engine_message),
              format, arguments);
    va_end(arguments);
}

static int video_engine_open(const char *path, int flags)
{
    return open(path, flags);
}

#ifndef CPU_BOOST_LOGGING
static void video_engine_cpu_boost(bool enabled)
{
    (void)enabled;
    cpu_boost(enabled);
}
#endif

static void video_engine_storage_sleep(void)
{
    storage_sleep();
}

static void video_engine_storage_spin(void)
{
    storage_spin();
}

static void *video_engine_audio_buffer(size_t *size)
{
    if(video_audio_buffer_handle <= 0) {
        audio_stop();
        video_audio_buffer_handle = core_alloc_maximum(
            &video_audio_buffer_size, &buflib_ops_locked);
        if(video_audio_buffer_handle <= 0)
            video_buffer_allocation_failed = true;
    }
    if(size != NULL)
        *size = video_audio_buffer_size;
    return video_audio_buffer_handle > 0
        ? core_get_data(video_audio_buffer_handle) : NULL;
}

static void video_engine_release_audio_buffer(void)
{
    if(video_audio_buffer_handle > 0)
        video_audio_buffer_handle =
            core_free(video_audio_buffer_handle);
    video_audio_buffer_size = 0;
}

static const struct crazypod_video_plugin_api video_engine_api = {
    .splash = video_engine_splashf,
    .splashf = video_engine_splashf,
    .lcd_update = lcd_update,
    .lcd_clear_display = lcd_clear_display,
    .lcd_update_rect = lcd_update_rect,
    .lcd_fillrect = lcd_fillrect,
    .lcd_set_foreground = lcd_set_foreground,
    .lcd_get_foreground = lcd_get_foreground,
    .lcd_set_background = lcd_set_background,
    .lcd_get_background = lcd_get_background,
    .lcd_blit_yuv = lcd_blit_yuv,
    .open = video_engine_open,
    .close = close,
    .read = read,
    .lseek = lseek,
    .filesize = filesize,
    .storage_sleep = video_engine_storage_sleep,
    .storage_spin = video_engine_storage_spin,
    .stub_storage_sleep = video_engine_storage_sleep,
    .stub_storage_spin = video_engine_storage_spin,
    .ata_spin = video_engine_storage_spin,
    .yield = yield,
    .create_thread = create_thread,
    .thread_self = thread_self,
    .thread_wait = thread_wait,
#ifdef HAVE_PRIORITY_SCHEDULING
    .thread_set_priority = thread_set_priority,
#endif
    .mutex_init = mutex_init,
    .mutex_lock = mutex_lock,
    .mutex_unlock = mutex_unlock,
#ifdef CPU_BOOST_LOGGING
    .cpu_boost_ = cpu_boost_,
#else
    .cpu_boost = video_engine_cpu_boost,
#endif
#ifdef HAVE_SCHEDULER_BOOSTCTRL
    .trigger_cpu_boost = trigger_cpu_boost,
    .cancel_cpu_boost = cancel_cpu_boost,
#endif
    .commit_dcache = commit_dcache,
    .commit_discard_dcache = commit_discard_dcache,
    .queue_init = queue_init,
    .queue_post = queue_post,
    .queue_wait_w_tmo = queue_wait_w_tmo,
    .queue_enable_queue_send = queue_enable_queue_send,
    .queue_wait = queue_wait,
    .queue_empty = queue_empty,
    .queue_send = queue_send,
    .queue_reply = queue_reply,
    .current_tick = &current_tick,
    .memset = memset,
    .memcpy = memcpy,
    .memmove = memmove,
    .memcmp = memcmp,
#ifdef HAVE_PITCHCONTROL
    .sound_set_pitch = sound_set_pitch,
    .dsp_set_timestretch = dsp_set_timestretch,
#endif
    .pcm_play_lock = pcm_play_lock,
    .pcm_play_unlock = pcm_play_unlock,
    .dsp_configure = dsp_configure,
    .dsp_get_config = dsp_get_config,
    .dsp_process = dsp_process,
#if INPUT_SRC_CAPS != 0
    .audio_set_output_source = audio_set_output_source,
    .audio_set_input_source = audio_set_input_source,
#endif
    .mixer_channel_status = mixer_channel_status,
    .mixer_channel_play_data = mixer_channel_play_data,
    .mixer_channel_play_pause = mixer_channel_play_pause,
    .mixer_channel_stop = mixer_channel_stop,
    .mixer_channel_set_amplitude = mixer_channel_set_amplitude,
    .mixer_channel_get_bytes_waiting =
        mixer_channel_get_bytes_waiting,
    .mixer_set_frequency = mixer_set_frequency,
    .mixer_get_frequency = mixer_get_frequency,
    .pcmbuf_fade = pcmbuf_fade,
    .codec_thread_do_callback = codec_thread_do_callback,
    .plugin_get_audio_buffer = video_engine_audio_buffer,
};

const struct crazypod_video_plugin_api *rb = &video_engine_api;

static void draw_video_controls(
                                const struct crazypod_video_catalog_entry *video,
                                bool paused, const char *message)
{
    uint32_t duration = stream_get_duration();
    uint32_t elapsed = stream_get_time();

    crazypod_lcd_draw_video_controls(
        video->name, elapsed / TS_SECOND,
        duration / TS_SECOND, global_status.volume,
        paused, message);
}

static uint32_t valid_resume_time(
                                  const struct crazypod_video_catalog_entry *video,
                                  uint32_t duration)
{
    uint32_t minimum = CRAZYPOD_VIDEO_RESUME_MIN_SECONDS * TS_SECOND;
    uint32_t end_margin = CRAZYPOD_VIDEO_RESUME_END_SECONDS * TS_SECOND;

    if(video->resume_ticks < minimum ||
       duration <= end_margin ||
       video->resume_ticks >= duration - end_margin)
        return 0;
    return video->resume_ticks;
}

static void adjust_video_volume(int direction)
{
    int next = global_status.volume + direction;

    if(next < sound_min(SOUND_VOLUME))
        next = sound_min(SOUND_VOLUME);
    if(next > sound_max(SOUND_VOLUME))
        next = sound_max(SOUND_VOLUME);
    if(next == global_status.volume)
        return;
    sound_set_volume(next);
    global_status.volume = next;
    crazypod_state_mark_dirty();
}

static uint32_t seek_target(int direction)
{
    uint32_t current = stream_get_time();
    uint32_t duration = stream_get_duration();
    uint32_t amount = CRAZYPOD_VIDEO_SEEK_SECONDS * TS_SECOND;

    if(direction < 0)
        return current > amount ? current - amount : 0;
    if(duration > amount && current < duration - amount)
        return current + amount;
    return duration > 0 ? duration - 1 : current;
}

void crazypod_videos_init(void)
{
    crazypod_video_poster_init();
    crazypod_video_catalog_init();
    crazypod_videos_refresh();
}

void crazypod_videos_refresh(void)
{
    crazypod_videos_suspend();
    crazypod_video_catalog_refresh();
    crazypod_video_poster_reset();
}

void crazypod_videos_suspend(void)
{
    crazypod_video_poster_suspend();
}

void crazypod_videos_resume(void)
{
    crazypod_video_poster_resume();
}

int crazypod_video_count(void)
{
    return crazypod_video_catalog_count();
}

const char *crazypod_video_path(int index)
{
    const struct crazypod_video_catalog_entry *entry =
        crazypod_video_catalog_get(index);

    return entry != NULL ? entry->path : "";
}

const char *crazypod_video_name(int index)
{
    const struct crazypod_video_catalog_entry *entry =
        crazypod_video_catalog_get(index);

    return entry != NULL ? entry->name : "";
}

uint32_t crazypod_video_resume_seconds(int index)
{
    const struct crazypod_video_catalog_entry *entry =
        crazypod_video_catalog_get(index);

    return entry != NULL ? entry->resume_ticks / TS_SECOND : 0;
}

uint32_t crazypod_video_duration_seconds(int index)
{
    const struct crazypod_video_catalog_entry *entry =
        crazypod_video_catalog_get(index);

    return entry != NULL ? entry->duration_ticks / TS_SECOND : 0;
}

const lv_image_dsc_t *crazypod_video_poster(int index)
{
    return crazypod_video_poster_get(index);
}

unsigned crazypod_video_generation(void)
{
    return crazypod_video_poster_generation();
}

bool crazypod_videos_busy(void)
{
    return crazypod_video_poster_busy();
}

enum crazypod_video_result crazypod_video_play(int index)
{
    const struct crazypod_video_catalog_entry *video;
    struct vo_rect clip;
    enum crazypod_video_result result = CRAZYPOD_VIDEO_OK;
    uint32_t duration = 0;
    uint32_t resume = 0;
    uint32_t last_draw_tick = 0;
    uint32_t clock_check_time = 0;
    long clock_check_tick = 0;
    bool engine_initialized = false;
    bool stream_opened = false;
    bool paused = false;
    bool clock_checked = false;
    bool clock_stalled = false;
    bool completed = false;
    bool repost_system_event = false;
    int system_event = 0;
    intptr_t system_event_data = 0;
#ifdef SIMULATOR
    bool simulator_dump =
        getenv("CRAZYPOD_SIM_VIDEO_DUMP") != NULL;
    int simulator_dump_seconds = 1;
    const char *simulator_dump_after =
        getenv("CRAZYPOD_SIM_VIDEO_DUMP_AFTER");
    long simulator_dump_due;

    if(simulator_dump_after != NULL) {
        simulator_dump_seconds = atoi(simulator_dump_after);
        if(simulator_dump_seconds < 1)
            simulator_dump_seconds = 1;
        if(simulator_dump_seconds > 30)
            simulator_dump_seconds = 30;
    }
    simulator_dump_due =
        current_tick + simulator_dump_seconds * HZ;
#endif

    video = crazypod_video_catalog_get(index);
    if(video == NULL) {
        last_video_result = CRAZYPOD_VIDEO_INVALID_FILE;
        return last_video_result;
    }
    if(!crazypod_video_catalog_path_supported(video->path)) {
        last_video_result = CRAZYPOD_VIDEO_UNSUPPORTED;
        return last_video_result;
    }

    crazypod_videos_suspend();
    video_engine_message[0] = '\0';
    video_buffer_allocation_failed = false;
    audio_stop();
    cpu_boost(true);
    lcd_clear_display();
    lcd_update();

    if(stream_init() < STREAM_OK) {
        result = video_buffer_allocation_failed
            ? CRAZYPOD_VIDEO_NO_MEMORY
            : CRAZYPOD_VIDEO_ENGINE_FAILED;
        goto cleanup;
    }
    engine_initialized = true;
    {
        int open_result = stream_open(video->path);

        if(open_result < STREAM_OK) {
            result = open_result == STREAM_UNSUPPORTED
                ? CRAZYPOD_VIDEO_UNSUPPORTED
                : CRAZYPOD_VIDEO_OPEN_FAILED;
            goto cleanup;
        }
    }
    stream_opened = true;
    duration = stream_get_duration();
    vo_rect_set_ext(&clip, 0, 0, LCD_WIDTH,
                    LCD_HEIGHT - CRAZYPOD_VIDEO_UI_HEIGHT);
    stream_vo_set_clip(&clip);
    stream_show_vo(true);
    resume = valid_resume_time(video, duration);
    if(resume > 0)
        stream_seek(resume, SEEK_SET);
    if(stream_play() < STREAM_OK) {
        result = CRAZYPOD_VIDEO_ENGINE_FAILED;
        goto cleanup;
    }
    clock_check_time = stream_get_time();
    clock_check_tick =
        current_tick + CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
    draw_video_controls(
        video, false, resume > 0 ? "RESUMED" : NULL);

    for(;;) {
        int status = stream_status();
        int button;
        int base;

        if(status == STREAM_STOPPED) {
            completed = true;
            break;
        }
        if(!clock_checked &&
           !TIME_BEFORE(current_tick, clock_check_tick)) {
            clock_checked = true;
            clock_stalled = stream_get_time() == clock_check_time;
        }
        if(!TIME_BEFORE(current_tick, last_draw_tick + HZ / 4)) {
            draw_video_controls(
                video, paused,
                clock_stalled ? "CLOCK STALLED" : NULL);
            last_draw_tick = current_tick;
        }
#ifdef SIMULATOR
        if(simulator_dump &&
           !TIME_BEFORE(current_tick, simulator_dump_due)) {
            screen_dump();
            break;
        }
#endif
        reset_poweroff_timer();
        button = button_get_w_tmo(HZ / 20);
        if(button == BUTTON_NONE)
            continue;
        if(button == SYS_USB_CONNECTED ||
           button == SYS_POWEROFF ||
           button == SYS_REBOOT) {
            repost_system_event = true;
            system_event = button;
            system_event_data = button_get_data();
            break;
        }
        base = button & ~(BUTTON_REL | BUTTON_REPEAT);
        if(base == BUTTON_MENU && (button & BUTTON_REL)) {
            break;
        }
        if(base == BUTTON_PLAY && (button & BUTTON_REL)) {
            if(paused) {
                stream_resume();
                paused = false;
                clock_check_time = stream_get_time();
                clock_check_tick =
                    current_tick +
                    CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
                clock_checked = false;
                clock_stalled = false;
            }
            else {
                stream_pause();
                paused = true;
            }
            draw_video_controls(video, paused, NULL);
        }
        else if((base == BUTTON_LEFT || base == BUTTON_RIGHT) &&
                !(button & BUTTON_REL)) {
            uint32_t target =
                seek_target(base == BUTTON_RIGHT ? 1 : -1);

            stream_seek(target, SEEK_SET);
            clock_check_time = stream_get_time();
            clock_check_tick =
                current_tick +
                CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
            clock_checked = paused;
            clock_stalled = false;
            draw_video_controls(video, paused,
                                base == BUTTON_RIGHT
                                    ? "FORWARD 10 SEC"
                                    : "BACK 10 SEC");
        }
        else if(base == BUTTON_SCROLL_FWD ||
                base == BUTTON_SCROLL_BACK) {
            adjust_video_volume(
                base == BUTTON_SCROLL_FWD ? 1 : -1);
            draw_video_controls(video, paused, "VOLUME");
        }
    }

cleanup:
    if(stream_opened) {
        uint32_t elapsed = stream_get_time();
        uint32_t next_resume;

        stream_stop();
        stream_close();
        if(completed ||
           (duration > 0 &&
            elapsed + CRAZYPOD_VIDEO_RESUME_END_SECONDS * TS_SECOND >=
                duration))
            next_resume = 0;
        else if(elapsed >=
                CRAZYPOD_VIDEO_RESUME_MIN_SECONDS * TS_SECOND)
            next_resume = elapsed;
        else
            next_resume = 0;
        crazypod_video_catalog_update_playback(
            index, next_resume, duration);
        crazypod_video_poster_mark_changed();
    }
    if(engine_initialized)
        stream_exit();
    video_engine_release_audio_buffer();
    cpu_boost(false);
    button_clear_queue();
    if(repost_system_event)
        button_queue_post(system_event, system_event_data);
    crazypod_videos_resume();
    last_video_result = result;
    return result;
}

enum crazypod_video_result crazypod_video_last_result(void)
{
    return last_video_result;
}

const char *crazypod_video_result_message(
    enum crazypod_video_result result)
{
    switch(result) {
    case CRAZYPOD_VIDEO_OK:
        return "";
    case CRAZYPOD_VIDEO_INVALID_FILE:
        return "Video is no longer available";
    case CRAZYPOD_VIDEO_NO_MEMORY:
        return "Not enough memory to play this video";
    case CRAZYPOD_VIDEO_UNSUPPORTED:
        return "Convert this video to MPEG-1 or MPEG-2";
    case CRAZYPOD_VIDEO_OPEN_FAILED:
        return "Could not open this video";
    case CRAZYPOD_VIDEO_ENGINE_FAILED:
    default:
        return video_engine_message[0] != '\0'
            ? video_engine_message : "Video playback failed";
    }
}

#endif
