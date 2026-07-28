#include "config.h"

#ifdef IPOD_6G

#include "crazypod_wallpaper_crop_controller.h"

static struct crazypod_wallpaper_crop_model model;
static bool render_requested;

static bool deadline_reached(long now, long deadline)
{
    return (long)(now - deadline) >= 0;
}

void crazypod_wallpaper_crop_controller_start(
    int photo_index, enum crazypod_appearance_field target)
{
    model.photo_index = photo_index;
    model.target = target;
    model.zoom_percent = 140;
    model.center_x = -1;
    model.center_y = -1;
    model.phase = CRAZYPOD_WALLPAPER_CROP_EDITING;
    model.error_loading = false;
    model.feedback_until = 0;
    model.menu_holding = false;
    model.menu_armed = false;
    model.menu_hold_start = 0;
    model.play_holding = false;
    model.play_armed = false;
    model.play_hold_start = 0;
    model.select_armed = false;
    model.load_progress_seen = -2;
    model.apply_progress = 0;
    model.apply_result = CRAZYPOD_WALLPAPER_APPLY_OK;
    render_requested = false;
}

const struct crazypod_wallpaper_crop_model *
crazypod_wallpaper_crop_controller_model(void)
{
    return &model;
}

bool crazypod_wallpaper_crop_controller_rect(
    int source_width, int source_height, int maximum_zoom,
    int *crop_x, int *crop_y, int *crop_width, int *crop_height)
{
    int maximum_width;
    int maximum_height;
    int width;
    int height;

    if(source_width <= 0 || source_height <= 0 ||
       crop_x == NULL || crop_y == NULL ||
       crop_width == NULL || crop_height == NULL)
        return false;
    if(source_width * 3 > source_height * 4) {
        maximum_height = source_height;
        maximum_width = source_height * 4 / 3;
    }
    else {
        maximum_width = source_width;
        maximum_height = source_width * 3 / 4;
    }
    if(model.zoom_percent < 100)
        model.zoom_percent = 100;
    if(maximum_zoom < 100)
        maximum_zoom = 100;
    if(maximum_zoom > 500)
        maximum_zoom = 500;
    if(model.zoom_percent > maximum_zoom)
        model.zoom_percent = maximum_zoom;
    width = maximum_width * 100 / model.zoom_percent;
    height = maximum_height * 100 / model.zoom_percent;
    if(width < 4)
        width = 4;
    if(height < 3)
        height = 3;
    width -= width % 4;
    height = width * 3 / 4;
    if(height > source_height) {
        height = source_height - source_height % 3;
        width = height * 4 / 3;
    }
    if(model.center_x < 0)
        model.center_x = source_width / 2;
    if(model.center_y < 0)
        model.center_y = source_height / 2;
    if(model.center_x < width / 2)
        model.center_x = width / 2;
    if(model.center_x > source_width - (width + 1) / 2)
        model.center_x = source_width - (width + 1) / 2;
    if(model.center_y < height / 2)
        model.center_y = height / 2;
    if(model.center_y > source_height - (height + 1) / 2)
        model.center_y = source_height - (height + 1) / 2;
    *crop_x = model.center_x - width / 2;
    *crop_y = model.center_y - height / 2;
    *crop_width = width;
    *crop_height = height;
    return true;
}

void crazypod_wallpaper_crop_controller_adjust_zoom(
    int source_width, int source_height, int maximum_zoom,
    int direction, int steps)
{
    int x;
    int y;
    int width;
    int height;

    while(steps-- > 0) {
        if(direction > 0)
            model.zoom_percent = (model.zoom_percent * 104 + 50) / 100;
        else
            model.zoom_percent = (model.zoom_percent * 96 + 50) / 100;
    }
    if(model.zoom_percent < 100)
        model.zoom_percent = 100;
    if(model.zoom_percent > 500)
        model.zoom_percent = 500;
    if(source_width > 0 && source_height > 0)
        crazypod_wallpaper_crop_controller_rect(
            source_width, source_height, maximum_zoom,
            &x, &y, &width, &height);
    model.phase = CRAZYPOD_WALLPAPER_CROP_EDITING;
    model.error_loading = false;
    render_requested = true;
}

bool crazypod_wallpaper_crop_controller_move(
    int source_width, int source_height, int maximum_zoom,
    int direction_x, int direction_y)
{
    int x;
    int y;
    int width;
    int height;
    int step_x;
    int step_y;

    if(!crazypod_wallpaper_crop_controller_rect(
           source_width, source_height, maximum_zoom,
           &x, &y, &width, &height))
        return false;
    step_x = width / 12;
    step_y = height / 12;
    if(step_x < 1)
        step_x = 1;
    if(step_y < 1)
        step_y = 1;
    model.center_x += direction_x * step_x;
    model.center_y += direction_y * step_y;
    crazypod_wallpaper_crop_controller_rect(
        source_width, source_height, maximum_zoom,
        &x, &y, &width, &height);
    model.phase = CRAZYPOD_WALLPAPER_CROP_EDITING;
    model.error_loading = false;
    render_requested = true;
    return true;
}

void crazypod_wallpaper_crop_controller_reset(void)
{
    model.zoom_percent = 140;
    model.center_x = -1;
    model.center_y = -1;
    model.phase = CRAZYPOD_WALLPAPER_CROP_EDITING;
    model.error_loading = false;
    render_requested = true;
}

void crazypod_wallpaper_crop_controller_begin_apply(void)
{
    model.phase = CRAZYPOD_WALLPAPER_CROP_APPLYING;
    model.error_loading = false;
    model.apply_progress = 5;
}

void crazypod_wallpaper_crop_controller_set_apply_progress(int progress)
{
    if(progress < 0)
        progress = 0;
    if(progress > 100)
        progress = 100;
    model.apply_progress = progress;
}

void crazypod_wallpaper_crop_controller_fail(
    enum crazypod_wallpaper_apply_result result, bool loading)
{
    model.phase = CRAZYPOD_WALLPAPER_CROP_ERROR;
    model.error_loading = loading;
    model.apply_result = result;
    render_requested = true;
}

void crazypod_wallpaper_crop_controller_finish(long now, long duration)
{
    model.apply_result = CRAZYPOD_WALLPAPER_APPLY_OK;
    model.phase = CRAZYPOD_WALLPAPER_CROP_APPLIED;
    model.feedback_until = now + duration;
}

bool crazypod_wallpaper_crop_controller_feedback_expired(long now)
{
    if(model.phase != CRAZYPOD_WALLPAPER_CROP_APPLIED ||
       model.feedback_until == 0 ||
       !deadline_reached(now, model.feedback_until))
        return false;
    model.feedback_until = 0;
    model.phase = CRAZYPOD_WALLPAPER_CROP_EDITING;
    return true;
}

void crazypod_wallpaper_crop_controller_press_menu(long now)
{
    model.menu_holding = true;
    model.menu_armed = false;
    model.menu_hold_start = now;
}

bool crazypod_wallpaper_crop_controller_release_menu(void)
{
    bool armed = model.menu_armed;

    model.menu_holding = false;
    model.menu_armed = false;
    return armed;
}

void crazypod_wallpaper_crop_controller_press_play(long now)
{
    model.play_holding = true;
    model.play_armed = false;
    model.play_hold_start = now;
}

bool crazypod_wallpaper_crop_controller_release_play(void)
{
    bool armed = model.play_armed;

    model.play_holding = false;
    model.play_armed = false;
    return armed;
}

void crazypod_wallpaper_crop_controller_press_select(void)
{
    model.select_armed = true;
}

bool crazypod_wallpaper_crop_controller_release_select(void)
{
    bool armed = model.select_armed;

    model.select_armed = false;
    return armed;
}

void crazypod_wallpaper_crop_controller_clear_holds(void)
{
    model.menu_holding = false;
    model.menu_armed = false;
    model.play_holding = false;
    model.play_armed = false;
    model.select_armed = false;
}

bool crazypod_wallpaper_crop_controller_update_holds(
    long now, long threshold)
{
    bool changed = false;

    if(model.menu_holding && !model.menu_armed &&
       deadline_reached(now, model.menu_hold_start + threshold)) {
        model.menu_armed = true;
        changed = true;
    }
    if(model.play_holding && !model.play_armed &&
       deadline_reached(now, model.play_hold_start + threshold)) {
        model.play_armed = true;
        changed = true;
    }
    if(changed)
        render_requested = true;
    return changed;
}

bool crazypod_wallpaper_crop_controller_note_load_progress(int progress)
{
    if(progress == model.load_progress_seen)
        return false;
    model.load_progress_seen = progress;
    return true;
}

void crazypod_wallpaper_crop_controller_request_render(void)
{
    render_requested = true;
}

bool crazypod_wallpaper_crop_controller_take_render_request(void)
{
    bool requested = render_requested;

    render_requested = false;
    return requested;
}

#endif
