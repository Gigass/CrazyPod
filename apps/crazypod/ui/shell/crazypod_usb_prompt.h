#ifndef CRAZYPOD_USB_PROMPT_H
#define CRAZYPOD_USB_PROMPT_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "events.h"
#include "lvgl.h"

#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
#define CRAZYPOD_USB_PROMPT_REQUEST \
    MAKE_SYS_EVENT(SYS_EVENT_CLS_PRIVATE, 0x31)
#define CRAZYPOD_USB_PROMPT_DONE \
    MAKE_SYS_EVENT(SYS_EVENT_CLS_PRIVATE, 0x32)

struct crazypod_usb_prompt_callbacks {
    void (*before_show)(void);
    lv_obj_t *(*create_panel)(
        lv_obj_t *parent, int x, int y, int width, int height);
    void (*animate_panel)(lv_obj_t *panel, int target_y);
    void (*dismissed)(void);
};

void crazypod_usb_prompt_configure(
    lv_obj_t *parent,
    const struct crazypod_usb_prompt_callbacks *callbacks);
void crazypod_usb_prompt_register(void);
void crazypod_usb_prompt_set_ui_ready(bool ready);
bool crazypod_usb_prompt_visible(void);
bool crazypod_usb_prompt_matches_request(unsigned request);
void crazypod_usb_prompt_show(unsigned request);
void crazypod_usb_prompt_finish(int mode);
void crazypod_usb_prompt_dismiss(void);
bool crazypod_usb_prompt_handle_button(
    long base, bool repeated, intptr_t data);
#else
static inline bool crazypod_usb_prompt_visible(void)
{
    return false;
}
#endif

#endif
