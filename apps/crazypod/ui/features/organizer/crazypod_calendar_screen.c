#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../../crazypod_organizer.h"
#include "crazypod_calendar_model.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_calendar_screen.h"

#define CALENDAR_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CALENDAR_WHITE 0xFFFFFF
#define CALENDAR_PANEL 0x1B1B22

/*
 * The month grid is 42 cells, each of which was a fresh label plus up to
 * two more objects on every render -- and moving the focus by one day is
 * a render. Building it once and restyling the cells in place turns an
 * arrow press from ~130 object creations into 42 style writes, which is
 * what made scrolling around the month so slow.
 *
 * The pane is cleaned wholesale on a route change, so the pointers are
 * dropped in crazypod_calendar_screen_forget(), which the route renderer
 * calls before that clean; the parent and month are re-checked here as a
 * backstop.
 */
static const char *const calendar_compact_weekdays[] = {
    CP_TR("SUN"), CP_TR("MON"), CP_TR("TUE"), CP_TR("WED"),
    CP_TR("THU"), CP_TR("FRI"), CP_TR("SAT")
};

struct calendar_cell {
    lv_obj_t *highlight;
    lv_obj_t *label;
    lv_obj_t *dot;
};

static struct {
    lv_obj_t *panel;
    int year;
    int month;
    struct calendar_cell cells[42];
    lv_obj_t *day_label;
} grid;

void crazypod_calendar_screen_forget(void)
{
    memset(&grid, 0, sizeof(grid));
}

static bool grid_usable(
    const struct crazypod_calendar_screen_date *date)
{
    int i;

    if(grid.panel == NULL ||
       grid.year != date->year || grid.month != date->month ||
       !lv_obj_is_valid(grid.panel))
        return false;
    for(i = 0; i < 42; ++i) {
        if(grid.cells[i].label == NULL || grid.cells[i].dot == NULL ||
           grid.cells[i].highlight == NULL)
            return false;
    }
    return grid.day_label != NULL;
}

/* Restyles one cell for the focused day; the text never changes while the
 * month does not, so only colour, opacity and visibility move. */
static void refresh_cell(
    struct calendar_cell *cell, bool in_month, bool selected,
    bool is_today, bool has_event)
{
    if(selected) {
        lv_obj_set_style_bg_opa(cell->highlight, LV_OPA_COVER, 0);
        lv_obj_set_style_border_opa(cell->highlight, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(cell->highlight, LV_OBJ_FLAG_HIDDEN);
    }
    else if(is_today) {
        lv_obj_set_style_bg_opa(cell->highlight, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(cell->highlight, 210, 0);
        lv_obj_remove_flag(cell->highlight, LV_OBJ_FLAG_HIDDEN);
    }
    else
        lv_obj_add_flag(cell->highlight, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_color(
        cell->label,
        lv_color_hex(selected ? CALENDAR_WHITE :
                     in_month ? 0x0E0E0E : 0xB8B8B8), 0);
    lv_obj_set_style_text_opa(
        cell->label, in_month ? LV_OPA_COVER : 155, 0);

    if(has_event) {
        lv_obj_set_style_bg_color(
            cell->dot,
            lv_color_hex(selected ? CALENDAR_WHITE : 0x0E0E0E), 0);
        lv_obj_set_style_bg_opa(
            cell->dot, in_month ? 205 : 70, 0);
        lv_obj_remove_flag(cell->dot, LV_OBJ_FLAG_HIDDEN);
    }
    else
        lv_obj_add_flag(cell->dot, LV_OBJ_FLAG_HIDDEN);
}

bool crazypod_calendar_screen_refocus(
    const struct crazypod_calendar_screen_date *date)
{
    char text[48];
    int first;
    int count;
    int previous_month;
    int previous_year;
    int previous_count;
    int slot;

    if(!grid_usable(date))
        return false;

    first = crazypod_ui_calendar_weekday(date->year, date->month, 1);
    count = crazypod_ui_calendar_days_in_month(date->year, date->month);
    previous_month = date->month - 1;
    previous_year = date->year;
    if(previous_month < 0) {
        previous_month = 11;
        --previous_year;
    }
    previous_count = crazypod_ui_calendar_days_in_month(
        previous_year, previous_month);

    snprintf(text, sizeof(text), CP_FMT("%s %d"),
             calendar_compact_weekdays[
                 crazypod_ui_calendar_weekday(
                     date->year, date->month, date->day)],
             date->day);
    CP_LV_LABEL_SET_TEXT(grid.day_label, text);

    for(slot = 0; slot < 42; ++slot) {
        int relative_day = slot - first + 1;
        int day = relative_day;
        int year = date->year;
        int month = date->month;
        bool in_month = true;
        int cell_date;
        int event_index;
        bool has_event = false;

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
        cell_date = year * 10000 + (month + 1) * 100 + day;
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
        refresh_cell(&grid.cells[slot], in_month,
                     in_month && day == date->day,
                     cell_date == date->today, has_event);
    }
    return true;
}

void crazypod_calendar_screen_render_grid(
    lv_obj_t *content,
    const struct crazypod_calendar_screen_date *date)
{
    static const char *const months[] = {
        CP_TR("January"), CP_TR("February"), CP_TR("March"), CP_TR("April"), CP_TR("May"), CP_TR("June"),
        CP_TR("July"), CP_TR("August"), CP_TR("September"), CP_TR("October"), CP_TR("November"), CP_TR("December")
    };
    static const char *const weekdays[] = {
        CP_TR("S"), CP_TR("M"), CP_TR("T"), CP_TR("W"), CP_TR("T"), CP_TR("F"), CP_TR("S")
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

    if(crazypod_calendar_screen_refocus(date))
        return;

    memset(&grid, 0, sizeof(grid));
    crazypod_ui_widget_box(
        content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
        0xF9F9F7, LV_OPA_COVER);
    panel = crazypod_ui_widget_box(
        content, 10, 38, 300, 194, 12, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    label = crazypod_ui_widget_label(
        panel, CP_TR("CALENDAR"), &lv_font_montserrat_8,
        0x949494, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 14, 5);
    snprintf(text, sizeof(text), CP_FMT("%s %d"),
             months[date->month], date->year);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_16,
        0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 22);
    snprintf(text, sizeof(text), CP_FMT("%s %d"),
             calendar_compact_weekdays[
                 crazypod_ui_calendar_weekday(
                     date->year, date->month, date->day)],
             date->day);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_10,
        0x5C5C5C, LV_OPA_COVER);
    lv_obj_set_width(label, 72);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 210, 24);
    grid.day_label = label;
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
        int y = 75 + row * 19;
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
        /* Every cell gets all three objects whatever its state, so a
         * later focus move only has to restyle them. */
        grid.cells[slot].highlight = crazypod_ui_widget_box(
            panel, x + 2, y - 1, 36, 19, 7,
            0x0E0E0E, LV_OPA_COVER);
        lv_obj_set_style_border_width(
            grid.cells[slot].highlight, 1, 0);
        lv_obj_set_style_border_color(
            grid.cells[slot].highlight, lv_color_hex(0x0E0E0E), 0);

        snprintf(day_text, sizeof(day_text), CP_FMT("%d"), day);
        label = crazypod_ui_widget_label(
            panel, day_text, &lv_font_montserrat_10,
            0x0E0E0E, LV_OPA_COVER);
        lv_obj_set_width(label, 40);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x, y);
        grid.cells[slot].label = label;

        grid.cells[slot].dot = crazypod_ui_widget_box(
            panel, x + 19, y + 15, 3, 3,
            LV_RADIUS_CIRCLE, 0x0E0E0E, 205);

        refresh_cell(&grid.cells[slot], in_month, selected,
                     is_today, has_event);
    }

    grid.panel = panel;
    grid.year = date->year;
    grid.month = date->month;
    lv_obj_null_on_delete(&grid.panel);
    for(slot = 0; slot < 42; ++slot) {
        lv_obj_null_on_delete(&grid.cells[slot].highlight);
        lv_obj_null_on_delete(&grid.cells[slot].label);
        lv_obj_null_on_delete(&grid.cells[slot].dot);
    }
    lv_obj_null_on_delete(&grid.day_label);
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
        overlay, CP_TR("SCHEDULE"), &lv_font_montserrat_8,
        0x949494, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 14, 5);
    snprintf(text, sizeof(text), CP_FMT("%04d-%02d-%02d"),
             date->year, date->month + 1, date->day);
    label = crazypod_ui_widget_label(
        overlay, text, &lv_font_montserrat_16,
        0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 22);
    snprintf(text, sizeof(text), CP_FMT("%d item%s"),
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
                overlay, CP_TR("Add Event"), &lv_font_montserrat_10,
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
            event->time[0] != '\0' ? event->time : CP_TR("All day"),
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
    snprintf(text, sizeof(text),
             CP_FMT("%s\n\n%04d-%02d-%02d\n%s\n\n%s"),
             event != NULL ? event->summary : CP_FMT("No Event"),
             event != NULL ? event->date / 10000 : 0,
             event != NULL ? event->date / 100 % 100 : 0,
             event != NULL ? event->date % 100 : 0,
             event != NULL && event->time[0] != '\0'
                ? event->time : CP_FMT("All day"),
             event != NULL && event->editable
                ? CP_FMT("Center: Event Actions")
                : CP_FMT("Imported from .ics"));
    label = crazypod_ui_widget_label(
        panel, text, CALENDAR_FONT, CALENDAR_WHITE, 235);
    lv_obj_set_pos(label, 14, 14);
    lv_obj_set_width(label, 256);
}

#endif
