#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "crazypod_miniapp_activation.h"
#include "crazypod_miniapp_runtime_controller.h"
#include "crazypod_miniapps.h"

static bool app_open;
static bool ui_refresh_requested;
static bool refresh_on_event;
static bool refresh_on_tick;
static bool close_on_event;
static int event_count;
static int tick_count;
static int push_count;
static int render_count;
static int rescan_begin_count;
static int rescan_step_count;
static bool rescan_active;
static uint8_t last_event_steps;

int crazypod_miniapps_init(void)
{
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapps_prepare(void)
{
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapps_rescan(void)
{
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapps_rescan_begin(void)
{
    ++rescan_begin_count;
    rescan_active = true;
    return CRAZYPOD_MINIAPP_OK;
}

bool crazypod_miniapps_rescan_step(void)
{
    assert(rescan_active);
    ++rescan_step_count;
    if(rescan_step_count == 2)
        rescan_active = false;
    return rescan_active;
}

bool crazypod_miniapps_rescan_active(void)
{
    return rescan_active;
}

int crazypod_miniapps_rescan_result(void)
{
    return rescan_active
        ? CRAZYPOD_MINIAPP_ERROR_BUSY : CRAZYPOD_MINIAPP_OK;
}

bool crazypod_miniapps_is_open(void)
{
    return app_open;
}

bool crazypod_miniapps_event(const struct cp_input_event *event)
{
    assert(event != NULL);
    ++event_count;
    last_event_steps = event->steps;
    if(refresh_on_event)
        ui_refresh_requested = true;
    if(close_on_event)
        app_open = false;
    return false;
}

bool crazypod_miniapps_tick(void)
{
    ++tick_count;
    if(refresh_on_tick)
        ui_refresh_requested = true;
    return false;
}

bool crazypod_miniapps_take_ui_refresh(void)
{
    bool requested = ui_refresh_requested;

    ui_refresh_requested = false;
    return requested;
}

bool crazypod_miniapps_has_scheduled_work(void)
{
    return false;
}

struct crazypod_miniapp_activation_result
crazypod_miniapp_activation_execute(
    enum crazypod_route route, int selected)
{
    struct crazypod_miniapp_activation_result result = {
        .handled = route == UTILITIES_ROUTE_MENU,
        .opened = false,
        .selected = selected,
        .error = CRAZYPOD_MINIAPP_OK,
    };

    return result;
}

static void push_route(enum crazypod_route route, int group)
{
    (void)route;
    (void)group;
    ++push_count;
}

static void render_route(bool transition)
{
    (void)transition;
    ++render_count;
}

static void reset_test(void)
{
    app_open = true;
    ui_refresh_requested = false;
    refresh_on_event = false;
    refresh_on_tick = false;
    close_on_event = false;
    event_count = 0;
    tick_count = 0;
    push_count = 0;
    render_count = 0;
    last_event_steps = 0;
    crazypod_miniapp_runtime_opened();
}

int main(void)
{
    const struct cp_input_event wheel = {
        .struct_size = sizeof(wheel),
        .type = CP_INPUT_WHEEL_CLOCKWISE,
        .steps = 1,
    };
    const struct crazypod_miniapp_activation_host host = {
        .push = push_route,
        .render = render_route,
    };
    const struct route_state route = {
        .route = UTILITIES_ROUTE_MENU,
        .selected = 0,
    };

    crazypod_miniapp_runtime_initialize();
    assert(crazypod_miniapp_runtime_last_error() ==
           CRAZYPOD_MINIAPP_OK);

    crazypod_miniapp_runtime_request_rescan();
    assert(crazypod_miniapp_runtime_rescan_pending());
    assert(crazypod_miniapp_runtime_prepare() ==
           CRAZYPOD_MINIAPP_ERROR_BUSY);
    crazypod_miniapp_runtime_service_rescan();
    assert(rescan_begin_count == 1);
    assert(rescan_step_count == 1);
    assert(crazypod_miniapp_runtime_rescan_pending());
    crazypod_miniapp_runtime_service_rescan();
    assert(rescan_step_count == 2);
    assert(!crazypod_miniapp_runtime_rescan_pending());
    assert(crazypod_miniapp_runtime_last_error() ==
           CRAZYPOD_MINIAPP_OK);

    reset_test();
    refresh_on_event = true;
    crazypod_miniapp_runtime_push_wheel_coalesced(&wheel);
    {
        struct cp_input_event accelerated = wheel;

        accelerated.steps = 4;
        accelerated.repeated = 1;
        crazypod_miniapp_runtime_push_wheel_coalesced(&accelerated);
    }
    assert(crazypod_miniapp_runtime_service(
        true, true, 10, 100) == false);
    assert(event_count == 1);
    assert(last_event_steps == 5);
    assert(tick_count == 1);
    assert(!crazypod_miniapp_runtime_take_render());

    refresh_on_event = false;
    refresh_on_tick = true;
    assert(crazypod_miniapp_runtime_service(
        true, true, 20, 100) == false);
    assert(tick_count == 2);
    assert(!crazypod_miniapp_runtime_take_render());

    refresh_on_tick = false;
    close_on_event = true;
    crazypod_miniapp_runtime_push_wheel(&wheel);
    assert(crazypod_miniapp_runtime_service(
        true, true, 30, 100));
    assert(!app_open);

    assert(crazypod_miniapp_runtime_activate(&route, &host));
    assert(push_count == 0);
    assert(render_count == 1);
    return 0;
}
