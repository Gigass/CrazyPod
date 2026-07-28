#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdio.h>

#include "../../../crazypod_organizer.h"
#include "crazypod_calendar_model.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_calendar_screen.h"

#define CALENDAR_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CALENDAR_WHITE 0xFFFFFF
#define CALENDAR_PANEL 0x1B1B22

void crazypod_calendar_screen_render_grid(
    lv_obj_t *content,
    const struct crazypod_calendar_screen_date *date)
{
    static const char *const months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    static const char *const weekdays[] = {
        "S", "M", "T", "W", "T", "F", "S"
    };
    static const char *const compact_weekdays[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[48];
    int first = crazypod_ui_calendar_weekday(
        date->year, date->month, 1);
    int count = crazypod_ui_calendar_days_in_month(
        date->year, date->month);
    int previous_month = date->month - 1;
    int previous_year = date->year;
    int previous_count;
    int slot;

    if(previous_month < 0) {
        previous_month = 11;
        --previous_year;
    }
    previous_count = crazypod_ui_calendar_days_in_month(
        previous_year, previous_month);
    crazypod_ui_widget_box(
        content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
        0xF9F9F7, LV_OPA_COVER);
    panel = crazypod_ui_widget_box(
        content, 10, 38, 300, 194, 12, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    label = crazypod_ui_widget_label(
        panel, "CALENDAR", &lv_font_montserrat_8,
        0x949494, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 14, 9);
    snprintf(text, sizeof(text), "%s %d",
             months[date->month], date->year);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_16,
        0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 23);
    snprintf(text, sizeof(text), "%s %d",
             compact_weekdays[
                 crazypod_ui_calendar_weekday(
                     date->year, date->month, date->day)],
             date->day);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_10,
        0x5C5C5C, LV_OPA_COVER);
    lv_obj_set_width(label, 72);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 210, 24);
    crazypod_ui_widget_box(
        panel, 12, 50, 276, 1, 0, 0x0E0E0E, 205);

    for(slot = 0; slot < 7; ++slot) {
        label = crazypod_ui_widget_label(
            panel, weekdays[slot], &lv_font_montserrat_8,
            0x949494, 230);
        lv_obj_set_width(label, 40);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 10 + slot * 40, 58);
    }
    for(slot = 0; slot < 42; ++slot) {
        int column = slot % 7;
        int row = slot / 7;
        int x = 10 + column * 40;
        int y = 75 + row * 18;
        int relative_day = slot - first + 1;
        int day;
        int year = date->year;
        int month = date->month;
        bool in_month = true;
        int cell_date;
        bool selected;
        bool is_today;
        bool has_event = false;
        int event_index;
        char day_text[4];

        if(relative_day < 1) {
            day = previous_count + relative_day;
            year = previous_year;
            month = previous_month;
            in_month = false;
        }
        else if(relative_day > count) {
            day = relative_day - count;
            if(++month > 11) {
                month = 0;
                ++year;
            }
            in_month = false;
        }
        else
            day = relative_day;
        cell_date = year * 10000 + (month + 1) * 100 + day;
        selected = in_month && day == date->day;
        is_today = cell_date == date->today;
        for(event_index = 0;
            event_index < crazypod_calendar_event_count();
            ++event_index) {
            const struct crazypod_calendar_event *event =
                crazypod_calendar_event_get(event_index);
            if(event != NULL && event->date == cell_date) {
                has_event = true;
                break;
            }
        }
        if(selected)
            crazypod_ui_widget_box(
                panel, x + 2, y - 1, 36, 17, 7,
                0x0E0E0E, LV_OPA_COVER);
        else if(is_today) {
            lv_obj_t *today_box = crazypod_ui_widget_box(
                panel, x + 2, y - 1, 36, 17, 7,
                0xFFFFFF, LV_OPA_TRANSP);
            lv_obj_set_style_border_width(today_box, 1, 0);
            lv_obj_set_style_border_color(
                today_box, lv_color_hex(0x0E0E0E), 0);
            lv_obj_set_style_border_opa(today_box, 210, 0);
        }
        snprintf(day_text, sizeof(day_text), "%d", day);
        label = crazypod_ui_widget_label(
            panel, day_text, &lv_font_montserrat_10,
            selected ? CALENDAR_WHITE :
                in_month ? 0x0E0E0E : 0xB8B8B8,
            in_month ? LV_OPA_COVER : 155);
        lv_obj_set_width(label, 40);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x, y);
        if(has_event)
            crazypod_ui_widget_box(
                panel, x + 19, y + 13, 3, 3,
                LV_RADIUS_CIRCLE,
                selected ? CALENDAR_WHITE : 0x0E0E0E,
                in_month ? 205 : 70);
    }
}

void crazypod_calendar_screen_render_day(
    lv_obj_t *content,
    const struct crazypod_calendar_screen_date *date,
    const struct crazypod_calendar_screen_events *events)
{
    lv_obj_t *overlay;
    lv_obj_t *label;
    char text[64];
    int start = events->selected > 3 ? events->selected - 3 : 0;
    int row;

    crazypod_calendar_screen_render_grid(content, date);
    overlay = crazypod_ui_widget_box(
        content, 25, 49, 270, 172, 12, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(overlay, 1, 0);
    lv_obj_set_style_border_color(
        overlay, lv_color_hex(0x0E0E0E), 0);
    lv_obj_set_style_border_opa(overlay, 210, 0);
    label = crazypod_ui_widget_label(
        overlay, "SCHEDULE", &lv_font_montserrat_8,
        0x949494, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 14, 9);
    snprintf(text, sizeof(text), "%04d-%02d-%02d",
             date->year, date->month + 1, date->day);
    label = crazypod_ui_widget_label(
        overlay, text, &lv_font_montserrat_16,
        0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 23);
    snprintf(text, sizeof(text), "%d item%s",
             events->count, events->count == 1 ? "" : "s");
    label = crazypod_ui_widget_label(
        overlay, text, &lv_font_montserrat_8, 0x5C5C5C, 220);
    lv_obj_set_width(label, 72);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 184, 29);
    crazypod_ui_widget_box(
        overlay, 12, 49, 246, 1, 0, 0x0E0E0E, 205);

    for(row = 0; row < 4; ++row) {
        int position = start + row;
        const struct crazypod_calendar_event *event;
        int y = 57 + row * 27;
        bool selected;

        if(position > events->count)
            break;
        selected = position == events->selected;
        if(selected)
            crazypod_ui_widget_box(
                overlay, 10, y - 2, 250, 25, 7,
                0xF3F3F0, LV_OPA_COVER);
        if(position == events->count) {
            label = crazypod_ui_widget_label(
                overlay, LV_SYMBOL_EDIT, &lv_font_montserrat_10,
                0x0E0E0E, 220);
            lv_obj_set_pos(label, 17, y + 4);
            label = crazypod_ui_widget_label(
                overlay, "Add Event", &lv_font_montserrat_10,
                0x0E0E0E, LV_OPA_COVER);
            lv_obj_set_pos(label, 43, y + 3);
            continue;
        }
        event = crazypod_calendar_event_get(
            events->index_at(events->context, position));
        if(event == NULL)
            continue;
        label = crazypod_ui_widget_label(
            overlay,
            event->time[0] != '\0' ? event->time : "All day",
            &lv_font_montserrat_8, 0x5C5C5C, 230);
        lv_obj_set_width(label, 44);
        lv_obj_set_pos(label, 17, y + 4);
        crazypod_ui_widget_box(
            overlay, 64, y + 2, 2, 17, 1, 0x0E0E0E, 210);
        label = crazypod_ui_widget_label(
            overlay, event->summary, &lv_font_montserrat_10,
            0x0E0E0E, LV_OPA_COVER);
        lv_obj_set_width(label, 180);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_pos(label, 75, y + 3);
    }
}

void crazypod_calendar_screen_render_detail(
    lv_obj_t *content, int event_index)
{
    const struct crazypod_calendar_event *event =
        crazypod_calendar_event_get(event_index);
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[180];

    panel = crazypod_ui_widget_box(
        content, 18, 54, 284, 145, 12, CALENDAR_PANEL, 230);
    snprintf(text, sizeof(text), "%s\n\n%04d-%02d-%02d\n%s\n\n%s",
             event != NULL ? event->summary : "No Event",
             event != NULL ? event->date / 10000 : 0,
             event != NULL ? event->date / 100 % 100 : 0,
             event != NULL ? event->date % 100 : 0,
             event != NULL && event->time[0] != '\0'
                ? event->time : "All day",
             event != NULL && event->editable
                ? "Center: Event Actions"
                : "Imported from .ics");
    label = crazypod_ui_widget_label(
        panel, text, CALENDAR_FONT, CALENDAR_WHITE, 235);
    lv_obj_set_pos(label, 14, 14);
    lv_obj_set_width(label, 256);
}

#endif
