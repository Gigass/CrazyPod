#ifndef CRAZYPOD_FRAMECLOCK_H
#define CRAZYPOD_FRAMECLOCK_H

#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>

/*
 * The kernel tick rate is 100 Hz on iPod 6G. 50 fps maps to an exact
 * two-tick cadence; 42 fps requires uneven 2/3 tick pacing and produces
 * visible motion jitter.
 */
#define CRAZYPOD_TARGET_FPS 50

struct crazypod_frameclock {
    long next_tick;
    int tick_error;
};

void crazypod_frameclock_reset(struct crazypod_frameclock *clock, long now);
bool crazypod_frameclock_due(const struct crazypod_frameclock *clock, long now);
void crazypod_frameclock_schedule_next(struct crazypod_frameclock *clock,
                                       long now);

void crazypod_present_init(long now);
void crazypod_present_queue_rect(int x, int y, int width, int height);
void crazypod_present_queue_full(void);
/* Commit an already-rendered queued frame before a synchronous operation. */
void crazypod_present_now(void);
void crazypod_present_tick(void);

#endif

#endif
