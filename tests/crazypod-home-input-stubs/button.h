#ifndef TEST_CRAZYPOD_HOME_INPUT_BUTTON_H
#define TEST_CRAZYPOD_HOME_INPUT_BUTTON_H

#define BUTTON_SCROLL_FWD  0x0001
#define BUTTON_SCROLL_BACK 0x0002
#define BUTTON_RIGHT       0x0004
#define BUTTON_LEFT        0x0008
#define BUTTON_SELECT      0x0010
#define BUTTON_PLAY        0x0020
#define BUTTON_MENU        0x0040
#define BUTTON_MAIN        0x00ff
#define BUTTON_REL         0x0100
#define BUTTON_REPEAT      0x0200

static inline int button_apply_acceleration(unsigned int data)
{
    return (int)data;
}

#endif
