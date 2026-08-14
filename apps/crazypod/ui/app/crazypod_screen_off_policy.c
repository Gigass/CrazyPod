#include <limits.h>

#include "crazypod_screen_off_policy.h"

int crazypod_screen_off_shorter_wait(int first, int second)
{
    if(first == CRAZYPOD_WAIT_BLOCK)
        return second;
    if(second == CRAZYPOD_WAIT_BLOCK)
        return first;
    return first < second ? first : second;
}

int crazypod_screen_off_alarm_wait(
    uint32_t now_epoch, uint32_t alarm_epoch,
    unsigned ticks_per_second)
{
    uint32_t seconds;

    if(alarm_epoch == 0)
        return CRAZYPOD_WAIT_BLOCK;
    if(now_epoch == 0) {
        if(ticks_per_second == 0)
            return 1;
        return ticks_per_second > (unsigned)INT_MAX
            ? INT_MAX : (int)ticks_per_second;
    }
    if(alarm_epoch <= now_epoch || ticks_per_second == 0)
        return 1;
    seconds = alarm_epoch - now_epoch;
    if(seconds > (uint32_t)INT_MAX / ticks_per_second)
        return INT_MAX;
    return (int)(seconds * ticks_per_second);
}
