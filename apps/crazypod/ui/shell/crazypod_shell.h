#ifndef CRAZYPOD_SHELL_H
#define CRAZYPOD_SHELL_H

#include <stdbool.h>

#include "lvgl.h"

void crazypod_shell_create(
    lv_obj_t *desktop, void (*boost)(int ticks));
lv_obj_t *crazypod_shell_product_screen(void);
lv_obj_t *crazypod_shell_product_content(void);
bool crazypod_shell_product_active(void);
void crazypod_shell_open_product(void);
void crazypod_shell_close_product(void);

#endif
