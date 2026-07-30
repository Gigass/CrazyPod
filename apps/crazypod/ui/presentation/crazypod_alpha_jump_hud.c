#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "crazypod_alpha_jump_hud.h"
#include "crazypod_ui_widgets.h"

#define COLOR_WHITE 0xFFFFFF
#define HUD_BACKGROUND 0x080A10
#define HUD_SIZE 78

static struct {
    lv_obj_t *parent;
    lv_obj_t *root;
    lv_obj_t *label;
    long deadline;
} hud;

void crazypod_alpha_jump_hud_configure(lv_obj_t *parent)
{
    hud.parent = parent;
    hud.root = NULL;
    hud.label = NULL;
    hud.deadline = 0;
}

void crazypod_alpha_jump_hud_reset(void)
{
    if(hud.root != NULL)
        lv_obj_delete(hud.root);
    hud.root = NULL;
    hud.label = NULL;
    hud.deadline = 0;
}

static void create_hud(void)
{
    if(hud.root != NULL || hud.parent == NULL)
        return;
    hud.root = crazypod_ui_widget_box(
        hud.parent, (320 - HUD_SIZE) / 2,
        (240 - HUD_SIZE) / 2,
        HUD_SIZE, HUD_SIZE, 18,
        HUD_BACKGROUND, 232);
    lv_obj_set_style_border_width(hud.root, 1, 0);
    lv_obj_set_style_border_color(
        hud.root, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(hud.root, 62, 0);
    lv_obj_set_style_shadow_width(hud.root, 18, 0);
    lv_obj_set_style_shadow_color(
        hud.root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(hud.root, 115, 0);
    lv_obj_remove_flag(hud.root, LV_OBJ_FLAG_CLICKABLE);
    hud.label = crazypod_ui_widget_label(
        hud.root, "#", &lv_font_montserrat_48,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(hud.label);
    lv_obj_remove_flag(hud.label, LV_OBJ_FLAG_CLICKABLE);
}

void crazypod_alpha_jump_hud_show(
    char key, long now, long duration_ticks)
{
    char text[2] = { key, '\0' };

    if(key != '#' && (key < 'A' || key > 'Z'))
        return;
    create_hud();
    if(hud.root == NULL)
        return;
    CP_LV_LABEL_SET_TEXT(hud.label, text);
    lv_obj_center(hud.label);
    lv_obj_move_foreground(hud.root);
    hud.deadline = now +
        (duration_ticks > 0 ? duration_ticks : 1);
}

void crazypod_alpha_jump_hud_tick(long now, bool active)
{
    if(hud.root == NULL)
        return;
    if(!active || (long)(now - hud.deadline) >= 0)
        crazypod_alpha_jump_hud_reset();
}

#endif
