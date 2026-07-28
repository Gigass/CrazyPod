#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_ui_widgets.h"
#include "crazypod_menu_preview_motion.h"

#define PREVIEW_PART_COUNT 20
#define PREVIEW_ENTER_DURATION_MS 380
#define PREVIEW_EXIT_DURATION_MS 180
#define PREVIEW_REDUCED_DURATION_MS 80
#define PREVIEW_PART_TIME_NUMERATOR 3
#define PREVIEW_PART_TIME_DENOMINATOR 2

enum motion_phase {
    MOTION_IDLE = 0,
    MOTION_ENTERING,
    MOTION_EXITING,
};

struct motion_part {
    lv_obj_t *object;
    int final_x;
    int final_y;
    int final_scale;
    int final_rotation;
    int final_opacity;
    int enter_dx;
    int enter_dy;
    int enter_scale;
    int enter_rotation;
    int enter_opacity;
    int exit_dx;
    int exit_dy;
    int exit_scale;
    int exit_rotation;
    int enter_delay;
    int enter_duration;
};

struct motion_state {
    lv_obj_t *root;
    lv_obj_t *content;
    struct motion_part parts[PREVIEW_PART_COUNT];
    int part_count;
    int panel_width;
    int status_height;
    int direction;
    bool reduced;
    enum motion_phase phase;
    enum crazypod_menu_preview_profile profile;
    crazypod_menu_preview_motion_callback callback;
    void *callback_context;
};

static struct motion_state state;

static int clamp_progress(int value)
{
    if(value < 0)
        return 0;
    if(value > 1024)
        return 1024;
    return value;
}

static int ease_out(int progress)
{
    int inverse = 1024 - clamp_progress(progress);

    return 1024 - inverse * inverse / 1024;
}

static int ease_in(int progress)
{
    int clamped = clamp_progress(progress);

    return clamped * clamped / 1024;
}

static int back_out(int progress)
{
    int shifted = clamp_progress(progress) - 1024;
    int squared = shifted * shifted / 1024;
    int cubed = squared * shifted / 1024;

    return 1024 + (2766 * cubed + 1742 * squared) / 1024;
}

static int smooth_step(int progress)
{
    int clamped = clamp_progress(progress);
    int squared = clamped * clamped / 1024;

    return squared * (3072 - 2 * clamped) / 1024;
}

static int arc(int progress)
{
    int clamped = clamp_progress(progress);

    return clamped * (1024 - clamped) / 256;
}

static int lerp(int from, int to, int progress)
{
    return from + (to - from) * progress / 1024;
}

static int scaled_part_time(int duration)
{
    return duration * PREVIEW_PART_TIME_NUMERATOR /
        PREVIEW_PART_TIME_DENOMINATOR;
}

static int part_raw_progress(const struct motion_part *part, int elapsed)
{
    int duration = part->enter_duration > 0
        ? part->enter_duration : PREVIEW_ENTER_DURATION_MS;
    int delay = scaled_part_time(part->enter_delay);

    duration = scaled_part_time(duration);
    return clamp_progress((elapsed - delay) * 1024 / duration);
}

static void profile_arc_offset(
    int index, int value, bool exiting, int *x, int *y)
{
    int visual_count = state.part_count - 1;
    bool is_caption = index == state.part_count - 1;
    int spread = visual_count > 1
        ? index * 2 - (visual_count - 1) : 0;

    *x = 0;
    *y = 0;
    if(is_caption) {
        *y = -value * (exiting ? 1 : 2) / 1024;
        return;
    }

    switch(state.profile) {
    case CRAZYPOD_MENU_PREVIEW_PROFILE_MUSIC:
        *x = value * state.direction *
            ((index & 1) != 0 ? -3 : 3) / 1024;
        *y = -value * (3 + index % 3) / 1024;
        break;
    case CRAZYPOD_MENU_PREVIEW_PROFILE_PHOTOS:
        *x = value * spread * (exiting ? -2 : 2) / 1024;
        *y = -value * (exiting ? 3 : 6) / 1024;
        break;
    case CRAZYPOD_MENU_PREVIEW_PROFILE_NOTES:
        *x = value * state.direction *
            ((index & 1) != 0 ? -4 : 4) / 1024;
        *y = -value * (exiting ? 2 : 5) / 1024;
        break;
    case CRAZYPOD_MENU_PREVIEW_PROFILE_BOOKS:
        *x = value * spread / 1024;
        *y = -value * (exiting ? 3 : 7) / 1024;
        break;
    default:
        *y = -value * 2 / 1024;
        break;
    }
}

static void settle_scene(void)
{
    int index;

    if(state.content != NULL && lv_obj_is_valid(state.content)) {
        lv_anim_delete(state.content, NULL);
        lv_obj_set_pos(
            state.content, -state.panel_width, -state.status_height);
        lv_obj_set_style_opa(state.content, LV_OPA_COVER, 0);
    }
    for(index = 0; index < state.part_count; ++index) {
        struct motion_part *part = &state.parts[index];
        if(part->object == NULL || !lv_obj_is_valid(part->object))
            continue;
        lv_anim_delete(part->object, NULL);
        lv_obj_set_pos(part->object, part->final_x, part->final_y);
        lv_obj_set_style_transform_scale(
            part->object, part->final_scale, 0);
        lv_obj_set_style_transform_rotation(
            part->object, part->final_rotation, 0);
        lv_obj_set_style_opa(
            part->object, (lv_opa_t)part->final_opacity, 0);
    }
}

static void timeline_anim(void *target, int32_t elapsed)
{
    lv_obj_t *content = target;
    int index;

    if(content == NULL || content != state.content ||
       !lv_obj_is_valid(content))
        return;

    if(state.phase == MOTION_ENTERING) {
        int raw = clamp_progress(
            elapsed * 1024 /
            (state.reduced
                ? PREVIEW_REDUCED_DURATION_MS
                : PREVIEW_ENTER_DURATION_MS));
        int position = state.reduced ? ease_out(raw) : back_out(raw);
        int opacity = smooth_step(raw);

        lv_obj_set_pos(
            content,
            lerp(-state.panel_width + 8, -state.panel_width, position),
            lerp(-state.status_height + state.direction * 5,
                 -state.status_height, ease_out(raw)));
        lv_obj_set_style_opa(
            content, (lv_opa_t)lerp(0, LV_OPA_COVER, opacity), 0);
        if(state.reduced)
            return;

        for(index = 0; index < state.part_count; ++index) {
            struct motion_part *part = &state.parts[index];
            lv_obj_t *object = part->object;
            int part_raw;
            int part_position;
            int part_opacity;
            int arc_x;
            int arc_y;

            if(object == NULL || !lv_obj_is_valid(object))
                continue;
            part_raw = part_raw_progress(part, elapsed);
            part_position = back_out(part_raw);
            part_opacity = smooth_step(
                clamp_progress((part_raw - 64) * 1024 / 960));
            profile_arc_offset(
                index, arc(part_raw), false, &arc_x, &arc_y);
            lv_obj_set_pos(
                object,
                lerp(part->final_x + part->enter_dx,
                     part->final_x, part_position) + arc_x,
                lerp(part->final_y + part->enter_dy,
                     part->final_y, part_position) + arc_y);
            lv_obj_set_style_opa(
                object,
                (lv_opa_t)lerp(
                    part->enter_opacity,
                    part->final_opacity, part_opacity),
                0);
        }
    }
    else if(state.phase == MOTION_EXITING) {
        int duration = state.reduced
            ? PREVIEW_REDUCED_DURATION_MS : PREVIEW_EXIT_DURATION_MS;
        int raw = clamp_progress(elapsed * 1024 / duration);
        int progress = ease_in(raw);

        lv_obj_set_pos(
            content,
            lerp(-state.panel_width, -state.panel_width - 6, progress),
            lerp(-state.status_height,
                 -state.status_height - state.direction * 4, progress));
        lv_obj_set_style_opa(
            content, (lv_opa_t)lerp(LV_OPA_COVER, 0, progress), 0);
        if(state.reduced)
            return;

        for(index = 0; index < state.part_count; ++index) {
            struct motion_part *part = &state.parts[index];
            int arc_x;
            int arc_y;

            if(part->object == NULL || !lv_obj_is_valid(part->object))
                continue;
            profile_arc_offset(index, arc(raw), true, &arc_x, &arc_y);
            lv_obj_set_pos(
                part->object,
                lerp(part->final_x,
                     part->final_x + part->exit_dx, progress) + arc_x,
                lerp(part->final_y,
                     part->final_y + part->exit_dy, progress) + arc_y);
        }
    }
}

static void emit(enum crazypod_menu_preview_motion_event event)
{
    if(state.callback != NULL)
        state.callback(event, state.callback_context);
}

static void timeline_completed(lv_anim_t *animation)
{
    lv_obj_t *content = lv_anim_get_user_data(animation);
    enum motion_phase completed = state.phase;

    if(content == NULL || content != state.content)
        return;
    state.phase = MOTION_IDLE;
    if(completed == MOTION_ENTERING) {
        settle_scene();
        emit(CRAZYPOD_MENU_PREVIEW_ENTERED);
        return;
    }
    if(completed != MOTION_EXITING)
        return;

    if(lv_obj_is_valid(content))
        lv_obj_delete(content);
    state.content = NULL;
    state.part_count = 0;
    emit(CRAZYPOD_MENU_PREVIEW_EXITED);
}

static bool start_timeline(int duration)
{
    lv_anim_t animation;

    if(state.content == NULL)
        return false;
    lv_anim_delete(state.content, timeline_anim);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, state.content);
    lv_anim_set_exec_cb(&animation, timeline_anim);
    lv_anim_set_values(&animation, 0, duration);
    lv_anim_set_duration(&animation, duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_completed_cb(&animation, timeline_completed);
    lv_anim_set_user_data(&animation, state.content);
    lv_anim_set_early_apply(&animation, true);
    return lv_anim_start(&animation) != NULL;
}

void crazypod_menu_preview_motion_init(
    crazypod_menu_preview_motion_callback callback, void *context)
{
    memset(&state, 0, sizeof(state));
    state.direction = 1;
    state.callback = callback;
    state.callback_context = context;
}

void crazypod_menu_preview_motion_invalidate(void)
{
    crazypod_menu_preview_motion_callback callback = state.callback;
    void *context = state.callback_context;

    memset(&state, 0, sizeof(state));
    state.direction = 1;
    state.callback = callback;
    state.callback_context = context;
}

void crazypod_menu_preview_motion_reset(
    lv_obj_t *parent, int panel_width, int status_height)
{
    state.phase = MOTION_IDLE;
    state.panel_width = panel_width;
    state.status_height = status_height;
    if(state.root == NULL) {
        state.root = lv_obj_create(parent);
        crazypod_ui_widget_make_plain(state.root);
        lv_obj_set_pos(state.root, panel_width, status_height);
        lv_obj_set_size(
            state.root, LCD_WIDTH - panel_width, LCD_HEIGHT - status_height);
        lv_obj_set_style_bg_opa(state.root, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(state.root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(state.root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    }
    else {
        settle_scene();
        state.content = NULL;
        state.part_count = 0;
        lv_obj_clean(state.root);
    }

    state.content = lv_obj_create(state.root);
    crazypod_ui_widget_make_plain(state.content);
    lv_obj_set_pos(state.content, -panel_width, -status_height);
    lv_obj_set_size(state.content, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(state.content, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(state.content, LV_OBJ_FLAG_CLICKABLE);
    state.part_count = 0;
}

lv_obj_t *crazypod_menu_preview_motion_content(void)
{
    return state.content;
}

bool crazypod_menu_preview_motion_has_content(void)
{
    return state.content != NULL;
}

bool crazypod_menu_preview_motion_active(void)
{
    return state.phase != MOTION_IDLE;
}

void crazypod_menu_preview_motion_settle(void)
{
    state.phase = MOTION_IDLE;
    settle_scene();
}

void crazypod_menu_preview_motion_set_profile(
    enum crazypod_menu_preview_profile profile)
{
    state.profile = profile;
}

void crazypod_menu_preview_motion_set_direction(int direction)
{
    state.direction = direction < 0 ? -1 : 1;
}

void crazypod_menu_preview_motion_register(
    lv_obj_t *object,
    int enter_dx, int enter_dy, int enter_scale,
    int enter_rotation, int enter_opacity,
    int enter_delay, int enter_duration,
    int exit_dx, int exit_dy, int exit_scale, int exit_rotation)
{
    struct motion_part *part;

    if(object == NULL || state.part_count >= PREVIEW_PART_COUNT)
        return;
    part = &state.parts[state.part_count++];
    part->object = object;
    part->final_x = lv_obj_get_style_x(object, 0);
    part->final_y = lv_obj_get_style_y(object, 0);
    part->final_scale = lv_obj_get_style_transform_scale_x(object, 0);
    part->final_rotation = lv_obj_get_style_transform_rotation(object, 0);
    part->final_opacity = lv_obj_get_style_opa(object, 0);
    part->enter_dx = enter_dx;
    part->enter_dy = enter_dy;
    part->enter_scale =
        enter_scale > 0 ? enter_scale : part->final_scale;
    part->enter_rotation = enter_rotation;
    part->enter_opacity = enter_opacity;
    part->exit_dx = exit_dx;
    part->exit_dy = exit_dy;
    part->exit_scale = exit_scale > 0 ? exit_scale : part->final_scale;
    part->exit_rotation = exit_rotation;
    part->enter_delay = enter_delay;
    part->enter_duration = enter_duration;
}

int crazypod_menu_preview_motion_start_entrance(bool reduced)
{
    int duration = reduced ? PREVIEW_REDUCED_DURATION_MS : 0;
    int index;

    if(state.content == NULL)
        return 0;
    if(!reduced) {
        duration = PREVIEW_ENTER_DURATION_MS;
        for(index = 0; index < state.part_count; ++index) {
            const struct motion_part *part = &state.parts[index];
            int end = scaled_part_time(
                part->enter_delay +
                (part->enter_duration > 0
                    ? part->enter_duration : PREVIEW_ENTER_DURATION_MS));
            if(end > duration)
                duration = end;
        }
    }
    state.reduced = reduced;
    state.phase = MOTION_ENTERING;
    if(!start_timeline(duration)) {
        state.phase = MOTION_IDLE;
        settle_scene();
        emit(CRAZYPOD_MENU_PREVIEW_ENTERED);
    }
    return duration;
}

int crazypod_menu_preview_motion_start_exit(bool reduced)
{
    int duration = reduced
        ? PREVIEW_REDUCED_DURATION_MS : PREVIEW_EXIT_DURATION_MS;

    if(state.content == NULL)
        return 0;
    state.reduced = reduced;
    state.phase = MOTION_EXITING;
    if(!start_timeline(duration)) {
        state.phase = MOTION_IDLE;
        emit(CRAZYPOD_MENU_PREVIEW_EXITED);
    }
    return duration;
}

#endif
