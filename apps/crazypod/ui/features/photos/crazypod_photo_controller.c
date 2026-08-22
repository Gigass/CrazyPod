#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_photos.h"
#include "crazypod_photo_controller.h"

static struct crazypod_photo_controller_model model;
static bool pan_render_pending;

void crazypod_photo_controller_reset(void)
{
    model.pan_x = 0;
    model.pan_y = 0;
    model.zoom_percent = 100;
    model.select_long_handled = true;
    model.select_holding = false;
    model.select_hold_start = 0;
    model.select_hold_percent = -1;
    model.favorite_feedback_added = false;
    model.favorite_feedback_error = false;
    model.wheel_touch_active = false;
    model.wheel_touch_start = -1;
    model.wheel_touch_max_delta = 0;
    model.direction_input_tick = 0;
    pan_render_pending = false;
}

void crazypod_photo_controller_open_detail(int zoom_percent)
{
    model.pan_x = 0;
    model.pan_y = 0;
    model.zoom_percent = zoom_percent < 100 ? 100 : zoom_percent;
    pan_render_pending = false;
}

const struct crazypod_photo_controller_model *
crazypod_photo_controller_model(void)
{
    return &model;
}

const lv_image_dsc_t *crazypod_photo_controller_render_viewport(
    int photo_index)
{
    return crazypod_photo_render_viewport(
        photo_index, model.zoom_percent, &model.pan_x, &model.pan_y);
}

bool crazypod_photo_controller_adjust_zoom(int direction, int steps)
{
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
    if(model.zoom_percent == 100) {
        model.pan_x = 0;
        model.pan_y = 0;
    }
    return model.zoom_percent > 100;
}

void crazypod_photo_controller_queue_pan(int delta_x, int delta_y)
{
    model.pan_x += delta_x;
    model.pan_y += delta_y;
    pan_render_pending = true;
}

bool crazypod_photo_controller_take_pan_render(void)
{
    bool pending = pan_render_pending;

    pan_render_pending = false;
    return pending;
}

void crazypod_photo_controller_note_direction(long now)
{
    model.direction_input_tick = now;
}

void crazypod_photo_controller_begin_select(bool valid, long now)
{
    model.select_long_handled = false;
    model.select_holding = valid;
    model.select_hold_start = now;
    model.select_hold_percent = -1;
}

void crazypod_photo_controller_release_select(
    bool *activate, bool *remove_progress)
{
    if(activate != NULL)
        *activate = !model.select_long_handled;
    if(remove_progress != NULL)
        *remove_progress =
            model.select_holding && model.select_hold_percent >= 0;
    model.select_holding = false;
    model.select_hold_percent = -1;
    model.select_long_handled = false;
}

enum crazypod_photo_controller_event crazypod_photo_controller_tick(
    long now, int photo_index, long progress_delay,
    long hold_duration)
{
    if(model.select_holding && !model.select_long_handled) {
        long elapsed = now - model.select_hold_start;
        int percent;

        if(elapsed < 0)
            elapsed = 0;
        if(elapsed < progress_delay)
            percent = -1;
        else
            percent = (int)(
                (elapsed - progress_delay) * 100 /
                (hold_duration - progress_delay));
        if(percent > 100)
            percent = 100;
        if(percent != model.select_hold_percent) {
            model.select_hold_percent = percent;
            if(percent < 100)
                return CRAZYPOD_PHOTO_EVENT_HOLD_PROGRESS;
        }
        if(percent >= 100) {
            bool was_favorite = crazypod_photo_is_favorite(photo_index);
            bool saved = crazypod_photo_toggle_favorite(photo_index);

            model.select_holding = false;
            model.select_long_handled = true;
            model.favorite_feedback_error = !saved;
            model.favorite_feedback_added =
                saved ? crazypod_photo_is_favorite(photo_index)
                      : was_favorite;
            return CRAZYPOD_PHOTO_EVENT_FAVORITE_CHANGED;
        }
    }
    return CRAZYPOD_PHOTO_EVENT_NONE;
}

void crazypod_photo_controller_wheel_sample(
    int position, long now, long recent_ticks,
    int move_threshold, int pan_step)
{
    if(model.zoom_percent <= 100) {
        model.wheel_touch_active = false;
        return;
    }
    if(position >= 0) {
        int delta;

        position %= 96;
        if(!model.wheel_touch_active) {
            model.wheel_touch_active = true;
            model.wheel_touch_start = position;
            model.wheel_touch_max_delta = 0;
            return;
        }
        delta = position - model.wheel_touch_start;
        if(delta < -48)
            delta += 96;
        else if(delta > 48)
            delta -= 96;
        if(delta < 0)
            delta = -delta;
        if(delta > model.wheel_touch_max_delta)
            model.wheel_touch_max_delta = delta;
        return;
    }
    if(model.wheel_touch_active) {
        bool recent =
            model.direction_input_tick != 0 &&
            now < model.direction_input_tick + recent_ticks;

        model.wheel_touch_active = false;
        if(model.wheel_touch_start >= 0 &&
           model.wheel_touch_max_delta < move_threshold && !recent) {
            int quadrant = ((model.wheel_touch_start + 12) / 24) & 3;

            if(quadrant == 0)
                crazypod_photo_controller_queue_pan(0, pan_step);
            else if(quadrant == 1)
                crazypod_photo_controller_queue_pan(pan_step, 0);
            else if(quadrant == 2)
                crazypod_photo_controller_queue_pan(0, -pan_step);
            else
                crazypod_photo_controller_queue_pan(-pan_step, 0);
        }
        model.wheel_touch_start = -1;
        model.wheel_touch_max_delta = 0;
    }
}

void crazypod_photo_controller_cancel_wheel(void)
{
    model.wheel_touch_active = false;
    model.wheel_touch_start = -1;
    model.wheel_touch_max_delta = 0;
}

#endif
