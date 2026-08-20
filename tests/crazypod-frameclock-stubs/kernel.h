#ifndef CRAZYPOD_FRAMECLOCK_TEST_KERNEL_H
#define CRAZYPOD_FRAMECLOCK_TEST_KERNEL_H

#include "system.h"

extern long test_current_tick;

#define current_tick test_current_tick

#endif
