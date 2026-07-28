#ifndef CRAZYPOD_UI_CALENDAR_H
#define CRAZYPOD_UI_CALENDAR_H

#include <stddef.h>

int crazypod_ui_calendar_days_in_month(int year, int month);
int crazypod_ui_calendar_weekday(int year, int month, int day);
int crazypod_ui_calendar_shift_date(int date, int direction);
int crazypod_ui_calendar_parse_minutes(const char *time);
void crazypod_ui_calendar_format_time(char *buffer, size_t size,
                                      int minutes);

#endif
