#include "crazypod_perf_log.h"

#ifdef CRAZYPOD_PERF_LOG

#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "buffering.h"
#include "file.h"
#include "kernel.h"
#include "pcmbuf.h"
#include "system.h"

#include "crazypod_frameclock.h"
#include "crazypod_music.h"

#define PERF_LOG_PATH "/.crazypod/perf.log"
#define PERF_LOG_MAX_BYTES (512 * 1024)
#define SAMPLE_INTERVAL (HZ / 2)
#define WRITE_INTERVAL (HZ * 10)

static struct {
    long last_sample;
    long last_write;
    bool stopped;
    bool header_written;
    unsigned lv_start_us;
    /* Window accumulators, reset after every write. */
    unsigned lv_calls;
    unsigned lv_max_us;
    unsigned lv_total_us;
    unsigned samples;
    unsigned lowdata_samples;
    size_t pcm_free_min;
    size_t buffered_min;
    size_t useful_min;
    struct crazypod_present_diagnostics present_base;
    unsigned write_us;
} perf = {
    .pcm_free_min = (size_t)-1,
    .buffered_min = (size_t)-1,
    .useful_min = (size_t)-1,
};

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
    perf.samples = 0;
    perf.lowdata_samples = 0;
    perf.pcm_free_min = (size_t)-1;
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
    if(pcmbuf_is_lowdata())
        perf.lowdata_samples++;
    buffering_get_debugdata(&buffering);
    if(buffering.buffered_data < perf.buffered_min)
        perf.buffered_min = buffering.buffered_data;
    if(buffering.useful_data < perf.useful_min)
        perf.useful_min = buffering.useful_data;
}

static void write_line(long now)
{
    struct crazypod_present_diagnostics present;
    struct buffering_debug buffering;
    unsigned start_us = USEC_TIMER;
    int fd;

    fd = open(PERF_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(fd < 0)
        return;
    if(filesize(fd) >= PERF_LOG_MAX_BYTES) {
        close(fd);
        perf.stopped = true;
        return;
    }
    if(!perf.header_written) {
        fdprintf(fd,
            "# t=seconds st=audio_status boost=cpu_boost_counter "
            "scan=music_scanning pcm=min_free/size low=lowdata_samples/samples "
            "buf=min_buffered/min_useful/watermark "
            "lv=calls/max_us/total_us pres=presents/full/misses/timeouts "
            "pmax=max_present_us home=renders/timeouts wr=prev_write_us\n");
        perf.header_written = true;
    }
    crazypod_present_get_diagnostics(&present);
    buffering_get_debugdata(&buffering);
    fdprintf(fd,
        "t=%ld st=%d boost=%d scan=%d pcm=%lu/%lu low=%u/%u "
        "buf=%lu/%lu/%lu lv=%u/%u/%u pres=%lu/%lu/%lu/%lu pmax=%lu "
        "home=%lu/%lu wr=%u\n",
        now / HZ, audio_status(), get_cpu_boost_counter(),
        crazypod_music_is_scanning() ? 1 : 0,
        (unsigned long)(perf.pcm_free_min == (size_t)-1
            ? 0 : perf.pcm_free_min),
        (unsigned long)pcmbuf_get_bufsize(),
        perf.lowdata_samples, perf.samples,
        (unsigned long)(perf.buffered_min == (size_t)-1
            ? 0 : perf.buffered_min),
        (unsigned long)(perf.useful_min == (size_t)-1
            ? 0 : perf.useful_min),
        (unsigned long)buffering.watermark,
        perf.lv_calls, perf.lv_max_us, perf.lv_total_us,
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
    close(fd);
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
        write_line(now);
        reset_window();
    }
}

#endif
