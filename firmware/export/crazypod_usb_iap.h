#ifndef CRAZYPOD_USB_IAP_H
#define CRAZYPOD_USB_IAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum crazypod_usb_iap_control {
    CRAZYPOD_USB_IAP_TOGGLE_PLAY_PAUSE = 0,
    CRAZYPOD_USB_IAP_STOP,
    CRAZYPOD_USB_IAP_NEXT,
    CRAZYPOD_USB_IAP_PREVIOUS,
};

bool crazypod_usb_iap_control(enum crazypod_usb_iap_control control);
void crazypod_usb_iap_adjust_volume(int direction);
bool crazypod_usb_iap_shuffle(void);
void crazypod_usb_iap_set_shuffle(bool enabled);
int crazypod_usb_iap_repeat(void);
void crazypod_usb_iap_set_repeat(int repeat_mode);
bool crazypod_usb_iap_copy_track_path(uint32_t index, char *buffer,
                                      size_t buffer_size);
bool crazypod_usb_iap_select_track(uint32_t index);

#endif
