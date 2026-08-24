#include "config.h"

#ifdef IPOD_6G

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
#include "../../crazypod_screen_recording.h"
#include "../../crazypod_screenshot.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../navigation/crazypod_alpha_jump.h"
#include "../navigation/crazypod_input_event.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../presentation/crazypod_hold_feedback.h"
#include "../shell/crazypod_desktop.h"
#include "../shell/crazypod_home_input.h"
#include "../shell/crazypod_shell.h"
#include "../shell/crazypod_screenshot_feedback.h"
#include "crazypod_app_input.h"
#include "crazypod_app_launcher.h"
#include "crazypod_choice_coordinator.h"
#include "crazypod_menu_preview.h"
#include "crazypod_playback.h"
#include "crazypod_route_actions.h"

static struct crazypod_app_input_host host;
static bool play_short_press_pending;
static bool home_hold_pending;
static bool home_menu_gesture_owned;
static long home_hold_deadline;
static long now_playing_direction_button;
static bool now_playing_direction_held;
static bool capture_chord_pending;
static bool capture_chord_recording_toggled;
static struct crazypod_hold_feedback home_hold_feedback;
static struct crazypod_hold_feedback capture_hold_feedback;
static struct crazypod_alpha_jump_state alpha_jump;

#define HOME_NOW_PLAYING_HOLD_MS 900
#define HOME_NOW_PLAYING_HOLD_TICKS \
    ((HZ * HOME_NOW_PLAYING_HOLD_MS / 1000) > 0 \
        ? (HZ * HOME_NOW_PLAYING_HOLD_MS / 1000) : 1)
#define CAPTURE_HOLD_MS 500
#define ALPHA_JUMP_WINDOW_TICKS \
    ((HZ * 320 / 1000) > 0 ? (HZ * 320 / 1000) : 1)
#define ALPHA_JUMP_STEP_THRESHOLD 7

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

static void move_wheel(
    struct route_state *state, int direction,
    intptr_t data, long now)
{
    int maximum =
        state->route == MUSIC_ROUTE_NOW_PLAYING
            ? 1
            : state->route == MUSIC_ROUTE_ALBUM_FLOW
                ? 15 : 12;
    int steps = crazypod_menu_preview_is_skeuomorphic_route(
        state->route) ? 1 : wheel_step(data, maximum);
    bool alpha_available =
        crazypod_music_feature_alpha_jump_available(state);

    if(alpha_available &&
       crazypod_alpha_jump_consume(
           &alpha_jump, state->route, state->group,
           direction, steps, now,
           ALPHA_JUMP_WINDOW_TICKS,
           ALPHA_JUMP_STEP_THRESHOLD) &&
       crazypod_route_actions_alpha_jump(direction, now))
        return;
    if(!alpha_available)
        crazypod_alpha_jump_reset(&alpha_jump);
    crazypod_route_actions_move(direction * steps, now);
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

static bool album_flow_owns_wheel_feedback(long base)
{
#ifdef HAVE_WHEEL_POSITION
    const struct route_state *state;

    if(base != BUTTON_SCROLL_FWD && base != BUTTON_SCROLL_BACK)
        return false;
    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0 ||
       crazypod_choice_coordinator_visible())
        return false;
    state = crazypod_ui_routes_current();
    return state != NULL && state->route == MUSIC_ROUTE_ALBUM_FLOW;
#else
    (void)base;
    return false;
#endif
}

static void home_next_track(void)
{
    crazypod_playback_next();
}

static void home_open_selected_app(void)
{
    crazypod_app_launcher_open(
        crazypod_apps_visible_id(
            crazypod_desktop_selected()));
}

static void handle_now_playing_overlay(
    long base, bool repeated, intptr_t data,
    bool refresh_on_dismiss)
{
    if(base == BUTTON_SCROLL_FWD)
        crazypod_now_playing_overlay_move(
            wheel_step(data, 12));
    else if(base == BUTTON_SCROLL_BACK)
        crazypod_now_playing_overlay_move(
            -wheel_step(data, 12));
    else if(base == BUTTON_RIGHT && !repeated)
        crazypod_playback_next();
    else if(base == BUTTON_LEFT && !repeated)
        host.previous_track();
    else if(base == BUTTON_SELECT && !repeated)
        crazypod_now_playing_overlay_activate();
    else if(base == BUTTON_MENU && !repeated)
        crazypod_now_playing_overlay_dismiss(
            refresh_on_dismiss);
    else if(base == BUTTON_PLAY && !repeated)
        host.toggle_playback();
}

static bool handle_capture_chord(long button, long now)
{
    long base = button_base(button);

    if(!capture_chord_pending &&
       base != (BUTTON_LEFT | BUTTON_RIGHT))
        return false;
    if(capture_chord_pending &&
       base != (BUTTON_LEFT | BUTTON_RIGHT)) {
        capture_chord_pending = false;
        capture_chord_recording_toggled = false;
        crazypod_hold_feedback_dismiss(
            &capture_hold_feedback);
        return false;
    }
    if(!capture_chord_pending) {
        capture_chord_pending = true;
        capture_chord_recording_toggled = false;
        backlight_on();
        crazypod_hold_feedback_begin(
            &capture_hold_feedback,
            crazypod_desktop_screen(), LV_SYMBOL_IMAGE,
            CAPTURE_HOLD_MS);
        return true;
    }
    if((button & BUTTON_REPEAT) != 0 &&
       base == (BUTTON_LEFT | BUTTON_RIGHT) &&
       !capture_chord_recording_toggled) {
        enum crazypod_screen_recording_result result =
            crazypod_screen_recording_toggle(now);

        capture_chord_recording_toggled = true;
        crazypod_hold_feedback_dismiss(
            &capture_hold_feedback);
        crazypod_screenshot_feedback_show_recording(result);
        return true;
    }
    if((button & BUTTON_REL) != 0) {
        if(!capture_chord_recording_toggled) {
            bool saved;

            saved = crazypod_screenshot_capture();
            crazypod_screenshot_feedback_show(saved);
        }
        capture_chord_pending = false;
        capture_chord_recording_toggled = false;
        crazypod_hold_feedback_dismiss(
            &capture_hold_feedback);
        return true;
    }
    return base == (BUTTON_LEFT | BUTTON_RIGHT);
}

static void finish_now_playing_direction_gesture(void)
{
    if(now_playing_direction_held)
        crazypod_playback_seek_finish();
    now_playing_direction_button = BUTTON_NONE;
    now_playing_direction_held = false;
}

static bool handle_now_playing_direction_button(long button)
{
    long base = button_base(button);
    bool owns_buttons =
        crazypod_shell_product_active() &&
        crazypod_ui_routes_depth() > 0 &&
        crazypod_ui_routes_current()->route ==
            MUSIC_ROUTE_NOW_PLAYING &&
        (!crazypod_now_playing_theme_open() ||
         !crazypod_now_playing_theme_modal_visible() ||
         crazypod_now_playing_overlay_visible());

    if(!owns_buttons ||
       (base != BUTTON_LEFT && base != BUTTON_RIGHT)) {
        if(now_playing_direction_button != BUTTON_NONE)
            finish_now_playing_direction_gesture();
        return false;
    }
    backlight_on();
    if((button & BUTTON_REL) != 0) {
        if(now_playing_direction_button == base) {
            if(now_playing_direction_held)
                crazypod_playback_seek_finish();
            else if(base == BUTTON_RIGHT)
                crazypod_playback_next();
            else
                crazypod_playback_previous_or_restart();
        }
        now_playing_direction_button = BUTTON_NONE;
        now_playing_direction_held = false;
        return true;
    }
    if((button & BUTTON_REPEAT) != 0) {
        if(now_playing_direction_button == base) {
            if(!now_playing_direction_held) {
                now_playing_direction_held = true;
                (void)crazypod_playback_seek_begin(
                    base == BUTTON_RIGHT ? 1 : -1);
            }
            crazypod_playback_seek_step();
        }
        return true;
    }
    if(now_playing_direction_button != BUTTON_NONE)
        finish_now_playing_direction_gesture();
    now_playing_direction_button = base;
    return true;
}

void crazypod_app_input_configure(
    const struct crazypod_app_input_host *new_host)
{
    if(new_host != NULL) {
        host = *new_host;
        home_hold_pending = false;
        home_menu_gesture_owned = false;
        now_playing_direction_button = BUTTON_NONE;
        now_playing_direction_held = false;
        capture_chord_pending = false;
        capture_chord_recording_toggled = false;
        crazypod_hold_feedback_dismiss(&home_hold_feedback);
        crazypod_hold_feedback_dismiss(&capture_hold_feedback);
        crazypod_alpha_jump_reset(&alpha_jump);
    }
}

int crazypod_app_input_wait_ticks(long now)
{
    long remaining;
    int wait = crazypod_choice_coordinator_wait_ticks(now);

    if(!home_hold_pending)
        return wait;
    remaining = home_hold_deadline - now;
    if(remaining <= 0)
        return 1;
    if(remaining < wait)
        wait = (int)remaining;
    return wait;
}

void crazypod_app_input_tick(long now, bool locked)
{
    const struct route_state *confirmation;
    bool home_active =
        !locked && !crazypod_shell_product_active() &&
        !crazypod_now_playing_overlay_visible() &&
        !host.power_prompt_visible() &&
        !host.headphone_prompt_visible();
    int feedback;

    confirmation = crazypod_choice_coordinator_tick(now);
    if(confirmation != NULL &&
       (host.handle_confirmation == NULL ||
        !host.handle_confirmation(confirmation)))
        (void)crazypod_choice_coordinator_cancel_select_hold();
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    home_active = home_active && !host.usb_prompt_visible();
#endif
    crazypod_desktop_set_active(home_active, now);
    crazypod_desktop_tick(now);
    feedback = crazypod_desktop_take_wheel_feedback();
    if(feedback != 0)
        wheel_feedback(feedback < 0
            ? BUTTON_SCROLL_BACK : BUTTON_SCROLL_FWD);
    if(locked || !crazypod_shell_product_active())
        crazypod_alpha_jump_reset(&alpha_jump);
    if(!home_hold_pending)
        return;
    if(locked ||
       crazypod_shell_product_active() ||
       host.power_prompt_visible() ||
       host.headphone_prompt_visible()
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
       || host.usb_prompt_visible()
#endif
       ) {
        home_hold_pending = false;
        crazypod_hold_feedback_dismiss(&home_hold_feedback);
        return;
    }
    if((long)(now - home_hold_deadline) < 0)
        return;

    home_hold_pending = false;
    crazypod_hold_feedback_dismiss(&home_hold_feedback);
    backlight_on();
    home_menu_gesture_owned = true;
    host.open_now_playing();
}

void crazypod_app_input_handle(
    long button, intptr_t data, long now)
{
    long base;
    bool play_initial_press = false;
    bool play_short_release = false;
    bool home_menu_short_release = false;
    bool repeated;
    struct route_state *state;

    if(button == BUTTON_NONE)
        return;
    if(crazypod_system_event_handle(
           button, data, &host.system_events))
        return;
    base = button_base(button);
    if(home_hold_pending && base != BUTTON_MENU) {
        home_hold_pending = false;
        crazypod_hold_feedback_dismiss(&home_hold_feedback);
    }
    if(handle_capture_chord(button, now))
        return;
    if(base == BUTTON_MENU &&
       (button & BUTTON_REL) != 0) {
        home_menu_short_release = home_hold_pending;
        home_hold_pending = false;
        crazypod_hold_feedback_dismiss(&home_hold_feedback);
    }
    if(host.power_prompt_visible()) {
        if(button_base(button) == BUTTON_PLAY)
            play_short_press_pending = false;
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
        if(button_base(button) == BUTTON_PLAY)
            play_short_press_pending = false;
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
    if(host.headphone_prompt_visible()) {
        if(button_base(button) == BUTTON_PLAY)
            play_short_press_pending = false;
        wheel_feedback(button);
        if(button & BUTTON_REL)
            return;
        repeated = (button & BUTTON_REPEAT) != 0;
        backlight_on();
        (void)host.handle_headphone_prompt(
            button_base(button), repeated, data);
        return;
    }
    if(host.handle_power_hold(button)) {
        if(base == BUTTON_PLAY &&
           (button & BUTTON_REPEAT) != 0)
            play_short_press_pending = false;
        return;
    }
    if(host.handle_lock(button, data)) {
        if(base == BUTTON_PLAY &&
           (button & BUTTON_REL) != 0)
            play_short_press_pending = false;
        return;
    }
    if(home_menu_gesture_owned && base == BUTTON_MENU) {
        if((button & BUTTON_REL) != 0)
            home_menu_gesture_owned = false;
        return;
    }
    if(base == BUTTON_PLAY) {
        if((button & BUTTON_REL) != 0) {
            play_short_release = play_short_press_pending;
            play_short_press_pending = false;
        }
        else if((button & BUTTON_REPEAT) == 0) {
            play_short_press_pending = true;
            play_initial_press = true;
        }
    }
    /* Album Flow samples absolute wheel position and emits feedback only
       after a complete album detent. Do not also click for queued Rockbox
       scroll events that this route discards below. */
    if(!album_flow_owns_wheel_feedback(base))
        wheel_feedback(button);
    if(handle_now_playing_direction_button(button))
        return;
    if(crazypod_shell_product_active() &&
       crazypod_ui_routes_depth() > 0 &&
       !crazypod_now_playing_overlay_visible()) {
        const struct crazypod_input_event event =
            crazypod_input_event_make(button, data);

        if(crazypod_feature_input_dispatch(
               crazypod_ui_routes_current(), &event,
               CRAZYPOD_FEATURE_INPUT_RAW,
               host.feature_bindings))
            return;
    }
    if(play_initial_press)
        return;
    if((button & BUTTON_REL) != 0 &&
       base == BUTTON_SELECT &&
       crazypod_choice_coordinator_cancel_select_hold())
        return;
    if(button & BUTTON_REL) {
        if(home_menu_short_release) {
            backlight_on();
            host.show_home_queue();
            return;
        }
        if(!play_short_release)
            return;
        /*
         * Replay a completed short Play gesture as a normal press only after
         * the raw-input owner has declined its release. This keeps feature
         * actions and playback toggling mutually exclusive with Play hold.
         */
        button = BUTTON_PLAY;
    }
    else if(base == BUTTON_PLAY &&
            (button & BUTTON_REPEAT) != 0)
        return;
    repeated = (button & BUTTON_REPEAT) != 0;
    base = button_base(button);
    backlight_on();
    if(crazypod_now_playing_overlay_visible()) {
        bool refresh_on_dismiss =
            crazypod_shell_product_active() &&
            crazypod_ui_routes_depth() > 0 &&
            crazypod_ui_routes_current()->route ==
                MUSIC_ROUTE_NOW_PLAYING;

        handle_now_playing_overlay(
            base, repeated, data, refresh_on_dismiss);
        return;
    }
    if(crazypod_shell_product_active() &&
       crazypod_ui_routes_depth() > 0 &&
       crazypod_music_library_loading()) {
        if(base == BUTTON_MENU && !repeated)
            host.close_product();
        return;
    }
    if(crazypod_shell_product_active() &&
       crazypod_ui_routes_depth() > 0 &&
       base == BUTTON_SELECT && repeated) {
        if(crazypod_choice_coordinator_handle_select_repeat())
            return;
        if(host.handle_confirmation(
               crazypod_choice_coordinator_confirmation_visible()
                   ? crazypod_choice_coordinator_route_state()
                   : crazypod_ui_routes_current()))
            return;
    }
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

        if(base == BUTTON_MENU) {
            if(!repeated) {
                home_hold_pending = true;
                home_hold_deadline =
                    now + HOME_NOW_PLAYING_HOLD_TICKS;
                crazypod_hold_feedback_begin(
                    &home_hold_feedback,
                    crazypod_desktop_screen(), LV_SYMBOL_AUDIO,
                    HOME_NOW_PLAYING_HOLD_MS);
            }
            return;
        }
        crazypod_home_input_handle(&event, &actions);
        return;
    }
    if(crazypod_ui_routes_depth() <= 0) {
        if(base == BUTTON_MENU)
            host.close_product();
        return;
    }
    state = crazypod_ui_routes_current();
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
            crazypod_choice_coordinator_activate(now);
        else if(base == BUTTON_MENU && !repeated)
            (void)crazypod_choice_coordinator_back();
        return;
    }
    {
        const struct crazypod_input_event event =
            crazypod_input_event_make(button, data);

        if(crazypod_feature_input_dispatch(
               state, &event,
               CRAZYPOD_FEATURE_INPUT_PRESSED,
               host.feature_bindings))
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
        move_wheel(state, 1, data, now);
    else if(base == BUTTON_SCROLL_BACK)
        move_wheel(state, -1, data, now);
    else if(base == BUTTON_RIGHT) {
        crazypod_alpha_jump_reset(&alpha_jump);
        if(state->route == MUSIC_ROUTE_NOW_PLAYING)
            crazypod_playback_next();
        else
            crazypod_route_actions_move(1, now);
    }
    else if(base == BUTTON_LEFT) {
        crazypod_alpha_jump_reset(&alpha_jump);
        if(state->route == MUSIC_ROUTE_NOW_PLAYING)
            crazypod_playback_previous_or_restart();
        else
            crazypod_route_actions_move(-1, now);
    }
    else if(base == BUTTON_SELECT && !repeated) {
        crazypod_alpha_jump_reset(&alpha_jump);
        crazypod_route_actions_activate(now);
    }
    else if(base == BUTTON_MENU) {
        crazypod_alpha_jump_reset(&alpha_jump);
        if(repeated && state->route == MUSIC_ROUTE_MENU)
            host.begin_music_scan();
        else if(!repeated)
            crazypod_route_actions_pop();
    }
    else if(base == BUTTON_PLAY) {
        crazypod_alpha_jump_reset(&alpha_jump);
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
