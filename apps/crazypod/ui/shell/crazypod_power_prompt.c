#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <string.h>

#include "button.h"
#include "events.h"
#include "queue.h"

#include "lvgl.h"

#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_power_prompt.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_PANEL 0x1B1B22

struct power_prompt_state {
    lv_obj_t *parent;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *rows[2];
    lv_obj_t *markers[2];
    lv_obj_t *hints[2];
    int selected;
    bool play_holding;
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
    return prompt.root != NULL;
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
        lv_obj_set_style_text_opa(
            prompt.hints[index], selected ? 190 : 120, 0);
    }
}

void crazypod_power_prompt_dismiss(void)
{
    if(prompt.root != NULL) {
        lv_anim_delete(prompt.panel, NULL);
        lv_obj_delete(prompt.root);
    }
    prompt.root = NULL;
    prompt.panel = NULL;
    memset(prompt.rows, 0, sizeof(prompt.rows));
    memset(prompt.markers, 0, sizeof(prompt.markers));
    memset(prompt.hints, 0, sizeof(prompt.hints));
    prompt.selected = 0;
    if(prompt.callbacks.dismissed != NULL)
        prompt.callbacks.dismissed();
}

void crazypod_power_prompt_show(void)
{
    static const char *const titles[] = { CP_TR("Shut Down"), CP_TR("Restart") };
    static const char *const hints[] = {
        CP_TR("Turn CrazyPod off"), CP_TR("Restart CrazyPod")
    };
    static const char *const symbols[] = {
        LV_SYMBOL_POWER, LV_SYMBOL_REFRESH
    };
    lv_obj_t *title;
    lv_obj_t *detail;
    int index;

    if(prompt.parent == NULL || crazypod_power_prompt_visible() ||
       prompt.callbacks.create_panel == NULL)
        return;
    if(prompt.callbacks.before_show != NULL)
        prompt.callbacks.before_show();
    prompt.selected = 0;
    prompt.root = make_box(
        prompt.parent, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0x000000, 86);
    lv_obj_remove_flag(prompt.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(prompt.root);
    prompt.panel = prompt.callbacks.create_panel(
        prompt.root, 35, 55, 250, 132);
    title = make_label(
        prompt.panel, CP_TR("POWER"), &lv_font_montserrat_10,
        COLOR_WHITE, 110);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 14);
    detail = make_label(
        prompt.panel, CP_TR("Choose Action"), &lv_font_montserrat_12,
        COLOR_WHITE, 235);
    lv_obj_set_width(detail, 250);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, 0, 29);

    for(index = 0; index < 2; ++index) {
        lv_obj_t *symbol;
        lv_obj_t *label;
        int y = 53 + index * 35;

        prompt.rows[index] = make_box(
            prompt.panel, 16, y, 218, 30, 9,
            COLOR_WHITE, LV_OPA_TRANSP);
        symbol = make_label(
            prompt.rows[index], symbols[index],
            &lv_font_montserrat_12, COLOR_WHITE, 220);
        lv_obj_set_pos(symbol, 13, 7);
        label = make_label(
            prompt.rows[index], titles[index],
            &lv_font_montserrat_10, COLOR_WHITE, 235);
        lv_obj_set_pos(label, 39, 4);
        lv_obj_set_size(label, 86, 14);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        prompt.hints[index] = make_label(
            prompt.rows[index], hints[index],
            &lv_font_montserrat_8, COLOR_WHITE, 130);
        lv_obj_set_pos(prompt.hints[index], 39, 17);
        lv_obj_set_size(prompt.hints[index], 142, 11);
        lv_label_set_long_mode(
            prompt.hints[index], LV_LABEL_LONG_MODE_DOTS);
        prompt.markers[index] = make_label(
            prompt.rows[index], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 90);
        lv_obj_set_width(prompt.markers[index], 24);
        lv_obj_set_style_text_align(
            prompt.markers[index], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(prompt.markers[index], 184, 11);
    }
    refresh_prompt();
    if(prompt.callbacks.animate_panel != NULL)
        prompt.callbacks.animate_panel(prompt.panel, 55);
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
