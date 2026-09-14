#ifndef CRAZYPOD_PERF_LOG_H
#define CRAZYPOD_PERF_LOG_H

#include "config.h"

/*
 * Bring-up performance log for the PortalPlayer targets. The iPod Video port
 * feels slow and playback stuttered, and every fix before this existed was a
 * guess. This appends one line every ten seconds to /.crazypod/perf.log
 * describing what the UI thread, the presenter, the LVGL renderer and the
 * audio pipeline were doing, so the next change can be aimed at a number
 * instead of a feeling. Lines are held in RAM and written only when the disk
 * is already awake, or when the buffer fills, because waking a sleeping ATA
 * device costs the UI thread most of a second.
 */
#if defined(HAVE_CRAZYPOD_UI) && defined(CPU_PP) && !defined(SIMULATOR)
#define CRAZYPOD_PERF_LOG

/* display is the lv_display_t* to observe; typed void so the stub costs
 * nothing on targets without the log. */
void crazypod_perf_log_attach_display(void *display);
void crazypod_perf_log_lv_begin(void);
void crazypod_perf_log_lv_end(void);
void crazypod_perf_log_flush(unsigned pixels);
/* Called by the LVGL software renderer around each draw task. */
void crazypod_perf_log_draw_begin(void);
void crazypod_perf_log_draw_end(int type);
void crazypod_perf_log_tick(long now);
#else
static inline void crazypod_perf_log_attach_display(void *display)
{
    (void)display;
}
static inline void crazypod_perf_log_lv_begin(void) {}
static inline void crazypod_perf_log_lv_end(void) {}
static inline void crazypod_perf_log_flush(unsigned pixels)
{
    (void)pixels;
}
static inline void crazypod_perf_log_tick(long now) { (void)now; }
#endif

#endif
