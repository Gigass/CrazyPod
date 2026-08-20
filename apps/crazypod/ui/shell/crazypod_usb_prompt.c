#include "config.h"

#include "../../crazypod_l10n.h"

#if defined(IPOD_6G) && defined(HAVE_USB_POWER) && !defined(USB_NONE)

#include <string.h>

#include "button.h"
#include "kernel.h"
#include "settings.h"
#include "usb.h"

#include "lvgl.h"

#include "../../crazypod_frameclock.h"
#include "../../crazypod_state.h"
#include "../presentation/crazypod_popup_layout.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_desktop_native.h"
#include "crazypod_usb_prompt.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_PANEL 0x1B1B22
#define USB_PROMPT_TIMEOUT (5 * HZ)

struct usb_prompt_state {
    lv_obj_t *parent;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *rows[2];
    lv_obj_t *markers[2];
    int selected;
    unsigned request;
    struct crazypod_usb_prompt_callbacks callbacks;
    struct semaphore response;
    volatile bool registered;
    volatile bool ui_ready;
    volatile bool waiting;
    volatile unsigned request_id;
    volatile int result;
    bool data_blocking;
    bool teardown_pending;
};

static struct usb_prompt_state prompt;

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

void crazypod_usb_prompt_configure(
    lv_obj_t *parent,
    const struct crazypod_usb_prompt_callbacks *callbacks)
{
    prompt.parent = parent;
    if(callbacks != NULL)
        prompt.callbacks = *callbacks;
}

bool crazypod_usb_prompt_visible(void)
{
    return prompt.root != NULL || prompt.teardown_pending;
}

bool crazypod_usb_prompt_matches_request(unsigned request)
{
    return prompt.root != NULL && prompt.request == request;
}

bool crazypod_usb_prompt_data_blocking(void)
{
    return prompt.root != NULL && prompt.data_blocking;
}

static void refresh_prompt(void)
{
    int index;

    if(prompt.root == NULL)
        return;
    for(index = 0; index < 2; ++index) {
        bool selected = index == prompt.selected;

        lv_obj_set_style_bg_color(
            prompt.rows[index],
            lv_color_hex(selected ? COLOR_WHITE : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(
            prompt.rows[index], selected ? 34 : LV_OPA_TRANSP, 0);
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

void crazypod_usb_prompt_dismiss(void)
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
    prompt.request = 0;
    prompt.data_blocking = false;
    display = lv_obj_get_display(root);
    lv_display_remove_event_cb_with_user_data(
        display, teardown_refresh_ready, NULL);
    lv_display_add_event_cb(
        display, teardown_refresh_ready, LV_EVENT_REFR_READY, NULL);
    lv_obj_delete(root);
}

static void apply_charge_mode(void)
{
#ifdef HAVE_USB_CHARGING_ENABLE
    if(global_settings.usb_charging != USB_CHARGING_FORCE) {
        global_settings.usb_charging = USB_CHARGING_FORCE;
        usb_charging_enable(global_settings.usb_charging);
        crazypod_state_mark_dirty();
    }
#endif
}

void crazypod_usb_prompt_show(unsigned request)
{
    static const char *const titles[] = { CP_TR("Charge"), CP_TR("Data") };
    static const char *const symbols[] = {
        LV_SYMBOL_CHARGE, LV_SYMBOL_DOWNLOAD
    };
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *dimmer;
    struct crazypod_popup_geometry geometry;
    int content_width;
    int title_y = 7;
    int detail_y;
    int rows_y;
    int row_height = 30;
    int row_gap = 5;
    int row_inset = 12;
    int row_side_reserve = 28;
    int text_gap = 8;
    int index;

    if(prompt.parent == NULL || prompt.callbacks.create_panel == NULL) {
        prompt.result = USB_MODE_CHARGE;
        if(prompt.waiting)
            semaphore_release(&prompt.response);
        return;
    }
    if(crazypod_usb_prompt_visible())
        crazypod_desktop_native_preserve_modal_underlay();
    crazypod_usb_prompt_dismiss();
    if(prompt.callbacks.before_show != NULL)
        prompt.callbacks.before_show();
    prompt.request = request;
    prompt.selected = 0;
    prompt.data_blocking = false;
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
        CP_TR("Choose Mode"), &lv_font_montserrat_12) + 32;
    for(index = 0; index < 2; ++index) {
        int required_width = crazypod_popup_text_width(
            titles[index], &lv_font_montserrat_10) +
            crazypod_popup_text_width(
                symbols[index], &lv_font_montserrat_12) +
            text_gap + 2 * row_side_reserve + 2 * row_inset;

        if(required_width > content_width)
            content_width = required_width;
    }
    detail_y = title_y +
        crazypod_popup_wrapped_text_height(
            CP_TR("USB CONNECTED"), &lv_font_montserrat_10,
            LCD_WIDTH, 0) + 10;
    rows_y = detail_y +
        crazypod_popup_wrapped_text_height(
            CP_TR("Choose Mode"), &lv_font_montserrat_12,
            LCD_WIDTH, 0) + 13;
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            content_width, 0, 176, LCD_WIDTH - 32),
        rows_y + 2 * row_height + row_gap + 11);
    prompt.panel = prompt.callbacks.create_panel(
        prompt.root, geometry.x, geometry.y,
        geometry.width, geometry.height);
    title = make_label(
        prompt.panel, CP_TR("USB CONNECTED"), &lv_font_montserrat_10,
        COLOR_WHITE, 110);
    lv_obj_set_width(title, geometry.width);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, title_y);
    detail = make_label(
        prompt.panel, CP_TR("Choose Mode"), &lv_font_montserrat_12,
        COLOR_WHITE, 235);
    lv_obj_set_width(detail, geometry.width);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail, 0, detail_y);

    for(index = 0; index < 2; ++index) {
        lv_obj_t *symbol;
        lv_obj_t *label;
        int row_width = geometry.width - 2 * row_inset;
        int center_width = row_width - 2 * row_side_reserve;
        int symbol_width = crazypod_popup_text_width(
            symbols[index], &lv_font_montserrat_12);
        int label_width = crazypod_popup_text_width(
            titles[index], &lv_font_montserrat_10);
        int maximum_label_width;
        int group_width;
        int group_x;
        int y = rows_y + index * (row_height + row_gap);

        prompt.rows[index] = make_box(
            prompt.panel, row_inset, y, row_width, row_height, 9,
            COLOR_WHITE, LV_OPA_TRANSP);
        maximum_label_width = center_width - symbol_width - text_gap;
        if(maximum_label_width < 1)
            maximum_label_width = 1;
        if(label_width > maximum_label_width)
            label_width = maximum_label_width;
        group_width = symbol_width + text_gap + label_width;
        group_x = row_side_reserve +
            (center_width - group_width) / 2;
        symbol = make_label(
            prompt.rows[index], symbols[index],
            &lv_font_montserrat_12, COLOR_WHITE, 220);
        lv_obj_set_width(symbol, symbol_width);
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
            label, group_x + symbol_width + text_gap,
            CRAZYPOD_UI_ROW_LABEL_TEXT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        prompt.markers[index] = make_label(
            prompt.rows[index], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 90);
        lv_obj_set_width(prompt.markers[index], 24);
        lv_obj_set_style_text_align(
            prompt.markers[index], LV_TEXT_ALIGN_CENTER, 0);
        crazypod_ui_widget_align_row_label(
            prompt.markers[index], row_width - row_side_reserve,
            CRAZYPOD_UI_ROW_LABEL_MARKER);
    }
    refresh_prompt();
    if(prompt.callbacks.animate_panel != NULL)
        prompt.callbacks.animate_panel(
            prompt.panel, geometry.y);
}

static void show_data_blocker(void)
{
    lv_obj_t *label;

    if(prompt.root == NULL)
        return;

    lv_anim_delete(prompt.panel, NULL);
    lv_obj_clean(prompt.root);
    prompt.panel = NULL;
    memset(prompt.rows, 0, sizeof(prompt.rows));
    memset(prompt.markers, 0, sizeof(prompt.markers));
    prompt.data_blocking = true;

    lv_obj_set_style_bg_color(prompt.root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(prompt.root, LV_OPA_COVER, 0);
    lv_obj_move_foreground(prompt.root);

    label = make_label(
        prompt.root, LV_SYMBOL_USB, &lv_font_montserrat_24,
        COLOR_WHITE, 220);
    lv_obj_set_size(label, LCD_WIDTH, 30);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 65);

    label = make_label(
        prompt.root, CP_TR("USB DATA MODE"), &lv_font_montserrat_16,
        COLOR_WHITE, 235);
    lv_obj_set_size(label, LCD_WIDTH, 20);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    /*
     * The USB worker is released immediately after this function returns.
     * Present synchronously so the blocker reaches the LCD before storage
     * teardown can stall the UI thread.
     */
    lv_obj_invalidate(prompt.root);
    lv_refr_now(NULL);
    crazypod_present_now();
}

void crazypod_usb_prompt_finish(int mode)
{
    unsigned request = prompt.request;

    if(prompt.root == NULL)
        return;
    if(mode == USB_MODE_CHARGE)
        apply_charge_mode();
    prompt.result = mode;
    if(mode == USB_MODE_MASS_STORAGE)
        show_data_blocker();
    else
        crazypod_usb_prompt_dismiss();
    if(prompt.waiting && request == prompt.request_id)
        semaphore_release(&prompt.response);
}

static void move_selection(int direction)
{
    int next;

    if(prompt.root == NULL)
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

bool crazypod_usb_prompt_handle_button(
    long base, bool repeated, intptr_t data)
{
    (void)data;

    if(prompt.root == NULL)
        return false;
    if(prompt.data_blocking)
        return true;
    if(base == BUTTON_SCROLL_FWD || base == BUTTON_RIGHT)
        move_selection(1);
    else if(base == BUTTON_SCROLL_BACK || base == BUTTON_LEFT)
        move_selection(-1);
    else if(base == BUTTON_SELECT && !repeated)
        crazypod_usb_prompt_finish(
            prompt.selected == 0
                ? USB_MODE_CHARGE : USB_MODE_MASS_STORAGE);
    else if(base == BUTTON_MENU && !repeated)
        crazypod_usb_prompt_finish(USB_MODE_CHARGE);
    else if(base == BUTTON_PLAY && !repeated)
        crazypod_usb_prompt_finish(USB_MODE_MASS_STORAGE);
    return true;
}

static void inserted_event(unsigned short id, void *data)
{
    int *mode = data;

    (void)id;
    if(mode == NULL)
        return;
    if(!prompt.registered || !prompt.ui_ready) {
        *mode = USB_MODE_CHARGE;
        return;
    }
    while(semaphore_wait(&prompt.response, TIMEOUT_NOBLOCK) ==
          OBJ_WAIT_SUCCEEDED) {
    }
    prompt.result = USB_MODE_CHARGE;
    prompt.waiting = true;
    ++prompt.request_id;
    button_queue_post(CRAZYPOD_USB_PROMPT_REQUEST, prompt.request_id);
    if(semaphore_wait(&prompt.response, USB_PROMPT_TIMEOUT) !=
       OBJ_WAIT_SUCCEEDED)
        prompt.result = USB_MODE_CHARGE;
    if(prompt.result == USB_MODE_CHARGE)
        apply_charge_mode();
    *mode = prompt.result;
    prompt.waiting = false;
    button_queue_post(CRAZYPOD_USB_PROMPT_DONE, prompt.request_id);
}

void crazypod_usb_prompt_register(void)
{
    if(prompt.registered)
        return;
    semaphore_init(&prompt.response, 1, 0);
    prompt.result = USB_MODE_CHARGE;
    prompt.registered =
        add_event(SYS_EVENT_USB_INSERTED, inserted_event) != 0;
}

void crazypod_usb_prompt_set_ui_ready(bool ready)
{
    prompt.ui_ready = ready;
}

#endif
