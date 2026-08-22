#include "config.h"

#include "crazypod_l10n.h"

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
#include "crazypod_screenshot.h"
#include "crazypod_state.h"
#include "crazypod_videos.h"
#include "platform/crazypod_platform_display.h"
#include "video/crazypod_video_catalog.h"
#include "video/crazypod_video_engine.h"
#include "video/crazypod_video_poster.h"

struct mpeg_settings settings = {
    .limitfps = 1,
    .skipframes = 1,
};

#define CRAZYPOD_VIDEO_SEEK_SECONDS 10u
#define CRAZYPOD_VIDEO_RESUME_MIN_SECONDS 5u
#define CRAZYPOD_VIDEO_RESUME_END_SECONDS 5u
#define CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS 2u
#define CRAZYPOD_VIDEO_CONTROLS_TIMEOUT (5 * HZ)

static enum crazypod_video_result last_video_result = CRAZYPOD_VIDEO_OK;
static int video_audio_buffer_handle;
static size_t video_audio_buffer_size;
static bool video_buffer_allocation_failed;
static char video_engine_message[96];
static bool videos_storage_suspended;
static bool videos_lock_suspended;
static bool videos_route_suspended;
static bool videos_refresh_pending;
static struct mutex video_controls_mutex;

static struct {
    const char *title;
    const char *message;
    uint32_t elapsed_seconds;
    uint32_t duration_seconds;
    int volume;
    bool paused;
    bool visible;
} video_controls;

static void video_engine_splashf(int ticks, const char *format, ...)
{
    va_list arguments;

    (void)ticks;
    va_start(arguments, format);
    vsnprintf(video_engine_message, sizeof(video_engine_message),
              format, arguments);
    va_end(arguments);
}

static void video_engine_set_error(const char *message)
{
    snprintf(video_engine_message, sizeof(video_engine_message),
             "%s", message != NULL ? message : "");
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

static void video_engine_lcd_blit_yuv(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height)
{
    mutex_lock(&video_controls_mutex);
    crazypod_lcd_draw_video_frame(
        source, source_x, source_y, stride,
        destination_x, destination_y, width, height,
        video_controls.visible, video_controls.title,
        video_controls.elapsed_seconds,
        video_controls.duration_seconds,
        video_controls.volume, video_controls.paused,
        video_controls.message);
    mutex_unlock(&video_controls_mutex);
}

static const struct crazypod_video_engine_host video_session_host = {
    .present_yuv = video_engine_lcd_blit_yuv,
    .set_error = video_engine_set_error,
};

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
    .lcd_blit_yuv = video_engine_lcd_blit_yuv,
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
    uint32_t duration = crazypod_video_engine_duration_ms();
    uint32_t elapsed = crazypod_video_engine_position_ms();

    mutex_lock(&video_controls_mutex);
    video_controls.visible = false;
    video_controls.title = video->name;
    video_controls.message = message;
    video_controls.elapsed_seconds = elapsed / 1000u;
    video_controls.duration_seconds = duration / 1000u;
    video_controls.volume = global_status.volume;
    video_controls.paused = paused;
    video_controls.visible = true;
    mutex_unlock(&video_controls_mutex);
    crazypod_video_engine_redraw();
}

static void reveal_video_controls(
    const struct crazypod_video_catalog_entry *video,
    bool paused, const char *message, long *hide_tick)
{
    draw_video_controls(video, paused, message);
    *hide_tick = current_tick + CRAZYPOD_VIDEO_CONTROLS_TIMEOUT;
}

static void hide_video_controls(void)
{
    mutex_lock(&video_controls_mutex);
    video_controls.visible = false;
    mutex_unlock(&video_controls_mutex);
    crazypod_video_engine_redraw();
}

static void flash_video_frame(void)
{
    fb_data *pixel =
        (fb_data *)crazypod_platform_display_framebuffer();
    fb_data *end = pixel + LCD_WIDTH * LCD_HEIGHT;

    while(pixel < end) {
        *pixel ^= (fb_data)LCD_WHITE;
        ++pixel;
    }
    lcd_update();
    sleep((HZ / 20) > 0 ? HZ / 20 : 1);
    pixel = (fb_data *)crazypod_platform_display_framebuffer();
    while(pixel < end) {
        *pixel ^= (fb_data)LCD_WHITE;
        ++pixel;
    }
    lcd_update();
}

static uint32_t valid_resume_position_ms(
    const struct crazypod_video_catalog_entry *video,
    uint32_t duration_ms)
{
    uint32_t resume_ms = (uint32_t)(
        (uint64_t)video->resume_ticks * 1000u / TS_SECOND);
    uint32_t minimum_ms = CRAZYPOD_VIDEO_RESUME_MIN_SECONDS * 1000u;
    uint32_t end_margin_ms =
        CRAZYPOD_VIDEO_RESUME_END_SECONDS * 1000u;

    if(resume_ms < minimum_ms || duration_ms <= end_margin_ms ||
       resume_ms >= duration_ms - end_margin_ms)
        return 0;
    return resume_ms;
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

static uint32_t seek_target_ms(int direction)
{
    uint32_t current = crazypod_video_engine_position_ms();
    uint32_t duration = crazypod_video_engine_duration_ms();
    uint32_t amount = CRAZYPOD_VIDEO_SEEK_SECONDS * 1000u;

    if(direction < 0)
        return current > amount ? current - amount : 0;
    if(duration > amount && current < duration - amount)
        return current + amount;
    return duration > 0 ? duration - 1 : current;
}

void crazypod_videos_init(void)
{
    bool catalog_loaded;

    mutex_init(&video_controls_mutex);
    crazypod_video_engine_set_host(&video_session_host);
    videos_storage_suspended = false;
    videos_lock_suspended = false;
    videos_route_suspended = true;
    crazypod_video_poster_init();
    crazypod_video_poster_suspend();
    catalog_loaded = crazypod_video_catalog_init();
    videos_refresh_pending = !catalog_loaded;
}

void crazypod_videos_refresh(void)
{
    if(videos_storage_suspended || videos_lock_suspended ||
       videos_route_suspended) {
        videos_refresh_pending = true;
        return;
    }
    videos_refresh_pending = false;
    crazypod_video_poster_suspend();
    crazypod_video_catalog_refresh();
    crazypod_video_poster_reset();
}

void crazypod_videos_ensure_catalog(void)
{
    if(videos_refresh_pending &&
       !videos_storage_suspended &&
       !videos_lock_suspended &&
       !videos_route_suspended)
        crazypod_videos_refresh();
}

void crazypod_videos_suspend(void)
{
    videos_storage_suspended = true;
    crazypod_video_poster_suspend();
}

void crazypod_videos_resume(void)
{
    videos_storage_suspended = false;
    videos_refresh_pending = true;
}

void crazypod_videos_set_lock_suspended(bool suspended)
{
    if(videos_lock_suspended == suspended)
        return;
    videos_lock_suspended = suspended;
    if(suspended) {
        crazypod_video_poster_suspend();
        return;
    }
    if(!videos_storage_suspended &&
       !videos_route_suspended &&
       !videos_refresh_pending)
        crazypod_video_poster_resume();
}

void crazypod_videos_set_route_suspended(bool suspended)
{
    if(videos_route_suspended == suspended)
        return;
    videos_route_suspended = suspended;
    if(suspended) {
        crazypod_video_poster_suspend();
        return;
    }
    if(!videos_storage_suspended &&
       !videos_lock_suspended &&
       !videos_refresh_pending)
        crazypod_video_poster_resume();
}

void crazypod_videos_invalidate_catalog(void)
{
    videos_refresh_pending = true;
    crazypod_video_catalog_invalidate();
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

bool crazypod_video_delete(int index)
{
    bool deleted;

    if(videos_storage_suspended || videos_lock_suspended)
        return false;
    crazypod_video_poster_suspend();
    deleted = crazypod_video_catalog_delete(index);
    if(deleted)
        crazypod_video_poster_reset();
    if(videos_storage_suspended || videos_lock_suspended ||
       videos_route_suspended)
        crazypod_video_poster_suspend();
    else
        crazypod_video_poster_resume();
    return deleted;
}

static enum crazypod_video_result map_engine_result(
    enum crazypod_video_engine_result result)
{
    switch(result) {
    case CRAZYPOD_VIDEO_ENGINE_OK:
        return CRAZYPOD_VIDEO_OK;
    case CRAZYPOD_VIDEO_ENGINE_UNSUPPORTED:
        return CRAZYPOD_VIDEO_UNSUPPORTED;
    case CRAZYPOD_VIDEO_ENGINE_OPEN_FAILED:
        return CRAZYPOD_VIDEO_OPEN_FAILED;
    case CRAZYPOD_VIDEO_ENGINE_NO_MEMORY:
        return CRAZYPOD_VIDEO_NO_MEMORY;
    case CRAZYPOD_VIDEO_ENGINE_ERROR:
    default:
        return CRAZYPOD_VIDEO_ENGINE_FAILED;
    }
}

enum crazypod_video_result crazypod_video_play(int index)
{
    const struct crazypod_video_catalog_entry *video;
    enum crazypod_video_result result = CRAZYPOD_VIDEO_OK;
    uint32_t duration_ms = 0;
    uint32_t resume_ms = 0;
    uint32_t last_draw_tick = 0;
    uint32_t clock_check_time = 0;
    long clock_check_tick = 0;
    bool engine_opened = false;
    bool paused = false;
    bool clock_checked = false;
    bool clock_stalled = false;
    bool completed = false;
    bool repost_system_event = false;
    bool controls_visible = true;
    long controls_hide_tick = 0;
    const char *screenshot_message = NULL;
    long screenshot_message_until = 0;
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

    crazypod_video_poster_suspend();
    video_engine_message[0] = '\0';
    video_buffer_allocation_failed = false;
    audio_stop();
    cpu_boost(true);
    lcd_clear_display();
    lcd_update();

    memset(&video_controls, 0, sizeof(video_controls));
    {
        enum crazypod_video_engine_result open_result =
            crazypod_video_engine_open(video->path);

        if(open_result != CRAZYPOD_VIDEO_ENGINE_OK) {
            result = video_buffer_allocation_failed
                ? CRAZYPOD_VIDEO_NO_MEMORY
                : map_engine_result(open_result);
            goto cleanup;
        }
    }
    engine_opened = true;
    duration_ms = crazypod_video_engine_duration_ms();
    resume_ms = valid_resume_position_ms(video, duration_ms);
    if(resume_ms > 0)
        crazypod_video_engine_seek(resume_ms);
    result = map_engine_result(crazypod_video_engine_play());
    if(result != CRAZYPOD_VIDEO_OK)
        goto cleanup;
    clock_check_time = crazypod_video_engine_position_ms();
    clock_check_tick =
        current_tick + CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
    reveal_video_controls(
        video, false, resume_ms > 0 ? CP_TR("RESUMED") : NULL,
        &controls_hide_tick);
    last_draw_tick = current_tick;

    for(;;) {
        enum crazypod_video_engine_status status;
        int button;
        int base;

        crazypod_video_engine_service();
        status = crazypod_video_engine_status();
        if(status == CRAZYPOD_VIDEO_ENGINE_ENDED ||
           status == CRAZYPOD_VIDEO_ENGINE_STOPPED) {
            completed = true;
            break;
        }
        if(status == CRAZYPOD_VIDEO_ENGINE_STATUS_FAILED) {
            result = CRAZYPOD_VIDEO_ENGINE_FAILED;
            break;
        }
        if(!clock_checked &&
           !TIME_BEFORE(current_tick, clock_check_tick)) {
            clock_checked = true;
            clock_stalled =
                crazypod_video_engine_position_ms() == clock_check_time;
        }
        if(controls_visible && !paused &&
           !TIME_BEFORE(current_tick, controls_hide_tick)) {
            hide_video_controls();
            controls_visible = false;
        }
        if(controls_visible &&
           !TIME_BEFORE(current_tick, last_draw_tick + HZ / 4)) {
            const char *message =
                clock_stalled ? CP_TR("CLOCK STALLED") : NULL;

            if(screenshot_message != NULL) {
                if(TIME_BEFORE(
                       current_tick,
                       screenshot_message_until))
                    message = screenshot_message;
                else
                    screenshot_message = NULL;
            }
            draw_video_controls(
                video, paused, message);
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
        if(base == (BUTTON_LEFT | BUTTON_RIGHT)) {
            if((button & (BUTTON_REL | BUTTON_REPEAT)) == 0) {
                bool resume_after_flash = !paused;
                bool saved;

                if(resume_after_flash)
                    crazypod_video_engine_pause();
                saved = crazypod_screenshot_capture();
                flash_video_frame();
                if(resume_after_flash)
                    crazypod_video_engine_resume();
                screenshot_message = saved
                    ? CP_TR("Saved to Photos")
                    : CP_TR("Screenshot failed");
                screenshot_message_until =
                    current_tick + HZ * 3 / 2;
                reveal_video_controls(
                    video, paused, screenshot_message,
                    &controls_hide_tick);
                controls_visible = true;
                last_draw_tick = current_tick;
            }
            continue;
        }
        if(base == BUTTON_MENU && (button & BUTTON_REL)) {
            break;
        }
        if(base == BUTTON_PLAY && (button & BUTTON_REL)) {
            if(paused) {
                crazypod_video_engine_resume();
                paused = false;
                clock_check_time =
                    crazypod_video_engine_position_ms();
                clock_check_tick =
                    current_tick +
                    CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
                clock_checked = false;
                clock_stalled = false;
            }
            else {
                crazypod_video_engine_pause();
                paused = true;
            }
            reveal_video_controls(
                video, paused, NULL, &controls_hide_tick);
            controls_visible = true;
            last_draw_tick = current_tick;
        }
        else if((base == BUTTON_LEFT || base == BUTTON_RIGHT) &&
                !(button & BUTTON_REL)) {
            uint32_t target =
                seek_target_ms(base == BUTTON_RIGHT ? 1 : -1);

            crazypod_video_engine_seek(target);
            clock_check_time =
                crazypod_video_engine_position_ms();
            clock_check_tick =
                current_tick +
                CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
            clock_checked = paused;
            clock_stalled = false;
            reveal_video_controls(
                video, paused,
                base == BUTTON_RIGHT
                    ? CP_TR("FORWARD 10 SEC")
                    : CP_TR("BACK 10 SEC"),
                &controls_hide_tick);
            controls_visible = true;
            last_draw_tick = current_tick;
        }
        else if(base == BUTTON_SCROLL_FWD ||
                base == BUTTON_SCROLL_BACK) {
            adjust_video_volume(
                base == BUTTON_SCROLL_FWD ? 1 : -1);
            reveal_video_controls(
                video, paused, CP_TR("VOLUME"),
                &controls_hide_tick);
            controls_visible = true;
            last_draw_tick = current_tick;
        }
    }

cleanup:
    mutex_lock(&video_controls_mutex);
    video_controls.visible = false;
    mutex_unlock(&video_controls_mutex);
    if(engine_opened) {
        uint32_t elapsed_ms =
            crazypod_video_engine_position_ms();
        uint32_t next_resume_ticks;
        uint32_t duration_ticks = (uint32_t)(
            (uint64_t)duration_ms * TS_SECOND / 1000u);

        if(completed ||
           (duration_ms > 0 &&
            elapsed_ms + CRAZYPOD_VIDEO_RESUME_END_SECONDS * 1000u >=
                duration_ms))
            next_resume_ticks = 0;
        else if(elapsed_ms >=
                CRAZYPOD_VIDEO_RESUME_MIN_SECONDS * 1000u)
            next_resume_ticks = (uint32_t)(
                (uint64_t)elapsed_ms * TS_SECOND / 1000u);
        else
            next_resume_ticks = 0;
        crazypod_video_engine_close();
        crazypod_video_catalog_update_playback(
            index, next_resume_ticks, duration_ticks);
        crazypod_video_poster_mark_changed();
    }
    else
        crazypod_video_engine_close();
    video_engine_release_audio_buffer();
    cpu_boost(false);
    button_clear_queue();
    if(repost_system_event)
        button_queue_post(system_event, system_event_data);
    if(!videos_storage_suspended &&
       !videos_lock_suspended &&
       !videos_route_suspended &&
       !videos_refresh_pending)
        crazypod_video_poster_resume();
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
        return CP_TR("Video is no longer available");
    case CRAZYPOD_VIDEO_NO_MEMORY:
        return CP_TR("Not enough memory to play this video");
    case CRAZYPOD_VIDEO_UNSUPPORTED:
        return video_engine_message[0] != '\0'
            ? video_engine_message
            : CP_TR("Convert this video to MPEG-1 or MPEG-2");
    case CRAZYPOD_VIDEO_OPEN_FAILED:
        return CP_TR("Could not open this video");
    case CRAZYPOD_VIDEO_ENGINE_FAILED:
    default:
        return video_engine_message[0] != '\0'
            ? video_engine_message : CP_TR("Video playback failed");
    }
}

#endif
