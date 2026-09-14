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
 * Push, pop and replace are the most visible motion in the product. With
 * Reduce Motion on they do not run at all: the new scene is presented
 * directly, because a shortened slide still costs the full-screen snapshots
 * and per-frame composites that make it choppy on slow hardware.
 */
int crazypod_scene_motion_duration_ms(
    enum crazypod_scene_motion_kind kind);
void crazypod_scene_motion_layout(
    enum crazypod_scene_motion_kind kind,
    int progress, int height,
    struct crazypod_scene_motion_layout *layout);

#endif
