#include "config.h"

#if defined(IPOD_6G) && defined(HAVE_USB_POWER) && !defined(USB_NONE)

#include <string.h>

#include "button.h"
#include "kernel.h"
#include "settings.h"
#include "usb.h"

#include "lvgl.h"

#include "../../crazypod_state.h"
#include "../presentation/crazypod_ui_widgets.h"
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
    lv_obj_t *hints[2];
    int selected;
    unsigned request;
    struct crazypod_usb_prompt_callbacks callbacks;
    struct semaphore response;
    volatile bool registered;
    volatile bool ui_ready;
    volatile bool waiting;
    volatile unsigned request_id;
    volatile int result;
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
    return prompt.root != NULL;
}

bool crazypod_usb_prompt_matches_request(unsigned request)
{
    return prompt.root != NULL && prompt.request == request;
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
        lv_label_set_text(
            prompt.markers[index],
            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_opa(
            prompt.markers[index], selected ? 210 : 90, 0);
        lv_obj_set_style_text_opa(
            prompt.hints[index], selected ? 190 : 120, 0);
    }
}

void crazypod_usb_prompt_dismiss(void)
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
    prompt.request = 0;
    if(prompt.callbacks.dismissed != NULL)
        prompt.callbacks.dismissed();
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
    static const char *const titles[] = { "Charge", "Data" };
    static const char *const hints[] = {
        "Keep CrazyPod running", "Mount disk on computer"
    };
    static const char *const symbols[] = {
        LV_SYMBOL_CHARGE, LV_SYMBOL_DOWNLOAD
    };
    lv_obj_t *title;
    lv_obj_t *detail;
    int index;

    if(prompt.parent == NULL || prompt.callbacks.create_panel == NULL) {
        prompt.result = USB_MODE_CHARGE;
        if(prompt.waiting)
            semaphore_release(&prompt.response);
        return;
    }
    crazypod_usb_prompt_dismiss();
    if(prompt.callbacks.before_show != NULL)
        prompt.callbacks.before_show();
    prompt.request = request;
    prompt.selected = 0;
    prompt.root = make_box(
        prompt.parent, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0x000000, 86);
    lv_obj_remove_flag(prompt.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(prompt.root);
    prompt.panel = prompt.callbacks.create_panel(
        prompt.root, 35, 55, 250, 132);
    title = make_label(
        prompt.panel, "USB CONNECTED", &lv_font_montserrat_10,
        COLOR_WHITE, 110);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 14);
    detail = make_label(
        prompt.panel, "Choose Mode", &lv_font_montserrat_12,
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

void crazypod_usb_prompt_finish(int mode)
{
    unsigned request = prompt.request;

    if(prompt.root == NULL)
        return;
    if(mode == USB_MODE_CHARGE)
        apply_charge_mode();
    prompt.result = mode;
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
