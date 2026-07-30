#include "config.h"

#include "../../../crazypod_l10n.h"

#include <string.h>

#include "lvgl.h"

#include "../../../crazypod_organizer.h"
#include "../../presentation/crazypod_ui_text.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_contacts_screen.h"

#define CRAZYPOD_CONTACT_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_CONTACT_WHITE 0xFFFFFF

void crazypod_contacts_screen_render(lv_obj_t *content, int contact_index)
{
    const struct crazypod_contact *contact =
        crazypod_contact_get(contact_index);
    lv_obj_t *card;
    lv_obj_t *avatar;
    lv_obj_t *label;
    lv_obj_t *row;
    char initials[8];
    int initial_bytes = 0;

    initials[0] = '?';
    initials[1] = '\0';
    if(contact != NULL && contact->name[0] != '\0') {
        initial_bytes = crazypod_ui_text_character_size(contact->name);
        if(initial_bytes > 0 &&
           initial_bytes < (int)sizeof(initials)) {
            memcpy(initials, contact->name,
                   (size_t)initial_bytes);
            initials[initial_bytes] = '\0';
        }
    }

    crazypod_ui_widget_box(content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
             0x000000, 105);
    card = crazypod_ui_widget_box(content, 70, 42, 180, 184, 18,
                    0x242A31, 238);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(CRAZYPOD_CONTACT_WHITE), 0);
    lv_obj_set_style_border_opa(card, 42, 0);
    lv_obj_set_style_shadow_width(card, 18, 0);
    lv_obj_set_style_shadow_offset_y(card, 10, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, 95, 0);

    avatar = crazypod_ui_widget_box(card, 63, 14, 54, 54,
                      LV_RADIUS_CIRCLE, 0x59B89E, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        avatar, lv_color_hex(0x2E4857), 0);
    lv_obj_set_style_bg_grad_dir(avatar, LV_GRAD_DIR_VER, 0);
    label = crazypod_ui_widget_label(avatar, initials,
                       CRAZYPOD_CONTACT_FONT,
                       CRAZYPOD_CONTACT_WHITE, LV_OPA_COVER);
    lv_obj_center(label);
    label = crazypod_ui_widget_label(
        card,
        contact != NULL ? contact->name : CP_TR("Missing Contact"),
        CRAZYPOD_CONTACT_FONT, CRAZYPOD_CONTACT_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 156);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 12, 76);

    row = crazypod_ui_widget_box(card, 12, 103, 156, 30, 8,
                   CRAZYPOD_CONTACT_WHITE, 28);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(CRAZYPOD_CONTACT_WHITE), 0);
    lv_obj_set_style_border_opa(row, 32, 0);
    label = crazypod_ui_widget_label(row, LV_SYMBOL_CALL,
                       &lv_font_montserrat_10,
                       CRAZYPOD_CONTACT_WHITE, 175);
    lv_obj_set_pos(label, 10, 9);
    label = crazypod_ui_widget_label(
        row,
        contact != NULL && contact->phone[0] != '\0'
            ? contact->phone : CP_TR("No phone number"),
        &lv_font_montserrat_10, CRAZYPOD_CONTACT_WHITE,
        contact != NULL && contact->phone[0] != '\0'
            ? 225 : 105);
    lv_obj_set_width(label, 118);
    lv_obj_set_height(label, 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 31, 8);

    row = crazypod_ui_widget_box(card, 12, 140, 156, 30, 8,
                   CRAZYPOD_CONTACT_WHITE, 16);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(CRAZYPOD_CONTACT_WHITE), 0);
    lv_obj_set_style_border_opa(row, 22, 0);
    label = crazypod_ui_widget_label(row, LV_SYMBOL_ENVELOPE,
                       &lv_font_montserrat_10,
                       CRAZYPOD_CONTACT_WHITE, 135);
    lv_obj_set_pos(label, 10, 9);
    label = crazypod_ui_widget_label(
        row,
        contact != NULL && contact->email[0] != '\0'
            ? contact->email : CP_TR("No email address"),
        &lv_font_montserrat_10, CRAZYPOD_CONTACT_WHITE,
        contact != NULL && contact->email[0] != '\0'
            ? 190 : 90);
    lv_obj_set_width(label, 118);
    lv_obj_set_height(label, 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 31, 8);
}
