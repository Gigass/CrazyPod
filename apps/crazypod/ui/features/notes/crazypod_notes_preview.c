#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_notes.h"
#include "crazypod_notes_controller.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../presentation/crazypod_menu_preview_motion.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "crazypod_notes_feature.h"
#include "crazypod_notes_preview.h"

#define COLOR_CYAN 0x55D6E7
#define COLOR_WHITE 0xFFFFFF

static lv_obj_t *make_box(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_box(
        parent, x, y, width, height, radius, color, opacity);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

static int notes_home_note_start(void)
{
    return crazypod_notes_controller_draft_available() ? 2 : 1;
}

static int notes_home_deleted_index(void)
{
    return notes_home_note_start() + crazypod_notes_count(false) + 1;
}

static int notes_home_search_index(void)
{
    return notes_home_deleted_index() - 1;
}

static const struct crazypod_note *notes_home_note(int index)
{
    int note_index = index - notes_home_note_start();

    return note_index >= 0
        ? crazypod_note_get(false, note_index) : NULL;
}

void crazypod_notes_preview_render(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font)
{
    const char *title = NULL;
    const char *detail = "";
    const struct crazypod_note *note = NULL;
    lv_obj_t *binder;
    lv_obj_t *paper;
    lv_obj_t *label;
    lv_obj_t *text_panel;
    lv_obj_t *part;
    char search_detail[48];
    bool is_home = state->route == NOTES_ROUTE_MENU;
    bool is_new = is_home && state->selected == 0;
    bool is_draft = is_home && crazypod_notes_controller_draft_available() &&
                    state->selected == 1;
    bool is_search = is_home &&
                     state->selected == notes_home_search_index();
    bool is_deleted = is_home &&
                      state->selected == notes_home_deleted_index();
    int i;

    crazypod_notes_feature_item_title(
        state, state->selected, &title);

    if(state->route == NOTES_ROUTE_MENU)
        note = notes_home_note(state->selected);
    else if(state->route == NOTES_ROUTE_SEARCH_RESULTS)
        note = crazypod_notes_search_get(
            crazypod_notes_controller_query(), state->selected);
    else if(state->group > 0)
        note = crazypod_note_find((uint32_t)state->group);
    if(state->route == NOTES_ROUTE_SEARCH) {
        title = crazypod_notes_controller_query()[0] != '\0'
            ? crazypod_notes_controller_query() : CP_TR("Search Notes");
        snprintf(search_detail, sizeof(search_detail),
                 CP_FMT("%d matching note%s"),
                 crazypod_notes_search_count(crazypod_notes_controller_query()),
                 crazypod_notes_search_count(crazypod_notes_controller_query()) == 1
                    ? "" : "s");
        detail = search_detail;
    }
    else if(note != NULL)
        detail = note->pinned ? CP_TR("Pinned note") :
                 note->deleted ? CP_TR("Recently deleted") : CP_TR("Saved note");
    else if(state->route == NOTES_ROUTE_MENU && state->selected == 0)
        detail = CP_TR("Write with the click wheel");
    else if(state->route == NOTES_ROUTE_MENU &&
            crazypod_notes_controller_draft_available() && state->selected == 1)
        detail = CP_TR("Resume unsaved changes");
    else if(is_search)
        detail = CP_TR("Search title and body");
    else if(is_deleted)
        detail = CP_TR("Restore or erase notes");
    else if(state->route == NOTES_ROUTE_MENU)
        detail = CP_TR("Saved note");

    crazypod_preview_make_plinth(
        parent, 187, 156, 108, 0xAEB7BA, 0x252A2C);
    if(is_new) {
        binder = make_box(parent, 190, 52, 92, 104, 6,
                          0x6B4B32, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x392517), 0);
        lv_obj_set_style_bg_grad_dir(
            binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 1, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xA68159), 0);
        lv_obj_set_style_border_opa(binder, 145, 0);
        crazypod_preview_add_bevel(
            binder, 92, 104, 0xB4936C, 0x1D120B);
        paper = make_box(binder, 7, 10, 78, 87, 3,
                         0xF4E9CF, LV_OPA_COVER);
        lv_obj_set_style_border_width(paper, 1, 0);
        lv_obj_set_style_border_color(
            paper, lv_color_hex(0xD5C49F), 0);
        lv_obj_set_style_border_opa(paper, 165, 0);
        crazypod_preview_add_bevel(
            paper, 78, 87, 0xFFFFFF, 0xA8987C);
        crazypod_preview_add_paper_rules(
            paper, 78, 25, 5, 12, 0x8FA7B5);
        make_box(paper, 14, 12, 34, 3, 1,
                 0x6F5540, 125);
        part = make_box(binder, 29, 2, 34, 13, 5,
                        0xB9C0C2, LV_OPA_COVER);
        crazypod_preview_add_bevel(
            part, 34, 13, 0xF2F5F6, 0x454B4D);
        make_box(part, 8, 4, 18, 4, 2,
                 0x53595B, 185);
        part = make_box(parent, 270, 70, 7, 77, 3,
                        0xE5B64A, LV_OPA_COVER);
        make_box(part, 0, 0, 7, 11, 2, 0xE26D5A, 245);
        make_box(part, 0, 11, 7, 4, 0,
                 0xB9BEC0, 240);
        make_box(part, 1, 69, 5, 8, 2, 0x252525, 245);
        lv_obj_set_style_transform_rotation(part, 235, 0);
        crazypod_menu_preview_motion_register(
            binder, 0, 18, 228, -15, 0, 0, 260,
            -8, 12, 228, -25);
        crazypod_menu_preview_motion_register(
            part, 22, -18, 196, 410, 0, 60, 300,
            18, -14, 196, 430);
    }
    else if(is_draft) {
        binder = make_box(parent, 194, 58, 92, 98, 8,
                          0x75492D, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x3B2519), 0);
        lv_obj_set_style_bg_grad_dir(binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 1, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xB0835D), 0);
        lv_obj_set_style_border_opa(binder, 145, 0);
        crazypod_preview_add_bevel(
            binder, 92, 98, 0xC29B73, 0x21150E);
        paper = make_box(binder, 12, 12, 68, 78, 3,
                         0xF4E9CF, LV_OPA_COVER);
        crazypod_preview_add_bevel(
            paper, 68, 78, 0xFFFFFF, 0x9C8B70);
        crazypod_preview_add_paper_rules(
            paper, 68, 23, 4, 12, 0x8FA7B5);
        make_box(paper, 13, 11, 28, 3, 1,
                 0x6F5540, 115);
        part = make_box(binder, 30, 3, 33, 12, 5,
                        0xB8B9B7, LV_OPA_COVER);
        crazypod_preview_add_bevel(
            part, 33, 12, 0xEDF0F1, 0x3A3D3E);
        make_box(part, 7, 3, 19, 4, 2, 0x4A4C4D, 165);
        crazypod_menu_preview_motion_register(
            binder, 15, 0, 218, 45, 0, 0, 270,
            14, 5, 218, 70);
        crazypod_menu_preview_motion_register(
            part, 0, -14, 200, 0, 0, 70, 230,
            0, -10, 200, 0);
    }
    else if(is_search) {
        paper = make_box(parent, 192, 61, 83, 91, 4,
                         0xF4E9CF, LV_OPA_COVER);
        lv_obj_set_style_border_width(paper, 1, 0);
        lv_obj_set_style_border_color(
            paper, lv_color_hex(0xC9B99A), 0);
        lv_obj_set_style_border_opa(paper, 135, 0);
        crazypod_preview_add_bevel(
            paper, 83, 91, 0xFFFFFF, 0x998970);
        crazypod_preview_add_paper_rules(
            paper, 83, 18, 5, 14, 0x6E7780);
        make_box(paper, 14, 8, 33, 3, 1,
                 0x6F5540, 105);
        part = make_box(parent, 228, 65, 44, 44,
                        LV_RADIUS_CIRCLE, COLOR_CYAN, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(part, 7, 0);
        lv_obj_set_style_border_color(
            part, lv_color_hex(COLOR_CYAN), 0);
        lv_obj_set_style_border_opa(part, 230, 0);
        binder = make_box(
            part, 6, 6, 32, 32, LV_RADIUS_CIRCLE,
            0xDAF6FA, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(binder, 1, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xF1FCFE), 0);
        lv_obj_set_style_border_opa(binder, 105, 0);
        binder = make_box(parent, 266, 103, 8, 37, 4,
                          0x76858C, 245);
        crazypod_preview_add_bevel(
            binder, 8, 37, 0xE3EAEC, 0x262E31);
        make_box(binder, 2, 25, 4, 8, 2,
                 0x23282A, 220);
        crazypod_menu_preview_motion_register(
            paper, -8, 7, 232, 0, 0, 0, 240,
            -10, 5, 232, 0);
        crazypod_menu_preview_motion_register(
            part, 42, 9, 185, 120, 0, 50, 300,
            38, 9, 185, 160);
    }
    else if(is_deleted) {
        binder = make_box(parent, 205, 94, 72, 61, 7,
                          0x6D7377, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x303639), 0);
        lv_obj_set_style_bg_grad_dir(
            binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 2, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xB9C0C4), 0);
        lv_obj_set_style_border_opa(binder, 150, 0);
        crazypod_preview_add_bevel(
            binder, 72, 61, 0xD6DCDE, 0x16191A);
        make_box(binder, 3, 4, 66, 4, 2,
                 0xBCC5C8, 225);
        for(i = 0; i < 5; ++i)
            make_box(binder, 9 + i * 12, 8, 2, 43, 1,
                     0x252A2D, 115);
        for(i = 0; i < 3; ++i)
            make_box(binder, 7, 19 + i * 12, 58, 1, 0,
                     0xD8DEE0, 45);
        make_box(binder, 9, 54, 10, 5, 2,
                 0x24282A, 210);
        make_box(binder, 53, 54, 10, 5, 2,
                 0x24282A, 210);
        part = make_box(parent, 225, 59, 37, 37,
                        LV_RADIUS_CIRCLE, 0xE9DDC4, LV_OPA_COVER);
        crazypod_preview_add_bevel(
            part, 37, 37, 0xFFFFFF, 0xA99D88);
        make_box(part, 8, 7, 20, 3, 1, 0x756B5F, 90);
        make_box(part, 13, 16, 16, 3, 1, 0x756B5F, 75);
        make_box(part, 4, 25, 18, 2, 1, 0x756B5F, 52);
        lv_obj_set_style_transform_rotation(part, 120, 0);
        crazypod_menu_preview_motion_register(
            binder, 0, 18, 225, 0, 0, 0, 240,
            0, 13, 225, 0);
        crazypod_menu_preview_motion_register(
            part, -18, -37, 150, -220, 0, 30, 300,
            13, 31, 160, 270);
    }
    else {
        binder = make_box(parent, 195, 60, 92, 96, 8,
                          0x5C2D23, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            binder, lv_color_hex(0x2B1514), 0);
        lv_obj_set_style_bg_grad_dir(binder, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(binder, 2, 0);
        lv_obj_set_style_border_color(
            binder, lv_color_hex(0xB06A4A), 0);
        lv_obj_set_style_border_opa(binder, 145, 0);
        crazypod_preview_add_bevel(
            binder, 92, 96, 0xC17B5C, 0x1B0D0C);
        make_box(binder, 6, 7, 5, 82, 2,
                 0x311515, 225);
        make_box(binder, 12, 7, 1, 82, 0,
                 0xC17B5C, 65);
        paper = make_box(binder, 17, 8, 67, 80, 3,
                         0xF4E9CF, LV_OPA_COVER);
        crazypod_preview_add_bevel(
            paper, 67, 80, 0xFFFFFF, 0x9B8B72);
        crazypod_preview_add_paper_rules(
            paper, 67, 20, 4, 13, 0x8FA7B5);
        make_box(paper, 13, 9, 34, 3, 1,
                 0x6F5540, 115);
        for(i = 0; i < 4; ++i) {
            make_box(binder, 8, 17 + i * 19, 13, 5,
                     LV_RADIUS_CIRCLE, 0xD6D7D9, 230);
            make_box(binder, 9, 18 + i * 19, 9, 2,
                     LV_RADIUS_CIRCLE, 0x52545A, 180);
        }
        part = make_box(binder, 76, 37, 12, 22, 3,
                        note != NULL && note->pinned
                            ? 0xF0B43C : 0xBD7B42, 220);
        crazypod_menu_preview_motion_register(
            binder, 16, 0, 215, 55, 0, 0, 280,
            14, 5, 215, 75);
        crazypod_menu_preview_motion_register(
            paper, 17, 0, 226, 0, 0, 60, 240,
            15, 0, 226, 0);
        crazypod_menu_preview_motion_register(
            part, 8, -8, 170, 120, 0, 90, 220,
            8, -7, 170, 160);
    }

    text_panel = crazypod_preview_make_text_panel(parent, 168, 52);
    label = make_label(text_panel, title != NULL ? title : CP_TR("Notes"),
                       metadata_font,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(text_panel, detail, &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 30);
    crazypod_menu_preview_motion_register(
        text_panel, 0, 9, 246, 0, 0, 70, 220,
        0, 6, 246, 0);
}



#endif
