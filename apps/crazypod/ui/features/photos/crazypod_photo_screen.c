#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "kernel.h"

#include "../../../crazypod_photos.h"
#include "../../presentation/crazypod_empty_state.h"
#include "../../presentation/crazypod_glass_panel.h"
#include "../../presentation/crazypod_glass_slots.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_photo_controller.h"
#include "crazypod_photo_screen.h"

#define WHITE 0xFFFFFF
#define FAVORITE_PROGRESS_WIDTH 126

static lv_obj_t *favorite_progress_fill;

int crazypod_photo_screen_grid_count(enum crazypod_photo_grid_mode mode)
{
    return mode == CRAZYPOD_PHOTO_GRID_FAVORITES
        ? crazypod_photo_favorite_count() : crazypod_photo_count();
}

void crazypod_photo_screen_render_favorite_status(
    lv_obj_t *parent, int photo_index, long now,
    uint32_t white_color, uint32_t muted_color)
{
    const struct crazypod_photo_controller_model *model =
        crazypod_photo_controller_model();
    bool show_progress =
        model->select_holding && !model->select_long_handled &&
        model->select_hold_percent >= 0;
    bool show_feedback =
        model->favorite_feedback_until != 0 &&
        TIME_BEFORE(now, model->favorite_feedback_until);
    lv_obj_t *panel;
    lv_obj_t *label;

    if(!show_progress && !show_feedback)
        return;
    panel = crazypod_glass_slot_panel(
        CRAZYPOD_GLASS_SLOT_INFO_TOAST,
        crazypod_glass_slot_prepare_frame(
            CRAZYPOD_GLASS_SLOT_INFO_TOAST,
            64, 172, 192, 34,
            CRAZYPOD_GLASS_INFO_TOAST),
        parent, 64, 172, 192, 34, 12,
        CRAZYPOD_GLASS_INFO_TOAST);
    crazypod_ui_widget_pixel_heart(
        panel, 11, 10, 2,
        model->favorite_feedback_error
            ? muted_color : 0xFF375F,
        LV_OPA_COVER);
    if(show_progress) {
        int width = FAVORITE_PROGRESS_WIDTH *
            model->select_hold_percent / 100;
        lv_obj_t *track;

        label = crazypod_ui_widget_label(
            panel,
            crazypod_photo_is_favorite(photo_index)
                ? CP_TR("Hold to Remove Favorite")
                : CP_TR("Hold to Add Favorite"),
            &lv_font_montserrat_8, white_color, 225);
        lv_obj_set_pos(label, 35, 5);
        track = crazypod_ui_widget_box(
            panel, 35, 22, FAVORITE_PROGRESS_WIDTH,
            3, LV_RADIUS_CIRCLE, white_color, 40);
        if(width < 1)
            width = 1;
        favorite_progress_fill = crazypod_ui_widget_box(
            track, 0, 0, width, 3, LV_RADIUS_CIRCLE,
            0xFF375F, LV_OPA_COVER);
    }
    else {
        const char *message = model->favorite_feedback_error
            ? CP_TR("Favorite Save Failed")
            : model->favorite_feedback_added
                ? CP_TR("Saved to Favorites")
                : CP_TR("Removed from Favorites");

        label = crazypod_ui_widget_label(
            panel, message, &lv_font_montserrat_10,
            white_color, LV_OPA_COVER);
        lv_obj_set_pos(label, 35, 10);
    }
}

void crazypod_photo_screen_reset_transient(void)
{
    favorite_progress_fill = NULL;
}

bool crazypod_photo_screen_favorite_progress_ready(void)
{
    return favorite_progress_fill != NULL;
}

void crazypod_photo_screen_update_favorite_progress(int width)
{
    if(favorite_progress_fill != NULL)
        lv_obj_set_width(
            favorite_progress_fill, width < 1 ? 1 : width);
}

int crazypod_photo_screen_grid_index(
    enum crazypod_photo_grid_mode mode, int position)
{
    return mode == CRAZYPOD_PHOTO_GRID_FAVORITES
        ? crazypod_photo_favorite_index(position) : position;
}

lv_obj_t *crazypod_photo_screen_render_image(
    lv_obj_t *parent, const lv_image_dsc_t *descriptor,
    int x, int y, int width, int height)
{
    lv_obj_t *image;
    uint32_t scale_x;
    uint32_t scale_y;
    uint32_t scale;
    int display_width;
    int display_height;

    if(descriptor == NULL || descriptor->header.w <= 0 ||
       descriptor->header.h <= 0)
        return NULL;
    scale_x = (uint32_t)width * LV_SCALE_NONE / descriptor->header.w;
    scale_y = (uint32_t)height * LV_SCALE_NONE / descriptor->header.h;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if(scale > LV_SCALE_NONE)
        scale = LV_SCALE_NONE;
    if(scale == 0)
        scale = 1;
    display_width = descriptor->header.w * scale / LV_SCALE_NONE;
    display_height = descriptor->header.h * scale / LV_SCALE_NONE;
    image = lv_image_create(parent);
    lv_image_set_src(image, descriptor);
    lv_image_set_scale(image, scale);
    lv_obj_set_pos(
        image, x + (width - display_width) / 2,
        y + (height - display_height) / 2);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    return image;
}

static void render_empty(
    lv_obj_t *parent, enum crazypod_photo_grid_mode mode)
{
    crazypod_empty_state_render(
        parent, LV_SYMBOL_IMAGE,
        mode == CRAZYPOD_PHOTO_GRID_FAVORITES
            ? CP_TR("No Favorites") : CP_TR("No Pictures"),
        mode == CRAZYPOD_PHOTO_GRID_FAVORITES
            ? CP_TR("Hold Select on a photo to save it here.")
            : CP_TR("Add JPG, JPEG or BMP files to /Pictures."));
}

void crazypod_photo_screen_render_grid(
    lv_obj_t *parent, enum crazypod_photo_grid_mode mode,
    int selected, const char *title,
    const lv_font_t *title_font,
    uint32_t primary_color, uint32_t panel_color)
{
    const int columns = 4;
    const int visible_rows = 2;
    const int cell_size = 64;
    const int column_gap = 8;
    const int row_gap = 12;
    int count = crazypod_photo_screen_grid_count(mode);
    int selected_row = selected / columns;
    int start_row = selected_row > 0 ? selected_row - 1 : 0;
    int total_rows = (count + columns - 1) / columns;
    int visible;
    lv_obj_t *label;
    char position[96];

    if(start_row > total_rows - visible_rows)
        start_row = total_rows - visible_rows;
    if(start_row < 0)
        start_row = 0;
    label = crazypod_ui_widget_label(
        parent, title != NULL ? title : "",
        title_font, WHITE, 150);
    lv_obj_set_pos(label, 14, 40);
    if(count <= 0) {
        render_empty(parent, mode);
        return;
    }
    for(visible = 0; visible < columns * visible_rows; ++visible) {
        int row = visible / columns;
        int column = visible % columns;
        int position_index = (start_row + row) * columns + column;
        int photo_index;
        int x;
        int y;
        bool is_selected;
        lv_obj_t *cell;
        const lv_image_dsc_t *descriptor;

        if(position_index >= count)
            continue;
        photo_index = crazypod_photo_screen_grid_index(
            mode, position_index);
        if(photo_index < 0)
            continue;
        x = 20 + column * (cell_size + column_gap);
        y = 59 + row * (cell_size + row_gap);
        is_selected = position_index == selected;
        cell = crazypod_ui_widget_box(
            parent, x, y, cell_size, cell_size, 7,
            is_selected ? primary_color : 0x050507, LV_OPA_COVER);
        lv_obj_set_style_clip_corner(cell, true, 0);
        descriptor = crazypod_photo_thumbnail(visible, photo_index);
        if(descriptor != NULL)
            crazypod_photo_screen_render_image(
                cell, descriptor, 0, 0, cell_size, cell_size);
        else {
            lv_obj_t *refresh = crazypod_ui_widget_label(
                cell, LV_SYMBOL_REFRESH, &lv_font_montserrat_16,
                WHITE, 65);
            lv_obj_center(refresh);
        }
        if(crazypod_photo_is_favorite(photo_index))
            crazypod_ui_widget_pixel_heart(
                cell, 52, 5, 1, 0xFF375F, LV_OPA_COVER);
        {
            lv_obj_t *ring = crazypod_ui_widget_box(
                cell, 0, 0, cell_size, cell_size, 7,
                WHITE, LV_OPA_TRANSP);
            lv_obj_set_style_border_width(
                ring, is_selected ? 3 : 1, 0);
            lv_obj_set_style_border_color(
                ring, lv_color_hex(
                    is_selected ? WHITE : panel_color), 0);
            lv_obj_set_style_border_opa(
                ring, is_selected ? 235 : 90, 0);
            lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    snprintf(
        position, sizeof(position), CP_FMT("%d / %d  %s"),
        selected + 1, count,
        crazypod_photo_name(
            crazypod_photo_screen_grid_index(mode, selected)));
    label = crazypod_ui_widget_label(
        parent, position, &lv_font_montserrat_8, WHITE, 145);
    lv_obj_set_width(label, 292);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 14, 211);
}

void crazypod_photo_screen_render_detail(
    lv_obj_t *parent, int photo_index,
    uint32_t white_color,
    crazypod_photo_info_panel_factory info_panel_factory,
    void *factory_context)
{
    const int info_x = 10;
    const int info_y = 196;
    const int info_width = LCD_WIDTH - 20;
    const int info_height = 34;
    const lv_image_dsc_t *descriptor =
        crazypod_photo_controller_render_viewport(photo_index);
    const struct crazypod_photo_controller_model *model =
        crazypod_photo_controller_model();
    lv_obj_t *viewport = crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_COVER);
    lv_obj_t *label;
    char zoom_label[24];

    lv_obj_set_style_clip_corner(viewport, true, 0);
    if(descriptor != NULL) {
        lv_obj_t *image = lv_image_create(viewport);
        lv_image_set_src(image, descriptor);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        label = crazypod_ui_widget_label(
            viewport, LV_SYMBOL_REFRESH, &lv_font_montserrat_24,
            white_color, 130);
        lv_obj_set_pos(label, 148, 89);
        label = crazypod_ui_widget_label(
            viewport, CP_TR("Loading photo"), &lv_font_montserrat_10,
            white_color, 160);
        lv_obj_set_width(label, 200);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 60, 124);
    }
    if(info_panel_factory != NULL)
        info_panel_factory(
            viewport, info_x, info_y, info_width, info_height,
            12, factory_context);
    label = crazypod_ui_widget_label(
        viewport, crazypod_photo_name(photo_index),
        &lv_font_montserrat_8, white_color, 225);
    lv_obj_set_width(label, 204);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, info_x + 12, info_y + 11);
    if(model->zoom_percent > 100) {
        int tenths = (model->zoom_percent + 5) / 10;
        snprintf(zoom_label, sizeof(zoom_label), CP_FMT("%d.%dx"),
                 tenths / 10, tenths % 10);
    }
    else
        snprintf(zoom_label, sizeof(zoom_label), CP_FMT("FIT"));
    if(crazypod_photo_is_favorite(photo_index))
        crazypod_ui_widget_pixel_heart(
            viewport, info_x + 216, info_y + 13, 1,
            0xFF375F, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        viewport, zoom_label, &lv_font_montserrat_8,
        white_color, 210);
    lv_obj_set_width(label, 54);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, info_x + 234, info_y + 11);
}

#endif
