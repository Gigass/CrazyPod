#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "lvgl.h"

#include "../../../crazypod_photos.h"
#include "../../../crazypod_presets.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_customize_catalog.h"
#include "crazypod_customize_feature.h"
#include "../now_playing/crazypod_now_playing_feature.h"

#define COLOR_WHITE 0xFFFFFF

struct crazypod_customize_preview_model {
    const char *title;
    const char *detail;
    const char *symbol;
    uint32_t swatch_color;
    bool gradient;
    bool editor;
};

static const char *basename(const char *path)
{
    const char *slash;

    if(path == NULL)
        return "";
    slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static const char *current_value(const struct route_state *state)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    enum crazypod_appearance_field field;
    int current;
    int value;

    if(state->route == DIY_ROUTE_ICONS)
        return state->selected == appearance->icon_theme
            ? CP_TR("Current selection") : CP_TR("Select to switch now");
    if(state->route == DIY_ROUTE_DETAILS) {
        field = crazypod_customize_detail_fields[state->selected];
        current = crazypod_customize_field_value(field);
        return crazypod_customize_choice_title(field, current);
    }
    if(state->route == DIY_ROUTE_LAYOUT) {
        static char radius_text[16];

        field = crazypod_customize_layout_fields[state->selected];
        snprintf(radius_text, sizeof(radius_text), CP_FMT("%d px"),
                 crazypod_customize_field_value(field));
        return radius_text;
    }
    if(state->route == DIY_ROUTE_NOW_PLAYING_THEMES)
        return crazypod_now_playing_theme_choice_current(state->selected)
            ? CP_TR("Current selection") : CP_TR("Apply");
    if(state->route == DIY_ROUTE_CHOICES) {
        field = (enum crazypod_appearance_field)state->group;
        current = crazypod_customize_field_value(field);
        value = crazypod_customize_choice_value(
            field, state->selected);
        return value == current
            ? CP_TR("Current selection") : CP_TR("Select to apply");
    }
    if(state->route == DIY_ROUTE_BACKGROUNDS) {
        enum crazypod_appearance_field background =
            crazypod_customize_background_field(state->selected);
        const char *path =
            crazypod_customize_background_wallpaper(
                appearance, background);
        int color = crazypod_customize_field_value(background);

        if(path[0] != '\0')
            return basename(path);
        return color == 0
            ? background == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND
                ? CP_TR("Follow Home") : CP_TR("Default")
            : crazypod_appearance_color_name(color - 1);
    }
    if(state->route == DIY_ROUTE_BACKGROUND_CHOICES) {
        field = (enum crazypod_appearance_field)state->group;
        const char *path =
            crazypod_customize_background_wallpaper(
                appearance, field);
        int color = crazypod_customize_field_value(field);

        if(state->selected == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1)
            return path[0] != '\0'
                ? basename(path) : CP_TR("Open /Pictures");
        return path[0] == '\0' && state->selected == color
            ? CP_TR("Current selection") : CP_TR("Select to apply");
    }
    if(state->route == DIY_ROUTE_WALLPAPER_FILES) {
        const char *current_path =
            crazypod_customize_background_wallpaper(
                appearance,
                (enum crazypod_appearance_field)state->group);

        return strcmp(
                   current_path,
                   crazypod_photo_path(state->selected)) == 0
            ? CP_TR("Current picture") : CP_TR("Select to crop");
    }
    return "";
}

static void preview_model_build(
    const struct route_state *state, const char *route_title,
    uint32_t primary_color, const char *editor_value,
    struct crazypod_customize_preview_model *model)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    memset(model, 0, sizeof(*model));
    model->title = route_title != NULL ? route_title : "";
    model->detail = current_value(state);
    model->symbol = LV_SYMBOL_SETTINGS;
    model->swatch_color = primary_color;
    model->gradient = appearance->highlight_style != 0;
    if(state->route == DIY_ROUTE_MENU) {
        model->symbol = crazypod_customize_menu_symbols[state->selected];
        model->detail =
            state->selected == 0 ? CP_TR("Save and reuse appearances") :
            state->selected == 1 ? CP_TR("16 complete icon themes") :
            state->selected == 2 ? CP_TR("Wave, size, glow and colors") :
            state->selected == 3 ? CP_TR("Home, menu and lock pictures") :
            state->selected == 4 ? CP_TR("Themes") :
                                   CP_TR("Screen corner radius");
    }
    else if(state->route == DIY_ROUTE_PRESETS) {
        model->symbol = state->selected == 0 ? LV_SYMBOL_SAVE :
            state->selected == 1 ? LV_SYMBOL_COPY : LV_SYMBOL_DOWNLOAD;
        model->detail = state->selected == 0
            ? CP_TR("Store the complete current appearance")
            : state->selected == 1
                ? CP_TR("Apply, export or edit saved appearances")
                : CP_TR("Copy import.upodtheme to /.crazypod");
    }
    else if(state->route == DIY_ROUTE_PRESET_LIBRARY) {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->selected);

        model->symbol = LV_SYMBOL_COPY;
        model->detail = preset != NULL && preset->builtin
            ? CP_TR("Built-in appearance") : CP_TR("User appearance");
    }
    else if(state->route == DIY_ROUTE_PRESET_ACTIONS ||
            state->route == DIY_ROUTE_PRESET_EDIT) {
        const struct crazypod_preset *preset =
            crazypod_preset_get(state->group);

        model->title = preset != NULL ? preset->name : CP_TR("Preset");
        model->symbol = state->route == DIY_ROUTE_PRESET_ACTIONS
            ? LV_SYMBOL_SAVE : LV_SYMBOL_EDIT;
        model->detail = route_title != NULL ? route_title : "";
    }
    else if(state->route == DIY_ROUTE_PRESET_RENAME) {
        model->title =
            editor_value != NULL && editor_value[0] != '\0'
                ? editor_value : CP_TR("New name");
        model->detail = CP_TR("Wheel selects characters; center adds.");
        model->symbol = LV_SYMBOL_KEYBOARD;
        model->editor = true;
    }
    else if(state->route == DIY_ROUTE_ICONS)
        model->symbol = LV_SYMBOL_IMAGE;
    else if(state->route == DIY_ROUTE_CHOICES) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)state->group;
        int value = crazypod_customize_choice_value(
            field, state->selected);

        if(field == CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE)
            model->symbol = LV_SYMBOL_AUDIO;
        if(field == CRAZYPOD_APPEARANCE_PRIMARY ||
           field == CRAZYPOD_APPEARANCE_SECONDARY)
            model->swatch_color = crazypod_appearance_color(value);
    }
    else if(state->route == DIY_ROUTE_BACKGROUNDS) {
        enum crazypod_appearance_field field =
            crazypod_customize_background_field(state->selected);
        int surface = crazypod_customize_field_value(field);

        model->symbol = LV_SYMBOL_DIRECTORY;
        model->swatch_color = surface == 0
            ? crazypod_customize_background_default_color(field)
            : crazypod_appearance_color(surface - 1);
        model->gradient = false;
    }
    else if(state->route == DIY_ROUTE_BACKGROUND_CHOICES) {
        if(state->selected > 0 &&
           state->selected <= CRAZYPOD_APPEARANCE_COLOR_COUNT)
            model->swatch_color =
                crazypod_appearance_color(state->selected - 1);
        model->symbol = state->selected ==
                CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
            ? LV_SYMBOL_IMAGE : LV_SYMBOL_DIRECTORY;
    }
    else if(state->route == DIY_ROUTE_WALLPAPER_FILES)
        model->symbol = LV_SYMBOL_IMAGE;
    else if(state->route == DIY_ROUTE_LAYOUT)
        model->symbol = LV_SYMBOL_SHUFFLE;
    else if(state->route == DIY_ROUTE_NOW_PLAYING_THEMES)
        model->symbol = LV_SYMBOL_AUDIO;
    else if(state->route == DIY_ROUTE_DETAILS) {
        if(state->selected == 1)
            model->symbol = LV_SYMBOL_AUDIO;
        else if(state->selected == 5)
            model->swatch_color = crazypod_appearance_color(
                appearance->secondary_color);
    }
}

static void render_editor(
    lv_obj_t *parent, const char *value,
    uint32_t primary_color, uint32_t secondary_color)
{
    lv_obj_t *card = crazypod_ui_widget_box(
        parent, 181, 78, 118, 64, 14, primary_color, 210);
    lv_obj_t *label;

    lv_obj_set_style_bg_grad_color(
        card, lv_color_hex(secondary_color), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_HOR, 0);
    label = crazypod_ui_widget_label(
        card, LV_SYMBOL_KEYBOARD, &lv_font_montserrat_16,
        COLOR_WHITE, 225);
    lv_obj_set_pos(label, 10, 9);
    label = crazypod_ui_widget_label(
        card, value != NULL && value[0] != '\0' ? value : CP_TR("New name"),
        &lv_font_montserrat_12, COLOR_WHITE,
        value != NULL && value[0] != '\0' ? 255 : 130);
    lv_obj_set_pos(label, 10, 35);
    lv_obj_set_width(label, 98);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    card = crazypod_preview_make_text_panel(parent, 154, 46);
    label = crazypod_ui_widget_label(
        card, CP_TR("Wheel selects characters; center adds."),
        &lv_font_montserrat_8, COLOR_WHITE, 125);
    lv_obj_set_pos(label, 11, 8);
    lv_obj_set_width(label, 118);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

void crazypod_customize_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const char *title, uint32_t primary_color,
    uint32_t secondary_color, const char *editor_value)
{
    struct crazypod_customize_preview_model model;
    lv_obj_t *text_panel;
    lv_obj_t *swatch;
    lv_obj_t *label;

    preview_model_build(
        state, title, primary_color, editor_value, &model);
    if(model.editor) {
        render_editor(
            parent, editor_value, primary_color, secondary_color);
        return;
    }

    swatch = crazypod_ui_widget_box(
        parent, 204, 76, 72, 72, 16,
        model.swatch_color, LV_OPA_COVER);
    if(model.gradient) {
        lv_obj_set_style_bg_grad_color(
            swatch, lv_color_hex(secondary_color), 0);
        lv_obj_set_style_bg_grad_dir(swatch, LV_GRAD_DIR_HOR, 0);
    }
    label = crazypod_ui_widget_label(
        swatch, model.symbol, &lv_font_montserrat_24,
        COLOR_WHITE, 225);
    lv_obj_center(label);

    text_panel = crazypod_preview_make_text_panel(parent, 158, 50);
    label = crazypod_ui_widget_label(
        text_panel, model.title, &lv_font_montserrat_12,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 6);
    label = crazypod_ui_widget_label(
        text_panel, model.detail, &lv_font_montserrat_8,
        COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 30);
}

#endif
