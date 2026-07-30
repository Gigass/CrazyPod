#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_eq_studio_screen.h"

#define WHITE 0xFFFFFF
#define MUTED 0x8E8E93
#define GREEN 0x30D158
#define ROSE 0xFF375F
#define CYAN 0x26CFF5
#define AMBER 0xFFB340
#define GAIN_MAX 240

static void chip(
    lv_obj_t *parent, int x, const char *title,
    bool active, uint32_t primary_color)
{
    lv_obj_t *box = crazypod_ui_widget_box(
        parent, x, 194, 58, 18, 9,
        active ? primary_color : WHITE, active ? 210 : 20);
    lv_obj_t *label = crazypod_ui_widget_label(
        box, title, &lv_font_montserrat_8,
        WHITE, active ? 255 : 140);

    lv_obj_set_width(label, 58);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 5);
}

static int bar_y(int gain)
{
    return 124 - gain * 38 / GAIN_MAX;
}

void crazypod_eq_studio_screen_render(
    lv_obj_t *parent,
    const struct crazypod_eq_studio_model *model,
    const lv_font_t *metadata_font,
    uint32_t primary_color)
{
    static const char *const fixed_labels[EQ_NUM_BANDS] = {
        "32", "64", "125", "250", "500",
        CP_TR("1k"), CP_TR("2k"), CP_TR("4k"), CP_TR("8k"), CP_TR("16k")
    };
    const struct eq_band_setting *current =
        &model->bands[model->band];
    lv_obj_t *label;
    char text[96];
    char gain_text[24];
    char frequency_text[24];
    char q_text[24];
    char precut_text[24];
    int max_gain = 0;
    int index;
    bool clipping_risk;

    crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
        0x050508, LV_OPA_COVER);
    crazypod_ui_widget_box(
        parent, 0, 29, LCD_WIDTH, 35, 0, 0x101017, 235);
    label = crazypod_ui_widget_label(
        parent, CP_TR("EQ Studio"), metadata_font, WHITE, 245);
    lv_obj_set_pos(label, 14, 39);
    lv_obj_set_width(label, 120);
    label = crazypod_ui_widget_label(
        parent, model->enabled ? CP_TR("On") : CP_TR("Bypass"),
        &lv_font_montserrat_10,
        model->enabled ? GREEN : MUTED, 240);
    lv_obj_set_width(label, 60);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 190, 41);
    label = crazypod_ui_widget_label(
        parent, model->editing ? CP_TR("EDIT") : CP_TR("BROWSE"),
        &lv_font_montserrat_8, WHITE, 125);
    lv_obj_set_width(label, 54);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 252, 42);

    crazypod_eq_studio_format_db(
        gain_text, sizeof(gain_text), current->gain);
    crazypod_eq_studio_format_frequency(
        frequency_text, sizeof(frequency_text), current->cutoff);
    crazypod_eq_studio_format_q(
        q_text, sizeof(q_text), current->q);
    crazypod_eq_studio_format_precut(
        precut_text, sizeof(precut_text), model->precut);
    snprintf(text, sizeof(text), "%s  %s  %s",
             frequency_text, gain_text, q_text);
    label = crazypod_ui_widget_label(
        parent, text, &lv_font_montserrat_10, WHITE, 180);
    lv_obj_set_pos(label, 14, 66);
    lv_obj_set_width(label, 198);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    for(index = 0; index < EQ_NUM_BANDS; ++index) {
        if(model->bands[index].gain > max_gain)
            max_gain = model->bands[index].gain;
    }
    clipping_risk = max_gain > 0 && max_gain > model->precut;
    snprintf(text, sizeof(text), CP_FMT("Precut %s"), precut_text);
    label = crazypod_ui_widget_label(
        parent, text, &lv_font_montserrat_8,
        clipping_risk ? AMBER : WHITE,
        clipping_risk ? 235 : 125);
    lv_obj_set_width(label, 90);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 216, 68);

    crazypod_ui_widget_box(
        parent, 14, 124, 292, 1, 0, WHITE, 70);
    crazypod_ui_widget_box(
        parent, 14, 84, 292, 1, 0, WHITE, 18);
    crazypod_ui_widget_box(
        parent, 14, 163, 292, 1, 0, WHITE, 18);
    label = crazypod_ui_widget_label(
        parent, CP_TR("0 dB"), &lv_font_montserrat_8, WHITE, 95);
    lv_obj_set_pos(label, 16, 113);

    for(index = 0; index < EQ_NUM_BANDS; ++index) {
        int gain = model->bands[index].gain;
        int absolute = gain < 0 ? -gain : gain;
        int height = absolute * 38 / GAIN_MAX;
        int x = 29 + index * 27;
        int y = gain >= 0 ? 124 - height : 125;
        int width = index == model->band ? 16 : 10;
        uint32_t color = gain >= 0 ? GREEN : ROSE;
        lv_opa_t opacity = model->enabled ? 230 : 80;
        lv_obj_t *bar;

        if(height < 2)
            height = 2;
        if(index == model->band)
            color = primary_color;
        bar = crazypod_ui_widget_box(
            parent, x - width / 2, y, width, height,
            4, color, opacity);
        if(index == model->band) {
            lv_obj_set_style_border_width(bar, 1, 0);
            lv_obj_set_style_border_color(
                bar, lv_color_hex(WHITE), 0);
            lv_obj_set_style_border_opa(bar, 95, 0);
        }
        crazypod_ui_widget_box(
            parent, x - 2, bar_y(gain) - 2, 4, 4,
            LV_RADIUS_CIRCLE, color, opacity);
        label = crazypod_ui_widget_label(
            parent, fixed_labels[index], &lv_font_montserrat_8,
            index == model->band ? WHITE : MUTED,
            index == model->band ? 235 : 110);
        lv_obj_set_width(label, 28);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x - 14, 168);
    }

    crazypod_ui_widget_box(
        parent, 0, 184, LCD_WIDTH, 34, 0, 0x111119, 235);
    label = crazypod_ui_widget_label(
        parent, fixed_labels[model->band],
        &lv_font_montserrat_16, WHITE, 245);
    lv_obj_set_pos(label, 14, 188);
    lv_obj_set_width(label, 44);
    label = crazypod_ui_widget_label(
        parent, crazypod_eq_studio_band_role(model->band),
        &lv_font_montserrat_8, WHITE, 120);
    lv_obj_set_pos(label, 62, 190);
    lv_obj_set_width(label, 75);
    label = crazypod_ui_widget_label(
        parent, crazypod_eq_studio_mode_title(model->mode),
        &lv_font_montserrat_10, CYAN, 225);
    lv_obj_set_pos(label, 142, 189);
    lv_obj_set_width(label, 60);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    label = crazypod_ui_widget_label(
        parent,
        model->editing ? CP_TR("Wheel adjusts") : CP_TR("Wheel selects"),
        &lv_font_montserrat_8, WHITE, 115);
    lv_obj_set_pos(label, 205, 190);
    lv_obj_set_width(label, 98);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);

    chip(parent, 14, CP_TR("Gain"),
         model->mode == CRAZYPOD_EQ_STUDIO_GAIN, primary_color);
    chip(parent, 80, CP_TR("Freq"),
         model->mode == CRAZYPOD_EQ_STUDIO_CUTOFF, primary_color);
    chip(parent, 146, CP_TR("Q"),
         model->mode == CRAZYPOD_EQ_STUDIO_Q, primary_color);
    chip(parent, 212, CP_TR("Precut"),
         model->mode == CRAZYPOD_EQ_STUDIO_PRECUT, primary_color);

    crazypod_ui_widget_box(
        parent, 0, 218, LCD_WIDTH, 22, 0, 0x050508, 245);
    label = crazypod_ui_widget_label(
        parent, CP_TR("Menu Done"), &lv_font_montserrat_8, WHITE, 125);
    lv_obj_set_pos(label, 14, 225);
    label = crazypod_ui_widget_label(
        parent, CP_TR("Select Edit"), &lv_font_montserrat_8, WHITE, 165);
    lv_obj_set_pos(label, 113, 225);
    label = crazypod_ui_widget_label(
        parent, model->editing ? CP_TR("Play Mode") : CP_TR("Play A/B"),
        &lv_font_montserrat_8, WHITE, 125);
    lv_obj_set_width(label, 82);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 224, 225);
}

#endif
