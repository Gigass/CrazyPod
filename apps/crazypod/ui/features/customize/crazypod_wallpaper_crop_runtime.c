#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "kernel.h"
#include "lvgl.h"

#include "../../../crazypod_frameclock.h"
#include "../../../crazypod_photos.h"
#include "../../../crazypod_wallpaper.h"
#include "../photos/crazypod_photos_feature.h"
#include "crazypod_customize_catalog.h"
#include "crazypod_customize_feature.h"
#include "crazypod_wallpaper_crop_controller.h"
#include "crazypod_wallpaper_crop_input.h"
#include "crazypod_wallpaper_crop_screen.h"

#define CROP_HOLD_TICKS ((HZ / 2) > 0 ? (HZ / 2) : 1)
#define CROP_SUCCESS_TICKS \
    ((HZ * 2 / 5) > 0 ? (HZ * 2 / 5) : 1)

static struct {
    struct crazypod_wallpaper_crop_runtime_host host;
    long now;
} runtime;

static bool active(void)
{
    return runtime.host.product_active() &&
        runtime.host.route_depth() > 0 &&
        runtime.host.current_route() ==
            DIY_ROUTE_WALLPAPER_CROP;
}

static void adjust_zoom(int direction, int steps)
{
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();
    const lv_image_dsc_t *source =
        crazypod_photo_view(model->photo_index);

    crazypod_wallpaper_crop_controller_adjust_zoom(
        source != NULL ? source->header.w : 0,
        source != NULL ? source->header.h : 0,
        source != NULL
            ? crazypod_wallpaper_crop_max_zoom(source) : 500,
        direction, steps);
}

static void move_crop(int direction_x, int direction_y)
{
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();
    const lv_image_dsc_t *source =
        crazypod_photo_view(model->photo_index);

    if(source != NULL)
        (void)crazypod_wallpaper_crop_controller_move(
            source->header.w, source->header.h,
            crazypod_wallpaper_crop_max_zoom(source),
            direction_x, direction_y);
}

static void apply_progress(int progress, void *user_data)
{
    char text[40];
    int fill_width;

    (void)user_data;
    if(progress < 0)
        progress = 0;
    if(progress > 100)
        progress = 100;
    crazypod_wallpaper_crop_controller_set_apply_progress(progress);
    if(!crazypod_wallpaper_crop_screen_progress_ready())
        return;
    fill_width = progress * 200 / 100;
    if(fill_width < 2)
        fill_width = 2;
    snprintf(
        text, sizeof(text),
        "Applying wallpaper  %d%%", progress);
    crazypod_wallpaper_crop_screen_update_progress(
        fill_width, text);
    lv_refr_now(NULL);
    crazypod_present_now();
}

static void apply_crop(void)
{
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();
    const lv_image_dsc_t *source =
        crazypod_photo_view(model->photo_index);
    const char *path =
        crazypod_photo_path(model->photo_index);
    enum crazypod_wallpaper_target target =
        crazypod_customize_background_target(model->target);
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    enum crazypod_wallpaper_apply_result result;

    if(source == NULL ||
       !crazypod_wallpaper_crop_controller_rect(
           source->header.w, source->header.h,
           crazypod_wallpaper_crop_max_zoom(source),
           &crop_x, &crop_y, &crop_width, &crop_height)) {
        crazypod_wallpaper_crop_controller_fail(
            CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE, true);
        return;
    }
    crazypod_wallpaper_crop_controller_begin_apply();
    runtime.host.render(false);
    lv_refr_now(NULL);
    crazypod_present_now();
    result = crazypod_wallpaper_apply_crop(
        target, path, source, crop_x, crop_y,
        crop_width, crop_height, apply_progress, NULL);
    if(result != CRAZYPOD_WALLPAPER_APPLY_OK) {
        crazypod_wallpaper_crop_controller_fail(result, false);
        runtime.host.render(false);
        return;
    }
    runtime.host.appearance_changed();
    crazypod_wallpaper_crop_controller_finish(
        runtime.now, CROP_SUCCESS_TICKS);
    runtime.host.render(false);
}

void crazypod_wallpaper_crop_runtime_configure(
    const struct crazypod_wallpaper_crop_runtime_host *host)
{
    if(host != NULL)
        runtime.host = *host;
}

bool crazypod_wallpaper_crop_runtime_handle_input(
    const struct crazypod_input_event *event, long now)
{
    const struct crazypod_wallpaper_crop_input_actions actions = {
        .note_direction =
            crazypod_photos_feature_note_direction,
        .adjust_zoom = adjust_zoom,
        .move = move_crop,
        .apply = apply_crop,
        .cancel = runtime.host.pop,
    };

    runtime.now = now;
    (void)crazypod_wallpaper_crop_input_handle(
        event, now, &actions);
    return true;
}

static void update_loading_progress(void)
{
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();
    int progress;
    int fill_width;
    char text[40];

    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLYING ||
       model->phase == CRAZYPOD_WALLPAPER_CROP_APPLIED ||
       crazypod_photo_view(model->photo_index) != NULL)
        return;
    progress = crazypod_photo_view_progress(model->photo_index);
    if(!crazypod_wallpaper_crop_controller_note_load_progress(
           progress) ||
       !crazypod_wallpaper_crop_screen_progress_ready())
        return;
    if(progress < 0) {
        snprintf(text, sizeof(text), "Could not load picture");
        fill_width = 200;
        crazypod_wallpaper_crop_screen_set_progress_error(true);
    }
    else {
        if(progress > 100)
            progress = 100;
        snprintf(
            text, sizeof(text),
            "Loading picture  %d%%", progress);
        fill_width = progress * 2;
        if(fill_width < 2)
            fill_width = 2;
        crazypod_wallpaper_crop_screen_set_progress_error(false);
    }
    crazypod_wallpaper_crop_screen_update_progress(
        fill_width, text);
}

void crazypod_wallpaper_crop_runtime_service(long now)
{
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();

    runtime.now = now;
    if(crazypod_wallpaper_crop_controller_take_render_request() &&
       active())
        runtime.host.render(false);
    if(!active()) {
        crazypod_wallpaper_crop_controller_clear_holds();
        return;
    }
    update_loading_progress();
    if(crazypod_wallpaper_crop_controller_feedback_expired(now)) {
        if(runtime.host.route_depth() >= 3)
            runtime.host.truncate_routes(
                runtime.host.route_depth() - 2);
        runtime.host.render(true);
        return;
    }
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLYING ||
       model->phase == CRAZYPOD_WALLPAPER_CROP_APPLIED)
        return;
    (void)crazypod_wallpaper_crop_controller_update_holds(
        now, CROP_HOLD_TICKS);
}

#endif
