#ifndef CRAZYPOD_FRAMECLOCK_H
#define CRAZYPOD_FRAMECLOCK_H

#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>

#define CRAZYPOD_TARGET_FPS 42

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
