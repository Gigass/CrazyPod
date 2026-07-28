#include "config.h"

#ifdef IPOD_6G

#include "button.h"
#include "kernel.h"

#include "../../../crazypod_photos.h"
#include "../../../crazypod_wallpaper.h"
#include "crazypod_photo_controller.h"
#include "crazypod_photo_input.h"
#include "crazypod_photo_screen.h"
#include "crazypod_photos_feature.h"

#define PAN_STEP 24
#define TOUCH_MOVE_THRESHOLD 4
#define FAVORITE_HOLD_TICKS \
    ((HZ * 8 / 10) > 0 ? (HZ * 8 / 10) : 1)
#define FAVORITE_PROGRESS_DELAY_TICKS \
    ((HZ / 2) > 0 ? (HZ / 2) : 1)
#define FAVORITE_FEEDBACK_TICKS \
    ((HZ * 6 / 5) > 0 ? (HZ * 6 / 5) : 1)
#define FAVORITE_PROGRESS_WIDTH 126

static struct {
    struct crazypod_photos_runtime_host host;
    struct route_state *state;
} runtime;

static int selected_index(const struct route_state *state)
{
    if(state->route == PHOTOS_ROUTE_DETAIL)
        return state->group;
    if(state->route == PHOTOS_ROUTE_LIBRARY ||
       state->route == PHOTOS_ROUTE_FAVORITES)
        return crazypod_photos_feature_route_index(
            state, state->selected);
    return -1;
}

static void adjust_zoom(int direction, int steps)
{
    runtime.state->selected =
        crazypod_photo_controller_adjust_zoom(
            direction, steps) ? 1 : 0;
    runtime.host.render(false);
}

static void queue_pan(int delta_x, int delta_y)
{
    crazypod_photo_controller_queue_pan(delta_x, delta_y);
}

static void feedback_removed(void)
{
    crazypod_photo_screen_reset_transient();
    runtime.host.render(false);
}

static void set_home_wallpaper(void)
{
    if(runtime.state->route == PHOTOS_ROUTE_DETAIL &&
       crazypod_wallpaper_select(
           CRAZYPOD_WALLPAPER_HOME,
           crazypod_photo_path(runtime.state->group)))
        runtime.host.appearance_changed();
}

void crazypod_photos_runtime_configure(
    const struct crazypod_photos_runtime_host *host)
{
    if(host != NULL)
        runtime.host = *host;
}

bool crazypod_photos_runtime_handle_input(
    struct route_state *state,
    const struct crazypod_input_event *event, long now)
{
    const struct crazypod_photo_input_context context = {
        .detail = state->route == PHOTOS_ROUTE_DETAIL,
        .selected_photo_available = selected_index(state) >= 0,
        .now = now,
        .pan_step = PAN_STEP,
    };
    const struct crazypod_photo_input_actions actions = {
        .note_direction =
            crazypod_photo_controller_note_direction,
        .adjust_zoom = adjust_zoom,
        .move_selection = runtime.host.move_selection,
        .queue_pan = queue_pan,
        .activate = runtime.host.activate,
        .select_feedback_removed = feedback_removed,
        .leave_detail = runtime.host.pop,
        .set_home_wallpaper = set_home_wallpaper,
    };

    runtime.state = state;
    return crazypod_photo_input_handle(
        event, &context, &actions);
}

static void service_favorite(
    struct route_state *state, long now)
{
    const struct crazypod_photo_controller_model *model;
    enum crazypod_photo_controller_event event;
    int photo_index = selected_index(state);

    event = crazypod_photo_controller_tick(
        now, photo_index,
        FAVORITE_PROGRESS_DELAY_TICKS,
        FAVORITE_HOLD_TICKS,
        FAVORITE_FEEDBACK_TICKS);
    model = crazypod_photo_controller_model();
    if(event == CRAZYPOD_PHOTO_EVENT_HOLD_PROGRESS) {
        if(model->select_hold_percent >= 0 &&
           !crazypod_photo_screen_favorite_progress_ready())
            runtime.host.render(false);
        else if(crazypod_photo_screen_favorite_progress_ready() &&
                model->select_hold_percent >= 0)
            crazypod_photo_screen_update_favorite_progress(
                FAVORITE_PROGRESS_WIDTH *
                model->select_hold_percent / 100);
        return;
    }
    if(event == CRAZYPOD_PHOTO_EVENT_FAVORITE_CHANGED) {
        if(state->route == PHOTOS_ROUTE_FAVORITES &&
           photo_index >= 0 &&
           !crazypod_photo_is_favorite(photo_index)) {
            int count = crazypod_photo_favorite_count();

            if(state->selected >= count)
                state->selected =
                    count > 0 ? count - 1 : 0;
        }
        runtime.host.render(false);
        return;
    }
    if(event == CRAZYPOD_PHOTO_EVENT_FEEDBACK_EXPIRED &&
       (state->route == PHOTOS_ROUTE_LIBRARY ||
        state->route == PHOTOS_ROUTE_FAVORITES ||
        state->route == PHOTOS_ROUTE_DETAIL))
        runtime.host.render(false);
}

void crazypod_photos_runtime_service(
    struct route_state *state, long now)
{
    int position;

    runtime.state = state;
    service_favorite(state, now);
    if(crazypod_photo_controller_take_pan_render() &&
       state->route == PHOTOS_ROUTE_DETAIL &&
       crazypod_photo_controller_model()->zoom_percent > 100)
        runtime.host.render(false);
    if(state->route != PHOTOS_ROUTE_DETAIL ||
       crazypod_photo_controller_model()->zoom_percent <= 100) {
        crazypod_photo_controller_cancel_wheel();
        return;
    }
#ifdef HAVE_WHEEL_POSITION
    position = wheel_status();
#else
    position = -1;
#endif
    crazypod_photo_controller_wheel_sample(
        position, now, HZ / 3,
        TOUCH_MOVE_THRESHOLD, PAN_STEP);
}

#endif
