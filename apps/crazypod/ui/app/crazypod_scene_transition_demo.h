#ifndef CRAZYPOD_SCENE_TRANSITION_DEMO_H
#define CRAZYPOD_SCENE_TRANSITION_DEMO_H

#include "../../crazypod_screen_recording.h"

void crazypod_scene_transition_demo_init(void);
void crazypod_scene_transition_demo_service(
    enum crazypod_screen_recording_event event, long now);

#endif
