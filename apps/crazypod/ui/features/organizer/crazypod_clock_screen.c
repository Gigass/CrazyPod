#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>

#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_clock_screen.h"

#define COLOR_WHITE 0xFFFFFF

static lv_obj_t *make_clock_hand(
    lv_obj_t *dial, int center, int length, int width,
    int angle_tenths, uint32_t color)
{
    lv_obj_t *hand = crazypod_ui_widget_box(
        dial, center - width / 2, center - length,
        width, length, width, color, LV_OPA_COVER);

    lv_obj_set_style_transform_pivot_x(hand, width / 2, 0);
    lv_obj_set_style_transform_pivot_y(hand, length, 0);
    lv_obj_set_style_transform_rotation(hand, angle_tenths, 0);
    return hand;
}

static lv_obj_t *make_analog_clock(
    lv_obj_t *parent, int x, int y, int size,
    int hour, int minute, int second_tenths,
    uint32_t dial_color, uint32_t ink_color)
{
    lv_obj_t *dial = crazypod_ui_widget_box(
        parent, x, y, size, size, LV_RADIUS_CIRCLE,
        dial_color, LV_OPA_COVER);
    int center = size / 2;
    int tick;

    lv_obj_set_style_border_width(dial, 2, 0);
    lv_obj_set_style_border_color(dial, lv_color_hex(ink_color), 0);
    lv_obj_set_style_border_opa(dial, 220, 0);
    for(tick = 0; tick < 12; ++tick) {
        int width = tick % 3 == 0 ? 2 : 1;
        int height = tick % 3 == 0 ? 10 : 6;
        lv_obj_t *mark = crazypod_ui_widget_box(
            dial, center - width / 2, 7,
            width, height, width,
            tick % 3 == 0 ? ink_color : 0x949494,
            tick % 3 == 0 ? 235 : 180);
        lv_obj_set_style_transform_pivot_x(mark, width / 2, 0);
        lv_obj_set_style_transform_pivot_y(mark, center - 7, 0);
        lv_obj_set_style_transform_rotation(mark, tick * 300, 0);
    }
    make_clock_hand(
        dial, center, size * 25 / 100, 4,
        ((hour % 12) * 30 + minute / 2) * 10, ink_color);
    make_clock_hand(
        dial, center, size * 36 / 100, 3,
        minute * 60 + second_tenths / 10, ink_color);
    make_clock_hand(
        dial, center, size * 42 / 100, 1,
        second_tenths * 6, ink_color);
    crazypod_ui_widget_box(
        dial, center - 4, center - 4, 8, 8,
        LV_RADIUS_CIRCLE, ink_color, LV_OPA_COVER);
    return dial;
}

void crazypod_clock_screen_render(
    lv_obj_t *content,
    const struct crazypod_clock_screen_time *time)
{
    static const char *const weekdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    static const char *const months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[64];

    crazypod_ui_widget_box(
        content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
        0xF9F9F7, LV_OPA_COVER);
    panel = crazypod_ui_widget_box(
        content, 10, 40, 300, 188, 12, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    make_analog_clock(
        panel, 14, 23, 140, time->hour, time->minute,
        time->second_tenths, 0xFFFFFF, 0x0E0E0E);

    label = crazypod_ui_widget_label(
        panel, "LOCAL TIME", &lv_font_montserrat_8,
        0x5C5C5C, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 170, 34);
    snprintf(text, sizeof(text), "%02d:%02d:%02d",
             time->hour, time->minute, time->second);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_24,
        0x0E0E0E, LV_OPA_COVER);
    lv_obj_set_pos(label, 170, 53);
    crazypod_ui_widget_box(
        panel, 170, 86, 112, 1, 0, 0x0E0E0E, 210);
    snprintf(text, sizeof(text), "%s\n%s %d",
             weekdays[time->weekday], months[time->month],
             time->month_day);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_10,
        0x5C5C5C, LV_OPA_COVER);
    lv_obj_set_pos(label, 170, 97);
    label = crazypod_ui_widget_label(
        panel, "DEVICE TIME", &lv_font_montserrat_8, 0x949494, 230);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_pos(label, 170, 132);
}

void crazypod_stopwatch_screen_render(
    lv_obj_t *content,
    const struct crazypod_stopwatch_screen_model *model)
{
    static const char *const style_names[] = {
        "CLASSIC SILVER", "OBSIDIAN GOLD", "CHAMPAGNE GOLD"
    };
    static const uint32_t canvas_colors[] = {
        0xF9F9F7, 0xF2F2F2, 0xFFFFFF
    };
    static const uint32_t dial_colors[] = {
        0xFFFFFF, 0xFFFFFF, 0xEDEDED
    };
    static const uint32_t ink_colors[] = {
        0x0E0E0E, 0x2C2416, 0x3B2A10
    };
    unsigned total_hundredths =
        (unsigned)(model->elapsed_ticks * 100 /
                   model->ticks_per_second);
    unsigned minutes = total_hundredths / 6000;
    unsigned seconds = total_hundredths / 100 % 60;
    unsigned hundredths = total_hundredths % 100;
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[32];
    int first_lap;
    int lap;
    int style = model->style % 3;

    crazypod_ui_widget_box(
        content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
        canvas_colors[style], LV_OPA_COVER);
    panel = crazypod_ui_widget_box(
        content, 10, 40, 300, 188, 12, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    make_analog_clock(
        panel, 8, 23, 140, (int)(minutes / 60),
        (int)minutes % 60,
        (int)seconds * 10 + (int)hundredths / 10,
        dial_colors[style], ink_colors[style]);
    label = crazypod_ui_widget_label(
        panel, style_names[style], &lv_font_montserrat_8,
        ink_colors[style], 135);
    lv_obj_set_width(label, 140);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 8, 168);

    label = crazypod_ui_widget_label(
        panel, "CHRONOGRAPH", &lv_font_montserrat_8,
        ink_colors[style], 110);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 166, 22);
    snprintf(text, sizeof(text), "%02u:%02u.%02u",
             minutes, seconds, hundredths);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_24,
        ink_colors[style], LV_OPA_COVER);
    lv_obj_set_pos(label, 166, 39);
    label = crazypod_ui_widget_label(
        panel, model->running ? "RUNNING" : "PAUSED",
        &lv_font_montserrat_8, ink_colors[style], 225);
    lv_obj_set_pos(label, 166, 69);
    if(model->lap_count > 0) {
        snprintf(text, sizeof(text), "%d LAPS", model->lap_count);
        label = crazypod_ui_widget_label(
            panel, text, &lv_font_montserrat_8,
            ink_colors[style], 140);
        lv_obj_set_pos(label, 224, 69);
    }
    crazypod_ui_widget_box(
        panel, 166, 84, 122, 1, 0, ink_colors[style], 52);
    first_lap = model->lap_count > 4 ? model->lap_count - 4 : 0;
    if(model->lap_count > 0) {
        label = crazypod_ui_widget_label(
            panel, "LAP       TOTAL", &lv_font_montserrat_8,
            ink_colors[style], 105);
        lv_obj_set_pos(label, 168, 90);
    }
    for(lap = first_lap; lap < model->lap_count; ++lap) {
        unsigned lap_hundredths =
            (unsigned)(model->laps[lap] * 100 /
                       model->ticks_per_second);
        snprintf(text, sizeof(text), "%02d     %02u:%02u.%02u",
                 lap + 1, lap_hundredths / 6000,
                 lap_hundredths / 100 % 60,
                 lap_hundredths % 100);
        label = crazypod_ui_widget_label(
            panel, text, &lv_font_montserrat_8,
            ink_colors[style], 225);
        lv_obj_set_pos(label, 168, 104 + (lap - first_lap) * 15);
    }
    if(model->lap_count == 0) {
        label = crazypod_ui_widget_label(
            panel,
            "CENTER  START / PAUSE\nRIGHT   RECORD LAP\nLEFT    RESET",
            &lv_font_montserrat_8, ink_colors[style], 150);
        lv_obj_set_pos(label, 166, 101);
    }
    label = crazypod_ui_widget_label(
        panel,
        model->reset_armed
            ? "Press LEFT again to reset"
            : "Wheel changes style before first lap",
        &lv_font_montserrat_8, 0x949494, 210);
    lv_obj_set_width(label, 136);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(label, 166, 168);
}

#endif
