#include "crazypod_perf_log.h"

#ifdef CRAZYPOD_PERF_LOG

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "buffering.h"
#include "file.h"
#include "kernel.h"
#include "pcmbuf.h"
#include "storage.h"
#include "system.h"

#include "lvgl.h"

#include "crazypod_frameclock.h"
#include "crazypod_music.h"

#define PERF_LOG_PATH "/.crazypod/perf.log"
#define PERF_LOG_MAX_BYTES (512 * 1024)
#define SAMPLE_INTERVAL (HZ / 2)
#define WRITE_INTERVAL (HZ * 10)
#define LINE_BUFFER_SIZE 4096
#define LINE_BUFFER_FLUSH_AT (LINE_BUFFER_SIZE - 640)
#define DRAW_TYPE_COUNT 16
#define INVALIDATION_SLOTS 4

struct draw_stats {
    unsigned count;
    unsigned total_us;
};

struct invalidation {
    lv_area_t area;
    unsigned count;
};

static const char *const draw_type_names[DRAW_TYPE_COUNT] = {
    "none", "fill", "bord", "shad", "let", "lab", "img", "lay",
    "line", "arc", "tri", "mask", "mbmp", "blur", "vec", "3d",
};

static struct {
    long last_sample;
    long last_write;
    bool stopped;
    bool header_written;
    unsigned lv_start_us;
    unsigned render_start_us;
    unsigned draw_start_us;
    unsigned draw_depth;
    /* Window accumulators, reset after every line. */
    unsigned lv_calls;
    unsigned lv_max_us;
    unsigned lv_total_us;
    unsigned renders;
    unsigned render_max_us;
    unsigned render_total_us;
    unsigned flushes;
    unsigned flushed_pixels;
    struct draw_stats draw[DRAW_TYPE_COUNT];
    unsigned invalidations;
    struct invalidation invalidation[INVALIDATION_SLOTS];
    unsigned samples;
    unsigned lowdata_samples;
    size_t pcm_free_min;
    size_t pcm_free_max;
    size_t buffered_min;
    size_t useful_min;
    struct crazypod_present_diagnostics present_base;
    unsigned write_us;
    /* Pending text, written out when the disk is awake anyway. */
    char lines[LINE_BUFFER_SIZE];
    size_t line_length;
} perf = {
    .pcm_free_min = (size_t)-1,
    .buffered_min = (size_t)-1,
    .useful_min = (size_t)-1,
};

static void note_invalidation(const lv_area_t *area)
{
    int index;
    int free_slot = -1;
    int least = 0;

    perf.invalidations++;
    for(index = 0; index < INVALIDATION_SLOTS; ++index) {
        struct invalidation *slot = &perf.invalidation[index];

        if(slot->count == 0) {
            if(free_slot < 0)
                free_slot = index;
            continue;
        }
        if(slot->area.x1 == area->x1 && slot->area.y1 == area->y1 &&
           slot->area.x2 == area->x2 && slot->area.y2 == area->y2) {
            slot->count++;
            return;
        }
        if(slot->count < perf.invalidation[least].count)
            least = index;
    }
    /* Keep the four most frequent shapes; a new one replaces the rarest
     * only after that one has been seen just once. */
    if(free_slot < 0) {
        if(perf.invalidation[least].count > 1)
            return;
        free_slot = least;
    }
    perf.invalidation[free_slot].area = *area;
    perf.invalidation[free_slot].count = 1;
}

static void display_event(lv_event_t *event)
{
    switch(lv_event_get_code(event)) {
    case LV_EVENT_RENDER_START:
        perf.render_start_us = USEC_TIMER;
        break;
    case LV_EVENT_RENDER_READY: {
        unsigned elapsed = USEC_TIMER - perf.render_start_us;

        perf.renders++;
        perf.render_total_us += elapsed;
        if(elapsed > perf.render_max_us)
            perf.render_max_us = elapsed;
        break;
    }
    case LV_EVENT_INVALIDATE_AREA:
        note_invalidation(lv_event_get_param(event));
        break;
    default:
        break;
    }
}

void crazypod_perf_log_attach_display(void *display)
{
    lv_display_add_event_cb(
        display, display_event, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(
        display, display_event, LV_EVENT_RENDER_READY, NULL);
    lv_display_add_event_cb(
        display, display_event, LV_EVENT_INVALIDATE_AREA, NULL);
}

void crazypod_perf_log_flush(unsigned pixels)
{
    perf.flushes++;
    perf.flushed_pixels += pixels;
}

void crazypod_perf_log_draw_begin(void)
{
    if(perf.draw_depth++ == 0)
        perf.draw_start_us = USEC_TIMER;
}

void crazypod_perf_log_draw_end(int type)
{
    if(perf.draw_depth == 0)
        return;
    if(--perf.draw_depth == 0) {
        struct draw_stats *stats;

        if(type < 0 || type >= DRAW_TYPE_COUNT)
            type = 0;
        stats = &perf.draw[type];
        stats->count++;
        stats->total_us += USEC_TIMER - perf.draw_start_us;
    }
}

void crazypod_perf_log_lv_begin(void)
{
    perf.lv_start_us = USEC_TIMER;
}

void crazypod_perf_log_lv_end(void)
{
    unsigned elapsed = USEC_TIMER - perf.lv_start_us;

    perf.lv_calls++;
    perf.lv_total_us += elapsed;
    if(elapsed > perf.lv_max_us)
        perf.lv_max_us = elapsed;
}

static void reset_window(void)
{
    perf.lv_calls = 0;
    perf.lv_max_us = 0;
    perf.lv_total_us = 0;
    perf.renders = 0;
    perf.render_max_us = 0;
    perf.render_total_us = 0;
    perf.flushes = 0;
    perf.flushed_pixels = 0;
    memset(perf.draw, 0, sizeof(perf.draw));
    perf.invalidations = 0;
    memset(perf.invalidation, 0, sizeof(perf.invalidation));
    perf.samples = 0;
    perf.lowdata_samples = 0;
    perf.pcm_free_min = (size_t)-1;
    perf.pcm_free_max = 0;
    perf.buffered_min = (size_t)-1;
    perf.useful_min = (size_t)-1;
    crazypod_present_get_diagnostics(&perf.present_base);
}

static void sample(void)
{
    struct buffering_debug buffering;
    size_t pcm_free = pcmbuf_free();

    perf.samples++;
    if(pcm_free < perf.pcm_free_min)
        perf.pcm_free_min = pcm_free;
    if(pcm_free > perf.pcm_free_max)
        perf.pcm_free_max = pcm_free;
    if(pcmbuf_is_lowdata())
        perf.lowdata_samples++;
    buffering_get_debugdata(&buffering);
    if(buffering.buffered_data < perf.buffered_min)
        perf.buffered_min = buffering.buffered_data;
    if(buffering.useful_data < perf.useful_min)
        perf.useful_min = buffering.useful_data;
}

static void append(const char *text)
{
    size_t length = strlen(text);
    size_t room = sizeof(perf.lines) - perf.line_length - 1;

    if(length > room)
        length = room;
    memcpy(perf.lines + perf.line_length, text, length);
    perf.line_length += length;
    perf.lines[perf.line_length] = '\0';
}

static void append_draw_stats(void)
{
    char text[24];
    int type;

    append(" dt=");
    for(type = 1; type < DRAW_TYPE_COUNT; ++type) {
        const struct draw_stats *stats = &perf.draw[type];

        if(stats->count == 0)
            continue;
        snprintf(text, sizeof(text), "%s:%u/%u,",
                 draw_type_names[type], stats->count,
                 stats->total_us / 1000);
        append(text);
    }
}

static void append_invalidations(void)
{
    char text[40];
    int index;

    snprintf(text, sizeof(text), " inv=%u", perf.invalidations);
    append(text);
    for(index = 0; index < INVALIDATION_SLOTS; ++index) {
        const struct invalidation *slot = &perf.invalidation[index];

        if(slot->count == 0)
            continue;
        snprintf(text, sizeof(text), ",%d.%d-%d.%d:%u",
                 (int)slot->area.x1, (int)slot->area.y1,
                 (int)slot->area.x2, (int)slot->area.y2,
                 slot->count);
        append(text);
    }
}

static void format_line(long now)
{
    struct crazypod_present_diagnostics present;
    struct buffering_debug buffering;
    char text[256];

    if(!perf.header_written) {
        append("# t=seconds st=audio_status boost=cpu_boost_counter "
               "scan=music_scanning pcm=min_free/max_free/size "
               "low=lowdata_samples/samples "
               "buf=min_buffered/min_useful/watermark "
               "lv=calls/max_us/total_us rend=renders/max_us/total_us "
               "fl=flushes/pixels pres=presents/full/misses/timeouts "
               "pmax=max_present_us home=renders/timeouts "
               "wr=prev_write_us dt=type:count/ms,... "
               "inv=count,x1.y1-x2.y2:count,...\n");
        perf.header_written = true;
    }
    crazypod_present_get_diagnostics(&present);
    buffering_get_debugdata(&buffering);
    snprintf(text, sizeof(text),
        "t=%ld st=%d boost=%d scan=%d pcm=%lu/%lu/%lu low=%u/%u "
        "buf=%lu/%lu/%lu lv=%u/%u/%u rend=%u/%u/%u fl=%u/%u "
        "pres=%lu/%lu/%lu/%lu pmax=%lu home=%lu/%lu wr=%u",
        now / HZ, audio_status(), get_cpu_boost_counter(),
        crazypod_music_is_scanning() ? 1 : 0,
        (unsigned long)(perf.pcm_free_min == (size_t)-1
            ? 0 : perf.pcm_free_min),
        (unsigned long)perf.pcm_free_max,
        (unsigned long)pcmbuf_get_bufsize(),
        perf.lowdata_samples, perf.samples,
        (unsigned long)(perf.buffered_min == (size_t)-1
            ? 0 : perf.buffered_min),
        (unsigned long)(perf.useful_min == (size_t)-1
            ? 0 : perf.useful_min),
        (unsigned long)buffering.watermark,
        perf.lv_calls, perf.lv_max_us, perf.lv_total_us,
        perf.renders, perf.render_max_us, perf.render_total_us,
        perf.flushes, perf.flushed_pixels,
        (unsigned long)(present.presents - perf.present_base.presents),
        (unsigned long)(present.full_presents -
            perf.present_base.full_presents),
        (unsigned long)(present.deadline_misses -
            perf.present_base.deadline_misses),
        (unsigned long)(present.present_timeouts -
            perf.present_base.present_timeouts),
        (unsigned long)present.max_present_us,
        (unsigned long)(present.home_renders -
            perf.present_base.home_renders),
        (unsigned long)(present.home_render_timeouts -
            perf.present_base.home_render_timeouts),
        perf.write_us);
    append(text);
    append_draw_stats();
    append_invalidations();
    append("\n");
}

static void write_lines(void)
{
    unsigned start_us = USEC_TIMER;
    int fd;

    fd = open(PERF_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(fd < 0)
        return;
    if(filesize(fd) >= PERF_LOG_MAX_BYTES)
        perf.stopped = true;
    else
        write(fd, perf.lines, perf.line_length);
    close(fd);
    perf.line_length = 0;
    perf.lines[0] = '\0';
    perf.write_us = USEC_TIMER - start_us;
}

void crazypod_perf_log_tick(long now)
{
    if(perf.stopped)
        return;
    if(perf.last_write == 0) {
        perf.last_write = now;
        perf.last_sample = now;
        reset_window();
        return;
    }
    if(TIME_AFTER(now, perf.last_sample + SAMPLE_INTERVAL)) {
        perf.last_sample = now;
        sample();
    }
    if(TIME_AFTER(now, perf.last_write + WRITE_INTERVAL)) {
        perf.last_write = now;
        format_line(now);
        reset_window();
    }
    if(perf.line_length > 0 &&
       (perf.line_length >= LINE_BUFFER_FLUSH_AT ||
        storage_disk_is_active()))
        write_lines();
}

#endif
