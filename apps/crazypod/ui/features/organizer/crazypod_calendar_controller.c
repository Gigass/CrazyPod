#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_organizer.h"
#include "crazypod_calendar_model.h"
#include "../../presentation/crazypod_ui_text.h"
#include "crazypod_calendar_controller.h"

static struct crazypod_calendar_focus focus;
static uint32_t editor_id;
static int editor_date;
static int editor_minutes;
static char editor_summary[96];
static size_t editor_cursor;
static int editor_error;

void crazypod_calendar_controller_set_focus_date(int date)
{
    focus.year = date / 10000;
    focus.month = date / 100 % 100 - 1;
    focus.day = date % 100;
}

int crazypod_calendar_controller_focus_date(void)
{
    return focus.year * 10000 + (focus.month + 1) * 100 + focus.day;
}

struct crazypod_calendar_focus crazypod_calendar_controller_focus(void)
{
    return focus;
}

void crazypod_calendar_controller_move_focus(int direction)
{
    if(direction > 0) {
        ++focus.day;
        if(focus.day >
           crazypod_ui_calendar_days_in_month(
               focus.year, focus.month)) {
            focus.day = 1;
            ++focus.month;
            if(focus.month > 11) {
                focus.month = 0;
                ++focus.year;
            }
        }
    }
    else if(direction < 0) {
        --focus.day;
        if(focus.day < 1) {
            --focus.month;
            if(focus.month < 0) {
                focus.month = 11;
                --focus.year;
            }
            focus.day = crazypod_ui_calendar_days_in_month(
                focus.year, focus.month);
        }
    }
}

int crazypod_calendar_controller_event_index_on_date(
    int date, int position)
{
    int visible = 0;
    int i;

    for(i = 0; i < crazypod_calendar_event_count(); ++i) {
        const struct crazypod_calendar_event *event =
            crazypod_calendar_event_get(i);
        if(event != NULL && event->date == date &&
           visible++ == position)
            return i;
    }
    return -1;
}

int crazypod_calendar_controller_upcoming_event_index(
    int today, int position)
{
    int visible = 0;
    int i;

    for(i = 0; i < crazypod_calendar_event_count(); ++i) {
        const struct crazypod_calendar_event *event =
            crazypod_calendar_event_get(i);
        if(event != NULL && event->date >= today &&
           visible++ == position)
            return i;
    }
    return -1;
}

void crazypod_calendar_controller_begin_editor(
    uint32_t id, int fallback_date)
{
    const struct crazypod_calendar_event *event =
        crazypod_calendar_event_find(id);

    editor_id = event != NULL ? event->id : 0;
    editor_date = event != NULL ? event->date : fallback_date;
    editor_minutes = event != NULL
        ? crazypod_ui_calendar_parse_minutes(event->time) : -1;
    snprintf(editor_summary, sizeof(editor_summary), "%s",
             event != NULL ? event->summary : "");
    editor_cursor = strlen(editor_summary);
    editor_error = 0;
}

struct crazypod_calendar_editor_model
crazypod_calendar_controller_editor(void)
{
    const struct crazypod_calendar_editor_model model = {
        .id = editor_id,
        .date = editor_date,
        .minutes = editor_minutes,
        .summary = editor_summary,
        .cursor = editor_cursor,
        .error = editor_error,
    };

    return model;
}

void crazypod_calendar_controller_shift_editor_date(int direction)
{
    editor_error = 0;
    editor_date = crazypod_ui_calendar_shift_date(
        editor_date, direction);
}

void crazypod_calendar_controller_shift_editor_time(int direction)
{
    editor_error = 0;
    if(direction > 0)
        editor_minutes = editor_minutes < 0
            ? 9 * 60 : (editor_minutes + 30) % (24 * 60);
    else if(direction < 0) {
        if(editor_minutes <= 0)
            editor_minutes = -1;
        else
            editor_minutes -= 30;
    }
}

void crazypod_calendar_controller_insert_editor_text(const char *text)
{
    editor_error = 0;
    crazypod_ui_text_insert(
        editor_summary, sizeof(editor_summary),
        &editor_cursor, text);
}

void crazypod_calendar_controller_backspace_editor_text(void)
{
    editor_error = 0;
    crazypod_ui_text_backspace_at(editor_summary, &editor_cursor);
}

void crazypod_calendar_controller_move_editor_cursor(int direction)
{
    crazypod_ui_text_move_cursor(
        editor_summary, &editor_cursor, direction);
}

void crazypod_calendar_controller_clear_error(void)
{
    editor_error = 0;
}

uint32_t crazypod_calendar_controller_commit(void)
{
    char time[16];
    uint32_t id;

    if(editor_summary[0] == '\0') {
        editor_error = 1;
        return 0;
    }
    crazypod_ui_calendar_format_time(
        time, sizeof(time), editor_minutes);
    if(editor_id != 0) {
        if(!crazypod_calendar_event_update(
               editor_id, editor_date, time, editor_summary)) {
            editor_error = 2;
            return 0;
        }
        id = editor_id;
    }
    else {
        id = crazypod_calendar_event_add(
            editor_date, time, editor_summary);
        if(id == 0) {
            editor_error = 2;
            return 0;
        }
    }
    editor_id = id;
    return id;
}

#endif
