#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_music.h"
#include "../../presentation/crazypod_ui_menu_layout.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../presentation/crazypod_menu_list.h"
#include "crazypod_search_screen.h"

#define COLOR_WHITE 0xFFFFFF
#define CRAZYPOD_VISIBLE_ROWS 7
#define CRAZYPOD_SEARCH_PREVIEW_ROWS 4
#define CRAZYPOD_MENU_HEADER_X 16
#define CRAZYPOD_MENU_HEADER_Y 42
#define CRAZYPOD_MENU_HEADER_WIDTH 128
#define CRAZYPOD_MENU_HEADER_HEIGHT 20
#define CRAZYPOD_MENU_ROW_X 8
#define CRAZYPOD_MENU_ROW_Y 64
#define CRAZYPOD_MENU_ROW_WIDTH 140
#define CRAZYPOD_MENU_ROW_HEIGHT 24
#define CRAZYPOD_MENU_ROW_STEP 24

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

void crazypod_search_screen_render(
    const struct route_state *state,
    const struct crazypod_search_screen_context *context)
{
    int count = context->item_count;
    int start;
    int row;
    int result_count = context->query[0] != '\0'
        ? crazypod_music_search_count(context->query) : 0;
    lv_obj_t *label;
    lv_obj_t *query_box;
    char text[96];

    crazypod_menu_list_reset(state->route);

    label = make_label(context->parent, CP_TR("SEARCH"),
                       context->metadata_font,
                       COLOR_WHITE, 85);
    lv_obj_set_pos(label, CRAZYPOD_MENU_HEADER_X,
                   CRAZYPOD_MENU_HEADER_Y);
    lv_obj_set_width(label, CRAZYPOD_MENU_HEADER_WIDTH);
    lv_obj_set_height(label, CRAZYPOD_MENU_HEADER_HEIGHT);

    query_box = context->make_panel(
        context->parent, CRAZYPOD_GLASS_SLOT_SEARCH_QUERY,
        170, 43, 136, 38, 12);
    if(context->query[0] != '\0') {
        lv_obj_t *active = make_box(
            query_box, 0, 0, 136, 38, 12,
            context->primary_color, 82);

        if(context->gradient_highlight) {
            lv_obj_set_style_bg_grad_color(
                active, lv_color_hex(context->secondary_color), 0);
            lv_obj_set_style_bg_grad_dir(active, LV_GRAD_DIR_HOR, 0);
        }
        lv_obj_remove_flag(active, LV_OBJ_FLAG_CLICKABLE);
    }
    label = make_label(query_box, LV_SYMBOL_KEYBOARD,
                       &lv_font_montserrat_12,
                       COLOR_WHITE, context->query[0] != '\0' ? 235 : 90);
    lv_obj_set_pos(label, 10, 12);
    label = make_label(query_box,
                       context->query[0] != '\0'
                           ? context->query : CP_TR("Start typing"),
                       context->metadata_font,
                       COLOR_WHITE,
                       context->query[0] != '\0' ? 255 : 120);
    lv_obj_set_pos(label, 31, 10);
    lv_obj_set_width(label, 92);
    lv_obj_set_height(label, 18);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    start = crazypod_ui_menu_window_start(
        count, state->selected, CRAZYPOD_VISIBLE_ROWS);

    for(row = 0; row < CRAZYPOD_VISIBLE_ROWS; ++row) {
        int index = start + row;
        int y = CRAZYPOD_MENU_ROW_Y + row * CRAZYPOD_MENU_ROW_STEP;
        bool selected = index == state->selected;
        lv_obj_t *row_box;
        lv_obj_t *marker;
        const char *title;

        if(index >= count)
            break;

        row_box = make_box(context->parent,
                           CRAZYPOD_MENU_ROW_X, y,
                           CRAZYPOD_MENU_ROW_WIDTH,
                           CRAZYPOD_MENU_ROW_HEIGHT, 8,
                           selected ? context->primary_color : context->panel_color,
                           selected ? 220 : LV_OPA_TRANSP);
        if(selected) {
            if(context->gradient_highlight) {
                lv_obj_set_style_bg_grad_color(
                    row_box, lv_color_hex(context->secondary_color), 0);
                lv_obj_set_style_bg_grad_dir(row_box, LV_GRAD_DIR_HOR, 0);
            }
            lv_obj_set_style_border_width(row_box, 1, 0);
            lv_obj_set_style_border_color(row_box,
                                           lv_color_hex(COLOR_WHITE), 0);
            lv_obj_set_style_border_opa(row_box, 90, 0);
        }

        title = context->item_title(state, index);
        label = make_label(row_box, title != NULL ? title : "",
                           context->metadata_font,
                           COLOR_WHITE,
                           selected ? 255 : 150);
        lv_obj_set_width(label, 104);
        crazypod_ui_widget_align_row_label(label, 14);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

        marker = make_label(row_box,
                            selected ? LV_SYMBOL_PLAY : "",
                            &lv_font_montserrat_8,
                            COLOR_WHITE, selected ? 205 : 75);
        crazypod_ui_widget_align_row_label(marker, 128);
        crazypod_menu_list_bind_row(row, row_box, label, marker);
    }

    context->make_panel(
        context->parent, CRAZYPOD_GLASS_SLOT_SEARCH_RESULTS,
        170, 91, 136, 104, 12);
    snprintf(text, sizeof(text), CP_FMT("%d match%s"),
             result_count, result_count == 1 ? "" : "es");
    label = make_label(context->parent,
                       context->query[0] != '\0'
                           ? text : CP_TR("Live results"),
                       &lv_font_montserrat_10,
                       COLOR_WHITE,
                       context->query[0] != '\0' ? 205 : 105);
    lv_obj_set_pos(label, 182, 101);
    lv_obj_set_width(label, 112);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    if(context->query[0] == '\0') {
        label = make_label(context->parent,
                           CP_TR("Choose a letter, then press Select."),
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 100);
        lv_obj_set_pos(label, 182, 125);
        lv_obj_set_width(label, 112);
        lv_obj_set_height(label, 40);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    }
    else if(result_count <= 0) {
        label = make_label(context->parent,
                           CP_TR("No title, artist or album matched."),
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 105);
        lv_obj_set_pos(label, 182, 125);
        lv_obj_set_width(label, 112);
        lv_obj_set_height(label, 45);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    }
    else {
        int shown = result_count < CRAZYPOD_SEARCH_PREVIEW_ROWS
            ? result_count : CRAZYPOD_SEARCH_PREVIEW_ROWS;
        int i;
        for(i = 0; i < shown; ++i) {
            const struct crazypod_track *track =
                crazypod_music_search_track(context->query, i);
            int y = 124 + i * 17;
            if(track == NULL)
                continue;
            label = make_label(context->parent, track->title,
                               &lv_font_montserrat_8,
                               COLOR_WHITE, i == 0 ? 210 : 145);
            lv_obj_set_pos(label, 182, y);
            lv_obj_set_width(label, 112);
            lv_obj_set_height(label, 10);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
            label = make_label(context->parent, track->artist,
                               &lv_font_montserrat_8,
                               COLOR_WHITE, 75);
            lv_obj_set_pos(label, 182, y + 9);
            lv_obj_set_width(label, 112);
            lv_obj_set_height(label, 10);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        }
    }

    label = make_label(context->parent,
                       CP_TR("Wheel Choose  Select Action"),
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 125);
    lv_obj_set_pos(label, 174, 202);
    lv_obj_set_width(label, 128);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    label = make_label(context->parent,
                       CP_TR("Choose View Results to listen"),
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 95);
    lv_obj_set_pos(label, 174, 216);
    lv_obj_set_width(label, 128);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}



#endif
