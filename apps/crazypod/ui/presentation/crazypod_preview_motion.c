#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "kernel.h"
#include "lvgl.h"

#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_preview_primitives.h"
#include "crazypod_preview_motion.h"

#define CRAZYPOD_PREVIEW_PART_COUNT 20
#define CRAZYPOD_PREVIEW_ENTER_DURATION_MS 380
#define CRAZYPOD_PREVIEW_EXIT_DURATION_MS 180
#define CRAZYPOD_PREVIEW_REDUCED_DURATION_MS 80
#define CRAZYPOD_PREVIEW_PART_TIME_NUMERATOR 3
#define CRAZYPOD_PREVIEW_PART_TIME_DENOMINATOR 2

struct menu_preview_motion_part {
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

struct menu_preview_scene {
    lv_obj_t *content;
    struct menu_preview_motion_part parts[CRAZYPOD_PREVIEW_PART_COUNT];
    int part_count;
};

enum menu_preview_motion_phase {
    MENU_PREVIEW_MOTION_IDLE = 0,
    MENU_PREVIEW_MOTION_ENTERING,
    MENU_PREVIEW_MOTION_EXITING,
};

static struct crazypod_preview_motion_host motion_host;
static lv_obj_t *motion_parent;
static lv_obj_t *menu_preview_root;
static lv_obj_t *menu_preview_content;
static struct menu_preview_scene menu_preview_scene;
static bool menu_preview_media_deferred;
static bool menu_preview_media_refresh_pending;
static bool menu_preview_motion_reduced;
static long menu_preview_media_due;
static enum menu_preview_motion_phase menu_preview_motion_phase;
static enum crazypod_preview_motion_profile menu_preview_motion_profile;
static int menu_preview_navigation_direction = 1;

static void menu_preview_register_motion(
    lv_obj_t *object,
    int enter_dx, int enter_dy, int enter_scale,
    int enter_rotation, int enter_opacity,
    int enter_delay, int enter_duration,
    int exit_dx, int exit_dy, int exit_scale, int exit_rotation)
{
    struct menu_preview_motion_part *part;

    if(object == NULL ||
       menu_preview_scene.part_count >= CRAZYPOD_PREVIEW_PART_COUNT)
        return;
    part = &menu_preview_scene.parts[menu_preview_scene.part_count++];
    part->object = object;
    /*
     * Preview objects are registered immediately after creation, before
     * LVGL has run layout. lv_obj_get_x/y() still report zero at that
     * point, which made every animated part settle outside the clipped
     * preview pane. The authored positions already live in the local
     * style, so read those directly.
     */
    part->final_x = lv_obj_get_style_x(object, 0);
    part->final_y = lv_obj_get_style_y(object, 0);
    part->final_scale =
        lv_obj_get_style_transform_scale_x(object, 0);
    part->final_rotation =
        lv_obj_get_style_transform_rotation(object, 0);
    part->final_opacity = lv_obj_get_style_opa(object, 0);
    part->enter_dx = enter_dx;
    part->enter_dy = enter_dy;
    part->enter_scale =
        enter_scale > 0 ? enter_scale : part->final_scale;
    part->enter_rotation = enter_rotation;
    part->enter_opacity = enter_opacity;
    part->exit_dx = exit_dx;
    part->exit_dy = exit_dy;
    part->exit_scale =
        exit_scale > 0 ? exit_scale : part->final_scale;
    part->exit_rotation = exit_rotation;
    part->enter_delay = enter_delay;
    part->enter_duration = enter_duration;
}

static void settle_menu_preview_scene(struct menu_preview_scene *scene)
{
    int index;

    if(scene->content != NULL && lv_obj_is_valid(scene->content)) {
        lv_anim_delete(scene->content, NULL);
        lv_obj_set_pos(
            scene->content,
            -CRAZYPOD_PREVIEW_PANE_X,
            -CRAZYPOD_PREVIEW_PANE_Y);
        lv_obj_set_style_opa(scene->content, LV_OPA_COVER, 0);
    }
    for(index = 0; index < scene->part_count; ++index) {
        struct menu_preview_motion_part *part = &scene->parts[index];
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

static int menu_preview_clamp_progress(int value)
{
    if(value < 0)
        return 0;
    if(value > 1024)
        return 1024;
    return value;
}

static int menu_preview_ease_out(int progress)
{
    int inverse = 1024 - menu_preview_clamp_progress(progress);

    return 1024 - inverse * inverse / 1024;
}

static int menu_preview_ease_in(int progress)
{
    int clamped = menu_preview_clamp_progress(progress);

    return clamped * clamped / 1024;
}

static int menu_preview_back_out(int progress)
{
    int shifted = menu_preview_clamp_progress(progress) - 1024;
    int squared = shifted * shifted / 1024;
    int cubed = squared * shifted / 1024;

    return 1024 + (2766 * cubed + 1742 * squared) / 1024;
}

static int menu_preview_smooth_step(int progress)
{
    int clamped = menu_preview_clamp_progress(progress);
    int squared = clamped * clamped / 1024;

    return squared * (3072 - 2 * clamped) / 1024;
}

static int menu_preview_arc(int progress)
{
    int clamped = menu_preview_clamp_progress(progress);

    return clamped * (1024 - clamped) / 256;
}

static int menu_preview_lerp(int from, int to, int progress)
{
    return from + (to - from) * progress / 1024;
}

static void menu_preview_profile_arc_offset(
    int index, int arc, bool exiting, int *x, int *y)
{
    int visual_count = menu_preview_scene.part_count - 1;
    bool is_caption = index == menu_preview_scene.part_count - 1;
    int spread = visual_count > 1
        ? index * 2 - (visual_count - 1) : 0;
    int direction = menu_preview_navigation_direction;

    *x = 0;
    *y = 0;
    if(is_caption) {
        *y = -arc * (exiting ? 1 : 2) / 1024;
        return;
    }

    switch(menu_preview_motion_profile) {
    case CRAZYPOD_PREVIEW_PROFILE_MUSIC:
        *x = arc * direction *
            ((index & 1) != 0 ? -3 : 3) / 1024;
        *y = -arc * (3 + index % 3) / 1024;
        break;
    case CRAZYPOD_PREVIEW_PROFILE_PHOTOS:
        *x = arc * spread * (exiting ? -2 : 2) / 1024;
        *y = -arc * (exiting ? 3 : 6) / 1024;
        break;
    case CRAZYPOD_PREVIEW_PROFILE_NOTES:
        *x = arc * direction *
            ((index & 1) != 0 ? -4 : 4) / 1024;
        *y = -arc * (exiting ? 2 : 5) / 1024;
        break;
    case CRAZYPOD_PREVIEW_PROFILE_BOOKS:
        *x = arc * spread / 1024;
        *y = -arc * (exiting ? 3 : 7) / 1024;
        break;
    default:
        *y = -arc * 2 / 1024;
        break;
    }
}

static int menu_preview_scaled_part_time(int duration)
{
    return duration * CRAZYPOD_PREVIEW_PART_TIME_NUMERATOR /
        CRAZYPOD_PREVIEW_PART_TIME_DENOMINATOR;
}

static int menu_preview_part_raw_progress(
    const struct menu_preview_motion_part *part, int elapsed)
{
    int duration = part->enter_duration > 0
        ? part->enter_duration : CRAZYPOD_PREVIEW_ENTER_DURATION_MS;
    int delay = menu_preview_scaled_part_time(part->enter_delay);

    duration = menu_preview_scaled_part_time(duration);
    return menu_preview_clamp_progress(
        (elapsed - delay) * 1024 / duration);
}

static void menu_preview_timeline_anim(void *target, int32_t elapsed)
{
    lv_obj_t *content = target;
    int index;

    if(content == NULL || content != menu_preview_content ||
       !lv_obj_is_valid(content))
        return;

    if(menu_preview_motion_phase == MENU_PREVIEW_MOTION_ENTERING) {
        int content_raw_progress = menu_preview_clamp_progress(
            elapsed * 1024 /
            (menu_preview_motion_reduced
                ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS
                : CRAZYPOD_PREVIEW_ENTER_DURATION_MS));
        int content_position_progress =
            menu_preview_motion_reduced
                ? menu_preview_ease_out(content_raw_progress)
                : menu_preview_back_out(content_raw_progress);
        int content_opacity_progress =
            menu_preview_smooth_step(content_raw_progress);

        lv_obj_set_pos(
            content,
            menu_preview_lerp(
                -CRAZYPOD_PREVIEW_PANE_X + 8,
                -CRAZYPOD_PREVIEW_PANE_X,
                content_position_progress),
            menu_preview_lerp(
                -CRAZYPOD_PREVIEW_PANE_Y +
                    menu_preview_navigation_direction * 5,
                -CRAZYPOD_PREVIEW_PANE_Y,
                menu_preview_ease_out(content_raw_progress)));
        lv_obj_set_style_opa(
            content,
            (lv_opa_t)menu_preview_lerp(
                0, LV_OPA_COVER, content_opacity_progress),
            0);
        if(menu_preview_motion_reduced)
            return;

        for(index = 0; index < menu_preview_scene.part_count; ++index) {
            struct menu_preview_motion_part *part =
                &menu_preview_scene.parts[index];
            lv_obj_t *object = part->object;
            int raw_progress;
            int position_progress;
            int opacity_progress;
            int arc_x;
            int arc_y;

            if(object == NULL || !lv_obj_is_valid(object))
                continue;
            raw_progress =
                menu_preview_part_raw_progress(part, elapsed);
            position_progress =
                menu_preview_back_out(raw_progress);
            opacity_progress =
                menu_preview_smooth_step(
                    menu_preview_clamp_progress(
                        (raw_progress - 64) * 1024 / 960));
            menu_preview_profile_arc_offset(
                index, menu_preview_arc(raw_progress),
                false, &arc_x, &arc_y);
            lv_obj_set_pos(
                object,
                menu_preview_lerp(
                    part->final_x + part->enter_dx,
                    part->final_x, position_progress) + arc_x,
                menu_preview_lerp(
                    part->final_y + part->enter_dy,
                    part->final_y, position_progress) + arc_y);
            lv_obj_set_style_opa(
                object,
                (lv_opa_t)menu_preview_lerp(
                    part->enter_opacity,
                    part->final_opacity, opacity_progress),
                0);
        }
    }
    else if(menu_preview_motion_phase == MENU_PREVIEW_MOTION_EXITING) {
        int duration = menu_preview_motion_reduced
            ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS
            : CRAZYPOD_PREVIEW_EXIT_DURATION_MS;
        int progress = menu_preview_ease_in(
            menu_preview_clamp_progress(elapsed * 1024 / duration));
        int raw_progress = menu_preview_clamp_progress(
            elapsed * 1024 / duration);

        lv_obj_set_pos(
            content,
            menu_preview_lerp(
                -CRAZYPOD_PREVIEW_PANE_X,
                -CRAZYPOD_PREVIEW_PANE_X - 6,
                progress),
            menu_preview_lerp(
                -CRAZYPOD_PREVIEW_PANE_Y,
                -CRAZYPOD_PREVIEW_PANE_Y -
                    menu_preview_navigation_direction * 4,
                progress));
        lv_obj_set_style_opa(
            content,
            (lv_opa_t)menu_preview_lerp(
                LV_OPA_COVER, 0, progress),
            0);
        if(menu_preview_motion_reduced)
            return;

        for(index = 0; index < menu_preview_scene.part_count; ++index) {
            struct menu_preview_motion_part *part =
                &menu_preview_scene.parts[index];
            lv_obj_t *object = part->object;
            int arc_x;
            int arc_y;

            if(object == NULL || !lv_obj_is_valid(object))
                continue;
            menu_preview_profile_arc_offset(
                index, menu_preview_arc(raw_progress),
                true, &arc_x, &arc_y);
            lv_obj_set_pos(
                object,
                menu_preview_lerp(
                    part->final_x,
                    part->final_x + part->exit_dx,
                    progress) + arc_x,
                menu_preview_lerp(
                    part->final_y,
                    part->final_y + part->exit_dy,
                    progress) + arc_y);
        }
    }
}

static void settle_menu_preview_motion(void)
{
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    menu_preview_media_deferred = false;
    menu_preview_media_refresh_pending = false;
    settle_menu_preview_scene(&menu_preview_scene);
}

static bool menu_preview_motion_active(void)
{
    return menu_preview_motion_phase != MENU_PREVIEW_MOTION_IDLE;
}

static void menu_preview_timeline_completed(lv_anim_t *animation)
{
    lv_obj_t *content = lv_anim_get_user_data(animation);
    enum menu_preview_motion_phase completed_phase =
        menu_preview_motion_phase;

    if(content == NULL || content != menu_preview_content)
        return;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    if(completed_phase == MENU_PREVIEW_MOTION_ENTERING) {
        settle_menu_preview_scene(&menu_preview_scene);
        if(menu_preview_media_deferred) {
            menu_preview_media_deferred = false;
            menu_preview_media_refresh_pending = true;
            menu_preview_media_due = motion_host.now() + 1;
        }
        return;
    }
    if(completed_phase != MENU_PREVIEW_MOTION_EXITING)
        return;

    if(lv_obj_is_valid(content))
        lv_obj_delete(content);
    menu_preview_content = NULL;
    memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
    if(motion_host.can_render())
        motion_host.render(true);
}

static bool start_menu_preview_timeline(int duration)
{
    lv_anim_t animation;

    if(menu_preview_content == NULL)
        return false;
    lv_anim_delete(menu_preview_content, menu_preview_timeline_anim);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, menu_preview_content);
    lv_anim_set_exec_cb(&animation, menu_preview_timeline_anim);
    lv_anim_set_values(&animation, 0, duration);
    lv_anim_set_duration(&animation, duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_completed_cb(
        &animation, menu_preview_timeline_completed);
    lv_anim_set_user_data(&animation, menu_preview_content);
    lv_anim_set_early_apply(&animation, true);
    return lv_anim_start(&animation) != NULL;
}

static void reset_menu_preview_root(void)
{
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
    if(menu_preview_root == NULL) {
        menu_preview_root = lv_obj_create(motion_parent);
        crazypod_ui_widget_make_plain(menu_preview_root);
        lv_obj_set_pos(menu_preview_root,
                       CRAZYPOD_PREVIEW_PANE_X,
                       CRAZYPOD_PREVIEW_PANE_Y);
        lv_obj_set_size(
            menu_preview_root,
            LCD_WIDTH - CRAZYPOD_PREVIEW_PANE_X,
            LCD_HEIGHT - CRAZYPOD_PREVIEW_PANE_Y);
        lv_obj_set_style_bg_opa(
            menu_preview_root, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(
            menu_preview_root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(
            menu_preview_root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    }
    else {
        settle_menu_preview_scene(&menu_preview_scene);
        menu_preview_content = NULL;
        memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
        lv_obj_clean(menu_preview_root);
    }

    menu_preview_content = lv_obj_create(menu_preview_root);
    crazypod_ui_widget_make_plain(menu_preview_content);
    lv_obj_set_pos(menu_preview_content,
                   -CRAZYPOD_PREVIEW_PANE_X,
                   -CRAZYPOD_PREVIEW_PANE_Y);
    lv_obj_set_size(menu_preview_content, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(
        menu_preview_content, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(
        menu_preview_content, LV_OBJ_FLAG_CLICKABLE);
    memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
    menu_preview_scene.content = menu_preview_content;
}

static void start_menu_preview_scene_entrance(void)
{
    bool reduced = motion_host.reduced_motion();
    int duration = reduced
        ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS : 0;
    int index;

    if(menu_preview_content == NULL)
        return;
    if(!reduced) {
        duration = CRAZYPOD_PREVIEW_ENTER_DURATION_MS;
        for(index = 0; index < menu_preview_scene.part_count; ++index) {
            struct menu_preview_motion_part *part =
                &menu_preview_scene.parts[index];
            int part_end = menu_preview_scaled_part_time(
                part->enter_delay +
                (part->enter_duration > 0
                    ? part->enter_duration
                    : CRAZYPOD_PREVIEW_ENTER_DURATION_MS));

            if(part_end > duration)
                duration = part_end;
        }
    }
    motion_host.boost((duration * HZ + 999) / 1000 + HZ / 10);
    menu_preview_motion_reduced = reduced;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_ENTERING;
    if(!start_menu_preview_timeline(duration)) {
        menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
        settle_menu_preview_scene(&menu_preview_scene);
    }
}

static void start_menu_preview_scene_exit(void)
{
    bool reduced = motion_host.reduced_motion();
    int duration = reduced
        ? CRAZYPOD_PREVIEW_REDUCED_DURATION_MS
        : CRAZYPOD_PREVIEW_EXIT_DURATION_MS;
    if(menu_preview_content == NULL)
        return;
    motion_host.boost((duration * HZ + 999) / 1000 + HZ / 10);
    menu_preview_motion_reduced = reduced;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_EXITING;
    if(!start_menu_preview_timeline(duration)) {
        menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
        motion_host.render(true);
    }
}


void crazypod_preview_motion_configure(
    const struct crazypod_preview_motion_host *host)
{
    memset(&motion_host, 0, sizeof(motion_host));
    if(host != NULL)
        motion_host = *host;
}

void crazypod_preview_motion_register(
    lv_obj_t *object,
    int enter_dx, int enter_dy, int enter_scale,
    int enter_rotation, int enter_opacity,
    int enter_delay, int enter_duration,
    int exit_dx, int exit_dy, int exit_scale, int exit_rotation)
{
    menu_preview_register_motion(
        object, enter_dx, enter_dy, enter_scale,
        enter_rotation, enter_opacity,
        enter_delay, enter_duration,
        exit_dx, exit_dy, exit_scale, exit_rotation);
}

void crazypod_preview_motion_set_profile(
    enum crazypod_preview_motion_profile profile)
{
    menu_preview_motion_profile = profile;
}

void crazypod_preview_motion_set_direction(int direction)
{
    menu_preview_navigation_direction = direction < 0 ? -1 : 1;
}

void crazypod_preview_motion_reset_root(lv_obj_t *parent)
{
    motion_parent = parent;
    reset_menu_preview_root();
}

lv_obj_t *crazypod_preview_motion_parent(lv_obj_t *fallback)
{
    return menu_preview_content != NULL
        ? menu_preview_content : fallback;
}

void crazypod_preview_motion_start_entrance(void)
{
    start_menu_preview_scene_entrance();
}

void crazypod_preview_motion_start_exit(void)
{
    start_menu_preview_scene_exit();
}

bool crazypod_preview_motion_active(void)
{
    return menu_preview_motion_active();
}

bool crazypod_preview_motion_reduced(void)
{
    return motion_host.reduced_motion != NULL &&
        motion_host.reduced_motion();
}

void crazypod_preview_motion_settle(void)
{
    settle_menu_preview_motion();
}

bool *crazypod_preview_motion_media_deferred_flag(void)
{
    return &menu_preview_media_deferred;
}

bool crazypod_preview_motion_media_refresh_pending(void)
{
    return menu_preview_media_refresh_pending;
}

long crazypod_preview_motion_media_due(void)
{
    return menu_preview_media_due;
}

void crazypod_preview_motion_clear_media_refresh(void)
{
    menu_preview_media_refresh_pending = false;
}

bool crazypod_preview_motion_has_content(void)
{
    return menu_preview_content != NULL;
}

void crazypod_preview_motion_forget(void)
{
    menu_preview_root = NULL;
    menu_preview_content = NULL;
    memset(&menu_preview_scene, 0, sizeof(menu_preview_scene));
    menu_preview_media_deferred = false;
    menu_preview_media_refresh_pending = false;
    menu_preview_motion_phase = MENU_PREVIEW_MOTION_IDLE;
}

#endif
