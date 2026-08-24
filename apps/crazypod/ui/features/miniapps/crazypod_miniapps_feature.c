#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#ifdef SIMULATOR
#include "button.h"
#include <stdlib.h>
#endif

#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_runtime_controller.h"
#include "crazypod_miniapp_scene.h"
#include "../../../miniapps/runtime/crazypod_miniapp_text_prompt_service.h"
#ifdef SIMULATOR
#endif
#include "crazypod_miniapp_screen.h"
#include "crazypod_miniapps_feature.h"

#define MINIAPP_MENU_HOLD_MS 900

int crazypod_miniapps_feature_item_count(
    const struct route_state *state)
{
    if(state->route == UTILITIES_ROUTE_MENU)
        return crazypod_miniapps_count();
    return state->route == MINIAPP_ROUTE_VIEW ? 1 : 0;
}

const char *crazypod_miniapps_feature_title(
    const struct route_state *state)
{
    if(state->route == MINIAPP_ROUTE_VIEW) {
        const struct crazypod_miniapp_metadata *metadata =
            crazypod_miniapps_metadata(state->group);

        const char *name =
            metadata != NULL ? metadata->name : NULL;

        return name != NULL ? name : CP_TR("MINI APP");
    }
    return CP_TR("MINI APPS");
}

bool crazypod_miniapps_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    const struct crazypod_miniapp_metadata *metadata =
        crazypod_miniapps_metadata(
            state->route == MINIAPP_ROUTE_VIEW
                ? state->group : index);

    *title = metadata != NULL
        ? metadata->name
        : state->route == MINIAPP_ROUTE_VIEW ? CP_TR("Mini App") : "";
    return state->route == UTILITIES_ROUTE_MENU ||
        state->route == MINIAPP_ROUTE_VIEW;
}

enum crazypod_menu_icon crazypod_miniapps_feature_item_icon(
    const struct route_state *state, int index)
{
    return state->route == UTILITIES_ROUTE_MENU && index >= 0
        ? CRAZYPOD_MENU_ICON_APPS
        : CRAZYPOD_MENU_ICON_NONE;
}

bool crazypod_miniapps_feature_render(
    const struct route_state *state, lv_obj_t *parent,
    uint32_t primary_color)
{
    if(state->route != MINIAPP_ROUTE_VIEW)
        return false;
    crazypod_miniapp_screen_render(parent, primary_color);
    return true;
}

void crazypod_miniapps_feature_render_active(
    lv_obj_t *parent, uint32_t primary_color)
{
    crazypod_miniapp_screen_render(parent, primary_color);
}

void crazypod_miniapps_feature_note_opened(void)
{
    crazypod_miniapp_runtime_opened();
}

void crazypod_miniapps_feature_push_wheel(
    const struct cp_input_event *event)
{
    crazypod_miniapp_runtime_push_wheel(event);
}

void crazypod_miniapps_feature_push_wheel_coalesced(
    const struct cp_input_event *event)
{
    crazypod_miniapp_runtime_push_wheel_coalesced(event);
}

bool crazypod_miniapps_feature_modal_visible(void)
{
    return crazypod_miniapp_scene_modal_visible() ||
        crazypod_miniapp_text_prompt_visible();
}

bool crazypod_miniapps_feature_surface_attached(lv_obj_t *parent)
{
    return crazypod_miniapp_screen_attached(parent);
}

void crazypod_miniapps_feature_initialize(void)
{
    const struct crazypod_miniapp_ui_host ui_host = {
        .begin_update = crazypod_miniapp_scene_begin_update,
        .create = crazypod_miniapp_scene_create,
        .insert = crazypod_miniapp_scene_insert,
        .set_i32 = crazypod_miniapp_scene_set_i32,
        .set_color = crazypod_miniapp_scene_set_color,
        .set_string = crazypod_miniapp_scene_set_string,
        .set_bytes = crazypod_miniapp_scene_set_bytes,
        .listen = crazypod_miniapp_scene_listen,
        .animate = crazypod_miniapp_scene_animate,
        .commit_drawing =
            crazypod_miniapp_scene_commit_drawing,
        .remove = crazypod_miniapp_scene_remove,
        .end_update = crazypod_miniapp_scene_end_update,
        .handle_count = crazypod_miniapp_scene_handle_count,
        .handle_high_water =
            crazypod_miniapp_scene_handle_high_water,
        .reset_handle_high_water =
            crazypod_miniapp_scene_reset_handle_high_water,
        .input = crazypod_miniapp_scene_input,
        .reset = crazypod_miniapp_scene_reset,
    };

    crazypod_miniapps_set_ui_host(&ui_host);
    crazypod_miniapp_screen_reset();
    crazypod_miniapp_input_reset_state();
}

bool crazypod_miniapps_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    bool was_holding = crazypod_miniapp_input_motion_active();
    bool handled;
    const struct crazypod_miniapp_input_actions actions = {
        .wake_display = context->wake_display,
        .keep_boosted = context->boost,
        .close = context->pop,
    };

    (void)state;
    handled = crazypod_miniapp_input_handle(
        event, context->now,
        context->ticks_per_second * MINIAPP_MENU_HOLD_MS / 1000,
        context->ticks_per_second / 10,
        &actions);
    if(!was_holding && crazypod_miniapp_input_motion_active())
        crazypod_miniapp_screen_begin_menu_hold(
            MINIAPP_MENU_HOLD_MS);
    else if(was_holding &&
            !crazypod_miniapp_input_motion_active())
        crazypod_miniapp_screen_cancel_menu_hold();
    return handled;
}

int crazypod_miniapps_feature_service(
    bool active, bool frame_due, long now,
    long ticks_per_second)
{
    int events = CRAZYPOD_MINIAPPS_SERVICE_NONE;
    bool was_holding = crazypod_miniapp_input_motion_active();

    crazypod_miniapp_input_service(active, now);
    if(was_holding &&
       !crazypod_miniapp_input_motion_active())
        crazypod_miniapp_screen_cancel_menu_hold();
    if(crazypod_miniapp_runtime_service(
           active, frame_due, now, ticks_per_second))
        events |= CRAZYPOD_MINIAPPS_SERVICE_CLOSE;
    if(active && crazypod_miniapp_runtime_take_render())
        events |= CRAZYPOD_MINIAPPS_SERVICE_RENDER;
    return events;
}

bool crazypod_miniapps_feature_is_open(void)
{
    return crazypod_miniapps_is_open();
}

bool crazypod_miniapps_feature_motion_active(void)
{
    return crazypod_miniapps_is_open() &&
        (crazypod_miniapp_runtime_motion_active() ||
         crazypod_miniapp_input_motion_active());
}

void crazypod_miniapps_feature_close(void)
{
    crazypod_miniapps_close();
}

void crazypod_miniapps_feature_reset_input(void)
{
    crazypod_miniapp_screen_cancel_menu_hold();
    crazypod_miniapp_runtime_reset_input();
    crazypod_miniapp_input_reset_state();
}

void crazypod_miniapps_feature_rescan(void)
{
    crazypod_miniapp_runtime_rescan();
}

void crazypod_miniapps_feature_request_rescan(void)
{
    crazypod_miniapp_runtime_request_rescan();
}

void crazypod_miniapps_feature_service_rescan(void)
{
    crazypod_miniapp_runtime_service_rescan();
}

bool crazypod_miniapps_feature_rescan_pending(void)
{
    return crazypod_miniapp_runtime_rescan_pending();
}

int crazypod_miniapps_feature_last_error(void)
{
    return crazypod_miniapp_runtime_last_error();
}

void crazypod_miniapps_feature_initialize_runtime(void)
{
    crazypod_miniapp_runtime_initialize();
}

int crazypod_miniapps_feature_prepare(void)
{
    return crazypod_miniapp_runtime_prepare();
}

bool crazypod_miniapps_feature_activate(
    const struct route_state *state,
    const struct crazypod_miniapps_activation_host *host)
{
    const struct crazypod_miniapp_activation_host internal = {
        .push = host->push,
        .render = host->render,
    };

    return crazypod_miniapp_runtime_activate(
        state, &internal);
}

unsigned crazypod_miniapps_feature_input_count(void)
{
    return crazypod_miniapp_runtime_input_count();
}

bool crazypod_miniapps_feature_exit_prompt_visible(void)
{
    return crazypod_miniapp_input_exit_prompt_visible();
}

bool crazypod_miniapps_feature_has_scene_content(void)
{
    return crazypod_miniapp_scene_has_content();
}

void crazypod_miniapps_feature_refresh_now_playing_artwork(void)
{
    (void)crazypod_miniapp_scene_refresh_now_playing_artwork();
}

#ifdef SIMULATOR
static void simulator_input_noop(void)
{
}

static void simulator_keep_boosted(int ticks)
{
    (void)ticks;
}

bool crazypod_miniapps_feature_simulate_long_menu(
    long now, long ticks_per_second)
{
    long hold_ticks =
        ticks_per_second * MINIAPP_MENU_HOLD_MS / 1000;
    const struct crazypod_miniapp_input_actions actions = {
        .wake_display = simulator_input_noop,
        .keep_boosted = simulator_keep_boosted,
        .close = simulator_input_noop,
    };
    const struct crazypod_input_event event = {
        .base = BUTTON_MENU,
    };

    if(hold_ticks < 1)
        hold_ticks = 1;
    if(!crazypod_miniapp_input_handle(
           &event, now, hold_ticks,
           ticks_per_second / 10, &actions))
        return false;
    crazypod_miniapp_input_service(
        true, now + hold_ticks);
    return crazypod_miniapp_input_exit_prompt_visible();
}
#endif

#endif
