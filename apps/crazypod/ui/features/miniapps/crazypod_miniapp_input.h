#ifndef CRAZYPOD_UI_MINIAPP_INPUT_H
#define CRAZYPOD_UI_MINIAPP_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_input_event.h"

struct crazypod_miniapp_input_actions {
    void (*wake_display)(void);
    void (*keep_boosted)(int ticks);
    void (*close)(void);
};

bool crazypod_miniapp_input_handle(
    const struct crazypod_input_event *event, long now,
    long menu_hold_ticks, int boost_ticks,
    const struct crazypod_miniapp_input_actions *actions);
void crazypod_miniapp_input_service(bool active, long now);
void crazypod_miniapp_input_reset_state(void);
bool crazypod_miniapp_input_motion_active(void);
bool crazypod_miniapp_input_exit_prompt_visible(void);
bool crazypod_miniapp_input_exit_selected(void);

#endif
