#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_apps.h"
#include "../../../crazypod_appearance.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../shell/crazypod_app_catalog.h"
#include "crazypod_settings_feature.h"
#include "crazypod_settings_model.h"
#include "crazypod_settings_catalog.h"

#define COLOR_WHITE 0xFFFFFF

struct crazypod_settings_preview_input {
    const struct route_state *state;
    const char *title;
    uint32_t primary_color;
    bool eq_enabled;
    bool shuffle_enabled;
    bool repeat_enabled;
};

struct crazypod_settings_preview_model {
    const char *title;
    const char *detail;
    const char *symbol;
    uint32_t swatch_color;
    char detail_buffer[48];
};

static void preview_model_build(
    const struct crazypod_settings_preview_input *input,
    struct crazypod_settings_preview_model *model)
{
    const struct crazypod_app_descriptor *app;
    const struct route_state *state;
    enum crazypod_app_id id;
    int action;
    int item;

    if(input == NULL || input->state == NULL || model == NULL)
        return;
    state = input->state;
    model->title = input->title != NULL ? input->title : "";
    model->detail = "";
    model->symbol = LV_SYMBOL_SETTINGS;
    model->swatch_color = input->primary_color;
    model->detail_buffer[0] = '\0';

    if(state->route == SETTINGS_ROUTE_MENU) {
        if(state->selected >= 0 &&
           state->selected < CRAZYPOD_SETTINGS_MENU_COUNT)
            model->symbol =
                crazypod_settings_menu_symbols[state->selected];
        model->detail =
            crazypod_ui_settings_group_detail(state->selected);
        return;
    }

    if(state->route == SETTINGS_ROUTE_MAIN_MENU) {
        id = crazypod_apps_ordered_id(state->selected);
        app = crazypod_app_catalog_find(id);
        model->symbol = app != NULL ? app->symbol : LV_SYMBOL_LIST;
        model->swatch_color =
            app != NULL ? app->color : input->primary_color;
        snprintf(model->detail_buffer, sizeof(model->detail_buffer),
                 CP_FMT("%s · Position %d"),
                 crazypod_apps_is_enabled(id) ? CP_FMT("Visible") : CP_FMT("In More"),
                 state->selected + 1);
        model->detail = model->detail_buffer;
        return;
    }

    if(state->route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS) {
        id = (enum crazypod_app_id)state->group;
        app = crazypod_app_catalog_find(id);
        action = state->selected + (crazypod_apps_is_fixed(id) ? 1 : 0);
        model->symbol = app != NULL ? app->symbol : LV_SYMBOL_LIST;
        model->swatch_color =
            app != NULL ? app->color : input->primary_color;
        model->detail = action == 0
            ? CP_TR("Changes More Features")
            : CP_TR("Changes launcher position");
        return;
    }

    item = crazypod_settings_catalog_item(state->route, state->selected);
    model->symbol = crazypod_ui_settings_item_symbol(item);
    model->detail = crazypod_ui_settings_item_value_label(item);
    if(item == SETTINGS_ITEM_EQ_ENABLED && input->eq_enabled)
        model->swatch_color = 0x26CFF5;
    else if(item == SETTINGS_ITEM_SHUFFLE && input->shuffle_enabled)
        model->swatch_color = 0xFF375F;
    else if(item == SETTINGS_ITEM_REPEAT && input->repeat_enabled)
        model->swatch_color = 0x30D158;
}

void crazypod_settings_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const char *title, uint32_t primary_color,
    uint32_t secondary_color, bool eq_enabled,
    bool shuffle_enabled, bool repeat_enabled)
{
    const struct crazypod_settings_preview_input input = {
        .state = state,
        .title = title,
        .primary_color = primary_color,
        .eq_enabled = eq_enabled,
        .shuffle_enabled = shuffle_enabled,
        .repeat_enabled = repeat_enabled,
    };
    struct crazypod_settings_preview_model model;
    lv_obj_t *text_panel;
    lv_obj_t *swatch;
    lv_obj_t *label;

    preview_model_build(&input, &model);
    swatch = crazypod_ui_widget_box(
        parent, 204, 76, 72, 72, 16,
        model.swatch_color, LV_OPA_COVER);
    if(crazypod_appearance_get()->highlight_style != 0) {
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
