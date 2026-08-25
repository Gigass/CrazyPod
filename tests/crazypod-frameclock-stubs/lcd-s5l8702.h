#ifndef CRAZYPOD_FRAMECLOCK_TEST_LCD_S5L8702_H
#define CRAZYPOD_FRAMECLOCK_TEST_LCD_S5L8702_H

#include <stdbool.h>

bool lcd_update_rect_frame_sync(
    int x, int y, int width, int height);
bool lcd_update_rect_music_sync(
    int x, int y, int width, int height);
bool lcd_update_full_sync(void);

#endif
