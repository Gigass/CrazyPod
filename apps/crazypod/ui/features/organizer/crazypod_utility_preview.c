#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "powermgmt.h"
#include "timefuncs.h"
#include "lvgl.h"

#include "../../../crazypod_miniapps.h"
#include "../../../crazypod_organizer.h"
#include "../../../crazypod_workouts.h"
#include "crazypod_calendar_controller.h"
#include "crazypod_calendar_model.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "crazypod_utility_preview.h"

#define COLOR_WHITE 0xFFFFFF

static lv_obj_t *make_box(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_box(
        parent, x, y, width, height, radius, color, opacity);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

static int calendar_today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
           (now->tm_mon + 1) * 100 + now->tm_mday;
}

static int calendar_route_event_index(
    const struct route_state *state, int position)
{
    if(state->route == CALENDAR_ROUTE_UPCOMING)
        return crazypod_calendar_controller_upcoming_event_index(
            calendar_today_date(), position);
    if(state->route == CALENDAR_ROUTE_TODAY)
        return crazypod_calendar_controller_event_index_on_date(
            calendar_today_date(), position);
    if(state->route == CALENDAR_ROUTE_DAY_EVENTS)
        return crazypod_calendar_controller_event_index_on_date(
            crazypod_calendar_controller_focus_date(), position);
    return -1;
}

static int calendar_route_event_count(
    const struct route_state *state)
{
    int count = 0;

    while(calendar_route_event_index(state, count) >= 0)
        ++count;
    return count;
}

static const char *miniapp_symbol(
    const struct crazypod_miniapp_metadata *metadata)
{
    return metadata != NULL && metadata->symbol[0] != '\0'
        ? metadata->symbol : LV_SYMBOL_FILE;
}

void crazypod_utility_preview_render(
    lv_obj_t *parent, const struct route_state *state,
    const char *title, int miniapp_error,
    const lv_font_t *metadata_font)
{
    const char *detail = "";
    const char *symbol = LV_SYMBOL_HOME;
    uint32_t color = 0x4F9BFF;
    lv_obj_t *swatch;
    lv_obj_t *label;
    lv_obj_t *text_panel;
    char detail_text[96];

    if(state->route == UTILITIES_ROUTE_MENU) {
        const struct crazypod_miniapp_metadata *metadata =
            crazypod_miniapps_metadata(state->selected);

        if(metadata != NULL) {
            symbol = miniapp_symbol(metadata);
            color = metadata->accent_rgb;
            detail = metadata->summary;
        }
        if(miniapp_error < 0) {
            snprintf(detail_text, sizeof(detail_text),
                     "Package error %d", miniapp_error);
            detail = detail_text;
        }
    }
    else if(state->route == WORKOUT_ROUTE_MENU) {
        symbol = state->selected == 0 ? LV_SYMBOL_PLAY :
                 state->selected == 1 ? LV_SYMBOL_REFRESH :
                 LV_SYMBOL_LIST;
        color = 0xA8F12D;
        detail = state->selected == 0
            ? "Choose a time-only workout"
            : state->selected == 1
                ? "Saved workout history"
                : "Elapsed-time totals";
    }
    else if(state->route == WORKOUT_ROUTE_TYPES) {
        symbol = LV_SYMBOL_PLAY;
        color = 0xA8F12D;
        detail = "Manual elapsed-time tracking";
    }
    else if(state->route == WORKOUT_ROUTE_HISTORY) {
        const struct crazypod_workout *workout =
            crazypod_workout_get(state->selected);
        symbol = LV_SYMBOL_REFRESH;
        color = 0xA8F12D;
        if(workout != NULL) {
            snprintf(detail_text, sizeof(detail_text),
                     "%04d-%02d-%02d · %lu min",
                     (int)(workout->date / 10000),
                     (int)(workout->date / 100 % 100),
                     (int)(workout->date % 100),
                     (unsigned long)(workout->duration_seconds / 60u));
            detail = detail_text;
        }
        else
            detail = "No saved workouts";
    }
    else if(state->route == WORKOUT_ROUTE_FINISH_CONFIRM ||
            state->route == WORKOUT_ROUTE_DELETE_CONFIRM) {
        symbol = LV_SYMBOL_TRASH;
        color = 0xFF453A;
        detail = state->route == WORKOUT_ROUTE_FINISH_CONFIRM
            ? "Hold center to save elapsed time"
            : "Hold center to delete permanently";
    }
    else if(state->route == CLOCK_ROUTE_MENU) {
        static const char *const symbols[] = {
            LV_SYMBOL_HOME, LV_SYMBOL_POWER, LV_SYMBOL_REFRESH
        };
        symbol = symbols[state->selected];
        color = state->selected == 0 ? 0xF26D5B :
                state->selected == 1 ? 0x7B61FF : 0xFFB340;
        detail = state->selected == 0 ? "Device local time" :
                 state->selected == 1
                    ? (get_sleep_timer_active()
                        ? "Timer is running" : "Power off after playback")
                    : "Precise elapsed time";
    }
    else if(state->route == CLOCK_ROUTE_SLEEP_TIMER) {
        symbol = LV_SYMBOL_POWER;
        color = 0x7B61FF;
        if(get_sleep_timer_active()) {
            int remaining = get_sleep_timer();
            snprintf(detail_text, sizeof(detail_text),
                     "%d:%02d remaining",
                     remaining / 60, remaining % 60);
            detail = detail_text;
        }
        else
            detail = "Select a shutdown delay";
    }
    else if(state->route == CALENDAR_ROUTE_MENU) {
        static const char *const symbols[] = {
            LV_SYMBOL_HOME, LV_SYMBOL_REFRESH,
            LV_SYMBOL_LIST, LV_SYMBOL_EDIT
        };
        static const char *const details[] = {
            "Events on the current date",
            "Future calendar events",
            "Browse the month grid",
            "Create a local event"
        };
        symbol = symbols[state->selected];
        color = 0xFF453A;
        detail = details[state->selected];
    }
    else if(state->route == CALENDAR_ROUTE_TODAY ||
            state->route == CALENDAR_ROUTE_UPCOMING ||
            state->route == CALENDAR_ROUTE_DAY_EVENTS) {
        int event_count = calendar_route_event_count(state);
        const struct crazypod_calendar_event *event =
            state->selected < event_count
                ? crazypod_calendar_event_get(
                      calendar_route_event_index(
                          state, state->selected))
                : NULL;
        color = 0xFF453A;
        if(event != NULL) {
            snprintf(detail_text, sizeof(detail_text),
                     "%d · %s", event->date,
                     event->time[0] != '\0' ? event->time : "All day");
            detail = detail_text;
        }
        else
            detail = "Create an event on this iPod";

        {
            struct tm *now = get_time();
            static const char *const weekday[] = {
                "S", "M", "T", "W", "T", "F", "S"
            };
            int first_day =
                (now->tm_wday - ((now->tm_mday - 1) % 7) + 7) % 7;
            int count = crazypod_ui_calendar_days_in_month(now->tm_year + 1900, now->tm_mon);
            int cell_width = 13;
            int day;
            char month[20];

            swatch = make_box(parent, 190, 58, 100, 92, 10,
                              0xF4F0E8, LV_OPA_COVER);
            make_box(swatch, 0, 0, 100, 20, 10,
                     color, LV_OPA_COVER);
            snprintf(month, sizeof(month), "%d / %d",
                     now->tm_mon + 1, now->tm_year + 1900);
            label = make_label(swatch, month, &lv_font_montserrat_8,
                               COLOR_WHITE, LV_OPA_COVER);
            lv_obj_set_width(label, 100);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(label, 0, 6);
            for(day = 0; day < 7; ++day) {
                label = make_label(swatch, weekday[day],
                                   &lv_font_montserrat_8,
                                   0x565656, 190);
                lv_obj_set_width(label, cell_width);
                lv_obj_set_style_text_align(
                    label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_pos(label, 4 + day * cell_width, 24);
            }
            for(day = 1; day <= count; ++day) {
                int slot = first_day + day - 1;
                int x = 4 + (slot % 7) * cell_width;
                int y = 35 + (slot / 7) * 10;
                char day_text[4];
                if(day == now->tm_mday)
                    make_box(swatch, x + 1, y - 1, 11, 10,
                             LV_RADIUS_CIRCLE, color, LV_OPA_COVER);
                snprintf(day_text, sizeof(day_text), "%d", day);
                label = make_label(
                    swatch, day_text, &lv_font_montserrat_8,
                    day == now->tm_mday ? COLOR_WHITE : 0x252525,
                    LV_OPA_COVER);
                lv_obj_set_width(label, cell_width);
                lv_obj_set_style_text_align(
                    label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_pos(label, x, y);
            }
        }
    }
    else if(state->route == CALENDAR_ROUTE_EDITOR) {
        const struct crazypod_calendar_editor_model editor =
            crazypod_calendar_controller_editor();
        symbol = state->selected == 3
            ? LV_SYMBOL_SAVE : LV_SYMBOL_EDIT;
        color = 0xFF453A;
        detail = state->selected == 0 ? "Edit the event title" :
                 state->selected == 1
                    ? "Center next day · Left previous"
                    : state->selected == 2
                        ? "Center later · Left earlier"
                        : editor.error == 1
                            ? "Enter a title before saving"
                            : editor.error == 2
                                ? "Could not write calendar.bin"
                                : "Write event to local storage";
    }
    else if(state->route == CALENDAR_ROUTE_ACTIONS ||
            state->route == CALENDAR_ROUTE_DELETE_CONFIRM) {
        symbol = state->route == CALENDAR_ROUTE_DELETE_CONFIRM ||
                 state->selected == 1
            ? LV_SYMBOL_TRASH : LV_SYMBOL_EDIT;
        color = 0xFF453A;
        detail = state->route == CALENDAR_ROUTE_DELETE_CONFIRM
            ? "Hold center to delete permanently"
            : state->selected == 0
                ? "Change this local event"
                : "Open delete confirmation";
    }
    else if(state->route == CONTACTS_ROUTE_LIST) {
        const struct crazypod_contact *contact =
            crazypod_contact_get(state->selected);
        symbol = LV_SYMBOL_HOME;
        color = 0x4F9BFF;
        detail = contact != NULL && contact->phone[0] != '\0'
            ? contact->phone : "Imported contact";
    }

    if(state->route != CALENDAR_ROUTE_TODAY &&
       state->route != CALENDAR_ROUTE_UPCOMING &&
       state->route != CALENDAR_ROUTE_DAY_EVENTS) {
        swatch = make_box(parent, 204, 76, 72, 72, 16,
                          color, LV_OPA_COVER);
        label = make_label(swatch, symbol, &lv_font_montserrat_24,
                           COLOR_WHITE, 230);
        lv_obj_center(label);
    }
    text_panel = crazypod_preview_make_text_panel(parent, 158, 50);
    label = make_label(text_panel, title != NULL ? title : "",
                       metadata_font,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(text_panel, detail, metadata_font,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 29);
}



#endif
