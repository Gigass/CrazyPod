#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <string.h>

#include "button.h"
#include "events.h"
#include "queue.h"

#include "lvgl.h"

#include "../presentation/crazypod_popup_layout.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_desktop_native.h"
#include "crazypod_power_prompt.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_PANEL 0x1B1B22

struct power_prompt_state {
    lv_obj_t *parent;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *rows[2];
    lv_obj_t *markers[2];
    int selected;
    bool play_holding;
    bool teardown_pending;
    long play_hold_start;
    struct crazypod_power_prompt_callbacks callbacks;
};

static struct power_prompt_state prompt;

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

void crazypod_power_prompt_configure(
    lv_obj_t *parent,
    const struct crazypod_power_prompt_callbacks *callbacks)
{
    prompt.parent = parent;
    if(callbacks != NULL)
        prompt.callbacks = *callbacks;
}

bool crazypod_power_prompt_visible(void)
{
    return prompt.root != NULL || prompt.teardown_pending;
}

static void refresh_prompt(void)
{
    int index;

    if(!crazypod_power_prompt_visible())
        return;
    for(index = 0; index < 2; ++index) {
        bool selected = index == prompt.selected;

        lv_obj_set_style_bg_color(
            prompt.rows[index],
            lv_color_hex(selected ? COLOR_WHITE : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(
            prompt.rows[index],
            selected ? 34 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(
            prompt.rows[index], selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(
            prompt.rows[index], lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            prompt.rows[index], selected ? 72 : 0, 0);
        CP_LV_LABEL_SET_TEXT(
            prompt.markers[index],
            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_opa(
            prompt.markers[index], selected ? 210 : 90, 0);
    }
}

static void teardown_refresh_ready(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);

    lv_display_remove_event_cb_with_user_data(
        display, teardown_refresh_ready, NULL);
    if(!prompt.teardown_pending)
        return;
    prompt.teardown_pending = false;
    if(prompt.callbacks.dismissed != NULL)
        prompt.callbacks.dismissed();
}

void crazypod_power_prompt_dismiss(void)
{
    lv_obj_t *root = prompt.root;
    bool had_prompt = root != NULL;
    lv_display_t *display;

    if(!had_prompt)
        return;

    if(prompt.panel != NULL)
        lv_anim_delete(prompt.panel, NULL);
    prompt.teardown_pending = had_prompt;
    prompt.root = NULL;
    prompt.panel = NULL;
    memset(prompt.rows, 0, sizeof(prompt.rows));
    memset(prompt.markers, 0, sizeof(prompt.markers));
    prompt.selected = 0;
    display = lv_obj_get_display(root);
    lv_display_remove_event_cb_with_user_data(
        display, teardown_refresh_ready, NULL);
    lv_display_add_event_cb(
        display, teardown_refresh_ready, LV_EVENT_REFR_READY, NULL);
    lv_obj_delete(root);
}

void crazypod_power_prompt_show(void)
{
    static const char *const titles[] = { CP_TR("Restart"), CP_TR("Shutdown") };
    static const char *const symbols[] = {
        LV_SYMBOL_REFRESH, LV_SYMBOL_POWER
    };
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *dimmer;
    struct crazypod_popup_geometry geometry;
    int content_width;
    int symbol_column_width = 0;
    int label_column_width = 0;
    int title_y = 7;
    int detail_y;
    int options_y;
    int option_height = 38;
    int option_gap = 8;
    int option_inset = 12;
    int option_inner_padding = 8;
    int marker_width = 16;
    int text_gap = 6;
    int option_width;
    int maximum_label_width;
    int index;

    if(prompt.parent == NULL || crazypod_power_prompt_visible() ||
       prompt.callbacks.create_panel == NULL)
        return;
    if(prompt.callbacks.before_show != NULL)
        prompt.callbacks.before_show();
    prompt.selected = 0;
    prompt.root = make_box(
        prompt.parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(prompt.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(prompt.root);
    (void)crazypod_desktop_native_create_modal_underlay(
        prompt.root);
    dimmer = make_box(
        prompt.root, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, 86);
    lv_obj_remove_flag(dimmer, LV_OBJ_FLAG_CLICKABLE);
    content_width = crazypod_popup_text_width(
        CP_TR("Choose Action"), &lv_font_montserrat_12) +
        2 * option_inset;
    if(crazypod_popup_text_width(
           CP_TR("POWER"), &lv_font_montserrat_10) +
       2 * option_inset > content_width)
        content_width = crazypod_popup_text_width(
            CP_TR("POWER"), &lv_font_montserrat_10) +
            2 * option_inset;
    for(index = 0; index < 2; ++index) {
        int symbol_width = crazypod_popup_text_width(
            symbols[index], &lv_font_montserrat_12);
        int label_width = crazypod_popup_text_width(
            titles[index], &lv_font_montserrat_10);

        if(symbol_width > symbol_column_width)
            symbol_column_width = symbol_width;
        if(label_width > label_column_width)
            label_column_width = label_width;
    }
    if(2 * (2 * option_inner_padding + symbol_column_width +
            2 * text_gap + label_column_width + marker_width) +
       option_gap + 2 * option_inset > content_width)
        content_width =
            2 * (2 * option_inner_padding + symbol_column_width +
                 2 * text_gap + label_column_width + marker_width) +
            option_gap + 2 * option_inset;
    detail_y = title_y +
        crazypod_popup_wrapped_text_height(
            CP_TR("POWER"), &lv_font_montserrat_10,
            LCD_WIDTH, 0) + 10;
    options_y = detail_y +
        crazypod_popup_wrapped_text_height(
            CP_TR("Choose Action"), &lv_font_montserrat_12,
            LCD_WIDTH, 0) + 13;
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            content_width, 0, 236, LCD_WIDTH - 32),
        options_y + option_height + 11);
    prompt.panel = prompt.callbacks.create_panel(
        prompt.root, geometry.x, geometry.y,
        geometry.width, geometry.height);
    option_width =
        (geometry.width - 2 * option_inset - option_gap) / 2;
    maximum_label_width = option_width -
        2 * option_inner_padding - symbol_column_width -
        2 * text_gap - marker_width;
    if(maximum_label_width < 1)
        maximum_label_width = 1;
    title = make_label(
        prompt.panel, CP_TR("POWER"), &lv_font_montserrat_10,
        COLOR_WHITE, 110);
    lv_obj_set_width(title, geometry.width - 2 * option_inset);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, option_inset, title_y);
    detail = make_label(
        prompt.panel, CP_TR("Choose Action"), &lv_font_montserrat_12,
        COLOR_WHITE, 235);
    lv_obj_set_width(detail, geometry.width - 2 * option_inset);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, option_inset, detail_y);

    for(index = 0; index < 2; ++index) {
        lv_obj_t *symbol;
        lv_obj_t *label;
        int x = option_inset + index * (option_width + option_gap);
        int label_width = crazypod_popup_text_width(
            titles[index], &lv_font_montserrat_10);
        int group_width;
        int group_x;

        if(label_width > maximum_label_width)
            label_width = maximum_label_width;
        group_width = symbol_column_width + 2 * text_gap +
            label_width + marker_width;
        group_x = (option_width - group_width) / 2;

        prompt.rows[index] = make_box(
            prompt.panel, x, options_y,
            option_width, option_height, 9,
            COLOR_WHITE, LV_OPA_TRANSP);
        symbol = make_label(
            prompt.rows[index], symbols[index],
            &lv_font_montserrat_12, COLOR_WHITE, 220);
        lv_obj_set_width(symbol, symbol_column_width);
        lv_obj_set_style_text_align(
            symbol, LV_TEXT_ALIGN_CENTER, 0);
        crazypod_ui_widget_align_row_label(
            symbol, group_x, CRAZYPOD_UI_ROW_LABEL_MARKER);
        label = make_label(
            prompt.rows[index], titles[index],
            &lv_font_montserrat_10, COLOR_WHITE, 235);
        lv_obj_set_width(label, label_width);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        crazypod_ui_widget_align_row_label(
            label, group_x + symbol_column_width + text_gap,
            CRAZYPOD_UI_ROW_LABEL_TEXT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        prompt.markers[index] = make_label(
            prompt.rows[index], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 90);
        lv_obj_set_width(prompt.markers[index], marker_width);
        lv_obj_set_style_text_align(
            prompt.markers[index], LV_TEXT_ALIGN_CENTER, 0);
        crazypod_ui_widget_align_row_label(
            prompt.markers[index],
            group_x + symbol_column_width + text_gap +
                label_width + text_gap,
            CRAZYPOD_UI_ROW_LABEL_MARKER);
    }
    refresh_prompt();
    if(prompt.callbacks.animate_panel != NULL)
        prompt.callbacks.animate_panel(
            prompt.panel, geometry.y);
}

static void move_selection(int direction)
{
    int next;

    if(!crazypod_power_prompt_visible())
        return;
    next = prompt.selected + (direction > 0 ? 1 : -1);
    if(next < 0)
        next = 0;
    if(next > 1)
        next = 1;
    if(next == prompt.selected)
        return;
    prompt.selected = next;
    refresh_prompt();
}

static void activate_selection(void)
{
    int selected;

    if(!crazypod_power_prompt_visible())
        return;
    selected = prompt.selected;
    crazypod_power_prompt_dismiss();
    if(prompt.callbacks.execute != NULL)
        prompt.callbacks.execute(selected);
}

bool crazypod_power_prompt_handle_button(
    long base, bool repeated, intptr_t data)
{
    (void)data;

    if(!crazypod_power_prompt_visible())
        return false;
    if(base == BUTTON_SCROLL_FWD || base == BUTTON_RIGHT)
        move_selection(1);
    else if(base == BUTTON_SCROLL_BACK || base == BUTTON_LEFT)
        move_selection(-1);
    else if(base == BUTTON_SELECT && !repeated)
        activate_selection();
    else if(base == BUTTON_MENU && !repeated)
        crazypod_power_prompt_dismiss();
    return true;
}

bool crazypod_power_prompt_handle_play_hold(
    long button, long now, long hold_ticks)
{
    long base;
    bool repeated;

    if((button & SYS_EVENT) != 0)
        return false;
    base = button & BUTTON_MAIN;
    if(base != BUTTON_PLAY)
        return false;
    if((button & BUTTON_REL) != 0) {
        prompt.play_holding = false;
        return false;
    }
    repeated = (button & BUTTON_REPEAT) != 0;
    if(!repeated) {
        prompt.play_holding = true;
        prompt.play_hold_start = now;
        return false;
    }
    if(!prompt.play_holding) {
        prompt.play_holding = true;
        prompt.play_hold_start = now;
        return true;
    }
    if((long)(now - (prompt.play_hold_start + hold_ticks)) < 0)
        return true;

    prompt.play_holding = false;
    if(prompt.callbacks.before_hold_show != NULL)
        prompt.callbacks.before_hold_show();
    crazypod_power_prompt_show();
    return true;
}

#endif
