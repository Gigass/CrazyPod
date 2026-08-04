#include "config.h"
#ifdef IPOD_6G
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "backlight.h"
#include "button.h"
#include "dir.h"
#include "events.h"
#include "file.h"
#include "font.h"
#include "kernel.h"
#include "lcd.h"
#include "misc.h"
#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
#include "piezo.h"
#endif
#include "powermgmt.h"
#include "playlist.h"
#include "settings.h"
#include "sound.h"
#include "system.h"
#include "timefuncs.h"
#include "usb.h"
#ifdef SIMULATOR
#include <stdlib.h>
#include "screendump.h"
#endif

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_audio_shims.h"
#include "crazypod_apps.h"
#include "crazypod_artwork.h"
#include "crazypod_appearance.h"
#include "crazypod_book_cover.h"
#include "crazypod_books.h"
#include "crazypod_coverflow.h"
#include "crazypod_frameclock.h"
#include "crazypod_image.h"
#include "crazypod_icons.h"
#include "crazypod_lyrics.h"
#include "crazypod_runtime_font.h"
#include "crazypod_lcd.h"
#include "crazypod_music.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_font.h"
#include "crazypod_notes.h"
#include "crazypod_organizer.h"
#include "crazypod_playlist.h"
#include "crazypod_photos.h"
#include "crazypod_presets.h"
#include "crazypod_state.h"
#include "crazypod_ui.h"
#include "platform/crazypod_platform_display.h"
#include "ui/app/crazypod_miniapp_repro.h"
#include "ui/app/crazypod_simulator_snapshot.h"
#include "ui/app/crazypod_choice_coordinator.h"
#include "ui/app/crazypod_menu_preview.h"
#include "ui/app/crazypod_menu_rows.h"
#include "ui/app/crazypod_route_renderer.h"
#include "ui/app/crazypod_app_launcher.h"
#include "ui/app/crazypod_route_actions.h"
#include "ui/app/crazypod_runtime_services.h"
#include "ui/app/crazypod_app_input.h"
#include "ui/app/crazypod_feature_input.h"
#include "ui/app/crazypod_playback.h"
#include "ui/app/crazypod_composition.h"
#include "ui/features/organizer/crazypod_organizer_feature.h"
#include "ui/navigation/crazypod_ui_routes.h"
#include "ui/features/settings/crazypod_settings_feature.h"
#include "ui/presentation/crazypod_ui_text.h"
#include "ui/presentation/crazypod_ui_widgets.h"
#include "ui/presentation/crazypod_artwork_widget.h"
#include "ui/presentation/crazypod_alpha_jump_hud.h"
#include "ui/features/books/crazypod_books_feature.h"
#include "ui/features/notes/crazypod_notes_feature.h"
#include "ui/features/photos/crazypod_photos_feature.h"
#include "ui/shell/crazypod_app_catalog.h"
#include "ui/shell/crazypod_desktop.h"
#include "ui/shell/crazypod_desktop_native.h"
#include "ui/shell/crazypod_home_input.h"
#include "ui/presentation/crazypod_menu_list.h"
#include "ui/features/miniapps/crazypod_miniapps_feature.h"
#include "ui/features/music/crazypod_music_feature.h"
#include "ui/navigation/crazypod_feature_dispatcher.h"
#include "ui/navigation/crazypod_input_event.h"
#include "ui/navigation/crazypod_route_registry.h"
#include "ui/navigation/crazypod_render_scheduler.h"
#include "ui/features/now_playing/crazypod_now_playing_feature.h"
#include "ui/shell/crazypod_extras_preview.h"
#include "ui/navigation/crazypod_route_query.h"
#include "ui/presentation/crazypod_popup_motion.h"
#include "ui/presentation/crazypod_preview_motion.h"
#include "ui/presentation/crazypod_preview_primitives.h"
#include "ui/presentation/crazypod_overlay_glass.h"
#include "ui/presentation/crazypod_screen_corners.h"
#include "ui/presentation/crazypod_glass_panel.h"
#include "ui/presentation/crazypod_glass_sampler.h"
#include "ui/features/customize/crazypod_customize_feature.h"
#include "ui/shell/crazypod_lock_screen.h"
#include "ui/shell/crazypod_now_capsule.h"
#include "ui/shell/crazypod_power_prompt.h"
#include "ui/shell/crazypod_shell.h"
#include "ui/shell/crazypod_status_bar.h"
#include "ui/shell/crazypod_system_event.h"
#include "ui/shell/crazypod_system_prompts.h"
#include "ui/shell/crazypod_usb_prompt.h"
#include "crazypod_videos.h"
#include "crazypod_wallpaper.h"
#include "crazypod_workouts.h"

#define CRAZYPOD_STATUS_BAR_HEIGHT 32
#define CRAZYPOD_MENU_PANEL_Y CRAZYPOD_STATUS_BAR_HEIGHT
#define CRAZYPOD_PREVIEW_SETTLE_TICKS \
    ((HZ * 120 / 1000) > 0 ? (HZ * 120 / 1000) : 1)
#define CRAZYPOD_METADATA_FONT (crazypod_runtime_font_at_size(12))
static bool cpu_is_boosted;
static long boost_until;
static struct crazypod_frameclock lvgl_clock;
static int appearance_tile_size(void);
static struct route_state *current_route(void);
static const char *route_item_title(
    const struct route_state *state, int index);
static int route_item_count(
    const struct route_state *state);
static bool route_item_is_current(
    const struct route_state *state, int index);
static void render_current_route(bool transition);
static void play_wheel_feedback(long button);
#ifdef SIMULATOR
static void activate_selected(void);
#endif
static void begin_music_scan(void);
static void close_product(void);
static bool modal_prompt_visible(void)
{
    return crazypod_system_prompts_usb_visible() ||
        crazypod_system_prompts_power_visible();
}
static void set_cpu_boost(bool enabled)
{
    if(cpu_is_boosted == enabled)
        return;
    cpu_boost(enabled);
    cpu_is_boosted = enabled;
}
static void keep_cpu_boosted(int ticks)
{
    long deadline = current_tick + ticks;

    set_cpu_boost(true);
    if(TIME_AFTER(deadline, boost_until))
        boost_until = deadline;
}
static void refresh_lock_clock(void)
{
    crazypod_lock_screen_refresh_clock();
}
static void refresh_lock_appearance(void)
{
    crazypod_lock_screen_refresh_appearance();
}
static void show_lock_screen(bool turn_display_off)
{
    crazypod_lock_screen_show(turn_display_off);
}

static void process_lock_state(void)
{
    crazypod_lock_screen_process();
}

static long main_button_base(long button)
{
    return button & BUTTON_MAIN;
}

static bool handle_lock_button(long button, intptr_t data)
{
    return crazypod_lock_screen_handle_button(button, data);
}

static void update_status_bars(lv_timer_t *timer)
{
    (void)timer;
    if(!is_backlight_on(true))
        return;
    crazypod_status_bars_update();
    if(crazypod_lock_screen_is_locked())
        refresh_lock_clock();
}

static int appearance_tile_size(void)
{
    static const int sizes[] = { 88, 96, 104, 112, 120 };
    return sizes[crazypod_appearance_get()->icon_scale];
}

static long ui_now(void)
{
    return current_tick;
}

static void configure_composition(void)
{
    const struct crazypod_composition_host host = {
        .metadata_font = CRAZYPOD_METADATA_FONT,
        .now = ui_now,
        .render = render_current_route,
        .item_count = route_item_count,
        .item_title = route_item_title,
        .item_is_current = route_item_is_current,
        .render_artwork = crazypod_artwork_widget_create,
        .boost = keep_cpu_boosted,
        .set_boost = set_cpu_boost,
        .close_product = close_product,
        .refresh_menu_rows = crazypod_menu_rows_refresh,
        .begin_music_scan = begin_music_scan,
        .show_lock = show_lock_screen,
    };

    crazypod_composition_configure(&host);
    crazypod_runtime_services_configure(render_current_route);
}

static lv_obj_t *create_boot_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *logo;

    crazypod_ui_widget_make_plain(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    logo = lv_image_create(screen);
    lv_image_set_src(logo, crazypod_lcd_boot_logo_image());
    lv_obj_center(logo);
    lv_obj_remove_flag(logo, LV_OBJ_FLAG_CLICKABLE);
    return screen;
}

static void lock_screen_unlocked(void)
{
    if(crazypod_shell_product_active())
        lv_obj_invalidate(crazypod_shell_product_screen());
    else
        lv_obj_invalidate(crazypod_desktop_screen());
}

static void create_lock_screen(void)
{
    const struct crazypod_lock_screen_callbacks callbacks = {
        .play_wheel_feedback = play_wheel_feedback,
        .unlocked = lock_screen_unlocked,
        .lock_inhibited =
            crazypod_music_library_preparing_artwork,
    };
    lv_obj_t *root = crazypod_lock_screen_create(
        crazypod_desktop_screen(), &callbacks);

    crazypod_screen_corners_create(root, 2);
}

static struct route_state *current_route(void)
{
    return crazypod_ui_routes_current();
}

static int calendar_today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
           (now->tm_mon + 1) * 100 + now->tm_mday;
}

static int route_item_count(const struct route_state *state)
{
    return crazypod_route_query_item_count(
        state, crazypod_music_search_query());
}

static const char *route_item_title(const struct route_state *state, int index)
{
    return crazypod_route_query_item_title(
        state, index, crazypod_music_search_query(),
        crazypod_organizer_feature_stopwatch_running(),
        crazypod_organizer_feature_workout_running());
}

static bool route_item_is_current(const struct route_state *state, int index)
{
    return crazypod_route_query_item_is_current(state, index);
}

static void render_current_route(bool transition)
{
    crazypod_render_scheduler_reset();
    crazypod_route_renderer_render(
        current_route(), current_tick, transition);
}

 static void begin_music_scan(void)
{
    crazypod_music_library_begin(current_tick);
}

#ifdef SIMULATOR
static void begin_note_composer(uint32_t id, bool resume_draft)
{
    crazypod_route_actions_begin_note(id, resume_draft);
}
#endif

static void close_product(void)
{
    if(!crazypod_shell_product_active())
        return;
    if(crazypod_coverflow_active())
        crazypod_coverflow_leave();
    crazypod_app_launcher_cancel_pending();
    crazypod_music_library_leave(current_tick);
    crazypod_artwork_cancel_product_requests();
    if(crazypod_miniapps_feature_is_open()) {
        crazypod_miniapps_feature_reset_input();
        crazypod_miniapps_feature_close();
    }
    crazypod_choice_coordinator_dismiss(false);
    crazypod_now_playing_overlay_dismiss(false);
    crazypod_shell_close_product();
    crazypod_ui_routes_clear();
    lv_obj_invalidate(crazypod_desktop_screen());
    crazypod_desktop_native_invalidate(true);
    crazypod_desktop_set_selected(
        crazypod_desktop_selected(), false);
    lv_refr_now(NULL);
}

#ifdef SIMULATOR
static void push_route(enum crazypod_route route, int group)
{
    crazypod_route_actions_push(route, group);
}
#endif

#ifdef SIMULATOR
static void activate_selected(void)
{
    crazypod_route_actions_activate(current_tick);
}
#endif

static void update_persistent_state(lv_timer_t *timer)
{
    (void)timer;
    crazypod_state_tick();
}

void crazypod_ui_usb_prompt_init(void)
{
    crazypod_system_prompts_initialize_usb();
}

static void play_wheel_feedback(long button)
{
    long base;

    if(button == BUTTON_NONE || (button & (SYS_EVENT | BUTTON_REL)) != 0)
        return;

    base = main_button_base(button);
    if(base != BUTTON_SCROLL_FWD && base != BUTTON_SCROLL_BACK)
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

static bool handle_confirmation(const struct route_state *state)
{
    struct crazypod_notes_confirmation_result notes =
        crazypod_notes_feature_confirm(
            state, crazypod_ui_routes_depth());
    struct crazypod_books_confirmation_result books;
    struct crazypod_organizer_confirmation_result organizer;

    if(crazypod_route_actions_confirm_photos(
           state, current_tick, HZ * 6 / 5))
        return true;
    if(notes.handled) {
        if(notes.navigation ==
           CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU) {
            crazypod_ui_routes_reset(NOTES_ROUTE_MENU, -1, 0);
        }
        else if(notes.navigation ==
                CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU_SHOW_DELETED) {
            crazypod_ui_routes_reset(NOTES_ROUTE_MENU, -1, 0);
            crazypod_ui_routes_push(NOTES_ROUTE_DELETED, -1, 0);
        }
        else if(notes.navigation ==
                CRAZYPOD_NOTES_CONFIRMATION_TRUNCATE)
            crazypod_ui_routes_truncate(notes.depth);
        render_current_route(true);
        return true;
    }

    books = crazypod_books_feature_confirm(state);
    if(books.handled) {
        if(books.deleted) {
            crazypod_books_feature_invalidate_metadata();
            crazypod_app_launcher_open_books();
        }
        else
            render_current_route(false);
        return true;
    }

    organizer = crazypod_organizer_feature_confirm(
        state, current_tick, HZ, calendar_today_date());
    if(!organizer.handled)
        return false;
    if(!organizer.succeeded) {
        render_current_route(false);
        return true;
    }
    if(organizer.navigation ==
       CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_CALENDAR_DAY) {
        crazypod_route_actions_show_calendar_day(
            organizer.date);
    }
    else {
        crazypod_ui_routes_reset(WORKOUT_ROUTE_MENU, -1, 1);
        if(organizer.navigation ==
           CRAZYPOD_ORGANIZER_CONFIRMATION_SHOW_WORKOUT_HISTORY)
            crazypod_ui_routes_push(
                WORKOUT_ROUTE_HISTORY, -1, 0);
    }
    render_current_route(true);
    return true;
}

 static void configure_feature_input(void)
{
    const struct crazypod_feature_input_host host = {
        .now = ui_now,
        .render = render_current_route,
        .boost = keep_cpu_boosted,
    };

    crazypod_feature_input_configure(&host);
}

static void configure_app_input(void)
{
    const struct crazypod_app_input_host host = {
        .feature_bindings =
            crazypod_feature_input_bindings(),
        .system_events = {
            .usb_prompt_request =
                crazypod_system_prompts_show_usb,
            .usb_prompt_done =
                crazypod_system_prompts_usb_done,
            .usb_connected =
                crazypod_system_prompts_usb_connected,
            .usb_disconnected =
                crazypod_system_prompts_usb_disconnected,
            .power_off =
                crazypod_system_prompts_power_off,
            .reboot = crazypod_system_prompts_reboot,
        },
        .power_prompt_visible =
            crazypod_system_prompts_power_visible,
        .handle_power_prompt =
            crazypod_system_prompts_handle_power,
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
        .usb_prompt_visible =
            crazypod_system_prompts_usb_visible,
        .handle_usb_prompt =
            crazypod_system_prompts_handle_usb,
#else
        .usb_prompt_visible = NULL,
        .handle_usb_prompt = NULL,
#endif
        .handle_power_hold =
            crazypod_system_prompts_handle_power_hold,
        .handle_lock = handle_lock_button,
        .close_product = close_product,
        .render = render_current_route,
        .handle_confirmation = handle_confirmation,
        .previous_track = crazypod_playback_previous_or_restart,
        .toggle_playback = crazypod_playback_toggle,
        .open_now_playing =
            crazypod_app_launcher_open_now_playing,
        .begin_music_scan = begin_music_scan,
    };

    crazypod_app_input_configure(&host);
}

static void handle_button(long button, intptr_t data)
{
    crazypod_app_input_handle(button, data, current_tick);
}
static uint32_t rockbox_tick_ms(void)
{
    return (uint32_t)((current_tick * 1000L) / HZ);
}

static bool platform_capture_desktop_native(
    const lv_area_t *area)
{
    return !crazypod_lock_screen_is_locked() &&
        !crazypod_shell_product_active() && !modal_prompt_visible() &&
        area->y1 < CRAZYPOD_DESKTOP_NATIVE_BOTTOM &&
        area->y2 >= CRAZYPOD_DESKTOP_NATIVE_TOP;
}

static void platform_queue_present(
    int x, int y, int width, int height)
{
    crazypod_present_queue_rect(x, y, width, height);
}

static void process_deferred_route_render(void)
{
    crazypod_render_scheduler_service(current_tick);
}
#ifdef SIMULATOR
static bool simulator_prepare_snapshot(void)
{
    const struct crazypod_simulator_snapshot_host host = {
        .show_power_prompt =
            crazypod_system_prompts_show_power,
        .open_app = crazypod_app_launcher_open,
        .open_root_route = crazypod_app_launcher_open_root,
        .push_route = push_route, .pop_route = crazypod_route_actions_pop,
        .render = render_current_route,
        .activate_selected = activate_selected,
        .begin_note_composer = begin_note_composer,
        .show_calendar_day =
            crazypod_route_actions_show_calendar_day,
    };

    return crazypod_simulator_snapshot_prepare(&host);
}
#endif

void crazypod_ui_run(void)
{
    lv_display_t *display;
    lv_obj_t *boot_screen;
#ifdef SIMULATOR
    bool simulator_snapshot_pending;
    long simulator_snapshot_due = 0;
    long simulator_snapshot_settle = HZ / 2;
    int simulator_snapshot_stage = 0;
#endif

    lcd_set_viewport(NULL);
#ifdef HAVE_SW_POWEROFF
    /*
     * SYS_POWEROFF is a committed shutdown broadcast. CrazyPod owns the
     * Play-button hold gesture so opening its confirmation UI cannot wake and
     * reinitialize the iPod LCD through the Rockbox backlight thread.
     */
    button_set_sw_poweroff_state(false);
#endif
    crazypod_present_init(current_tick);
    crazypod_frameclock_reset(&lvgl_clock, current_tick);
    crazypod_now_capsule_reset_motion(current_tick);
    crazypod_image_init();
    crazypod_artwork_init();
    crazypod_icons_init();
    crazypod_photos_init();
    crazypod_videos_init();
    crazypod_wallpaper_init();
    crazypod_miniapps_feature_initialize_runtime();
    crazypod_miniapps_feature_initialize();

    {
        const struct crazypod_platform_display_host display_host = {
            .capture_desktop_native =
                platform_capture_desktop_native,
            .capture_flush =
                crazypod_desktop_native_capture_flush,
            .coverflow_active = crazypod_coverflow_active,
            .coverflow_invalidate =
                crazypod_coverflow_invalidate,
            .queue_present = platform_queue_present,
        };
        display = crazypod_platform_display_init(
            rockbox_tick_ms, &display_host);
    }
    font_unload_all();
    (void)crazypod_runtime_font_init();

    boot_screen = create_boot_screen();
    {
        const struct crazypod_desktop_host desktop_host = {
            .create_corner_masks = crazypod_screen_corners_create,
            .refresh_corner_masks = crazypod_screen_corners_refresh,
            .refresh_lock_appearance = refresh_lock_appearance,
        };
        (void)crazypod_desktop_create(
            current_tick, CRAZYPOD_METADATA_FONT, &desktop_host);
    }
    configure_composition();
    configure_feature_input();
    configure_app_input();
    create_lock_screen();
    update_status_bars(NULL);
    lv_timer_create(update_status_bars, 1000, NULL);
    lv_timer_create(crazypod_playback_update_timer, 250, NULL);
    lv_timer_create(update_persistent_state, 1000, NULL);

    lv_screen_load(boot_screen);
    lv_refr_now(display);
    crazypod_present_tick();
    set_cpu_boost(true);
    /* Publish Mini Apps and themes while the boot surface is visible.  Entry
     * points only read the finished registry, so opening either UI is
     * immediate.  USB disconnect performs the same explicit rescan. */
    crazypod_miniapps_feature_rescan();
    crazypod_now_playing_theme_prepare();
    crazypod_now_capsule_refresh_material();
    lv_screen_load(crazypod_desktop_screen());
    lv_refr_now(display);
    crazypod_system_prompts_set_ui_ready();
    boost_until = current_tick + HZ / 2;
    crazypod_playback_initialize();
    crazypod_now_playing_navigation_initialize();
    crazypod_now_capsule_initialize_artwork();
    crazypod_photos_feature_initialize_media();
    crazypod_customize_feature_initialize_media();
#ifdef SIMULATOR
    simulator_snapshot_pending =
        getenv("CRAZYPOD_SIM_DUMP") != NULL;
    if(simulator_snapshot_pending) {
        simulator_snapshot_settle =
            crazypod_simulator_snapshot_settle_ticks();
        simulator_snapshot_due = current_tick + HZ / 2;
    }
#endif
    crazypod_music_library_initialize(current_tick);
    crazypod_lock_screen_initialize_backlight_state();
#if defined(SIMULATOR) || \
    defined(CRAZYPOD_REPRO_DIAGNOSTICS)
    (void)crazypod_miniapp_repro_start(
        current_tick);
#endif
    while(true) {
        long button;
        bool locked;
        int drained = 0;
        int wait_ticks;
        /* Once mass storage is acknowledged, the host owns the filesystem.
         * The UI thread must not run timers, services, rendering or accept
         * local actions until USB broadcasts disconnect. */
        if(crazypod_system_prompts_storage_active()) {
            button = button_get_w_tmo(HZ);
            if(button == SYS_USB_DISCONNECTED)
                handle_button(button, button_get_data());
            if(crazypod_system_prompts_storage_active()) {
                set_cpu_boost(false);
                continue;
            }
        }
        process_lock_state();
        wait_ticks = crazypod_runtime_services_wait_ticks();
        {
            int input_wait =
                crazypod_app_input_wait_ticks(current_tick);

            if(input_wait < wait_ticks)
                wait_ticks = input_wait;
        }
        wait_ticks = MIN(
            wait_ticks,
            crazypod_render_scheduler_wait_ticks(current_tick));
#if defined(SIMULATOR) || \
    defined(CRAZYPOD_REPRO_DIAGNOSTICS)
        {
            int repro_wait =
                crazypod_miniapp_repro_wait_ticks();

            if(repro_wait < wait_ticks)
                wait_ticks = repro_wait;
        }
#endif
        button = button_get_w_tmo(wait_ticks);
        process_lock_state();
        while(button != BUTTON_NONE && drained < 16) {
            intptr_t data = button_get_data();
            handle_button(button, data);
            ++drained;
            if(crazypod_system_prompts_storage_active())
                break;
            button = button_get_w_tmo(0);
        }
        if(crazypod_system_prompts_storage_active()) {
            set_cpu_boost(false);
            continue;
        }
        process_lock_state();
        locked = crazypod_lock_screen_is_locked();
        crazypod_app_input_tick(current_tick, locked);
        crazypod_alpha_jump_hud_tick(
            current_tick,
            !locked && crazypod_shell_product_active());
        crazypod_runtime_services_tick(
            current_tick,
            crazypod_frameclock_due(&lvgl_clock, current_tick),
            locked);
        if(!locked) {
            process_deferred_route_render();
            crazypod_playback_warm_album_flow(
                current_tick, false);
            crazypod_playback_process_artwork();
            crazypod_playback_process_media();
        }
        crazypod_now_capsule_tick(
            current_tick, !locked && !crazypod_shell_product_active() &&
            !modal_prompt_visible());
        if(!locked) {
            crazypod_playback_tick_wave(current_tick);
        }
        if(crazypod_frameclock_due(&lvgl_clock, current_tick)) {
            lv_timer_handler();
            crazypod_frameclock_schedule_next(&lvgl_clock, current_tick);
        }
        if(!locked) {
            int coverflow_feedback;

            crazypod_desktop_render_icon(
                appearance_tile_size(),
                crazypod_shell_product_active() || modal_prompt_visible());
            crazypod_coverflow_tick();
            coverflow_feedback =
                crazypod_coverflow_take_wheel_feedback();
            if(coverflow_feedback != 0)
                play_wheel_feedback(
                    coverflow_feedback < 0
                        ? BUTTON_SCROLL_BACK
                        : BUTTON_SCROLL_FWD);
            crazypod_playback_sync_album_flow();
        }
        crazypod_present_tick();
#if defined(CRAZYPOD_REPRO_DIAGNOSTICS) && \
    !defined(SIMULATOR)
        crazypod_miniapp_repro_service(
            current_tick);
        if(crazypod_miniapp_repro_cpu_boost_requested())
            keep_cpu_boosted(HZ / 10);
#endif
#ifdef SIMULATOR
        crazypod_miniapp_repro_service(
            current_tick);
        if(simulator_snapshot_pending &&
           !TIME_BEFORE(current_tick, simulator_snapshot_due)) {
            if(simulator_snapshot_stage == 0) {
                simulator_snapshot_pending =
                    simulator_prepare_snapshot();
                simulator_snapshot_stage = 1;
                simulator_snapshot_due =
                    current_tick + simulator_snapshot_settle;
            }
            else {
                lv_refr_now(display);
                crazypod_present_tick();
                screen_dump();
                simulator_snapshot_pending = false;
                if(getenv(
                       "CRAZYPOD_SIM_EXIT_AFTER_DUMP") != NULL)
                    exit(0);
            }
        }
#endif
        if(crazypod_artwork_busy() || crazypod_photos_busy() ||
           crazypod_videos_busy())
            keep_cpu_boosted(HZ / 10);
        if(crazypod_music_is_scanning() && !locked)
            keep_cpu_boosted(HZ / 10);
        if(!(locked
             ? crazypod_lock_screen_motion_active()
             : (lv_anim_count_running() ||
                crazypod_coverflow_motion_active())) &&
           (!crazypod_music_is_scanning() || locked) &&
           !crazypod_artwork_busy() &&
           !crazypod_photos_busy() &&
           !crazypod_videos_busy() &&
           !TIME_BEFORE(current_tick, boost_until))
            set_cpu_boost(false);
    }
}

#endif
