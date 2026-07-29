#include "config.h"

#ifdef IPOD_6G

#include "kernel.h"
#include "misc.h"
#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
#include "piezo.h"
#endif
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../shell/crazypod_shell.h"
#include "../shell/crazypod_system_prompts.h"
#include "crazypod_route_actions.h"
#include "crazypod_runtime_services.h"

static void (*render_route)(bool transition);

void crazypod_runtime_services_configure(
    void (*render)(bool transition))
{
    render_route = render;
}

static void service_miniapp(
    bool active, long now, bool frame_due)
{
    int events = crazypod_miniapps_feature_service(
        active, frame_due, now, HZ);

    if((events & CRAZYPOD_MINIAPPS_SERVICE_CLOSE) != 0) {
        crazypod_route_actions_pop();
        return;
    }
    if((events & CRAZYPOD_MINIAPPS_SERVICE_BEEP) != 0) {
#if defined(HAVE_HARDWARE_CLICK) && !defined(SIMULATOR)
        piezo_button_beep(false, false);
#endif
        beep_play(1568, 90, 6000);
    }
    if((events & CRAZYPOD_MINIAPPS_SERVICE_RENDER) != 0)
        render_route(false);
}

void crazypod_runtime_services_tick(
    long now, bool frame_due, bool locked)
{
    struct route_state *state = crazypod_ui_routes_current();
    bool routed = crazypod_shell_product_active() &&
        crazypod_ui_routes_depth() > 0;
    bool miniapp_active = !locked && routed &&
        state->route == MINIAPP_ROUTE_VIEW &&
        crazypod_miniapps_feature_is_open();

    if(!locked && routed)
        crazypod_photos_runtime_service(state, now);
    if(!locked)
        crazypod_wallpaper_crop_runtime_service(now);
    crazypod_music_library_service(
        now, crazypod_system_prompts_storage_active());
    crazypod_route_actions_service_notes();
    if(routed && crazypod_organizer_feature_service(
           state->route, now, HZ))
        render_route(false);
    service_miniapp(miniapp_active, now, frame_due);
}

#endif
