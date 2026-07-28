#include "config.h"

#ifdef IPOD_6G

#include "lvgl.h"

#include "../../crazypod_lcd.h"
#include "../presentation/crazypod_glass_slots.h"
#include "../presentation/crazypod_overlay_glass.h"
#include "../presentation/crazypod_screen_corners.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_shell.h"
#include "crazypod_status_bar.h"

#define COLOR_DETAIL 0x08080D

static struct {
    lv_obj_t *screen;
    lv_obj_t *content;
    bool active;
} shell;

void crazypod_shell_create(
    lv_obj_t *desktop, void (*boost)(int ticks))
{
    shell.screen = lv_obj_create(desktop);
    crazypod_ui_widget_make_plain(shell.screen);
    lv_obj_set_pos(shell.screen, 0, 0);
    lv_obj_set_size(shell.screen, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(
        shell.screen, lv_color_hex(COLOR_DETAIL), 0);
    lv_obj_set_style_bg_opa(
        shell.screen, LV_OPA_COVER, 0);

    shell.content = lv_obj_create(shell.screen);
    crazypod_ui_widget_make_plain(shell.content);
    lv_obj_set_pos(shell.content, 0, 0);
    lv_obj_set_size(shell.content, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(
        shell.content, LV_OPA_TRANSP, 0);
    crazypod_status_bar_create(1, shell.screen);
    crazypod_screen_corners_create(shell.screen, 1);
    crazypod_glass_slots_configure(boost);
    crazypod_overlay_glass_configure(boost);
    shell.active = false;
    lv_obj_add_flag(shell.screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(shell.screen);
}

lv_obj_t *crazypod_shell_product_screen(void)
{
    return shell.screen;
}

lv_obj_t *crazypod_shell_product_content(void)
{
    return shell.content;
}

bool crazypod_shell_product_active(void)
{
    return shell.active;
}

void crazypod_shell_open_product(void)
{
    shell.active = true;
    lv_obj_remove_flag(shell.screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(shell.screen);
}

void crazypod_shell_close_product(void)
{
    shell.active = false;
    lv_obj_add_flag(shell.screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(shell.screen);
}

#endif
