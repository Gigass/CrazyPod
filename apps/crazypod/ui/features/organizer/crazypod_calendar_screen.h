#ifndef CRAZYPOD_CALENDAR_SCREEN_H
#define CRAZYPOD_CALENDAR_SCREEN_H

#include "lvgl.h"

struct crazypod_calendar_screen_date {
    int year;
    int month;
    int day;
    int today;
};

typedef int (*crazypod_calendar_event_index_cb)(
    void *context, int position);

struct crazypod_calendar_screen_events {
    int selected;
    int count;
    crazypod_calendar_event_index_cb index_at;
    void *context;
};

void crazypod_calendar_screen_render_grid(
    lv_obj_t *content,
    const struct crazypod_calendar_screen_date *date);
void crazypod_calendar_screen_render_day(
    lv_obj_t *content,
    const struct crazypod_calendar_screen_date *date,
    const struct crazypod_calendar_screen_events *events);
void crazypod_calendar_screen_render_detail(
    lv_obj_t *content, int event_index);

#endif
