#include "config.h"

#include "../../../crazypod_l10n.h"

#include <stdio.h>

#include "lvgl.h"

#include "../../presentation/crazypod_ui_text.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_notes_screen.h"

#define CRAZYPOD_NOTES_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_NOTES_WHITE 0xFFFFFF
#define CRAZYPOD_NOTES_AMBER 0xFFD166

void crazypod_notes_screen_render_composer(
    lv_obj_t *content, const struct crazypod_notes_screen_model *model,
    const char *selection)
{
    static char title_display[CRAZYPOD_NOTE_TITLE_SIZE + 2];
    static char body_display[CRAZYPOD_NOTE_BODY_SIZE + 2];
    lv_obj_t *paper;
    lv_obj_t *label;
    lv_obj_t *key;
    int line;

    paper = crazypod_ui_widget_box(content, 10, 38, 300, 190, 12,
                     0xFAEFCB, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        paper, lv_color_hex(0xE8D5A4), 0);
    lv_obj_set_style_bg_grad_dir(paper, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(paper, 1, 0);
    lv_obj_set_style_border_color(paper, lv_color_hex(0xB79F70), 0);
    lv_obj_set_style_border_opa(paper, 100, 0);
    crazypod_ui_widget_box(paper, 36, 12, 1, 166, 0, 0xB82E26, 140);
    for(line = 0; line < 6; ++line)
        crazypod_ui_widget_box(paper, 48, 82 + line * 17, 232, 1, 0,
                 0x7F9EB7, 55);
    for(line = 0; line < 3; ++line) {
        lv_obj_t *hole = crazypod_ui_widget_box(
            paper, 15, 25 + line * 55, 8, 8,
            LV_RADIUS_CIRCLE, 0x5A4A34, 45);
        lv_obj_set_style_border_width(hole, 1, 0);
        lv_obj_set_style_border_color(
            hole, lv_color_hex(CRAZYPOD_NOTES_WHITE), 0);
        lv_obj_set_style_border_opa(hole, 110, 0);
    }

    label = crazypod_ui_widget_label(
        paper,
        (*model->editor).source_id == 0 ? CP_TR("NEW NOTE") : CP_TR("EDIT NOTE"),
        &lv_font_montserrat_8, 0x7F2D23, LV_OPA_COVER);
    lv_obj_set_pos(label, 48, 13);
    label = crazypod_ui_widget_label(
        paper, model->dirty ? CP_TR("Unsaved") : CP_TR("Saved"),
        &lv_font_montserrat_8, 0x6E5B42, 220);
    lv_obj_set_width(label, 88);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 192, 13);

    label = crazypod_ui_widget_label(paper, CP_TR("TITLE"), &lv_font_montserrat_8,
                       model->body_active ? 0x8C7958 : 0x94291F,
                       230);
    lv_obj_set_pos(label, 48, 31);
    label = crazypod_ui_widget_label(
        paper,
        model->body_active
            ? ((*model->editor).title[0] != '\0'
                ? (*model->editor).title : CP_TR("Untitled"))
            : crazypod_ui_text_with_cursor(
                (*model->editor).title, model->title_cursor,
                title_display, sizeof(title_display)),
        CRAZYPOD_NOTES_FONT, 0x30291F,
        (*model->editor).title[0] != '\0' ? 255 : 125);
    lv_obj_set_pos(label, 48, 47);
    lv_obj_set_width(label, 232);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    crazypod_ui_widget_box(paper, 48, 68, 232, 1, 0,
             model->body_active ? 0x8C7958 : 0xBC4034,
             model->body_active ? 55 : 145);

    label = crazypod_ui_widget_label(paper, CP_TR("BODY"), &lv_font_montserrat_8,
                       model->body_active ? 0x94291F : 0x8C7958,
                       230);
    lv_obj_set_pos(label, 48, 74);
    label = crazypod_ui_widget_label(
        paper,
        !model->body_active
            ? ((*model->editor).body[0] != '\0'
                ? (*model->editor).body : CP_TR("Empty body"))
            : crazypod_ui_text_with_cursor(
                (*model->editor).body, model->body_cursor,
                body_display, sizeof(body_display)),
        CRAZYPOD_NOTES_FONT, 0x2A261F,
        (*model->editor).body[0] != '\0' ? 235 : 115);
    lv_obj_set_pos(label, 48, 90);
    lv_obj_set_width(label, 232);
    lv_obj_set_height(label, 44);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);

    key = crazypod_ui_widget_box(paper, 48, 138, 232, 50, 7,
                   0x74422F, LV_OPA_COVER);
    lv_obj_set_style_border_width(key, 1, 0);
    lv_obj_set_style_border_color(key, lv_color_hex(CRAZYPOD_NOTES_WHITE), 0);
    lv_obj_set_style_border_opa(key, 75, 0);
    label = crazypod_ui_widget_label(
        key, selection != NULL ? selection : "",
        &lv_font_montserrat_12,
        CRAZYPOD_NOTES_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 224);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 4, 3);
    label = crazypod_ui_widget_label(
        key,
        CP_TR("Wheel choose  ·  Center type  ·  PLAY field"),
        &lv_font_montserrat_8, CRAZYPOD_NOTES_WHITE, 155);
    lv_obj_set_width(label, 216);
    lv_obj_set_height(label, 30);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 8, 20);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

void crazypod_notes_screen_render_reader(
    lv_obj_t *content, uint32_t note_id, int first_line,
    const char *body)
{
    static char window[CRAZYPOD_NOTE_BODY_SIZE];
    const struct crazypod_note *note =
        crazypod_note_find(note_id);
    lv_obj_t *paper;
    lv_obj_t *label;
    char progress[64];
    int lines = crazypod_ui_text_note_line_count(body);
    int maximum = lines > 9 ? lines - 9 : 0;
    int first = first_line;

    if(first > maximum)
        first = maximum;
    label = crazypod_ui_widget_label(content,
                       note != NULL ? note->title : CP_TR("Missing Note"),
                       CRAZYPOD_NOTES_FONT, CRAZYPOD_NOTES_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 42);
    lv_obj_set_width(label, 250);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    if(note != NULL && note->pinned) {
        label = crazypod_ui_widget_label(content, LV_SYMBOL_OK,
                           &lv_font_montserrat_10, CRAZYPOD_NOTES_AMBER, 230);
        lv_obj_set_pos(label, 288, 44);
    }

    paper = crazypod_ui_widget_box(content, 10, 64, 300, 145, 6,
                     0xF5EEDC, LV_OPA_COVER);
    lv_obj_set_style_border_width(paper, 1, 0);
    lv_obj_set_style_border_color(paper, lv_color_hex(0xB7A98E), 0);
    lv_obj_set_style_border_opa(paper, 180, 0);
    crazypod_ui_text_note_window(body, first,
                            window, sizeof(window));
    label = crazypod_ui_widget_label(paper,
                       window[0] != '\0' ? window : CP_TR("This note is empty."),
                       CRAZYPOD_NOTES_FONT, 0x302A22,
                       window[0] != '\0' ? 255 : 125);
    lv_obj_set_pos(label, 9, 7);
    lv_obj_set_width(label, 282);
    lv_obj_set_height(label, 126);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);

    snprintf(progress, sizeof(progress), CP_FMT("%d / %d  ·  Center: Actions"),
             first + 1, lines);
    label = crazypod_ui_widget_label(content, progress, &lv_font_montserrat_8,
                       CRAZYPOD_NOTES_WHITE, 125);
    lv_obj_set_width(label, 292);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 14, 216);
}
