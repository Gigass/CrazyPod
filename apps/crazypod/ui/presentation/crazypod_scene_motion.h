#ifndef CRAZYPOD_SCENE_MOTION_H
#define CRAZYPOD_SCENE_MOTION_H

#include <stdint.h>

#define CRAZYPOD_SCENE_MOTION_PROGRESS_MAX 1024

enum crazypod_scene_motion_kind {
    CRAZYPOD_SCENE_MOTION_NONE = 0,
    CRAZYPOD_SCENE_MOTION_PUSH,
    CRAZYPOD_SCENE_MOTION_POP,
    CRAZYPOD_SCENE_MOTION_REPLACE,
};

struct crazypod_scene_motion_layout {
    int from_x;
    int from_y;
    int to_x;
    int to_y;
    uint8_t edge_shadow_opacity;
};

/*
 * Push, pop and replace are the most visible motion in the product, and they
 * ignored Reduce Motion entirely: the setting reached the menu preview, the
 * notifications and the lock screen, but not these.
 */
int crazypod_scene_motion_reduced_duration_ms(
    enum crazypod_scene_motion_kind kind);
int crazypod_scene_motion_duration_ms(
    enum crazypod_scene_motion_kind kind);
void crazypod_scene_motion_layout(
    enum crazypod_scene_motion_kind kind,
    int progress, int height,
    struct crazypod_scene_motion_layout *layout);

#endif
