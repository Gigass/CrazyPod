#include <assert.h>

#include "button.h"
#include "shell/crazypod_home_input.h"

static int selection_delta;
static int next_calls;
static int previous_calls;
static int open_calls;
static int playback_calls;

static void move_selection(int direction)
{
    selection_delta += direction;
}

static void next_track(void)
{
    ++next_calls;
}

static void previous_track(void)
{
    ++previous_calls;
}

static void open_selected_app(void)
{
    ++open_calls;
}

static void toggle_playback(void)
{
    ++playback_calls;
}

static void send(long button, intptr_t data)
{
    static const struct crazypod_home_input_actions actions = {
        .move_selection = move_selection,
        .next_track = next_track,
        .previous_track = previous_track,
        .open_selected_app = open_selected_app,
        .toggle_playback = toggle_playback,
    };
    struct crazypod_input_event event =
        crazypod_input_event_make(button, data);

    crazypod_home_input_handle(&event, &actions);
}

int main(void)
{
    send(BUTTON_SCROLL_FWD, 1);
    assert(selection_delta == 1);
    send(BUTTON_SCROLL_FWD | BUTTON_REPEAT, 8);
    assert(selection_delta == 4);
    send(BUTTON_SCROLL_BACK, 2);
    assert(selection_delta == 2);

    send(BUTTON_RIGHT, 0);
    send(BUTTON_RIGHT | BUTTON_REPEAT, 0);
    send(BUTTON_LEFT, 0);
    send(BUTTON_LEFT | BUTTON_REPEAT, 0);
    assert(next_calls == 1);
    assert(previous_calls == 1);

    send(BUTTON_SELECT, 0);
    send(BUTTON_PLAY, 0);
    send(BUTTON_PLAY | BUTTON_REPEAT, 0);
    assert(open_calls == 1);
    assert(playback_calls == 1);
    return 0;
}
