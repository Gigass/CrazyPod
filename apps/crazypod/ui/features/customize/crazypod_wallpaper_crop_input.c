#include "config.h"

#ifdef IPOD_6G

#include "button.h"

#include "crazypod_wallpaper_crop_controller.h"
#include "crazypod_wallpaper_crop_input.h"

static void note_direction(
    const struct crazypod_wallpaper_crop_input_actions *actions,
    long now)
{
    if(actions->note_direction != NULL)
        actions->note_direction(now);
}

bool crazypod_wallpaper_crop_input_handle(
    const struct crazypod_input_event *event, long now,
    const struct crazypod_wallpaper_crop_input_actions *actions)
{
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();

    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLYING ||
       model->phase == CRAZYPOD_WALLPAPER_CROP_APPLIED)
        return true;

    if(event->base == BUTTON_SCROLL_FWD ||
       event->base == BUTTON_SCROLL_BACK) {
        note_direction(actions, now);
        if(!event->release)
            actions->adjust_zoom(
                event->base == BUTTON_SCROLL_FWD ? 1 : -1,
                crazypod_input_wheel_steps(event, 12));
        return true;
    }
    if(event->base == BUTTON_LEFT || event->base == BUTTON_RIGHT) {
        note_direction(actions, now);
        if(!event->release)
            actions->move(
                event->base == BUTTON_RIGHT ? 1 : -1, 0);
        return true;
    }
    if(event->base == BUTTON_SELECT) {
        note_direction(actions, now);
        if(event->release) {
            if(crazypod_wallpaper_crop_controller_release_select())
                actions->apply();
        }
        else if(!event->repeated)
            crazypod_wallpaper_crop_controller_press_select();
        return true;
    }
    if(event->base == BUTTON_MENU) {
        note_direction(actions, now);
        if(event->release) {
            if(crazypod_wallpaper_crop_controller_release_menu())
                actions->cancel();
            else
                actions->move(0, -1);
        }
        else if(!event->repeated)
            crazypod_wallpaper_crop_controller_press_menu(now);
        return true;
    }
    if(event->base == BUTTON_PLAY) {
        note_direction(actions, now);
        if(event->release) {
            if(crazypod_wallpaper_crop_controller_release_play())
                crazypod_wallpaper_crop_controller_reset();
            else
                actions->move(0, 1);
        }
        else if(!event->repeated)
            crazypod_wallpaper_crop_controller_press_play(now);
        return true;
    }
    return true;
}

#endif
