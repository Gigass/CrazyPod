#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "button.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_runtime_controller.h"
#include "crazypod_miniapps.h"

static bool app_open;
static bool app_handles_event;
static bool close_requested;
static bool ui_refresh_requested;
static int wake_count;
static int boost_count;
static int close_count;
static int event_count;
static int reset_count;
static int wheel_count;
static int render_count;

bool crazypod_miniapps_is_open(void)
{
    return app_open;
}

bool crazypod_miniapps_event(const struct cp_input_event *event)
{
    assert(event != NULL);
    ++event_count;
    return app_handles_event;
}

bool crazypod_miniapps_take_close_request(void)
{
    bool requested = close_requested;

    close_requested = false;
    return requested;
}

bool crazypod_miniapps_take_ui_refresh(void)
{
    bool requested = ui_refresh_requested;

    ui_refresh_requested = false;
    return requested;
}

void crazypod_miniapp_runtime_reset_input(void)
{
    ++reset_count;
}

void crazypod_miniapp_runtime_push_wheel(
    const struct cp_input_event *event)
{
    assert(event != NULL);
    ++wheel_count;
}

void crazypod_miniapp_runtime_request_render(void)
{
    ++render_count;
}

int crazypod_input_wheel_steps(
    const struct crazypod_input_event *event, int maximum)
{
    (void)event;
    (void)maximum;
    return 1;
}

static void wake_display(void)
{
    ++wake_count;
}

static void keep_boosted(int ticks)
{
    assert(ticks == 10);
    ++boost_count;
}

static void close_app(void)
{
    ++close_count;
}

static void reset_test(void)
{
    app_open = false;
    app_handles_event = false;
    close_requested = false;
    ui_refresh_requested = false;
    wake_count = 0;
    boost_count = 0;
    close_count = 0;
    event_count = 0;
    reset_count = 0;
    wheel_count = 0;
    render_count = 0;
}

static struct crazypod_input_event input_event(long base)
{
    struct crazypod_input_event event;

    memset(&event, 0, sizeof(event));
    event.base = base;
    return event;
}

int main(void)
{
    const struct crazypod_miniapp_input_actions actions = {
        .wake_display = wake_display,
        .keep_boosted = keep_boosted,
        .close = close_app,
    };
    struct crazypod_input_event event = input_event(BUTTON_SELECT);

    reset_test();
    assert(!crazypod_miniapp_input_handle(&event, 10, &actions));
    assert(wake_count == 0);
    assert(event_count == 0);
    assert(reset_count == 0);

    app_open = true;
    app_handles_event = true;
    assert(crazypod_miniapp_input_handle(&event, 10, &actions));
    assert(wake_count == 1);
    assert(event_count == 1);
    assert(reset_count == 1);
    assert(render_count == 1);
    assert(boost_count == 1);

    event = input_event(BUTTON_SCROLL_FWD);
    assert(crazypod_miniapp_input_handle(&event, 10, &actions));
    assert(wheel_count == 1);
    assert(boost_count == 2);

    event = input_event(BUTTON_MENU);
    app_handles_event = false;
    assert(crazypod_miniapp_input_handle(&event, 10, &actions));
    assert(close_count == 1);
    return 0;
}
