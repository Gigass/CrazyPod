#include "config.h"

#ifdef IPOD_6G

#include "audio.h"
#include "backlight.h"
#include "button.h"
#include "kernel.h"
#include "misc.h"
#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
#include "piezo.h"
#endif
#include "settings.h"
#include "sound.h"

#include "../../crazypod_apps.h"
#include "../../crazypod_playlist.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../navigation/crazypod_input_event.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../shell/crazypod_desktop.h"
#include "../shell/crazypod_home_input.h"
#include "../shell/crazypod_shell.h"
#include "crazypod_app_input.h"
#include "crazypod_app_launcher.h"
#include "crazypod_choice_coordinator.h"
#include "crazypod_menu_preview.h"
#include "crazypod_route_actions.h"

static struct crazypod_app_input_host host;

static long button_base(long button)
{
    return button & BUTTON_MAIN;
}

static int wheel_step(intptr_t data, int maximum)
{
    int step = 1;

#ifdef HAVE_WHEEL_ACCELERATION
    step = button_apply_acceleration((unsigned int)data);
#else
    (void)data;
#endif
    if(step < 1)
        step = 1;
    if(step > maximum)
        step = maximum;
    return step;
}

static void wheel_feedback(long button)
{
    long base;

    if(button == BUTTON_NONE ||
       (button & (SYS_EVENT | BUTTON_REL)) != 0)
        return;
    base = button_base(button);
    if(base != BUTTON_SCROLL_FWD &&
       base != BUTTON_SCROLL_BACK)
        return;
    if((button & BUTTON_REPEAT) != 0 &&
       !global_settings.keyclick_repeats)
        return;
#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
    if(global_settings.keyclick_hardware)
        piezo_button_beep(false, false);
#endif
    if(global_settings.keyclick)
        system_sound_play(SOUND_KEYCLICK);
}

static void home_next_track(void)
{
    if(crazypod_queue_count() > 0)
        audio_next();
}

static void home_open_selected_app(void)
{
    crazypod_app_launcher_open(
        crazypod_apps_visible_id(
            crazypod_desktop_selected()));
}

void crazypod_app_input_configure(
    const struct crazypod_app_input_host *new_host)
{
    if(new_host != NULL)
        host = *new_host;
}

void crazypod_app_input_handle(
    long button, intptr_t data, long now)
{
    long base;
    bool repeated;
    struct route_state *state;

    if(button == BUTTON_NONE)
        return;
    if(crazypod_system_event_handle(
           button, data, &host.system_events))
        return;
    if(host.power_prompt_visible()) {
        wheel_feedback(button);
        if(button & BUTTON_REL)
            return;
        repeated = (button & BUTTON_REPEAT) != 0;
        backlight_on();
        (void)host.handle_power_prompt(
            button_base(button), repeated, data);
        return;
    }
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(host.usb_prompt_visible()) {
        wheel_feedback(button);
        if(button & BUTTON_REL)
            return;
        repeated = (button & BUTTON_REPEAT) != 0;
        backlight_on();
        (void)host.handle_usb_prompt(
            button_base(button), repeated, data);
        return;
    }
#endif
    if(host.handle_power_hold(button) ||
       host.handle_lock(button, data))
        return;
    wheel_feedback(button);
    if(crazypod_shell_product_active() &&
       crazypod_ui_routes_depth() > 0) {
        const struct crazypod_input_event event =
            crazypod_input_event_make(button, data);

        if(crazypod_feature_input_dispatch(
               crazypod_ui_routes_current(), &event,
               CRAZYPOD_FEATURE_INPUT_RAW,
               host.feature_bindings))
            return;
    }
    if(button & BUTTON_REL)
        return;
    repeated = (button & BUTTON_REPEAT) != 0;
    base = button_base(button);
    backlight_on();
    if(crazypod_shell_product_active() &&
       crazypod_ui_routes_depth() > 0 &&
       crazypod_music_library_loading()) {
        if(base == BUTTON_MENU && !repeated)
            host.close_product();
        return;
    }
    if(crazypod_shell_product_active() &&
       crazypod_ui_routes_depth() > 0 &&
       base == BUTTON_SELECT && repeated &&
       host.handle_confirmation(
           crazypod_ui_routes_current()))
        return;
    if(!crazypod_shell_product_active()) {
        const struct crazypod_input_event event =
            crazypod_input_event_make(button, data);
        const struct crazypod_home_input_actions actions = {
            .move_selection =
                crazypod_desktop_move_selection,
            .next_track = home_next_track,
            .previous_track = host.previous_track,
            .open_selected_app = home_open_selected_app,
            .toggle_playback = host.toggle_playback,
        };

        crazypod_home_input_handle(&event, &actions);
        return;
    }
    if(crazypod_ui_routes_depth() <= 0) {
        if(base == BUTTON_MENU)
            host.close_product();
        return;
    }
    state = crazypod_ui_routes_current();
    {
        const struct crazypod_input_event event =
            crazypod_input_event_make(button, data);

        if(crazypod_feature_input_dispatch(
               state, &event,
               CRAZYPOD_FEATURE_INPUT_PRESSED,
               host.feature_bindings))
            return;
    }
    if(crazypod_choice_coordinator_visible()) {
        if(base == BUTTON_SCROLL_FWD)
            crazypod_choice_coordinator_move(
                wheel_step(data, 12));
        else if(base == BUTTON_SCROLL_BACK)
            crazypod_choice_coordinator_move(
                -wheel_step(data, 12));
        else if(base == BUTTON_RIGHT)
            crazypod_choice_coordinator_move(1);
        else if(base == BUTTON_LEFT)
            crazypod_choice_coordinator_move(-1);
        else if(base == BUTTON_SELECT && !repeated)
            crazypod_choice_coordinator_activate();
        else if(base == BUTTON_MENU && !repeated)
            crazypod_choice_coordinator_dismiss(true);
        return;
    }
    if(state->route == MUSIC_ROUTE_NOW_PLAYING &&
       crazypod_now_playing_overlay_visible() &&
       base == BUTTON_MENU && !repeated) {
        crazypod_now_playing_overlay_dismiss(true);
        return;
    }
#ifdef HAVE_WHEEL_POSITION
    if(state->route == MUSIC_ROUTE_ALBUM_FLOW &&
       (base == BUTTON_SCROLL_FWD ||
        base == BUTTON_SCROLL_BACK))
        return;
#endif
    if(base == BUTTON_SCROLL_FWD)
        crazypod_route_actions_move(
            crazypod_menu_preview_is_skeuomorphic_route(
                state->route)
                ? 1
                : wheel_step(
                    data,
                    state->route == MUSIC_ROUTE_NOW_PLAYING
                        ? 1
                        : state->route ==
                              MUSIC_ROUTE_ALBUM_FLOW
                            ? 15 : 12),
            now);
    else if(base == BUTTON_SCROLL_BACK)
        crazypod_route_actions_move(
            crazypod_menu_preview_is_skeuomorphic_route(
                state->route)
                ? -1
                : -wheel_step(
                    data,
                    state->route == MUSIC_ROUTE_NOW_PLAYING
                        ? 1
                        : state->route ==
                              MUSIC_ROUTE_ALBUM_FLOW
                            ? 15 : 12),
            now);
    else if(base == BUTTON_RIGHT) {
        if(state->route == MUSIC_ROUTE_NOW_PLAYING)
            audio_next();
        else
            crazypod_route_actions_move(1, now);
    }
    else if(base == BUTTON_LEFT) {
        if(state->route == MUSIC_ROUTE_NOW_PLAYING)
            audio_prev();
        else
            crazypod_route_actions_move(-1, now);
    }
    else if(base == BUTTON_SELECT && !repeated)
        crazypod_route_actions_activate(now);
    else if(base == BUTTON_MENU) {
        if(repeated && state->route == MUSIC_ROUTE_MENU)
            host.begin_music_scan();
        else if(!repeated)
            crazypod_route_actions_pop();
    }
    else if(base == BUTTON_PLAY) {
        if(state->route == NOTES_ROUTE_COMPOSER &&
           !repeated) {
        crazypod_notes_feature_toggle_editor_field();
            host.render(false);
        }
        else
            host.toggle_playback();
    }
}

#endif
