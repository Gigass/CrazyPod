#ifndef CRAZYPOD_STATE_H
#define CRAZYPOD_STATE_H

#include <stdbool.h>

enum crazypod_headphone_popup_style {
    CRAZYPOD_HEADPHONE_POPUP_WIRED_EARBUDS = 0,
    CRAZYPOD_HEADPHONE_POPUP_OVER_EAR,
    CRAZYPOD_HEADPHONE_POPUP_AIRPODS,
    CRAZYPOD_HEADPHONE_POPUP_STYLE_COUNT,
};

void crazypod_state_load(void);
void crazypod_state_mark_dirty(void);
bool crazypod_state_reduce_motion(void);
void crazypod_state_set_reduce_motion(bool enabled);
enum crazypod_headphone_popup_style
crazypod_state_headphone_popup_style(void);
void crazypod_state_set_headphone_popup_style(
    enum crazypod_headphone_popup_style style);
void crazypod_state_forget_resume(void);
unsigned long crazypod_state_take_resume_elapsed(void);
void crazypod_state_save(bool force);
void crazypod_state_tick(void);
int crazypod_state_wait_ticks(void);

#endif
