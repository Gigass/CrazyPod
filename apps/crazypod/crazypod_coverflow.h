#ifndef CRAZYPOD_COVERFLOW_H
#define CRAZYPOD_COVERFLOW_H

#include <stdbool.h>

void crazypod_coverflow_enter(int selected);
bool crazypod_coverflow_warm(int selected);
void crazypod_coverflow_leave(void);
bool crazypod_coverflow_active(void);
bool crazypod_coverflow_motion_active(void);
void crazypod_coverflow_set_input_suspended(bool suspended);
int crazypod_coverflow_step(int direction);
int crazypod_coverflow_center_album(void);
int crazypod_coverflow_take_wheel_feedback(void);
void crazypod_coverflow_invalidate(void);
void crazypod_coverflow_tick(void);

#endif
