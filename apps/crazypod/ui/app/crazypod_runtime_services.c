#include "config.h"

#ifdef IPOD_6G

#include "audio.h"
#include "backlight.h"
#include "kernel.h"
#include "lvgl.h"
#include "../../crazypod_artwork.h"
#include "../../crazypod_coverflow.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_music.h"
#include "../../crazypod_photos.h"
#include "../../crazypod_videos.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../shell/crazypod_desktop.h"
#include "../shell/crazypod_lock_screen.h"
#include "../shell/crazypod_shell.h"
#include "../shell/crazypod_system_prompts.h"
#include "crazypod_route_actions.h"
#include "crazypod_miniapp_repro.h"
#include "crazypod_runtime_services.h"

#if defined(SIMULATOR) || \
    defined(CRAZYPOD_REPRO_DIAGNOSTICS)
#define REPRO_MARK(PHASE, VALUE) \
    crazypod_miniapp_repro_trace_marker((PHASE), (VALUE))
#else
#define REPRO_MARK(PHASE, VALUE) do { } while(0)
#endif

static void (*render_route)(bool transition);

#define CRAZYPOD_STATIC_WAIT_TICKS \
    (HZ > 0 ? HZ : 1)
#define CRAZYPOD_CLOCK_WAIT_TICKS \
    ((HZ / 4) > 0 ? (HZ / 4) : 1)
#define CRAZYPOD_PLAYBACK_WAIT_TICKS \
    ((HZ / 10) > 0 ? (HZ / 10) : 1)
#define CRAZYPOD_MEDIA_WAIT_TICKS \
    ((HZ / 20) > 0 ? (HZ / 20) : 1)
#define CRAZYPOD_SCREEN_OFF_WAIT_TICKS \
    ((HZ / 2) > 0 ? (HZ / 2) : 1)
#define CRAZYPOD_FRAME_WAIT_TICKS \
    ((HZ / CRAZYPOD_TARGET_FPS) > 0 \
        ? (HZ / CRAZYPOD_TARGET_FPS) : 1)

static int active_route_wait_ticks(void)
{
    enum crazypod_route route;

    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0)
        return CRAZYPOD_STATIC_WAIT_TICKS;
    route = crazypod_ui_routes_current()->route;
    if(route == CLOCK_ROUTE_VIEW)
        return CRAZYPOD_CLOCK_WAIT_TICKS;
    if(route == STOPWATCH_ROUTE_VIEW ||
       route == WORKOUT_ROUTE_ACTIVE)
        return CRAZYPOD_PLAYBACK_WAIT_TICKS;
    return CRAZYPOD_STATIC_WAIT_TICKS;
}

void crazypod_runtime_services_configure(
    void (*render)(bool transition))
{
    render_route = render;
}

int crazypod_runtime_services_wait_ticks(void)
{
    int status;

    if(crazypod_lock_screen_is_locked()) {
        if(!is_backlight_on(true))
            return CRAZYPOD_SCREEN_OFF_WAIT_TICKS;
        return crazypod_lock_screen_motion_active()
            ? CRAZYPOD_FRAME_WAIT_TICKS
            : CRAZYPOD_STATIC_WAIT_TICKS;
    }
    if(crazypod_miniapps_feature_motion_active() ||
       lv_anim_count_running() ||
       crazypod_desktop_motion_active() ||
       crazypod_coverflow_motion_active())
        return CRAZYPOD_FRAME_WAIT_TICKS;
    if(crazypod_artwork_busy() || crazypod_photos_busy() ||
       crazypod_videos_busy())
        return CRAZYPOD_MEDIA_WAIT_TICKS;
    if(crazypod_music_is_scanning() ||
       crazypod_music_catalog_validation() ==
           CRAZYPOD_MUSIC_VALIDATION_RUNNING)
        return CRAZYPOD_CLOCK_WAIT_TICKS;
    status = audio_status();
    if((status & AUDIO_STATUS_PLAY) != 0 &&
       (status & AUDIO_STATUS_PAUSE) == 0)
        return CRAZYPOD_PLAYBACK_WAIT_TICKS;
    return active_route_wait_ticks();
}

static void service_miniapp(
    bool active, long now, bool frame_due)
{
    int events;
#if defined(SIMULATOR) || \
    defined(CRAZYPOD_REPRO_DIAGNOSTICS)
    long started = current_tick;
#endif

    events = crazypod_miniapps_feature_service(
        active, frame_due, now, HZ);
#if defined(SIMULATOR) || \
    defined(CRAZYPOD_REPRO_DIAGNOSTICS)
    if(current_tick != started)
        REPRO_MARK(
            "miniapp-service-duration",
            current_tick - started);
#endif

    if((events & CRAZYPOD_MINIAPPS_SERVICE_CLOSE) != 0) {
        crazypod_route_actions_pop();
        return;
    }
    if((events & CRAZYPOD_MINIAPPS_SERVICE_RENDER) != 0) {
#if defined(SIMULATOR) || \
    defined(CRAZYPOD_REPRO_DIAGNOSTICS)
        started = current_tick;
#endif
        render_route(false);
#if defined(SIMULATOR) || \
    defined(CRAZYPOD_REPRO_DIAGNOSTICS)
        if(current_tick != started)
            REPRO_MARK(
                "miniapp-render-duration",
                current_tick - started);
#endif
    }
}

void crazypod_runtime_services_tick(
    long now, bool frame_due, bool locked)
{
    struct route_state *state = crazypod_ui_routes_current();
    bool routed = crazypod_shell_product_active() &&
        crazypod_ui_routes_depth() > 0;
    bool photos_route = routed &&
        ((state->route >= PHOTOS_ROUTE_MENU &&
          state->route <= PHOTOS_ROUTE_DETAIL) ||
         state->route == DIY_ROUTE_WALLPAPER_FILES ||
         state->route == DIY_ROUTE_WALLPAPER_CROP);
    bool videos_route = routed &&
        state->route >= PHOTOS_ROUTE_MENU &&
        state->route <= PHOTOS_ROUTE_DETAIL;
    bool miniapp_active = !locked && routed &&
        ((state->route == MINIAPP_ROUTE_VIEW &&
          crazypod_miniapps_feature_is_open()) ||
         (state->route == MUSIC_ROUTE_NOW_PLAYING &&
          crazypod_now_playing_theme_open()));

    crazypod_music_set_scan_suspended(locked);
    crazypod_artwork_set_lock_suspended(locked);
    crazypod_photos_set_lock_suspended(locked);
    crazypod_videos_set_lock_suspended(locked);
    crazypod_photos_set_route_suspended(!photos_route);
    crazypod_videos_set_route_suspended(!videos_route);
    if(!locked) {
        if(videos_route) {
            crazypod_photos_ensure_catalog();
            crazypod_videos_ensure_catalog();
        }
        else if(photos_route)
            crazypod_photos_ensure_catalog();
    }
    if(!locked && routed)
        crazypod_photos_runtime_service(state, now);
    if(!locked && routed &&
       crazypod_photos_feature_service_feedback(now))
        render_route(false);
    if(!locked)
        crazypod_wallpaper_crop_runtime_service(now);
    crazypod_music_library_service(
        now, locked ||
             crazypod_system_prompts_storage_active());
    crazypod_route_actions_service_notes();
    if(!locked && routed && crazypod_organizer_feature_service(
           state->route, now, HZ))
        render_route(false);
    if(miniapp_active && crazypod_now_playing_theme_open()) {
        crazypod_now_playing_artwork_sync();
        crazypod_miniapps_feature_refresh_now_playing_artwork();
    }
    service_miniapp(miniapp_active, now, frame_due);
}

#endif
