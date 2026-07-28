#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "../../../crazypod_workouts.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_workout_screen.h"

#define CRAZYPOD_WORKOUT_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_WORKOUT_WHITE 0xFFFFFF

static void format_duration(
    char *text, size_t size, uint32_t seconds)
{
    snprintf(text, size, "%02lu:%02lu:%02lu",
             (unsigned long)(seconds / 3600u),
             (unsigned long)(seconds / 60u % 60u),
             (unsigned long)(seconds % 60u));
}

void crazypod_workout_screen_render_ready(
    lv_obj_t *content, int activity)
{
    lv_obj_t *panel;
    lv_obj_t *label;

    crazypod_ui_widget_box(
        content, 0, 32, 320, 208, 0, 0x050505, LV_OPA_COVER);
    panel = crazypod_ui_widget_box(
        content, 18, 48, 284, 166, 18, 0x111512, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xA8F12D), 0);
    lv_obj_set_style_border_opa(panel, 100, 0);
    label = crazypod_ui_widget_label(
        panel, crazypod_workout_activity_title(activity),
        &lv_font_montserrat_16, CRAZYPOD_WORKOUT_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 20);
    label = crazypod_ui_widget_label(
        panel, "READY", &lv_font_montserrat_24,
        0xA8F12D, LV_OPA_COVER);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 57);
    label = crazypod_ui_widget_label(
        panel, "TIME ONLY\nNo motion, distance, or calorie estimates",
        &lv_font_montserrat_10, CRAZYPOD_WORKOUT_WHITE, 145);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 96);
    label = crazypod_ui_widget_label(
        panel, "CENTER  START", &lv_font_montserrat_10, 0xA8F12D, 230);
    lv_obj_set_width(label, 248);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 18, 140);
}

void crazypod_workout_screen_render_active(
    lv_obj_t *content, int activity, bool running, uint32_t seconds)
{
    lv_obj_t *ring;
    lv_obj_t *label;
    char elapsed[24];

    crazypod_ui_widget_box(
        content, 0, 32, 320, 208, 0, 0x050505, LV_OPA_COVER);
    ring = crazypod_ui_widget_box(
        content, 26, 49, 126, 126, LV_RADIUS_CIRCLE,
        0x0A0A0A, LV_OPA_COVER);
    lv_obj_set_style_border_width(ring, 5, 0);
    lv_obj_set_style_border_color(
        ring, lv_color_hex(running ? 0xA8F12D : 0xFFB340), 0);
    lv_obj_set_style_border_opa(ring, 235, 0);
    label = crazypod_ui_widget_label(
        ring, running ? LV_SYMBOL_PLAY : "II",
        &lv_font_montserrat_24,
        running ? 0xA8F12D : 0xFFB340, LV_OPA_COVER);
    lv_obj_center(label);
    label = crazypod_ui_widget_label(
        content, crazypod_workout_activity_title(activity),
        &lv_font_montserrat_10, CRAZYPOD_WORKOUT_WHITE, 165);
    lv_obj_set_pos(label, 174, 56);
    format_duration(elapsed, sizeof(elapsed), seconds);
    label = crazypod_ui_widget_label(
        content, elapsed, &lv_font_montserrat_24,
        CRAZYPOD_WORKOUT_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 174, 76);
    crazypod_ui_widget_box(
        content, 174, 108, 126, 1, 0,
        CRAZYPOD_WORKOUT_WHITE, 90);
    label = crazypod_ui_widget_label(
        content, running ? "RUNNING" : "PAUSED",
        &lv_font_montserrat_10,
        running ? 0xA8F12D : 0xFFB340, 235);
    lv_obj_set_pos(label, 174, 119);
    label = crazypod_ui_widget_label(
        content,
        "CENTER  PAUSE / RESUME\nPLAY  FINISH\nTIME-ONLY LOG",
        &lv_font_montserrat_8, CRAZYPOD_WORKOUT_WHITE, 125);
    lv_obj_set_pos(label, 174, 145);
}

void crazypod_workout_screen_render_summary(lv_obj_t *content)
{
    lv_obj_t *panel;
    lv_obj_t *label;
    uint32_t total_seconds = 0;
    char text[160];
    int i;

    for(i = 0; i < crazypod_workouts_count(); ++i) {
        const struct crazypod_workout *workout =
            crazypod_workout_get(i);
        if(workout != NULL)
            total_seconds += workout->duration_seconds;
    }
    panel = crazypod_ui_widget_box(
        content, 18, 52, 284, 150, 14, 0x111512, 238);
    snprintf(text, sizeof(text),
             "WORKOUT SUMMARY\n\n%d saved workouts\n%lu total minutes\n\n"
             "Metrics: elapsed time only\nNo sensor data is fabricated.",
             crazypod_workouts_count(),
             (unsigned long)(total_seconds / 60u));
    label = crazypod_ui_widget_label(
        panel, text, CRAZYPOD_WORKOUT_FONT,
        CRAZYPOD_WORKOUT_WHITE, 230);
    lv_obj_set_pos(label, 16, 14);
    lv_obj_set_width(label, 252);
}

void crazypod_workout_screen_render_detail(
    lv_obj_t *content, int workout_index)
{
    const struct crazypod_workout *workout =
        crazypod_workout_get(workout_index);
    lv_obj_t *panel;
    lv_obj_t *label;
    char duration[24];
    char text[192];

    format_duration(
        duration, sizeof(duration),
        workout != NULL ? workout->duration_seconds : 0);
    panel = crazypod_ui_widget_box(
        content, 18, 52, 284, 150, 14, 0x111512, 238);
    snprintf(text, sizeof(text), "%s\n\n%04d-%02d-%02d\n%s\n\n"
             "Time-only workout\nCenter: Delete",
             workout != NULL
                 ? crazypod_workout_activity_title(workout->activity)
                 : "Missing Workout",
             workout != NULL ? (int)(workout->date / 10000) : 0,
             workout != NULL ? (int)(workout->date / 100 % 100) : 0,
             workout != NULL ? (int)(workout->date % 100) : 0,
             duration);
    label = crazypod_ui_widget_label(
        panel, text, CRAZYPOD_WORKOUT_FONT,
        CRAZYPOD_WORKOUT_WHITE, 230);
    lv_obj_set_pos(label, 16, 14);
    lv_obj_set_width(label, 252);
}

#endif
