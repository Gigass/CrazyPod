#ifndef CRAZYPOD_PERF_LOG_H
#define CRAZYPOD_PERF_LOG_H

#include "config.h"

/*
 * Bring-up performance log for the PortalPlayer targets. The iPod Video port
 * feels slow and playback stutters, and every fix so far was a guess. This
 * appends one line every ten seconds to /.crazypod/perf.log describing what
 * the UI thread, the presenter and the audio pipeline were doing, so the next
 * change can be aimed at a number instead of a feeling.
 */
#if defined(HAVE_CRAZYPOD_UI) && defined(CPU_PP) && !defined(SIMULATOR)
#define CRAZYPOD_PERF_LOG

void crazypod_perf_log_lv_begin(void);
void crazypod_perf_log_lv_end(void);
void crazypod_perf_log_tick(long now);
#else
static inline void crazypod_perf_log_lv_begin(void) {}
static inline void crazypod_perf_log_lv_end(void) {}
static inline void crazypod_perf_log_tick(long now) { (void)now; }
#endif

#endif
