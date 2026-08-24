#ifndef TEST_CRAZYPOD_IAP_BUTTON_H
#define TEST_CRAZYPOD_IAP_BUTTON_H

#include <stdint.h>

#define BUTTON_NONE        0x00000000
#define BUTTON_RC_VOL_DOWN 0x00008000
#define BUTTON_RC_VOL_UP   0x00010000
#define BUTTON_RC_RIGHT    0x00020000
#define BUTTON_RC_LEFT     0x00040000
#define BUTTON_RC_STOP     0x00080000
#define BUTTON_RC_PLAY     0x00100000
#define BUTTON_RC_MENU     0x00200000
#define BUTTON_RC_SELECT   0x00400000
#define BUTTON_RC_UP       0x00800000
#define BUTTON_RC_DOWN     0x01000000

void button_queue_post(long id, intptr_t data);

#endif
