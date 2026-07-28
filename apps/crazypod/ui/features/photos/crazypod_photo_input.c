#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_photo_controller.h"
#include "crazypod_photo_input.h"

static void note_direction(
    const struct crazypod_photo_input_context *context,
    const struct crazypod_photo_input_actions *actions)
{
    if(actions->note_direction != NULL)
        actions->note_direction(context->now);
}

bool crazypod_photo_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_photo_input_context *context,
    const struct crazypod_photo_input_actions *actions)
{
    const struct crazypod_photo_controller_model *model =
        crazypod_photo_controller_model();

    if(context->detail &&
       (event->base == BUTTON_SCROLL_FWD ||
        event->base == BUTTON_SCROLL_BACK)) {
        note_direction(context, actions);
        if(model->zoom_percent > 100)
            actions->adjust_zoom(
                event->base == BUTTON_SCROLL_FWD ? 1 : -1,
                crazypod_input_wheel_steps(event, 12));
        return true;
    }
    if(context->detail &&
       (event->base == BUTTON_LEFT || event->base == BUTTON_RIGHT)) {
        int direction = event->base == BUTTON_RIGHT ? 1 : -1;

        note_direction(context, actions);
        if(model->zoom_percent > 100) {
            if(!event->release)
                actions->queue_pan(
                    -direction * context->pan_step, 0);
        }
        else if(event->release && !event->repeated)
            actions->move_selection(direction);
        return true;
    }
    if(event->base == BUTTON_SELECT) {
        if(event->release) {
            bool activate;
            bool remove_progress;

            crazypod_photo_controller_release_select(
                &activate, &remove_progress);
            if(activate)
                actions->activate();
            else if(remove_progress)
                actions->select_feedback_removed();
        }
        else if(!event->repeated)
            crazypod_photo_controller_begin_select(
                context->selected_photo_available, context->now);
        return true;
    }
    if(context->detail && event->base == BUTTON_MENU) {
        note_direction(context, actions);
        if(model->zoom_percent > 100) {
            if(!event->release)
                actions->queue_pan(0, context->pan_step);
        }
        else if(event->release && !event->repeated)
            actions->leave_detail();
        return true;
    }
    if(context->detail && event->base == BUTTON_PLAY) {
        note_direction(context, actions);
        if(model->zoom_percent > 100) {
            if(!event->release)
                actions->queue_pan(0, -context->pan_step);
        }
        else if(event->release && !event->repeated)
            actions->set_home_wallpaper();
        return true;
    }
    return false;
}

#endif
