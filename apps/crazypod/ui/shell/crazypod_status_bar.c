#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdio.h>

#include "audio.h"
#include "powermgmt.h"
#include "timefuncs.h"

#include "../../crazypod_screen_recording.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_status_bar.h"

#define STATUS_WHITE 0xFFFFFF
#define STATUS_DARK 0x08080D
#define STATUS_RECORDING 0xFF3B30

struct status_bar {
    lv_obj_t *time;
    lv_obj_t *battery;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_cap;
    lv_obj_t *charge;
    lv_obj_t *playing;
    int rendered_minute;
    int rendered_battery_width;
    int rendered_charging;
    int rendered_playing;
    bool visible;
};

static struct status_bar status_bars[CRAZYPOD_STATUS_BAR_COUNT];
static lv_obj_t *recording_indicator;
static int rendered_recording = -1;

static struct status_bar *status_bar_at(int index)
{
    if(index < 0 || index >= CRAZYPOD_STATUS_BAR_COUNT)
        return NULL;
    return &status_bars[index];
}

void crazypod_status_bar_create(int index, lv_obj_t *screen)
{
    struct status_bar *bar = status_bar_at(index);

    if(bar == NULL || screen == NULL)
        return;
    bar->time = crazypod_ui_widget_label(
        screen, "00:00", &lv_font_montserrat_12,
        STATUS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->time, 34, 7);

    bar->playing = crazypod_ui_widget_label(
        screen, LV_SYMBOL_PLAY, &lv_font_montserrat_10,
        STATUS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->playing, 241, 11);
    lv_obj_add_flag(bar->playing, LV_OBJ_FLAG_HIDDEN);

    if(recording_indicator == NULL) {
        recording_indicator = crazypod_ui_widget_box(
            lv_layer_top(), 228, 14, 7, 7,
            LV_RADIUS_CIRCLE, STATUS_RECORDING, LV_OPA_COVER);
        lv_obj_add_flag(
            recording_indicator, LV_OBJ_FLAG_HIDDEN);
    }

    bar->battery = crazypod_ui_widget_box(
        screen, 258, 11, 27, 12, 3, STATUS_WHITE, 64);
    bar->battery_fill = crazypod_ui_widget_box(
        bar->battery, 1, 1, 24, 10, 2,
        STATUS_WHITE, LV_OPA_COVER);
    bar->charge = crazypod_ui_widget_label(
        bar->battery, LV_SYMBOL_CHARGE, &lv_font_montserrat_8,
        STATUS_DARK, LV_OPA_COVER);
    lv_obj_center(bar->charge);
    bar->battery_cap = crazypod_ui_widget_box(
        screen, 287, 15, 2, 5, 1, STATUS_WHITE, 128);
    bar->rendered_minute = -1;
    bar->rendered_battery_width = -1;
    bar->rendered_charging = -1;
    bar->rendered_playing = -1;
    bar->visible = true;
}

void crazypod_status_bars_update(void)
{
    char time_text[8];
    struct tm *now = get_time();
    int minute = now->tm_hour * 60 + now->tm_min;
    int level = battery_level();
    int battery_width;
    bool charging = false;
    int status = audio_status();
    bool playing = (status & AUDIO_STATUS_PLAY) != 0 &&
                   (status & AUDIO_STATUS_PAUSE) == 0;
    bool screen_recording = crazypod_screen_recording_active();
    bool time_formatted = false;
    int i;

    if(recording_indicator != NULL &&
       rendered_recording != screen_recording) {
        if(screen_recording)
            lv_obj_remove_flag(
                recording_indicator, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(
                recording_indicator, LV_OBJ_FLAG_HIDDEN);
        rendered_recording = screen_recording;
    }

    if(level < 0)
        level = 0;
    if(level > 100)
        level = 100;
    battery_width = level > 0 ? 3 + (21 * level / 100) : 0;
#if CONFIG_CHARGING >= CHARGING_MONITOR
    charging = charge_state > DISCHARGING;
#endif

    for(i = 0; i < CRAZYPOD_STATUS_BAR_COUNT; ++i) {
        struct status_bar *bar = &status_bars[i];

        if(bar->time == NULL)
            continue;
        if(!bar->visible)
            continue;
        if(bar->rendered_minute != minute) {
            if(!time_formatted) {
                snprintf(time_text, sizeof(time_text), CP_FMT("%02d:%02d"),
                         now->tm_hour, now->tm_min);
                time_formatted = true;
            }
            CP_LV_LABEL_SET_TEXT(bar->time, time_text);
            bar->rendered_minute = minute;
        }
        if(bar->rendered_battery_width != battery_width) {
            lv_obj_set_width(bar->battery_fill, battery_width);
            bar->rendered_battery_width = battery_width;
        }
        if(bar->rendered_charging != charging) {
            if(charging)
                lv_obj_remove_flag(
                    bar->charge, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(
                    bar->charge, LV_OBJ_FLAG_HIDDEN);
            bar->rendered_charging = charging;
        }
        if(bar->rendered_playing != playing) {
            if(playing)
                lv_obj_remove_flag(
                    bar->playing, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(
                    bar->playing, LV_OBJ_FLAG_HIDDEN);
            bar->rendered_playing = playing;
        }
    }
}

void crazypod_status_bar_set_palette(
    int index, uint32_t foreground, uint32_t background)
{
    struct status_bar *bar = status_bar_at(index);

    if(bar == NULL || bar->time == NULL)
        return;
    lv_obj_set_style_text_color(
        bar->time, lv_color_hex(foreground), 0);
    lv_obj_set_style_text_color(
        bar->playing, lv_color_hex(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery, lv_color_hex(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery_fill, lv_color_hex(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery_cap, lv_color_hex(foreground), 0);
    lv_obj_set_style_text_color(
        bar->charge, lv_color_hex(background), 0);
}

void crazypod_status_bar_foreground(int index)
{
    struct status_bar *bar = status_bar_at(index);

    if(bar == NULL || bar->time == NULL)
        return;
    lv_obj_move_foreground(bar->time);
    lv_obj_move_foreground(bar->playing);
    if(recording_indicator != NULL)
        lv_obj_move_foreground(recording_indicator);
}

void crazypod_status_bar_set_visible(int index, bool visible)
{
    struct status_bar *bar = status_bar_at(index);
    lv_obj_t *objects[4];
    int i;

    if(bar == NULL || bar->time == NULL)
        return;
    if(bar->visible == visible)
        return;
    bar->visible = visible;
    objects[0] = bar->time;
    objects[1] = bar->battery;
    objects[2] = bar->battery_cap;
    objects[3] = NULL;
    for(i = 0; objects[i] != NULL; ++i) {
        if(visible)
            lv_obj_remove_flag(objects[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects[i], LV_OBJ_FLAG_HIDDEN);
    }
    if(!visible) {
        lv_obj_add_flag(bar->charge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar->playing, LV_OBJ_FLAG_HIDDEN);
    }
    if(visible) {
        bar->rendered_charging = -1;
        bar->rendered_playing = -1;
    }
}

#endif
