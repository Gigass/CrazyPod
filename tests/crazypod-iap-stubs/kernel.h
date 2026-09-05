#ifndef TEST_CRAZYPOD_IAP_KERNEL_H
#define TEST_CRAZYPOD_IAP_KERNEL_H

#include "config.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define TIME_AFTER(a, b) ((long)((b) - (a)) < 0)
#define TIME_BEFORE(a, b) ((long)((a) - (b)) < 0)

extern long current_tick;
void sleep(int ticks);

static inline void yield(void)
{
}

#endif
