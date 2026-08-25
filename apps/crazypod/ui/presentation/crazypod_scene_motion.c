#include <stddef.h>
#include <stdint.h>

#include "crazypod_scene_motion.h"

#define PUSH_DURATION_MS 340
#define POP_DURATION_MS 320
#define REPLACE_DURATION_MS 280
#define EDGE_SHADOW_OPACITY 154

static int clamp_progress(int progress)
{
    if(progress < 0)
        return 0;
    if(progress > CRAZYPOD_SCENE_MOTION_PROGRESS_MAX)
        return CRAZYPOD_SCENE_MOTION_PROGRESS_MAX;
    return progress;
}

/* A fixed-point cubic ease-out gives the decisive start and long, quiet
 * settlement used by iOS navigation without pulling floating point into the
 * firmware UI loop. */
static int ease_out_cubic(int progress)
{
    int64_t inverse =
        CRAZYPOD_SCENE_MOTION_PROGRESS_MAX - clamp_progress(progress);
    int64_t cube = inverse * inverse * inverse;
    int64_t scale =
        (int64_t)CRAZYPOD_SCENE_MOTION_PROGRESS_MAX *
        CRAZYPOD_SCENE_MOTION_PROGRESS_MAX;

    return CRAZYPOD_SCENE_MOTION_PROGRESS_MAX - (int)(cube / scale);
}

static uint8_t opacity_between(int from, int to, int progress)
{
    int value = from +
        (to - from) * progress /
        CRAZYPOD_SCENE_MOTION_PROGRESS_MAX;

    if(value < 0)
        value = 0;
    if(value > 255)
        value = 255;
    return (uint8_t)value;
}

int crazypod_scene_motion_duration_ms(
    enum crazypod_scene_motion_kind kind)
{
    switch(kind) {
    case CRAZYPOD_SCENE_MOTION_PUSH:
        return PUSH_DURATION_MS;
    case CRAZYPOD_SCENE_MOTION_POP:
        return POP_DURATION_MS;
    case CRAZYPOD_SCENE_MOTION_REPLACE:
        return REPLACE_DURATION_MS;
    default:
        return 0;
    }
}

void crazypod_scene_motion_layout(
    enum crazypod_scene_motion_kind kind,
    int progress, int height,
    struct crazypod_scene_motion_layout *layout)
{
    int eased;

    if(layout == NULL)
        return;
    progress = clamp_progress(progress);
    eased = ease_out_cubic(progress);
    *layout = (struct crazypod_scene_motion_layout) {
        .from_x = 0,
        .from_y = 0,
        .to_x = 0,
        .to_y = 0,
        .edge_shadow_opacity = 0,
    };

    switch(kind) {
    case CRAZYPOD_SCENE_MOTION_PUSH:
        layout->to_y = height *
            (CRAZYPOD_SCENE_MOTION_PROGRESS_MAX - eased) /
            CRAZYPOD_SCENE_MOTION_PROGRESS_MAX;
        layout->edge_shadow_opacity = opacity_between(
            EDGE_SHADOW_OPACITY, 0, eased);
        break;
    case CRAZYPOD_SCENE_MOTION_POP:
        layout->from_y = height * eased /
            CRAZYPOD_SCENE_MOTION_PROGRESS_MAX;
        layout->edge_shadow_opacity = opacity_between(
            EDGE_SHADOW_OPACITY, 0, eased);
        break;
    case CRAZYPOD_SCENE_MOTION_REPLACE:
        layout->to_y = height *
            (CRAZYPOD_SCENE_MOTION_PROGRESS_MAX - eased) /
            CRAZYPOD_SCENE_MOTION_PROGRESS_MAX;
        layout->edge_shadow_opacity = opacity_between(
            EDGE_SHADOW_OPACITY, 0, eased);
        break;
    default:
        break;
    }
}
