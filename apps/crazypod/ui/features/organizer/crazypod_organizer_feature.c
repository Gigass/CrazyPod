#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "powermgmt.h"
#include "kernel.h"
#include "timefuncs.h"

#include "../../../crazypod_organizer.h"
#include "../../../crazypod_workouts.h"
#include "../../presentation/crazypod_ui_text.h"
#include "crazypod_calendar_model.h"
#include "crazypod_calendar_controller.h"
#include "crazypod_calendar_screen.h"
#include "crazypod_activity_controller.h"
#include "crazypod_activity_input.h"
#include "crazypod_calendar_input.h"
#include "crazypod_clock_screen.h"
#include "crazypod_contacts_screen.h"
#include "crazypod_clock_activation.h"
#include "crazypod_organizer_actions.h"
#include "crazypod_organizer_confirmation.h"
#include "crazypod_organizer_feature.h"
#include "crazypod_utility_preview.h"
#include "crazypod_workout_screen.h"

static long clock_last_render_tick;

static int today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
        (now->tm_mon + 1) * 100 + now->tm_mday;
}

static int event_index(
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

static int event_count(const struct route_state *state)
{
    int count = 0;

    while(event_index(state, count) >= 0)
        ++count;
    return count;
}

static const char *editor_title(int index)
{
    static const char *const characters[36] = {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
        "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
        "U", "V", "W", "X", "Y", "Z",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
    };

    if(index >= 0 && index < 36)
        return characters[index];
    if(index == 36)
        return "Space";
    if(index == 37)
        return "Backspace";
    if(index == 38)
        return "Done";
    return "";
}

int crazypod_organizer_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case CLOCK_ROUTE_MENU:
        return 3;
    case CLOCK_ROUTE_SLEEP_TIMER:
        return get_sleep_timer_active() ? 2 : 4;
    case CLOCK_ROUTE_VIEW:
    case STOPWATCH_ROUTE_VIEW:
    case WORKOUT_ROUTE_READY:
    case WORKOUT_ROUTE_ACTIVE:
    case WORKOUT_ROUTE_SUMMARY:
    case WORKOUT_ROUTE_DETAIL:
    case CALENDAR_ROUTE_MONTH:
    case CALENDAR_ROUTE_DETAIL:
    case CONTACTS_ROUTE_DETAIL:
    case WORKOUT_ROUTE_FINISH_CONFIRM:
    case WORKOUT_ROUTE_DELETE_CONFIRM:
    case CALENDAR_ROUTE_DELETE_CONFIRM:
        return 1;
    case WORKOUT_ROUTE_MENU:
        return 3;
    case WORKOUT_ROUTE_TYPES:
        return CRAZYPOD_WORKOUT_ACTIVITY_COUNT;
    case WORKOUT_ROUTE_HISTORY:
        return crazypod_workouts_count();
    case CALENDAR_ROUTE_MENU:
        return 4;
    case CALENDAR_ROUTE_TODAY:
    case CALENDAR_ROUTE_UPCOMING:
    case CALENDAR_ROUTE_DAY_EVENTS:
        return event_count(state) + 1;
    case CALENDAR_ROUTE_EDITOR:
        return 4;
    case CALENDAR_ROUTE_TITLE_EDITOR:
        return 39;
    case CALENDAR_ROUTE_ACTIONS:
        return 2;
    case CONTACTS_ROUTE_LIST:
        return crazypod_contacts_count();
    default:
        return 0;
    }
}

const char *crazypod_organizer_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case CLOCK_ROUTE_MENU:
    case CLOCK_ROUTE_VIEW:
        return "CLOCK";
    case CLOCK_ROUTE_SLEEP_TIMER:
        return "SLEEP TIMER";
    case STOPWATCH_ROUTE_VIEW:
        return "STOPWATCH";
    case WORKOUT_ROUTE_MENU:
        return "WORKOUTS";
    case WORKOUT_ROUTE_TYPES:
        return "CHOOSE WORKOUT";
    case WORKOUT_ROUTE_READY:
    case WORKOUT_ROUTE_ACTIVE:
    case WORKOUT_ROUTE_SUMMARY:
    case WORKOUT_ROUTE_DETAIL:
        return "WORKOUT";
    case WORKOUT_ROUTE_FINISH_CONFIRM:
        return "END WORKOUT";
    case WORKOUT_ROUTE_HISTORY:
        return "HISTORY";
    case WORKOUT_ROUTE_DELETE_CONFIRM:
        return "DELETE WORKOUT";
    case CALENDAR_ROUTE_MENU:
        return "CALENDAR";
    case CALENDAR_ROUTE_TODAY:
        return "TODAY";
    case CALENDAR_ROUTE_UPCOMING:
        return "UPCOMING";
    case CALENDAR_ROUTE_MONTH:
        return "MONTH";
    case CALENDAR_ROUTE_DAY_EVENTS:
        return "EVENTS";
    case CALENDAR_ROUTE_EDITOR:
        return crazypod_calendar_controller_editor().id != 0
            ? "EDIT EVENT" : "ADD EVENT";
    case CALENDAR_ROUTE_TITLE_EDITOR:
        return "EVENT TITLE";
    case CALENDAR_ROUTE_DETAIL:
        return "EVENT";
    case CALENDAR_ROUTE_ACTIONS:
        return "EVENT ACTIONS";
    case CALENDAR_ROUTE_DELETE_CONFIRM:
        return "DELETE EVENT";
    case CONTACTS_ROUTE_LIST:
        return "CONTACTS";
    case CONTACTS_ROUTE_DETAIL:
        return "CONTACT";
    default:
        return "";
    }
}

bool crazypod_organizer_feature_item_title(
    const struct route_state *state, int index,
    bool stopwatch_running, bool workout_running,
    const char **title)
{
    switch(state->route) {
    case CLOCK_ROUTE_MENU:
        *title = index == 0 ? "Local Time" :
            index == 1 ? "Sleep Timer" :
            index == 2 ? "Stopwatch" : "";
        return true;
    case CLOCK_ROUTE_SLEEP_TIMER:
        if(get_sleep_timer_active())
            *title = index == 0 ? "Cancel Timer" :
                index == 1 ? "End Now" : "";
        else {
            static const char *const durations[] = {
                "15 Minutes", "30 Minutes", "45 Minutes", "60 Minutes"
            };

            *title = index >= 0 && index < 4 ? durations[index] : "";
        }
        return true;
    case CLOCK_ROUTE_VIEW:
        *title = "Current Time";
        return true;
    case STOPWATCH_ROUTE_VIEW:
        *title = stopwatch_running ? "Pause" : "Start";
        return true;
    case WORKOUT_ROUTE_MENU: {
        static const char *const titles[] = {
            "Start Workout", "History", "Summary"
        };

        *title = index >= 0 && index < 3 ? titles[index] : "";
        return true;
    }
    case WORKOUT_ROUTE_TYPES:
        *title = crazypod_workout_activity_title(index);
        return true;
    case WORKOUT_ROUTE_READY:
        *title = "Start";
        return true;
    case WORKOUT_ROUTE_ACTIVE:
        *title = workout_running ? "Pause" : "Resume";
        return true;
    case WORKOUT_ROUTE_FINISH_CONFIRM:
        *title = "Hold Center to Save";
        return true;
    case WORKOUT_ROUTE_HISTORY: {
        const struct crazypod_workout *workout =
            crazypod_workout_get(index);

        *title = workout != NULL
            ? crazypod_workout_activity_title(workout->activity) : "";
        return true;
    }
    case WORKOUT_ROUTE_SUMMARY:
        *title = "Workout Summary";
        return true;
    case WORKOUT_ROUTE_DETAIL:
        *title = "Workout Details";
        return true;
    case WORKOUT_ROUTE_DELETE_CONFIRM:
        *title = "Hold Center to Delete";
        return true;
    case CALENDAR_ROUTE_MENU:
        *title = index == 0 ? "Today" :
            index == 1 ? "Upcoming" :
            index == 2 ? "Month" :
            index == 3 ? "Add Event" : "";
        return true;
    case CALENDAR_ROUTE_TODAY:
    case CALENDAR_ROUTE_UPCOMING:
    case CALENDAR_ROUTE_DAY_EVENTS: {
        int count = event_count(state);
        const struct crazypod_calendar_event *event;

        if(index == count)
            *title = "Add Event";
        else {
            event = crazypod_calendar_event_get(
                event_index(state, index));
            *title = event != NULL ? event->summary : "";
        }
        return true;
    }
    case CALENDAR_ROUTE_MONTH:
        *title = "Month";
        return true;
    case CALENDAR_ROUTE_EDITOR: {
        static char text[128];
        char time[16];
        const struct crazypod_calendar_editor_model editor =
            crazypod_calendar_controller_editor();

        if(index == 0)
            snprintf(text, sizeof(text), "Title: %s",
                     editor.summary[0] != '\0'
                        ? editor.summary : "Untitled");
        else if(index == 1)
            snprintf(text, sizeof(text), "Date: %04d-%02d-%02d",
                     editor.date / 10000,
                     editor.date / 100 % 100,
                     editor.date % 100);
        else if(index == 2) {
            crazypod_ui_calendar_format_time(
                time, sizeof(time), editor.minutes);
            snprintf(text, sizeof(text), "Time: %s",
                     time[0] != '\0' ? time : "All Day");
        }
        else if(index == 3)
            snprintf(text, sizeof(text), "%s",
                     editor.error == 1 ? "Title Required" :
                     editor.error == 2 ? "Storage Error" : "Save Event");
        else
            text[0] = '\0';
        *title = text;
        return true;
    }
    case CALENDAR_ROUTE_TITLE_EDITOR:
        *title = editor_title(index);
        return true;
    case CALENDAR_ROUTE_DETAIL:
        *title = "Event Details";
        return true;
    case CALENDAR_ROUTE_ACTIONS:
        *title = index == 0 ? "Edit" :
            index == 1 ? "Delete" : "";
        return true;
    case CALENDAR_ROUTE_DELETE_CONFIRM:
        *title = "Hold Center to Delete";
        return true;
    case CONTACTS_ROUTE_LIST: {
        const struct crazypod_contact *contact =
            crazypod_contact_get(index);

        *title = contact != NULL ? contact->name : "";
        return true;
    }
    case CONTACTS_ROUTE_DETAIL:
        *title = "Contact Details";
        return true;
    default:
        return false;
    }
}

bool crazypod_organizer_feature_service(
    enum crazypod_route route, long now,
    long ticks_per_second)
{
    long interval;

    if(route == STOPWATCH_ROUTE_VIEW)
        return crazypod_activity_service_stopwatch(
            now, ticks_per_second);
    if(route == WORKOUT_ROUTE_ACTIVE)
        return crazypod_activity_service_workout(
            now, ticks_per_second);
    if(route != CLOCK_ROUTE_VIEW &&
       route != CLOCK_ROUTE_SLEEP_TIMER)
        return false;
    interval = route == CLOCK_ROUTE_VIEW
        ? (ticks_per_second / 4 > 0
            ? ticks_per_second / 4 : 1)
        : ticks_per_second;
    if(TIME_BEFORE(now, clock_last_render_tick + interval))
        return false;
    clock_last_render_tick = now;
    return true;
}

bool crazypod_organizer_feature_activate(
    struct route_state *state, long now,
    const struct crazypod_organizer_activation_host *host)
{
    const struct crazypod_activity_action activity =
        crazypod_activity_activate(state, now);
    const struct crazypod_organizer_action organizer =
        crazypod_organizer_actions_activate(state);
    const struct crazypod_clock_activation_result clock =
        crazypod_clock_activation_execute(state);

    if(activity.kind != CRAZYPOD_ACTIVITY_ACTION_UNHANDLED) {
        if(activity.kind == CRAZYPOD_ACTIVITY_ACTION_RENDER)
            host->render(false);
        else if(activity.kind == CRAZYPOD_ACTIVITY_ACTION_PUSH)
            host->push(activity.route, activity.group);
        return true;
    }
    if(organizer.kind != CRAZYPOD_ORGANIZER_ACTION_UNHANDLED) {
        switch(organizer.kind) {
        case CRAZYPOD_ORGANIZER_ACTION_RENDER:
            host->render(false);
            break;
        case CRAZYPOD_ORGANIZER_ACTION_PUSH:
            host->push(organizer.route, organizer.group);
            break;
        case CRAZYPOD_ORGANIZER_ACTION_POP:
            host->pop();
            break;
        case CRAZYPOD_ORGANIZER_ACTION_BEGIN_EDITOR:
            host->begin_editor(
                organizer.event_id, organizer.fallback_date);
            break;
        case CRAZYPOD_ORGANIZER_ACTION_COMMIT_EDITOR:
            (void)host->commit_editor();
            break;
        case CRAZYPOD_ORGANIZER_ACTION_NONE:
        case CRAZYPOD_ORGANIZER_ACTION_UNHANDLED:
        default:
            break;
        }
        return true;
    }
    if(clock.kind == CRAZYPOD_CLOCK_ACTIVATION_UNHANDLED)
        return false;
    if(clock.kind == CRAZYPOD_CLOCK_ACTIVATION_PUSH)
        host->push(clock.route, -1);
    else if(clock.kind == CRAZYPOD_CLOCK_ACTIVATION_RENDER)
        host->render(false);
    return true;
}

static struct crazypod_calendar_screen_date screen_date(void)
{
    const struct crazypod_calendar_focus focus =
        crazypod_calendar_controller_focus();
    const struct crazypod_calendar_screen_date date = {
        .year = focus.year,
        .month = focus.month,
        .day = focus.day,
        .today = today_date(),
    };

    return date;
}

static int screen_event_index(void *context, int position)
{
    return event_index(
        (const struct route_state *)context, position);
}

bool crazypod_organizer_feature_render(
    const struct route_state *state,
    const struct crazypod_organizer_render_context *context)
{
    if(state->route == CLOCK_ROUTE_VIEW) {
        struct tm *now = get_time();
        const struct crazypod_clock_screen_time time = {
            .hour = now->tm_hour,
            .minute = now->tm_min,
            .second = now->tm_sec,
            .second_tenths = now->tm_sec * 10 +
                (context->now % context->ticks_per_second) *
                10 / context->ticks_per_second,
            .weekday = now->tm_wday,
            .month = now->tm_mon,
            .month_day = now->tm_mday,
        };

        crazypod_clock_screen_render(context->parent, &time);
        return true;
    }
    if(state->route == STOPWATCH_ROUTE_VIEW) {
        struct crazypod_stopwatch_screen_model model;

        crazypod_activity_stopwatch_model(
            context->now, context->ticks_per_second, &model);
        crazypod_stopwatch_screen_render(
            context->parent, &model);
        return true;
    }
    if(state->route == WORKOUT_ROUTE_READY) {
        crazypod_workout_screen_render_ready(
            context->parent, crazypod_activity_workout_type());
        return true;
    }
    if(state->route == WORKOUT_ROUTE_ACTIVE) {
        crazypod_workout_screen_render_active(
            context->parent, crazypod_activity_workout_type(),
            crazypod_activity_workout_running(),
            crazypod_activity_workout_seconds(
                context->now, context->ticks_per_second));
        return true;
    }
    if(state->route == WORKOUT_ROUTE_SUMMARY) {
        crazypod_workout_screen_render_summary(context->parent);
        return true;
    }
    if(state->route == WORKOUT_ROUTE_DETAIL) {
        crazypod_workout_screen_render_detail(
            context->parent, state->group);
        return true;
    }
    if(state->route == CALENDAR_ROUTE_MONTH) {
        const struct crazypod_calendar_screen_date date =
            screen_date();

        crazypod_calendar_screen_render_grid(
            context->parent, &date);
        return true;
    }
    if(state->route == CALENDAR_ROUTE_DAY_EVENTS) {
        const struct crazypod_calendar_screen_date date =
            screen_date();
        const struct crazypod_calendar_screen_events events = {
            .selected = state->selected,
            .count = event_count(state),
            .index_at = screen_event_index,
            .context = (void *)state,
        };

        crazypod_calendar_screen_render_day(
            context->parent, &date, &events);
        return true;
    }
    if(state->route == CALENDAR_ROUTE_DETAIL) {
        crazypod_calendar_screen_render_detail(
            context->parent, state->group);
        return true;
    }
    if(state->route == CONTACTS_ROUTE_DETAIL) {
        crazypod_contacts_screen_render(
            context->parent, state->group);
        return true;
    }
    return false;
}

uint32_t crazypod_organizer_feature_background(
    enum crazypod_route route)
{
    if(route == STOPWATCH_ROUTE_VIEW) {
        static const uint32_t backgrounds[] = {
            0xF9F9F7, 0xF2F2F2, 0xFFFFFF
        };

        return backgrounds[
            crazypod_activity_stopwatch_style() % 3];
    }
    if(route == CLOCK_ROUTE_VIEW ||
       route == CALENDAR_ROUTE_MONTH ||
       route == CALENDAR_ROUTE_DAY_EVENTS)
        return 0xF9F9F7;
    if(route == WORKOUT_ROUTE_READY ||
       route == WORKOUT_ROUTE_ACTIVE ||
       route == WORKOUT_ROUTE_SUMMARY ||
       route == WORKOUT_ROUTE_DETAIL)
        return 0x050505;
    return 0x08080D;
}

static struct crazypod_feature_input_context organizer_input_context;

static void organizer_input_render(void)
{
    organizer_input_context.render(false);
}

static void show_finish_confirmation(void)
{
    organizer_input_context.push(
        WORKOUT_ROUTE_FINISH_CONFIRM, -1);
}

bool crazypod_organizer_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    const struct crazypod_activity_input_actions activity = {
        .activate = context->activate,
        .render = organizer_input_render,
        .show_finish_confirmation =
            show_finish_confirmation,
        .leave = context->pop,
    };
    const struct crazypod_calendar_input_actions calendar = {
        .move_selection = context->move,
        .activate = context->activate,
        .render = organizer_input_render,
        .leave = context->pop,
    };

    organizer_input_context = *context;
    if(crazypod_activity_input_handle(
           state->route, event, context->now,
           context->ticks_per_second, &activity))
        return true;
    return crazypod_calendar_input_handle(
        state, event, context->today_date, &calendar);
}

bool crazypod_organizer_feature_stopwatch_running(void)
{
    return crazypod_activity_stopwatch_running();
}

bool crazypod_organizer_feature_workout_running(void)
{
    return crazypod_activity_workout_running();
}

const char *crazypod_organizer_feature_editor_title(
    char *buffer, size_t buffer_size)
{
    const struct crazypod_calendar_editor_model editor =
        crazypod_calendar_controller_editor();

    return crazypod_ui_text_with_cursor(
        editor.summary, editor.cursor,
        buffer, buffer_size);
}

void crazypod_organizer_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const char *title, int miniapp_error,
    const lv_font_t *metadata_font)
{
    crazypod_utility_preview_render(
        parent, state, title, miniapp_error,
        metadata_font);
}

void crazypod_organizer_feature_set_focus_date(int date)
{
    crazypod_calendar_controller_set_focus_date(date);
}

int crazypod_organizer_feature_focus_date(void)
{
    return crazypod_calendar_controller_focus_date();
}

void crazypod_organizer_feature_begin_editor(
    uint32_t id, int fallback_date)
{
    crazypod_calendar_controller_begin_editor(
        id, fallback_date);
}

uint32_t crazypod_organizer_feature_commit_editor(int *date)
{
    uint32_t id = crazypod_calendar_controller_commit();

    if(date != NULL)
        *date = crazypod_calendar_controller_editor().date;
    return id;
}

#ifdef SIMULATOR
void crazypod_organizer_feature_simulator_workout(
    int activity, long accumulated_ticks,
    long started_at, bool running)
{
    crazypod_activity_simulator_workout(
        activity, accumulated_ticks,
        started_at, running);
}
#endif

struct crazypod_organizer_confirmation_result
crazypod_organizer_feature_confirm(
    const struct route_state *state, long now,
    int ticks_per_second, int fallback_date)
{
    return crazypod_organizer_confirmation_execute(
        state, now, ticks_per_second, fallback_date);
}

#endif
