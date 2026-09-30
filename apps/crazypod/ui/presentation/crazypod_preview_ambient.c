#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_preview_ambient.h"

#define AMBIENT_PART_COUNT 20
#define AMBIENT_SESSION_MS 12000
#define AMBIENT_RAMP_MS 500
#define AMBIENT_TAIL_MS 2000
#define AMBIENT_SPIN_UP_MS 900
#define AMBIENT_DEFAULT_PERIOD_MS 2400

struct ambient_part {
    lv_obj_t *object;
    struct crazypod_preview_ambient_spec spec;
    int x;
    int y;
    int rotation;
    int scale;
    int opacity;
};

static struct {
    struct ambient_part parts[AMBIENT_PART_COUNT];
    int count;
    lv_obj_t *anchor;
    bool running;
} state;

static int clamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

static int lerp(int from, int to, int progress)
{
    return from + (to - from) * progress / 1024;
}

static int smooth_step(int progress)
{
    int clamped = clamp(progress, 0, 1024);
    int squared = clamped * clamped / 1024;

    return squared * (3072 - 2 * clamped) / 1024;
}

static int ease_out(int progress)
{
    int inverse = 1024 - clamp(progress, 0, 1024);

    return 1024 - inverse * inverse / 1024;
}

/* Sine in -32768..32767 for a repeating period and a degree offset. */
static int wave(int elapsed, int period, int phase)
{
    int degrees = (elapsed % period) * 360 / period + phase;

    degrees %= 360;
    if(degrees < 0)
        degrees += 360;
    return lv_trigo_sin((int16_t)degrees);
}

/* Session envelope in 0..1024: eased in at the start, eased out at the end. */
static int envelope(int elapsed)
{
    int in = elapsed >= AMBIENT_RAMP_MS
        ? 1024 : smooth_step(elapsed * 1024 / AMBIENT_RAMP_MS);
    int left = AMBIENT_SESSION_MS - elapsed;
    int out = left >= AMBIENT_TAIL_MS
        ? 1024 : smooth_step(left * 1024 / AMBIENT_TAIL_MS);

    return in < out ? in : out;
}

static int amplitude(int peak, int sine, int envelope_value)
{
    return (peak * sine / 32768) * envelope_value / 1024;
}

/*
 * Distance travelled by a motor that spins up linearly, cruises, then
 * brakes linearly. Both ends have zero speed so the turn count is exact and
 * the object lands on its rest angle without a snap.
 */
static int spin_distance(int elapsed)
{
    int cruise_end = AMBIENT_SESSION_MS - AMBIENT_TAIL_MS;

    if(elapsed < AMBIENT_SPIN_UP_MS)
        return elapsed * elapsed / (2 * AMBIENT_SPIN_UP_MS);
    if(elapsed < cruise_end)
        return AMBIENT_SPIN_UP_MS / 2 + elapsed - AMBIENT_SPIN_UP_MS;
    elapsed = clamp(elapsed, cruise_end, AMBIENT_SESSION_MS) - cruise_end;
    return AMBIENT_SPIN_UP_MS / 2 + cruise_end - AMBIENT_SPIN_UP_MS +
        elapsed - elapsed * elapsed / (2 * AMBIENT_TAIL_MS);
}

static int spin_angle(int turns, int elapsed)
{
    int total = spin_distance(AMBIENT_SESSION_MS);

    return turns * 3600 * spin_distance(elapsed) / total;
}

static void write_pose(
    lv_obj_t *object, int x, int y, int rotation, int scale, int opacity)
{
    if(lv_obj_get_style_x(object, 0) != x ||
       lv_obj_get_style_y(object, 0) != y)
        lv_obj_set_pos(object, x, y);
    if(lv_obj_get_style_transform_rotation(object, 0) != rotation)
        lv_obj_set_style_transform_rotation(object, rotation, 0);
    if(lv_obj_get_style_transform_scale_x(object, 0) != scale)
        lv_obj_set_style_transform_scale(object, scale, 0);
    if(lv_obj_get_style_opa(object, 0) != opacity)
        lv_obj_set_style_opa(object, (lv_opa_t)opacity, 0);
}

static void apply_part(
    const struct ambient_part *part, int elapsed, int envelope_value)
{
    const struct crazypod_preview_ambient_spec *spec = &part->spec;
    int x = part->x;
    int y = part->y;
    int rotation = part->rotation;
    int scale = part->scale;
    int opacity = part->opacity;
    int sine = wave(elapsed, spec->period_ms, spec->phase);

    if(spec->dx != 0)
        x += amplitude(spec->dx, sine, envelope_value);
    if(spec->dy != 0)
        y += amplitude(
            spec->dy,
            wave(elapsed, spec->period_ms,
                 spec->phase + spec->dy_phase),
            envelope_value);
    if(spec->rock != 0)
        rotation += amplitude(spec->rock, sine, envelope_value);
    if(spec->pulse != 0)
        scale += amplitude(spec->pulse, sine, envelope_value);
    if(spec->spin_turns != 0)
        rotation += spin_angle(spec->spin_turns, elapsed);
    if(spec->opa_hi > spec->opa_lo) {
        int level = spec->opa_lo +
            (spec->opa_hi - spec->opa_lo) *
            ((sine + 32768) >> 6) / 1024;

        opacity = lerp(part->opacity, level, envelope_value);
    }
    if(spec->sweep_ms > 0) {
        int cycle = (elapsed + spec->period_ms * spec->phase / 360) %
            spec->period_ms;
        bool live = elapsed < AMBIENT_SESSION_MS - AMBIENT_TAIL_MS &&
            cycle < spec->sweep_ms;

        x = live
            ? lerp(spec->sweep_from, spec->sweep_to,
                   smooth_step(cycle * 1024 / spec->sweep_ms))
            : spec->sweep_from;
    }
    if(spec->ripple_opa > 0) {
        int cycle = (elapsed + spec->period_ms * spec->phase / 360) %
            spec->period_ms;
        int progress = cycle * 1024 / spec->period_ms;

        scale = lerp(spec->ripple_from, spec->ripple_to,
                     ease_out(progress));
        opacity = spec->ripple_opa * (1024 - progress) / 1024 *
            envelope_value / 1024;
    }
    write_pose(
        part->object, x, y, rotation, clamp(scale, 1, 2048),
        clamp(opacity, 0, LV_OPA_COVER));
}

static void restore_parts(void)
{
    int index;

    for(index = 0; index < state.count; ++index) {
        const struct ambient_part *part = &state.parts[index];

        if(part->object != NULL && lv_obj_is_valid(part->object))
            write_pose(
                part->object, part->x, part->y, part->rotation,
                part->scale, part->opacity);
    }
}

static void ambient_anim(void *target, int32_t elapsed)
{
    int envelope_value;
    int index;

    if(target == NULL || target != state.anchor ||
       !lv_obj_is_valid(state.anchor))
        return;
    envelope_value = envelope(elapsed);
    for(index = 0; index < state.count; ++index) {
        const struct ambient_part *part = &state.parts[index];

        if(part->object != NULL && lv_obj_is_valid(part->object))
            apply_part(part, elapsed, envelope_value);
    }
}

static void ambient_completed(lv_anim_t *animation)
{
    if(lv_anim_get_user_data(animation) != state.anchor)
        return;
    state.running = false;
    restore_parts();
}

void crazypod_preview_ambient_reset(void)
{
    crazypod_preview_ambient_stop(false);
    memset(&state, 0, sizeof(state));
}

void crazypod_preview_ambient_add(
    lv_obj_t *object, const struct crazypod_preview_ambient_spec *spec)
{
    struct ambient_part *part;

    if(object == NULL || spec == NULL ||
       state.count >= AMBIENT_PART_COUNT)
        return;
    part = &state.parts[state.count++];
    part->object = object;
    part->spec = *spec;
    if(part->spec.period_ms <= 0)
        part->spec.period_ms = AMBIENT_DEFAULT_PERIOD_MS;
    /* Same reasoning as the motion engine: layout has not run yet, so read
     * the authored local styles instead of the computed coordinates. */
    part->x = lv_obj_get_style_x(object, 0);
    part->y = lv_obj_get_style_y(object, 0);
    part->rotation = lv_obj_get_style_transform_rotation(object, 0);
    part->scale = lv_obj_get_style_transform_scale_x(object, 0);
    part->opacity = lv_obj_get_style_opa(object, 0);
}

bool crazypod_preview_ambient_start(lv_obj_t *anchor)
{
    lv_anim_t animation;

    if(anchor == NULL || state.count == 0)
        return false;
    crazypod_preview_ambient_stop(true);
    state.anchor = anchor;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, anchor);
    lv_anim_set_exec_cb(&animation, ambient_anim);
    lv_anim_set_values(&animation, 0, AMBIENT_SESSION_MS);
    lv_anim_set_duration(&animation, AMBIENT_SESSION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_completed_cb(&animation, ambient_completed);
    lv_anim_set_user_data(&animation, anchor);
    lv_anim_set_early_apply(&animation, true);
    state.running = lv_anim_start(&animation) != NULL;
    return state.running;
}

void crazypod_preview_ambient_stop(bool restore)
{
    if(state.anchor != NULL && lv_obj_is_valid(state.anchor))
        (void)lv_anim_delete(state.anchor, ambient_anim);
    state.running = false;
    if(restore)
        restore_parts();
}

bool crazypod_preview_ambient_active(void)
{
    return state.running;
}

#endif
