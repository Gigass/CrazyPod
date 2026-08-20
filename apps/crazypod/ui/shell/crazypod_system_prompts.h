#ifndef CRAZYPOD_SYSTEM_PROMPTS_H
#define CRAZYPOD_SYSTEM_PROMPTS_H

#include <stdint.h>

struct crazypod_system_prompts_host {
    long (*now)(void);
    void (*close_product)(void);
    void (*present)(void);
};

void crazypod_system_prompts_configure(
    const struct crazypod_system_prompts_host *host);
void crazypod_system_prompts_initialize_usb(void);
void crazypod_system_prompts_set_ui_ready(void);
bool crazypod_system_prompts_storage_active(void);
bool crazypod_system_prompts_power_visible(void);
bool crazypod_system_prompts_handle_power(
    long base, bool repeated, intptr_t data);
bool crazypod_system_prompts_handle_power_hold(long button);
void crazypod_system_prompts_dismiss_power(void);
#ifdef SIMULATOR
void crazypod_system_prompts_show_power(void);
#endif
bool crazypod_system_prompts_usb_visible(void);
bool crazypod_system_prompts_handle_usb(
    long base, bool repeated, intptr_t data);
bool crazypod_system_prompts_headphone_visible(void);
bool crazypod_system_prompts_handle_headphone(
    long base, bool repeated, intptr_t data);
void crazypod_system_prompts_headphone_changed(bool inserted);
void crazypod_system_prompts_show_usb(unsigned request);
void crazypod_system_prompts_usb_done(unsigned request);
void crazypod_system_prompts_usb_connected(intptr_t data);
void crazypod_system_prompts_usb_disconnected(void);
void crazypod_system_prompts_power_off(void);
void crazypod_system_prompts_reboot(void);

#endif
