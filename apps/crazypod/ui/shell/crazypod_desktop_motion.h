#ifndef CRAZYPOD_DESKTOP_MOTION_H
#define CRAZYPOD_DESKTOP_MOTION_H

#include <stdbool.h>

void crazypod_desktop_motion_initialize(long now, int selected);
void crazypod_desktop_motion_set_input_enabled(
    long now, bool enabled, bool restore_wheel_events,
    int item_count);
void crazypod_desktop_motion_select(
    long now, int selected, bool animated);
bool crazypod_desktop_motion_tick(long now, int item_count);
bool crazypod_desktop_motion_active(void);
int crazypod_desktop_motion_position_q8(void);
int crazypod_desktop_motion_center(int item_count);
int crazypod_desktop_motion_take_wheel_feedback(void);

#endif
