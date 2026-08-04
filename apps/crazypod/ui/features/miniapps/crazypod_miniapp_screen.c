#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "../../../crazypod_miniapps.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_miniapp_input.h"
#include "crazypod_miniapp_scene.h"
#include "crazypod_miniapp_screen.h"
#include "../../../miniapps/runtime/crazypod_miniapp_text_prompt_service.h"

#define COLOR_DETAIL 0x08080D
#define COLOR_WHITE 0xFFFFFF

static lv_obj_t *screen_parent;
static lv_obj_t *overlay_parent;

static void render_text_prompt(void)
{
    lv_obj_t *panel;
    lv_obj_t *label;
    lv_obj_t *field;
    const char *title = crazypod_miniapp_text_prompt_title();

    crazypod_ui_widget_box(
        overlay_parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, 150);
    panel = crazypod_ui_widget_box(
        overlay_parent, 20, 39, 280, 162,
        14, 0x24242C, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        panel, title[0] != '\0' ? title : "TEXT INPUT",
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 12);
    lv_obj_set_size(label, 252, 20);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    field = crazypod_ui_widget_box(
        panel, 14, 40, 252, 42, 8,
        0x101016, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        field, crazypod_miniapp_text_prompt_value(),
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 10);
    lv_obj_set_size(label, 236, 22);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    field = crazypod_ui_widget_box(
        panel, 75, 92, 130, 38, 9,
        0xFFFFFF, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        field, crazypod_miniapp_text_prompt_choice(),
        &lv_font_montserrat_12, 0x111116, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 10);
    lv_obj_set_size(label, 114, 18);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    label = crazypod_ui_widget_label(
        panel, "WHEEL: CHOOSE   SELECT: APPLY   MENU: CANCEL",
        &lv_font_montserrat_8, 0xB8B8C2, LV_OPA_COVER);
    lv_obj_set_pos(label, 10, 141);
    lv_obj_set_size(label, 260, 12);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static void render_exit_prompt(void)
{
    bool exit_selected = crazypod_miniapp_input_exit_selected();
    lv_obj_t *panel;
    lv_obj_t *label;
    lv_obj_t *button;

    if(!crazypod_miniapp_input_exit_prompt_visible())
        return;
    crazypod_ui_widget_box(
        overlay_parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, 120);
    panel = crazypod_ui_widget_box(
        overlay_parent, 45, 68, 230, 108,
        14, 0x24242C, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        panel, CP_TR("EXIT MINI APP?"),
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 15, 18);
    lv_obj_set_size(label, 200, 18);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    button = crazypod_ui_widget_box(
        panel, 15, 55, 96, 36, 9,
        exit_selected ? 0x34343D : COLOR_WHITE,
        exit_selected ? 180 : LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        button, CP_TR("Cancel"),
        &lv_font_source_han_sans_sc_14_cjk,
        exit_selected ? COLOR_WHITE : 0x111116, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 9);
    lv_obj_set_size(label, 80, 18);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    button = crazypod_ui_widget_box(
        panel, 119, 55, 96, 36, 9,
        exit_selected ? 0xFF453A : 0x34343D,
        exit_selected ? LV_OPA_COVER : 180);
    label = crazypod_ui_widget_label(
        button, CP_TR("Exit"),
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 9);
    lv_obj_set_size(label, 80, 18);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static void render_error(void)
{
    lv_obj_t *label;

    lv_obj_set_style_bg_color(
        screen_parent, lv_color_hex(COLOR_DETAIL), 0);
    label = crazypod_ui_widget_label(
        screen_parent, CP_TR("APP RENDER ERROR"),
        &lv_font_montserrat_12, 0xFF453A, LV_OPA_COVER);
    lv_obj_set_pos(label, 30, 126);
    lv_obj_set_width(label, 260);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

void crazypod_miniapp_screen_reset(void)
{
    if(overlay_parent != NULL && lv_obj_is_valid(overlay_parent))
        lv_obj_delete(overlay_parent);
    overlay_parent = NULL;
    screen_parent = NULL;
    crazypod_miniapp_scene_reset();
}

bool crazypod_miniapp_screen_attached(lv_obj_t *parent)
{
    return parent != NULL && screen_parent == parent &&
        crazypod_miniapp_scene_attached(parent);
}

void crazypod_miniapp_screen_render(
    lv_obj_t *parent, uint32_t accent)
{
    if(overlay_parent != NULL && lv_obj_is_valid(overlay_parent))
        lv_obj_delete(overlay_parent);
    overlay_parent = NULL;
    screen_parent = parent;
    crazypod_miniapp_scene_attach(parent, accent);
    if(!crazypod_miniapp_scene_has_content())
        render_error();
    if(crazypod_miniapp_input_exit_prompt_visible() ||
       crazypod_miniapp_text_prompt_visible()) {
        overlay_parent = lv_obj_create(parent);
        lv_obj_remove_style_all(overlay_parent);
        lv_obj_set_pos(overlay_parent, 0, 0);
        lv_obj_set_size(overlay_parent, LCD_WIDTH, LCD_HEIGHT);
        lv_obj_remove_flag(overlay_parent, LV_OBJ_FLAG_SCROLLABLE);
        if(crazypod_miniapp_text_prompt_visible())
            render_text_prompt();
        else
            render_exit_prompt();
    }
}

#endif
