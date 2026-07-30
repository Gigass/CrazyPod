#include "../../../crazypod_l10n.h"

#include <stdio.h>
#include <string.h>

#include "crazypod_calendar_model.h"

int crazypod_ui_calendar_days_in_month(int year, int month)
{
    static const int days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if(month == 1 &&
       ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month];
}

int crazypod_ui_calendar_weekday(int year, int month, int day)
{
    static const int offsets[] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };

    if(month < 2)
        --year;
    return (year + year / 4 - year / 100 + year / 400 +
            offsets[month] + day) % 7;
}

int crazypod_ui_calendar_shift_date(int date, int direction)
{
    int year = date / 10000;
    int month = date / 100 % 100 - 1;
    int day = date % 100;

    if(direction > 0) {
        ++day;
        if(day > crazypod_ui_calendar_days_in_month(year, month)) {
            day = 1;
            if(++month > 11) {
                month = 0;
                ++year;
            }
        }
    }
    else if(direction < 0) {
        if(--day < 1) {
            if(--month < 0) {
                month = 11;
                --year;
            }
            day = crazypod_ui_calendar_days_in_month(year, month);
        }
    }
    return year * 10000 + (month + 1) * 100 + day;
}

int crazypod_ui_calendar_parse_minutes(const char *time)
{
    if(time == NULL || strlen(time) < 5 || time[2] != ':')
        return -1;
    return ((time[0] - '0') * 10 + time[1] - '0') * 60 +
           (time[3] - '0') * 10 + time[4] - '0';
}

void crazypod_ui_calendar_format_time(char *buffer, size_t size,
                                      int minutes)
{
    if(minutes < 0)
        buffer[0] = '\0';
    else
        snprintf(buffer, size, CP_FMT("%02d:%02d"),
                 minutes / 60, minutes % 60);
}
