#ifndef CRAZYPOD_CALENDAR_SCREEN_H
#define CRAZYPOD_CALENDAR_SCREEN_H

#include <stdbool.h>

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

/* Drop the cached month grid; the route renderer calls this before it
 * cleans the pane out from under it. */
void crazypod_calendar_screen_forget(void);
/* Restyles the built month grid for a new focused day without touching
 * the object tree. False when no usable grid is up (a different month, or
 * the pane was rebuilt) and the caller must do a full render. */
bool crazypod_calendar_screen_refocus(
    const struct crazypod_calendar_screen_date *date);
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
