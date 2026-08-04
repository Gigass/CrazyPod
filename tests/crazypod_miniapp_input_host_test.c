#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "button.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_runtime_controller.h"
#include "crazypod_miniapps.h"

static bool app_open;
static bool app_handles_event;
static bool ui_refresh_requested;
static int wake_count;
static int boost_count;
static int close_count;
static int event_count;
static int reset_count;
static int wheel_count;
static int render_count;
static enum cp_input_type last_event_type;

bool crazypod_miniapp_text_prompt_visible(void)
{
    return false;
}

void crazypod_miniapp_text_prompt_move(int delta)
{
    (void)delta;
    assert(false);
}

void crazypod_miniapp_text_prompt_select(void)
{
    assert(false);
}

void crazypod_miniapp_text_prompt_cancel(void)
{
    assert(false);
}

bool crazypod_miniapps_is_open(void)
{
    return app_open;
}

bool crazypod_miniapps_event(const struct cp_input_event *event)
{
    assert(event != NULL);
    ++event_count;
    last_event_type = (enum cp_input_type)event->type;
    return app_handles_event;
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
    ui_refresh_requested = false;
    wake_count = 0;
    boost_count = 0;
    close_count = 0;
    event_count = 0;
    reset_count = 0;
    wheel_count = 0;
    render_count = 0;
    last_event_type = CP_INPUT_WHEEL_CLOCKWISE;
    crazypod_miniapp_input_reset_state();
}

static struct crazypod_input_event input_event(long base)
{
    struct crazypod_input_event event;

    memset(&event, 0, sizeof(event));
    event.base = base;
    return event;
}

static bool handle(
    struct crazypod_input_event *event, long now,
    const struct crazypod_miniapp_input_actions *actions)
{
    return crazypod_miniapp_input_handle(
        event, now, 50, 10, actions);
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
    assert(!handle(&event, 10, &actions));
    assert(wake_count == 0);
    assert(event_count == 0);
    assert(reset_count == 0);

    app_open = true;
    app_handles_event = true;
    ui_refresh_requested = true;
    assert(handle(&event, 10, &actions));
    assert(wake_count == 1);
    assert(event_count == 1);
    assert(reset_count == 1);
    assert(render_count == 0);
    assert(boost_count == 1);

    event = input_event(BUTTON_SCROLL_FWD);
    assert(handle(&event, 20, &actions));
    assert(wheel_count == 1);
    assert(boost_count == 2);

    reset_test();
    app_open = true;
    event = input_event(BUTTON_MENU);
    app_handles_event = false;
    assert(handle(&event, 100, &actions));
    assert(crazypod_miniapp_input_motion_active());
    assert(event_count == 0);
    assert(close_count == 0);

    event.release = true;
    assert(handle(&event, 120, &actions));
    assert(!crazypod_miniapp_input_motion_active());
    assert(event_count == 1);
    assert(last_event_type == CP_INPUT_MENU);
    assert(close_count == 0);

    reset_test();
    app_open = true;
    app_handles_event = true;
    event = input_event(BUTTON_MENU);
    assert(handle(&event, 130, &actions));
    event.release = true;
    assert(handle(&event, 140, &actions));
    assert(event_count == 1);
    assert(last_event_type == CP_INPUT_MENU);
    assert(render_count == 0);
    assert(boost_count == 1);
    assert(close_count == 0);

    reset_test();
    app_open = true;
    event = input_event(BUTTON_MENU);
    assert(handle(&event, 200, &actions));
    event.repeated = true;
    assert(handle(&event, 249, &actions));
    assert(!crazypod_miniapp_input_exit_prompt_visible());
    crazypod_miniapp_input_service(true, 249);
    assert(!crazypod_miniapp_input_exit_prompt_visible());
    crazypod_miniapp_input_service(true, 250);
    assert(crazypod_miniapp_input_exit_prompt_visible());
    assert(!crazypod_miniapp_input_exit_selected());
    assert(event_count == 0);

    event = input_event(BUTTON_MENU);
    event.release = true;
    assert(handle(&event, 251, &actions));
    assert(crazypod_miniapp_input_exit_prompt_visible());

    event = input_event(BUTTON_SELECT);
    assert(handle(&event, 260, &actions));
    assert(!crazypod_miniapp_input_exit_prompt_visible());
    assert(close_count == 0);

    event = input_event(BUTTON_MENU);
    assert(handle(&event, 300, &actions));
    event.release = true;
    assert(handle(&event, 350, &actions));
    assert(crazypod_miniapp_input_exit_prompt_visible());
    assert(event_count == 0);

    event = input_event(BUTTON_RIGHT);
    assert(handle(&event, 360, &actions));
    assert(crazypod_miniapp_input_exit_selected());
    event = input_event(BUTTON_SELECT);
    assert(handle(&event, 370, &actions));
    assert(close_count == 1);
    assert(boost_count == 4);

    reset_test();
    app_open = true;
    event = input_event(BUTTON_MENU);
    assert(handle(&event, 400, &actions));
    crazypod_miniapp_input_service(false, 450);
    assert(!crazypod_miniapp_input_motion_active());
    assert(!crazypod_miniapp_input_exit_prompt_visible());
    return 0;
}
