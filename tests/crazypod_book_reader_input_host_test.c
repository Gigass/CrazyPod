#include <assert.h>
#include <stdbool.h>

#include "button.h"
#include "features/books/crazypod_book_reader_input.h"

static int page_delta;
static int page_turns;
static int refresh_calls;
static int playlist_calls;
static int bookmark_calls;
static int leave_calls;
static bool page_can_change = true;

static bool turn_page(int direction)
{
    ++page_turns;
    if(!page_can_change)
        return false;
    page_delta += direction;
    return true;
}

static void refresh(void)
{
    ++refresh_calls;
}

static void choose_playback_playlist(void)
{
    ++playlist_calls;
}

static void toggle_bookmark(void)
{
    ++bookmark_calls;
}

static void leave(void)
{
    ++leave_calls;
}

static void send(long button, intptr_t data)
{
    static const struct crazypod_book_reader_input_actions actions = {
        .turn_page = turn_page,
        .refresh = refresh,
        .choose_playback_playlist = choose_playback_playlist,
        .toggle_bookmark = toggle_bookmark,
        .leave = leave,
    };
    struct crazypod_input_event event =
        crazypod_input_event_make(button, data);

    crazypod_book_reader_input_handle(&event, &actions);
}

int main(void)
{
    send(BUTTON_RIGHT, 0);
    assert(page_delta == 1);
    assert(page_turns == 1);
    assert(refresh_calls == 1);

    send(BUTTON_LEFT, 0);
    assert(page_delta == 0);
    assert(page_turns == 2);
    assert(refresh_calls == 2);

    send(BUTTON_SCROLL_FWD, 3);
    assert(page_delta == 3);
    assert(page_turns == 5);
    assert(refresh_calls == 3);

    page_can_change = false;
    send(BUTTON_RIGHT, 0);
    assert(page_delta == 3);
    assert(page_turns == 6);
    assert(refresh_calls == 3);

    send(BUTTON_SELECT, 0);
    send(BUTTON_SELECT | BUTTON_REPEAT, 0);
    send(BUTTON_PLAY, 0);
    send(BUTTON_PLAY | BUTTON_REPEAT, 0);
    send(BUTTON_MENU, 0);
    send(BUTTON_MENU | BUTTON_REPEAT, 0);
    assert(playlist_calls == 1);
    assert(bookmark_calls == 1);
    assert(leave_calls == 1);
    return 0;
}
