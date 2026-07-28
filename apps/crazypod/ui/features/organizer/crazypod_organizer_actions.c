#include "config.h"

#ifdef IPOD_6G

#include "timefuncs.h"

#include "../../../crazypod_organizer.h"
#include "crazypod_calendar_controller.h"
#include "crazypod_organizer_actions.h"

#define EDITOR_CHAR_COUNT 36

static const char *const editor_characters[EDITOR_CHAR_COUNT] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static struct crazypod_organizer_action action(
    enum crazypod_organizer_action_kind kind)
{
    const struct crazypod_organizer_action result = {
        .kind = kind,
    };
    return result;
}

static struct crazypod_organizer_action push(
    enum crazypod_route route, int group)
{
    struct crazypod_organizer_action result =
        action(CRAZYPOD_ORGANIZER_ACTION_PUSH);

    result.route = route;
    result.group = group;
    return result;
}

static struct crazypod_organizer_action begin_editor(
    uint32_t id, int fallback_date)
{
    struct crazypod_organizer_action result =
        action(CRAZYPOD_ORGANIZER_ACTION_BEGIN_EDITOR);

    result.event_id = id;
    result.fallback_date = fallback_date;
    return result;
}

static int today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
           (now->tm_mon + 1) * 100 + now->tm_mday;
}

static int route_event_index(
    const struct route_state *state, int position)
{
    if(state->route == CALENDAR_ROUTE_UPCOMING)
        return crazypod_calendar_controller_upcoming_event_index(
            today_date(), position);
    if(state->route == CALENDAR_ROUTE_TODAY)
        return crazypod_calendar_controller_event_index_on_date(
            today_date(), position);
    if(state->route == CALENDAR_ROUTE_DAY_EVENTS)
        return crazypod_calendar_controller_event_index_on_date(
            crazypod_calendar_controller_focus_date(), position);
    return -1;
}

static int route_event_count(const struct route_state *state)
{
    int count = 0;

    while(route_event_index(state, count) >= 0)
        ++count;
    return count;
}

struct crazypod_organizer_action
crazypod_organizer_actions_activate(
    const struct route_state *state)
{
    switch(state->route) {
    case CALENDAR_ROUTE_MENU:
        if(state->selected == 0) {
            int today = today_date();

            crazypod_calendar_controller_set_focus_date(today);
            return push(CALENDAR_ROUTE_TODAY, -1);
        }
        if(state->selected == 1)
            return push(CALENDAR_ROUTE_UPCOMING, -1);
        if(state->selected == 2)
            return push(CALENDAR_ROUTE_MONTH, -1);
        return begin_editor(0, today_date());
    case CALENDAR_ROUTE_TODAY:
    case CALENDAR_ROUTE_UPCOMING:
    case CALENDAR_ROUTE_DAY_EVENTS: {
        int event_count = route_event_count(state);
        int event_index;

        if(state->selected == event_count) {
            int date = state->route == CALENDAR_ROUTE_DAY_EVENTS
                ? crazypod_calendar_controller_focus_date()
                : today_date();

            return begin_editor(0, date);
        }
        event_index = route_event_index(state, state->selected);
        return event_index >= 0
            ? push(CALENDAR_ROUTE_DETAIL, event_index)
            : action(CRAZYPOD_ORGANIZER_ACTION_NONE);
    }
    case CALENDAR_ROUTE_MONTH:
        return push(CALENDAR_ROUTE_DAY_EVENTS, -1);
    case CALENDAR_ROUTE_EDITOR:
        if(state->selected == 0)
            return push(CALENDAR_ROUTE_TITLE_EDITOR, -1);
        if(state->selected == 1) {
            crazypod_calendar_controller_shift_editor_date(1);
            return action(CRAZYPOD_ORGANIZER_ACTION_RENDER);
        }
        if(state->selected == 2) {
            crazypod_calendar_controller_shift_editor_time(1);
            return action(CRAZYPOD_ORGANIZER_ACTION_RENDER);
        }
        return action(CRAZYPOD_ORGANIZER_ACTION_COMMIT_EDITOR);
    case CALENDAR_ROUTE_TITLE_EDITOR:
        if(state->selected < EDITOR_CHAR_COUNT)
            crazypod_calendar_controller_insert_editor_text(
                editor_characters[state->selected]);
        else if(state->selected == EDITOR_CHAR_COUNT)
            crazypod_calendar_controller_insert_editor_text(" ");
        else if(state->selected == EDITOR_CHAR_COUNT + 1)
            crazypod_calendar_controller_backspace_editor_text();
        else
            return action(CRAZYPOD_ORGANIZER_ACTION_POP);
        return action(CRAZYPOD_ORGANIZER_ACTION_RENDER);
    case CALENDAR_ROUTE_DETAIL: {
        const struct crazypod_calendar_event *event =
            crazypod_calendar_event_get(state->group);

        return event != NULL && event->editable
            ? push(CALENDAR_ROUTE_ACTIONS, (int)event->id)
            : action(CRAZYPOD_ORGANIZER_ACTION_NONE);
    }
    case CALENDAR_ROUTE_ACTIONS:
        return state->selected == 0
            ? begin_editor((uint32_t)state->group, 0)
            : push(CALENDAR_ROUTE_DELETE_CONFIRM, state->group);
    case CALENDAR_ROUTE_DELETE_CONFIRM:
    case CONTACTS_ROUTE_DETAIL:
        return action(CRAZYPOD_ORGANIZER_ACTION_NONE);
    case CONTACTS_ROUTE_LIST:
        return crazypod_contact_get(state->selected) != NULL
            ? push(CONTACTS_ROUTE_DETAIL, state->selected)
            : action(CRAZYPOD_ORGANIZER_ACTION_NONE);
    default:
        return action(CRAZYPOD_ORGANIZER_ACTION_UNHANDLED);
    }
}

#endif
