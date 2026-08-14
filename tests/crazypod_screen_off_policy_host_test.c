#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include "app/crazypod_screen_off_policy.h"

int main(void)
{
    assert(crazypod_screen_off_shorter_wait(
               CRAZYPOD_WAIT_BLOCK, CRAZYPOD_WAIT_BLOCK) ==
           CRAZYPOD_WAIT_BLOCK);
    assert(crazypod_screen_off_shorter_wait(
               CRAZYPOD_WAIT_BLOCK, 30) == 30);
    assert(crazypod_screen_off_shorter_wait(40, 30) == 30);

    assert(crazypod_screen_off_alarm_wait(100, 0, 100) ==
           CRAZYPOD_WAIT_BLOCK);
    assert(crazypod_screen_off_alarm_wait(100, 100, 100) == 1);
    assert(crazypod_screen_off_alarm_wait(100, 101, 100) == 100);
    assert(crazypod_screen_off_alarm_wait(0, 101, 100) == 100);
    assert(crazypod_screen_off_alarm_wait(100, 200, 0) == 1);
    assert(crazypod_screen_off_alarm_wait(
               1, UINT32_MAX, 100) == INT_MAX);
    return 0;
}
