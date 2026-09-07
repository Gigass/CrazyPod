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

#include "../../accessory/crazypod_iap_simple.h"
#include "../../crazypod_apps.h"
#include "../../crazypod_screen_recording.h"
#include "../../crazypod_screenshot.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../navigation/crazypod_alpha_jump.h"
#include "../navigation/crazypod_input_event.h"
#include "../navigation/crazypod_remote_multitap.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../presentation/crazypod_hold_feedback.h"
#include "../shell/crazypod_desktop.h"
#include "../shell/crazypod_home_actions.h"
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
static bool home_play_hold_pending;
static bool home_play_gesture_owned;
static long home_play_hold_deadline;
static bool home_hold_pending;
static bool home_menu_tap_pending;
static bool home_menu_gesture_owned;
static bool home_menu_release_seen;
static long home_hold_deadline;
static long home_menu_release_deadline;
static long now_playing_direction_button;
static bool now_playing_direction_held;
static bool capture_chord_pending;
static bool capture_chord_recording_toggled;
static struct crazypod_hold_feedback capture_hold_feedback;
static struct crazypod_alpha_jump_state alpha_jump;
static struct crazypod_remote_multitap_state remote_down_multitap;
static struct crazypod_remote_multitap_state headset_multitap;
static struct crazypod_remote_multitap_state now_select_multitap;
static struct crazypod_remote_multitap_state lock_select_multitap;
static bool remote_down_hold_active;
static bool remote_down_hold_confirmed;
static long remote_down_hold_start;
static bool remote_home_select_pending;
static bool remote_home_select_held;

#define HOME_NOW_PLAYING_HOLD_MS 900
#define HOME_NOW_PLAYING_HOLD_TICKS \
    ((HZ * HOME_NOW_PLAYING_HOLD_MS / 1000) > 0 \
        ? (HZ * HOME_NOW_PLAYING_HOLD_MS / 1000) : 1)
#define HOME_PLAY_LOCK_HOLD_MS 700
#define HOME_PLAY_LOCK_HOLD_TICKS \
    ((HZ * HOME_PLAY_LOCK_HOLD_MS / 1000) > 0 \
        ? (HZ * HOME_PLAY_LOCK_HOLD_MS / 1000) : 1)
#define HOME_HOLD_FEEDBACK_DELAY_MS 200
#define HOME_HOLD_FEEDBACK_DELAY_TICKS \
    ((HZ * HOME_HOLD_FEEDBACK_DELAY_MS / 1000) > 0 \
        ? (HZ * HOME_HOLD_FEEDBACK_DELAY_MS / 1000) : 1)
#define HOME_HOLD_FEEDBACK_COMPLETION_LEAD_MS 40
#define HOME_HOLD_FEEDBACK_COMPLETION_LEAD_TICKS \
    ((HZ * HOME_HOLD_FEEDBACK_COMPLETION_LEAD_MS / 1000) > 0 \
        ? (HZ * HOME_HOLD_FEEDBACK_COMPLETION_LEAD_MS / 1000) : 1)
/* iAP remote state is leased for 300 ms. Keep gesture ownership through
 * that lease plus one scheduler margin so a delayed duplicate state cannot
 * become a fresh Menu press on the newly opened route. */
#define HOME_MENU_RELEASE_DEBOUNCE_MS 350
#define HOME_MENU_RELEASE_DEBOUNCE_TICKS \
    ((HZ * HOME_MENU_RELEASE_DEBOUNCE_MS / 1000) > 0 \
        ? (HZ * HOME_MENU_RELEASE_DEBOUNCE_MS / 1000) : 1)
#define CAPTURE_HOLD_MS 500
#define ALPHA_JUMP_WINDOW_TICKS \
    ((HZ * 320 / 1000) > 0 ? (HZ * 320 / 1000) : 1)
#define ALPHA_JUMP_STEP_THRESHOLD 7
#define REMOTE_MULTITAP_WINDOW_MS 500
#define REMOTE_MULTITAP_WINDOW_TICKS \
    ((HZ * REMOTE_MULTITAP_WINDOW_MS / 1000) > 0 \
        ? (HZ * REMOTE_MULTITAP_WINDOW_MS / 1000) : 1)
#define LOCK_REMOTE_MULTITAP_WINDOW_MS 900
#define LOCK_REMOTE_MULTITAP_WINDOW_TICKS \
    ((HZ * LOCK_REMOTE_MULTITAP_WINDOW_MS / 1000) > 0 \
        ? (HZ * LOCK_REMOTE_MULTITAP_WINDOW_MS / 1000) : 1)
#define REMOTE_DOWN_LONG_CONFIRM_MS 700
#define REMOTE_DOWN_LONG_CONFIRM_TICKS \
    ((HZ * REMOTE_DOWN_LONG_CONFIRM_MS / 1000) > 0 \
        ? (HZ * REMOTE_DOWN_LONG_CONFIRM_MS / 1000) : 1)

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

static void execute_remote_multitap(
    enum crazypod_remote_multitap_action action)
{
    bool locked;

    if(action == CRAZYPOD_REMOTE_MULTITAP_NONE)
        return;
    backlight_on();
    locked = host.locked != NULL && host.locked();
    if(locked) {
        if(action == CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE)
            host.toggle_playback_async();
        else if(action == CRAZYPOD_REMOTE_MULTITAP_NEXT)
            host.next_track_async();
        else if(action == CRAZYPOD_REMOTE_MULTITAP_PREVIOUS)
            host.previous_track_async();
        return;
    }
    if(action == CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE)
        host.toggle_playback();
    else if(action == CRAZYPOD_REMOTE_MULTITAP_NEXT)
        crazypod_playback_next();
    else if(action == CRAZYPOD_REMOTE_MULTITAP_PREVIOUS)
        host.previous_track();
}

static bool lock_select_multitap_active(void)
{
    return host.locked != NULL && host.locked() &&
        host.lock_media_controls_ready != NULL &&
        host.lock_media_controls_ready();
}

static void execute_lock_select_multitap(
    enum crazypod_remote_multitap_action action)
{
    if(action == CRAZYPOD_REMOTE_MULTITAP_NONE ||
       !lock_select_multitap_active())
        return;
    backlight_on();
    if(action == CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE)
        host.toggle_playback_async();
    else if(action == CRAZYPOD_REMOTE_MULTITAP_NEXT)
        host.next_track_async();
    else if(action == CRAZYPOD_REMOTE_MULTITAP_PREVIOUS)
        host.previous_track_async();
}

static void clear_remote_down_hold(bool release_power)
{
    if(release_power && remote_down_hold_confirmed)
        (void)host.handle_power_hold(
            BUTTON_PLAY | BUTTON_REL);
    remote_down_hold_active = false;
    remote_down_hold_confirmed = false;
    remote_down_hold_start = 0;
}

static long remote_down_multitap_window_ticks(void)
{
    return host.locked != NULL && host.locked()
        ? LOCK_REMOTE_MULTITAP_WINDOW_TICKS
        : REMOTE_MULTITAP_WINDOW_TICKS;
}

static void confirm_remote_down_hold(long now)
{
#ifdef IPOD_ACCESSORY_PROTOCOL
    if(!remote_down_hold_active ||
       remote_down_hold_confirmed ||
       (long)(now - (remote_down_hold_start +
                     REMOTE_DOWN_LONG_CONFIRM_TICKS)) < 0)
        return;
    remote_down_hold_confirmed = true;
    (void)crazypod_remote_multitap_handle_down(
        &remote_down_multitap,
        BUTTON_RC_DOWN | BUTTON_REPEAT,
        now, remote_down_multitap_window_ticks());
    host.begin_power_hold(remote_down_hold_start);
#else
    (void)now;
#endif
}

static void home_open_selected_app(void)
{
    crazypod_app_launcher_open(
        crazypod_apps_visible_id(
            crazypod_desktop_selected()));
}

static bool home_play_can_start(void)
{
    if(crazypod_shell_product_active() ||
       (host.locked != NULL && host.locked()) ||
       host.power_prompt_visible() ||
       host.headphone_prompt_visible())
        return false;
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(host.usb_prompt_visible())
        return false;
#endif
    return true;
}

static void cancel_home_play_gesture(void)
{
    home_play_hold_pending = false;
    home_play_gesture_owned = false;
    home_play_hold_deadline = 0;
}

static bool handle_home_play_gesture(long button, long now)
{
    long base = button_base(button);
    bool release = (button & BUTTON_REL) != 0;
    bool repeated = (button & BUTTON_REPEAT) != 0;

    if(home_play_gesture_owned) {
        if(base == BUTTON_PLAY) {
            if(release)
                cancel_home_play_gesture();
            return true;
        }
        cancel_home_play_gesture();
        return false;
    }
    if(home_play_hold_pending) {
        if(base != BUTTON_PLAY) {
            cancel_home_play_gesture();
            return false;
        }
        if(release) {
            cancel_home_play_gesture();
            backlight_on();
            host.toggle_playback();
        }
        return true;
    }
    if(base != BUTTON_PLAY || release || repeated ||
       !home_play_can_start())
        return false;
    home_play_hold_pending = true;
    home_play_hold_deadline = now + HOME_PLAY_LOCK_HOLD_TICKS;
    return true;
}

static bool now_playing_select_multitap_active(void)
{
    const struct route_state *state;

    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0 ||
       crazypod_now_playing_overlay_visible() ||
       crazypod_choice_coordinator_visible())
        return false;
    state = crazypod_ui_routes_current();
    return state != NULL &&
        state->route == MUSIC_ROUTE_NOW_PLAYING &&
        (!crazypod_now_playing_theme_open() ||
         !crazypod_now_playing_theme_modal_visible());
}

static void execute_now_select_multitap(
    enum crazypod_remote_multitap_action action, long now)
{
    const struct route_state *state;
    struct crazypod_input_event event;

    if(action == CRAZYPOD_REMOTE_MULTITAP_NONE ||
       !now_playing_select_multitap_active())
        return;
    backlight_on();
    if(action == CRAZYPOD_REMOTE_MULTITAP_NEXT) {
        crazypod_playback_next();
        return;
    }
    if(action == CRAZYPOD_REMOTE_MULTITAP_PREVIOUS) {
        crazypod_playback_previous_or_restart();
        return;
    }

    state = crazypod_ui_routes_current();
    event = crazypod_input_event_make(BUTTON_SELECT, 0);
    if(crazypod_feature_input_dispatch(
           state, &event, CRAZYPOD_FEATURE_INPUT_RAW,
           host.feature_bindings))
        return;
    crazypod_route_actions_activate(now);
}

static bool remote_home_select_can_start(void)
{
    if(crazypod_shell_product_active() ||
       crazypod_home_actions_visible() ||
       crazypod_now_playing_overlay_visible() ||
       host.power_prompt_visible() ||
       host.headphone_prompt_visible() ||
       (host.locked != NULL && host.locked()))
        return false;
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(host.usb_prompt_visible())
        return false;
#endif
    return true;
}

static bool handle_remote_home_select(
    long button, bool remote_input)
{
    long base = button_base(button);
    bool matching = remote_input && base == BUTTON_SELECT;
    bool release = (button & BUTTON_REL) != 0;
    bool repeated = (button & BUTTON_REPEAT) != 0;
    long power_button;

    if(!remote_home_select_pending) {
        if(!matching || release || repeated ||
           !remote_home_select_can_start())
            return false;
        remote_home_select_pending = true;
        remote_home_select_held = false;
        (void)host.handle_power_hold(BUTTON_PLAY);
        return true;
    }

    if(!matching) {
        (void)host.handle_power_hold(
            BUTTON_PLAY | BUTTON_REL);
        remote_home_select_pending = false;
        remote_home_select_held = false;
        return false;
    }
    if(host.power_prompt_visible()) {
        if(release) {
            remote_home_select_pending = false;
            remote_home_select_held = false;
        }
        return true;
    }

    power_button = BUTTON_PLAY |
        (button & (BUTTON_REL | BUTTON_REPEAT));
    if(repeated)
        remote_home_select_held = true;
    (void)host.handle_power_hold(power_button);
    if(release) {
        bool short_press = !remote_home_select_held;

        remote_home_select_pending = false;
        remote_home_select_held = false;
        if(short_press) {
            backlight_on();
            home_open_selected_app();
        }
    }
    return true;
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
        crazypod_app_input_cancel_pending();
    }
}

void crazypod_app_input_cancel_pending(void)
{
    play_short_press_pending = false;
    cancel_home_play_gesture();
    home_hold_pending = false;
    home_menu_tap_pending = false;
    home_menu_gesture_owned = false;
    home_menu_release_seen = false;
    home_hold_deadline = 0;
    home_menu_release_deadline = 0;
    finish_now_playing_direction_gesture();
    capture_chord_pending = false;
    capture_chord_recording_toggled = false;
    crazypod_desktop_hold_feedback_dismiss(false);
    crazypod_hold_feedback_dismiss(&capture_hold_feedback);
    crazypod_alpha_jump_reset(&alpha_jump);
    crazypod_remote_multitap_reset(&remote_down_multitap);
    crazypod_remote_multitap_reset(&headset_multitap);
    crazypod_remote_multitap_reset(&now_select_multitap);
    crazypod_remote_multitap_reset(&lock_select_multitap);
    clear_remote_down_hold(false);
    remote_home_select_pending = false;
    remote_home_select_held = false;
    crazypod_photos_feature_cancel_input();
    crazypod_customize_feature_clear_input_holds();
}

int crazypod_app_input_wait_ticks(long now)
{
    long next_deadline;
    long remaining;
    int wait = crazypod_choice_coordinator_wait_ticks(now);

    if(home_play_hold_pending) {
        remaining = home_play_hold_deadline - now;
        if(remaining <= 0)
            return 1;
        if(remaining < wait)
            wait = (int)remaining;
    }

    wait = crazypod_remote_multitap_wait_ticks(
        &remote_down_multitap, now, wait);
    wait = crazypod_remote_multitap_wait_ticks(
        &headset_multitap, now, wait);
    wait = crazypod_remote_multitap_wait_ticks(
        &now_select_multitap, now, wait);
    wait = crazypod_remote_multitap_wait_ticks(
        &lock_select_multitap, now, wait);
    if(remote_down_hold_active &&
       !remote_down_hold_confirmed) {
        remaining = remote_down_hold_start +
            REMOTE_DOWN_LONG_CONFIRM_TICKS - now;
        if(remaining <= 0)
            return 1;
        if(remaining < wait)
            wait = (int)remaining;
    }
    if(home_menu_gesture_owned && home_menu_release_seen) {
        remaining = home_menu_release_deadline - now;
        if(remaining <= 0)
            return 1;
        if(remaining < wait)
            wait = (int)remaining;
    }

    if(!home_hold_pending)
        return wait;
    next_deadline = home_hold_deadline;
    if(!crazypod_desktop_hold_feedback_visible()) {
        long feedback_deadline = home_hold_deadline -
            HOME_NOW_PLAYING_HOLD_TICKS +
            HOME_HOLD_FEEDBACK_DELAY_TICKS;

        if(feedback_deadline < next_deadline)
            next_deadline = feedback_deadline;
    }
    remaining = next_deadline - now;
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
        !crazypod_home_actions_visible() &&
        !crazypod_now_playing_overlay_visible() &&
        !crazypod_desktop_hold_feedback_visible() &&
        !host.power_prompt_visible() &&
        !host.headphone_prompt_visible();
    int feedback;

    if(home_menu_gesture_owned && home_menu_release_seen &&
       (long)(now - home_menu_release_deadline) >= 0) {
        home_menu_gesture_owned = false;
        home_menu_release_seen = false;
        home_menu_release_deadline = 0;
    }

    confirm_remote_down_hold(now);
    execute_remote_multitap(crazypod_remote_multitap_tick(
        &remote_down_multitap, now));
    execute_remote_multitap(crazypod_remote_multitap_tick(
        &headset_multitap, now));
    if(!locked || !lock_select_multitap_active())
        crazypod_remote_multitap_reset(&lock_select_multitap);
    else
        execute_lock_select_multitap(
            crazypod_remote_multitap_tick(
                &lock_select_multitap, now));
    if(locked || !now_playing_select_multitap_active())
        crazypod_remote_multitap_reset(&now_select_multitap);
    else
        execute_now_select_multitap(
            crazypod_remote_multitap_tick(
                &now_select_multitap, now), now);

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
    if(home_play_hold_pending) {
        if(!home_play_can_start())
            cancel_home_play_gesture();
        else if((long)(now - home_play_hold_deadline) >= 0) {
            home_play_hold_pending = false;
            home_play_gesture_owned = true;
            home_play_hold_deadline = 0;
            if(crazypod_home_actions_visible())
                crazypod_home_actions_dismiss(true);
            if(crazypod_now_playing_overlay_visible())
                crazypod_now_playing_overlay_dismiss(false);
            backlight_on();
            host.show_lock(false);
        }
    }
    if(!home_hold_pending)
        return;
    if(locked ||
       crazypod_shell_product_active() ||
       crazypod_now_playing_overlay_visible() ||
       host.power_prompt_visible() ||
       host.headphone_prompt_visible()
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
       || host.usb_prompt_visible()
#endif
       ) {
        home_hold_pending = false;
        home_menu_tap_pending = false;
        crazypod_desktop_hold_feedback_dismiss(false);
        return;
    }
    if((long)(now - home_hold_deadline) < 0) {
        long feedback_deadline = home_hold_deadline -
            HOME_NOW_PLAYING_HOLD_TICKS +
            HOME_HOLD_FEEDBACK_DELAY_TICKS;

        if(!crazypod_desktop_hold_feedback_visible() &&
           (long)(now - feedback_deadline) >= 0) {
            long remaining_ticks = home_hold_deadline - now;
            long animation_ticks = MAX(
                1,
                remaining_ticks -
                    HOME_HOLD_FEEDBACK_COMPLETION_LEAD_TICKS);

            home_menu_tap_pending = false;
            crazypod_desktop_hold_feedback_begin(
                LV_SYMBOL_AUDIO,
                (int)(animation_ticks * 1000 / HZ));
        }
        return;
    }

    home_hold_pending = false;
    home_menu_tap_pending = false;
    crazypod_desktop_hold_feedback_dismiss(false);
    backlight_on();
    home_menu_gesture_owned = true;
    home_menu_release_seen = false;
    home_menu_release_deadline = 0;
    host.open_now_playing();
}

void crazypod_app_input_handle(
    long button, intptr_t data, long now)
{
    long base;
    bool play_initial_press = false;
    bool play_short_release = false;
    bool home_menu_short_release = false;
    bool remote_input = false;
    bool repeated;
    struct route_state *state;

    if(button == BUTTON_NONE)
        return;
#ifdef HAVE_MULTIMEDIA_KEYS
    if((button & SYS_EVENT) == 0 &&
       (button & BUTTON_MULTIMEDIA) != 0) {
        switch(button) {
        case BUTTON_MULTIMEDIA_PLAYPAUSE:
        case BUTTON_MULTIMEDIA_PLAYPAUSE | BUTTON_REL:
        case BUTTON_MULTIMEDIA_PLAYPAUSE | BUTTON_REPEAT:
            execute_remote_multitap(crazypod_multitap_handle_button(
                &headset_multitap, button,
                BUTTON_MULTIMEDIA_PLAYPAUSE, now,
                REMOTE_MULTITAP_WINDOW_TICKS));
            break;
        case BUTTON_MULTIMEDIA_VOLUME_UP:
        case BUTTON_MULTIMEDIA_VOLUME_UP | BUTTON_REPEAT:
            crazypod_now_playing_adjust_volume(1);
            break;
        case BUTTON_MULTIMEDIA_VOLUME_DOWN:
        case BUTTON_MULTIMEDIA_VOLUME_DOWN | BUTTON_REPEAT:
            crazypod_now_playing_adjust_volume(-1);
            break;
        }
        return;
    }
#endif
    if(crazypod_iap_simple_handle_event(button, data)) {
        if(crazypod_iap_simple_take_dock_connected() &&
           host.dock_connected != NULL)
            host.dock_connected();
        return;
    }
    if(crazypod_system_event_handle(
           button, data, &host.system_events))
        return;
    if(remote_down_hold_active &&
       !crazypod_remote_multitap_is_down(button)) {
        clear_remote_down_hold(true);
    }
    if(crazypod_remote_multitap_is_down(button)) {
        bool release = (button & BUTTON_REL) != 0;
        bool remote_repeat = (button & BUTTON_REPEAT) != 0;

        if(host.power_prompt_visible()) {
            clear_remote_down_hold(false);
            crazypod_remote_multitap_reset(
                &remote_down_multitap);
            return;
        }
        if(!release && !remote_repeat &&
           !remote_down_hold_active) {
            remote_down_hold_active = true;
            remote_down_hold_confirmed = false;
            remote_down_hold_start = now;
        }
        if(remote_repeat) {
            confirm_remote_down_hold(now);
            return;
        }
        if(release)
            clear_remote_down_hold(true);
        execute_remote_multitap(
            crazypod_remote_multitap_handle_down(
                &remote_down_multitap, button, now,
                remote_down_multitap_window_ticks()));
        return;
    }
    execute_remote_multitap(crazypod_remote_multitap_flush(
        &remote_down_multitap));
    remote_input = crazypod_input_button_is_remote(button);
    button = crazypod_input_translate_remote(button);
    if(button == BUTTON_NONE)
        return;
    base = button_base(button);
    if(home_menu_gesture_owned && base != BUTTON_MENU) {
        home_menu_gesture_owned = false;
        home_menu_release_seen = false;
        home_menu_release_deadline = 0;
    }
    if(home_hold_pending && base != BUTTON_MENU) {
        home_hold_pending = false;
        home_menu_tap_pending = false;
        crazypod_desktop_hold_feedback_dismiss(false);
    }
    if(handle_capture_chord(button, now))
        return;
    if(base == BUTTON_MENU &&
       (button & BUTTON_REL) != 0) {
        long tap_deadline = home_hold_deadline -
            HOME_NOW_PLAYING_HOLD_TICKS +
            HOME_HOLD_FEEDBACK_DELAY_TICKS;

        home_menu_short_release =
            home_hold_pending && home_menu_tap_pending &&
            (long)(now - tap_deadline) < 0;
        home_hold_pending = false;
        home_menu_tap_pending = false;
        crazypod_desktop_hold_feedback_dismiss(
            home_menu_short_release);
    }
    if(handle_remote_home_select(button, remote_input))
        return;
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
    if(handle_home_play_gesture(button, now))
        return;
    if(host.handle_power_hold(button)) {
        if(base == BUTTON_PLAY &&
           (button & BUTTON_REPEAT) != 0)
            play_short_press_pending = false;
        return;
    }
    if(remote_input && base == BUTTON_SELECT &&
       host.locked != NULL && host.locked()) {
        if(lock_select_multitap_active())
            execute_lock_select_multitap(
                crazypod_multitap_handle_button(
                    &lock_select_multitap, button,
                    BUTTON_SELECT, now,
                    LOCK_REMOTE_MULTITAP_WINDOW_TICKS));
        else
            crazypod_remote_multitap_reset(
                &lock_select_multitap);
        (void)host.handle_lock(button, data);
        return;
    }
    if(host.handle_lock(button, data)) {
        if(base == BUTTON_PLAY &&
           (button & BUTTON_REL) != 0)
            play_short_press_pending = false;
        return;
    }
    if(home_menu_gesture_owned && base == BUTTON_MENU) {
        if((button & BUTTON_REL) != 0 ||
           home_menu_release_seen) {
            home_menu_release_seen = true;
            home_menu_release_deadline =
                now + HOME_MENU_RELEASE_DEBOUNCE_TICKS;
        }
        return;
    }
    if(now_playing_select_multitap_active() &&
       base == BUTTON_SELECT) {
        execute_now_select_multitap(
            crazypod_multitap_handle_button(
                &now_select_multitap, button,
                BUTTON_SELECT, now,
                REMOTE_MULTITAP_WINDOW_TICKS),
            now);
        return;
    }
    execute_now_select_multitap(
        crazypod_remote_multitap_flush(
            &now_select_multitap), now);
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
            crazypod_home_actions_show();
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
    if(crazypod_home_actions_visible()) {
        (void)crazypod_home_actions_handle_button(
            base, repeated, data);
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
            if(repeated) {
                home_menu_tap_pending = false;
            }
            else if(!home_hold_pending) {
                home_hold_pending = true;
                home_menu_tap_pending = true;
                home_hold_deadline =
                    now + HOME_NOW_PLAYING_HOLD_TICKS;
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
       !remote_input &&
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
