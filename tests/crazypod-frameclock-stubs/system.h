#ifndef CRAZYPOD_FRAMECLOCK_TEST_SYSTEM_H
#define CRAZYPOD_FRAMECLOCK_TEST_SYSTEM_H

#include <stdint.h>

extern uint32_t test_usec_timer;

#define USEC_TIMER test_usec_timer
#define TIME_AFTER(a, b) ((long)(b) - (long)(a) < 0)
#define TIME_BEFORE(a, b) TIME_AFTER(b, a)

#endif
