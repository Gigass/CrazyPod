#ifndef CRAZYPOD_ORGANIZER_CALENDAR_CONTROLLER_H
#define CRAZYPOD_ORGANIZER_CALENDAR_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

struct crazypod_calendar_focus {
    int year;
    int month;
    int day;
};

struct crazypod_calendar_editor_model {
    uint32_t id;
    int date;
    int minutes;
    const char *summary;
    size_t cursor;
    int error;
};

void crazypod_calendar_controller_set_focus_date(int date);
int crazypod_calendar_controller_focus_date(void);
struct crazypod_calendar_focus
crazypod_calendar_controller_focus(void);
void crazypod_calendar_controller_move_focus(int direction);

int crazypod_calendar_controller_event_index_on_date(
    int date, int position);
int crazypod_calendar_controller_upcoming_event_index(
    int today, int position);

void crazypod_calendar_controller_begin_editor(
    uint32_t id, int fallback_date);
struct crazypod_calendar_editor_model
crazypod_calendar_controller_editor(void);
void crazypod_calendar_controller_shift_editor_date(int direction);
void crazypod_calendar_controller_shift_editor_time(int direction);
void crazypod_calendar_controller_insert_editor_text(const char *text);
void crazypod_calendar_controller_backspace_editor_text(void);
void crazypod_calendar_controller_move_editor_cursor(int direction);
void crazypod_calendar_controller_clear_error(void);
uint32_t crazypod_calendar_controller_commit(void);

#endif
