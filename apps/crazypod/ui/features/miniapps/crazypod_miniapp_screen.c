#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "../../../crazypod_miniapps.h"
#include "../../presentation/crazypod_popup_layout.h"
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
    const char *heading = title[0] != '\0' ? title : "TEXT INPUT";
    const char *instruction =
        "WHEEL: CHOOSE   SELECT: APPLY   MENU: CANCEL";
    struct crazypod_popup_geometry geometry;
    int measured_width;
    int content_width;
    int instruction_height;
    int choice_width;
    int heading_y = 12;
    int heading_height;
    int field_y;
    int field_height = 42;
    int choice_y;
    int choice_height = 38;
    int instruction_y;

    crazypod_ui_widget_box(
        overlay_parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, 150);
    measured_width = crazypod_popup_text_width(
        instruction, &lv_font_montserrat_8);
    content_width = crazypod_popup_text_width(
        heading, &lv_font_source_han_sans_sc_14_cjk);
    if(content_width > measured_width)
        measured_width = content_width;
    content_width = crazypod_popup_text_width(
        crazypod_miniapp_text_prompt_value(),
        &lv_font_source_han_sans_sc_14_cjk);
    if(content_width > measured_width)
        measured_width = content_width;
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            measured_width, 22, 210, LCD_WIDTH - 32),
        1);
    content_width = geometry.width - 28;
    instruction_height = crazypod_popup_wrapped_text_height(
        instruction, &lv_font_montserrat_8,
        content_width, 2);
    heading_height = lv_font_get_line_height(
        &lv_font_source_han_sans_sc_14_cjk);
    field_y = heading_y + heading_height + 10;
    choice_y = field_y + field_height + 10;
    instruction_y = choice_y + choice_height + 6;
    geometry = crazypod_popup_centered_geometry(
        geometry.width, instruction_y + instruction_height + 12);
    panel = crazypod_ui_widget_box(
        overlay_parent, geometry.x, geometry.y,
        geometry.width, geometry.height,
        14, 0x24242C, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        panel, heading,
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, heading_y);
    lv_obj_set_size(label, content_width, heading_height);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    field = crazypod_ui_widget_box(
        panel, 14, field_y, content_width, field_height, 8,
        0x101016, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        field, crazypod_miniapp_text_prompt_value(),
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 10);
    lv_obj_set_size(label, content_width - 16, 22);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    choice_width = crazypod_popup_clamp_width(
        crazypod_popup_text_width(
            crazypod_miniapp_text_prompt_choice(),
            &lv_font_montserrat_12),
        18, 96, content_width);
    field = crazypod_ui_widget_box(
        panel, (geometry.width - choice_width) / 2,
        choice_y, choice_width, choice_height, 9,
        0xFFFFFF, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        field, crazypod_miniapp_text_prompt_choice(),
        &lv_font_montserrat_12, 0x111116, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 10);
    lv_obj_set_size(label, choice_width - 16, 23);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    label = crazypod_ui_widget_label(
        panel, instruction,
        &lv_font_montserrat_8, 0xB8B8C2, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, instruction_y);
    lv_obj_set_size(
        label, content_width, instruction_height);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

static void render_exit_prompt(void)
{
    bool exit_selected = crazypod_miniapp_input_exit_selected();
    lv_obj_t *panel;
    lv_obj_t *label;
    lv_obj_t *button;
    struct crazypod_popup_geometry geometry;
    int cancel_width;
    int exit_width;
    int button_width;
    int gap = 8;
    int title_y = 18;
    int title_height;
    int button_y;
    int button_height = 36;

    if(!crazypod_miniapp_input_exit_prompt_visible())
        return;
    crazypod_ui_widget_box(
        overlay_parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, 120);
    cancel_width = crazypod_popup_text_width(
        CP_TR("Cancel"),
        &lv_font_source_han_sans_sc_14_cjk);
    exit_width = crazypod_popup_text_width(
        CP_TR("Exit"),
        &lv_font_source_han_sans_sc_14_cjk);
    button_width = (cancel_width > exit_width
        ? cancel_width : exit_width) + 32;
    if(button_width < 82)
        button_width = 82;
    title_height = lv_font_get_line_height(
        &lv_font_source_han_sans_sc_14_cjk);
    button_y = title_y + title_height + 18;
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            button_width * 2 + gap, 15,
            198, LCD_WIDTH - 32),
        button_y + button_height + 17);
    button_width = (geometry.width - 30 - gap) / 2;
    panel = crazypod_ui_widget_box(
        overlay_parent, geometry.x, geometry.y,
        geometry.width, geometry.height,
        14, 0x24242C, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        panel, CP_TR("EXIT MINI APP?"),
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 15, title_y);
    lv_obj_set_size(label, geometry.width - 30, title_height);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    button = crazypod_ui_widget_box(
        panel, 15, button_y, button_width, button_height, 9,
        exit_selected ? 0x34343D : COLOR_WHITE,
        exit_selected ? 180 : LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        button, CP_TR("Cancel"),
        &lv_font_source_han_sans_sc_14_cjk,
        exit_selected ? COLOR_WHITE : 0x111116, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 9);
    lv_obj_set_size(label, button_width - 16, 18);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        button = crazypod_ui_widget_box(
        panel, 15 + button_width + gap,
        button_y, button_width, button_height, 9,
        exit_selected ? 0xFF453A : 0x34343D,
        exit_selected ? LV_OPA_COVER : 180);
    label = crazypod_ui_widget_label(
        button, CP_TR("Exit"),
        &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 8, 9);
    lv_obj_set_size(label, button_width - 16, 18);
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
