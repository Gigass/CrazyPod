#ifndef CRAZYPOD_STATE_H
#define CRAZYPOD_STATE_H

#include <stdbool.h>

void crazypod_state_load(void);
void crazypod_state_mark_dirty(void);
bool crazypod_state_reduce_motion(void);
void crazypod_state_set_reduce_motion(bool enabled);
void crazypod_state_forget_resume(void);
unsigned long crazypod_state_take_resume_elapsed(void);
void crazypod_state_save(bool force);
void crazypod_state_tick(void);
int crazypod_state_wait_ticks(void);

#endif
