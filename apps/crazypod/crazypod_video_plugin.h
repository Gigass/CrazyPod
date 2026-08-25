#ifndef CRAZYPOD_VIDEO_PLUGIN_H
#define CRAZYPOD_VIDEO_PLUGIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "config.h"
#include "audio.h"
#include "button.h"
#include "codec_thread.h"
#include "dir.h"
#include "dsp_core.h"
#include "file.h"
#include "gcc_extensions.h"
#include "kernel.h"
#include "lcd.h"
#include "pcm_mixer.h"
#include "pcmbuf.h"
#include "playback.h"
#include "sound.h"
#include "storage.h"
#include "system.h"
#include "thread.h"

#ifdef CRAZYPOD_VIDEO_CORE
#undef IBSS_ATTR
#define IBSS_ATTR
#endif

#undef memset
#undef memcpy
#undef memmove
#undef memcmp

#ifndef DEBUGF
#define DEBUGF(...) do { } while(0)
#endif

struct menu_item_ex;
struct gui_synclist;

struct crazypod_video_plugin_api {
    void (*splash)(int ticks, const char *format, ...);
    void (*splashf)(int ticks, const char *format, ...);
    void (*lcd_update)(void);
    void (*lcd_clear_display)(void);
    void (*lcd_update_rect)(int x, int y, int width, int height);
    void (*lcd_fillrect)(int x, int y, int width, int height);
    void (*lcd_set_foreground)(unsigned color);
    unsigned (*lcd_get_foreground)(void);
    void (*lcd_set_background)(unsigned color);
    unsigned (*lcd_get_background)(void);
    void (*lcd_blit_yuv)(unsigned char * const source[3],
                         int source_x, int source_y, int stride,
                         int x, int y, int width, int height);

    int (*open)(const char *path, int flags);
    int (*close)(int fd);
    ssize_t (*read)(int fd, void *buffer, size_t size);
    off_t (*lseek)(int fd, off_t offset, int whence);
    off_t (*filesize)(int fd);
    void (*storage_sleep)(void);
    void (*storage_spin)(void);
    void (*stub_storage_sleep)(void);
    void (*stub_storage_spin)(void);
    void (*ata_spin)(void);

    void (*yield)(void);
    unsigned int (*create_thread)(
        void (*function)(void), void *stack, size_t stack_size,
        unsigned flags, const char *name
        IF_PRIO(, int priority)
        IF_COP(, unsigned int core));
    unsigned int (*thread_self)(void);
    void (*thread_wait)(unsigned int thread_id);
#ifdef HAVE_PRIORITY_SCHEDULING
    int (*thread_set_priority)(unsigned int thread_id, int priority);
#endif
    void (*mutex_init)(struct mutex *mutex);
    void (*mutex_lock)(struct mutex *mutex);
    void (*mutex_unlock)(struct mutex *mutex);
#ifdef CPU_BOOST_LOGGING
    void (*cpu_boost_)(bool enabled, char *location, int line);
#else
    void (*cpu_boost)(bool enabled);
#endif
#ifdef HAVE_SCHEDULER_BOOSTCTRL
    void (*trigger_cpu_boost)(void);
    void (*cancel_cpu_boost)(void);
#endif
    void (*commit_dcache)(void);
    void (*commit_discard_dcache)(void);

    void (*queue_init)(struct event_queue *queue, bool register_queue);
    void (*queue_post)(struct event_queue *queue, long id, intptr_t data);
    void (*queue_wait_w_tmo)(struct event_queue *queue,
                             struct queue_event *event, int ticks);
    void (*queue_enable_queue_send)(
        struct event_queue *queue, struct queue_sender_list *send,
        unsigned int thread_id);
    void (*queue_wait)(struct event_queue *queue,
                       struct queue_event *event);
    bool (*queue_empty)(const struct event_queue *queue);
    intptr_t (*queue_send)(struct event_queue *queue,
                           long id, intptr_t data);
    void (*queue_reply)(struct event_queue *queue, intptr_t value);

    volatile long *current_tick;
    void *(*memset)(void *destination, int value, size_t size);
    void *(*memcpy)(void *destination, const void *source, size_t size);
    void *(*memmove)(void *destination, const void *source, size_t size);
    int (*memcmp)(const void *left, const void *right, size_t size);

#ifdef HAVE_PITCHCONTROL
    void (*sound_set_pitch)(int32_t pitch);
#endif
    void (*pcm_play_lock)(void);
    void (*pcm_play_unlock)(void);
    intptr_t (*dsp_configure)(
        struct dsp_config *dsp, unsigned int setting, intptr_t value);
    struct dsp_config *(*dsp_get_config)(unsigned int id);
    void (*dsp_process)(struct dsp_config *dsp,
                        struct dsp_buffer *source,
                        struct dsp_buffer *destination,
                        bool thread_yield);
#ifdef HAVE_PITCHCONTROL
    void (*dsp_set_timestretch)(int32_t percent);
#endif

#if INPUT_SRC_CAPS != 0
    void (*audio_set_output_source)(int source);
    void (*audio_set_input_source)(int source, unsigned flags);
#endif

    enum channel_status (*mixer_channel_status)(
        enum pcm_mixer_channel channel);
    void (*mixer_channel_play_data)(
        enum pcm_mixer_channel channel,
        pcm_play_callback_type get_more,
        const void *start, size_t size);
    void (*mixer_channel_play_pause)(
        enum pcm_mixer_channel channel, bool play);
    void (*mixer_channel_stop)(enum pcm_mixer_channel channel);
    void (*mixer_channel_set_amplitude)(
        enum pcm_mixer_channel channel, unsigned int amplitude);
    size_t (*mixer_channel_get_bytes_waiting)(
        enum pcm_mixer_channel channel);
    void (*mixer_set_frequency)(unsigned int sample_rate);
    unsigned int (*mixer_get_frequency)(void);
    void (*pcmbuf_fade)(bool fade, bool in);

    void (*codec_thread_do_callback)(
        void (*callback)(void), unsigned int *audio_thread_id);
    bool (*codec_thread_is_borrowed)(void);
    void *(*plugin_get_audio_buffer)(size_t *size);
};

extern const struct crazypod_video_plugin_api *rb;

#endif
