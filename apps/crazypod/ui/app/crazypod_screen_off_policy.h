#ifndef CRAZYPOD_SCREEN_OFF_POLICY_H
#define CRAZYPOD_SCREEN_OFF_POLICY_H

#include <stdint.h>

#define CRAZYPOD_WAIT_BLOCK (-1)

int crazypod_screen_off_shorter_wait(int first, int second);
int crazypod_screen_off_alarm_wait(
    uint32_t now_epoch, uint32_t alarm_epoch,
    unsigned ticks_per_second);

#endif
