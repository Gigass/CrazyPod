#ifndef CRAZYPOD_SCENE_TRANSITION_H
#define CRAZYPOD_SCENE_TRANSITION_H

#include <stdbool.h>

#include "lvgl.h"

#include "../presentation/crazypod_scene_motion.h"

bool crazypod_scene_transition_begin(
    enum crazypod_scene_motion_kind kind);
bool crazypod_scene_transition_commit(lv_obj_t *parent);
bool crazypod_scene_transition_active(void);
bool crazypod_scene_transition_owns_framebuffer(void);
void crazypod_scene_transition_service(void);
void crazypod_scene_transition_finish(void);
void crazypod_scene_transition_reset(void);

#endif
