#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "../../../crazypod_photos.h"
#include "../../../crazypod_wallpaper.h"
#include "../../presentation/crazypod_glass_panel.h"
#include "../../presentation/crazypod_glass_slots.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_wallpaper_crop_controller.h"
#include "crazypod_wallpaper_crop_screen.h"

static struct crazypod_wallpaper_crop_view crop_view;

static lv_obj_t *make_panel(
    enum crazypod_crop_panel_slot slot, lv_obj_t *parent,
    int x, int y, int width, int height, int radius)
{
    enum crazypod_glass_material material;
    enum crazypod_glass_slot glass_slot;

    if(slot == CRAZYPOD_CROP_PANEL_HINT) {
        material = CRAZYPOD_GLASS_INFO_TOAST;
        glass_slot = CRAZYPOD_GLASS_SLOT_INFO_BAR_ALT;
    }
    else if(slot == CRAZYPOD_CROP_PANEL_APPLY) {
        material = CRAZYPOD_GLASS_INFO_TOAST;
        glass_slot = CRAZYPOD_GLASS_SLOT_INFO_TOAST;
    }
    else {
        material = CRAZYPOD_GLASS_TEXT_PANEL;
        glass_slot = CRAZYPOD_GLASS_SLOT_INFO_BAR;
    }
    return crazypod_glass_slot_panel(
        glass_slot,
        crazypod_glass_slot_prepare_frame(
            glass_slot, x, y + 40, width, height, material),
        parent, x, y, width, height, radius, material);
}

static bool crop_rect(
    const lv_image_dsc_t *source,
    int *x, int *y, int *width, int *height)
{
    if(source == NULL || source->header.w <= 0 || source->header.h <= 0)
        return false;
    return crazypod_wallpaper_crop_controller_rect(
        source->header.w, source->header.h,
        crazypod_wallpaper_crop_max_zoom(source),
        x, y, width, height);
}

static const char *instruction_text(
    const struct crazypod_wallpaper_crop_model *model)
{
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLYING)
        return "Applying wallpaper...";
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLIED) {
        if(model->target == CRAZYPOD_APPEARANCE_HOME_BACKGROUND)
            return "Applied to Home";
        if(model->target == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
            return "Applied to Menu";
        return "Applied to Lock Screen";
    }
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_ERROR) {
        if(model->error_loading)
            return "Picture is still loading";
        switch(model->apply_result) {
        case CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE:
            return "Invalid crop area";
        case CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED:
            return "Not enough image workspace";
        case CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED:
            return "Could not decode full picture";
        case CRAZYPOD_WALLPAPER_APPLY_CACHE_OPEN_FAILED:
            return "Could not open wallpaper cache";
        case CRAZYPOD_WALLPAPER_APPLY_CACHE_WRITE_FAILED:
            return "Could not write wallpaper cache";
        case CRAZYPOD_WALLPAPER_APPLY_CACHE_PUBLISH_FAILED:
            return "Could not publish wallpaper cache";
        case CRAZYPOD_WALLPAPER_APPLY_SETTINGS_FAILED:
            return "Could not save wallpaper setting";
        case CRAZYPOD_WALLPAPER_APPLY_ACTIVATE_FAILED:
            return "Could not activate wallpaper";
        default:
            return "Could not apply wallpaper";
        }
    }
    if(model->menu_armed)
        return "Release MENU to Cancel";
    if(model->play_armed)
        return "Release PLAY to Reset";
    return "SELECT Apply  \xe2\x80\xa2  Wheel Zoom  "
           "\xe2\x80\xa2  Hold MENU Cancel";
}

void crazypod_wallpaper_crop_screen_render(
    lv_obj_t *parent, uint32_t white_color, uint32_t cyan_color)
{
    struct crazypod_wallpaper_crop_view *view = &crop_view;
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();
    const lv_image_dsc_t *source = crazypod_photo_view(model->photo_index);
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    bool crop_valid = crop_rect(
        source, &crop_x, &crop_y, &crop_width, &crop_height);
    const lv_image_dsc_t *preview = crop_valid
        ? crazypod_photo_render_crop_preview(
              model->photo_index, model->center_y)
        : NULL;
    lv_obj_t *viewport = crazypod_ui_widget_box(
        parent, 0, 40, LCD_WIDTH, LCD_HEIGHT - 40,
        0, 0x000000, LV_OPA_COVER);
    lv_obj_t *label;
    const char *instruction;

    view->progress_fill = NULL;
    view->progress_label = NULL;
    if(source != NULL && preview != NULL) {
        const int canvas_height = 168;
        int display_height = source->header.h * LCD_WIDTH / source->header.w;
        int display_y;
        int frame_x;
        int frame_y;
        int frame_width;
        int frame_height;
        int visible_frame_y;
        int visible_frame_bottom;
        int visible_frame_height;
        lv_obj_t *image;
        lv_obj_t *ring;

        if(display_height < 1)
            display_height = 1;
        if(display_height <= canvas_height)
            display_y = (canvas_height - display_height) / 2;
        else {
            display_y = canvas_height / 2 -
                model->center_y * display_height / source->header.h;
            if(display_y > 0)
                display_y = 0;
            if(display_y < canvas_height - display_height)
                display_y = canvas_height - display_height;
        }
        image = lv_image_create(viewport);
        lv_image_set_src(image, preview);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
        frame_x = crop_x * LCD_WIDTH / source->header.w;
        frame_y = display_y + crop_y * display_height / source->header.h;
        frame_width = crop_width * LCD_WIDTH / source->header.w;
        frame_height = crop_height * display_height / source->header.h;
        visible_frame_y = frame_y > 0 ? frame_y : 0;
        visible_frame_bottom =
            frame_y + frame_height < canvas_height
                ? frame_y + frame_height : canvas_height;
        visible_frame_height = visible_frame_bottom - visible_frame_y;
        if(frame_y > 0)
            crazypod_ui_widget_box(
                viewport, 0, 0, LCD_WIDTH, frame_y,
                0, 0x000000, 150);
        if(frame_y + frame_height < canvas_height)
            crazypod_ui_widget_box(
                viewport, 0, frame_y + frame_height, LCD_WIDTH,
                canvas_height - frame_y - frame_height,
                0, 0x000000, 150);
        if(frame_x > 0 && visible_frame_height > 0)
            crazypod_ui_widget_box(
                viewport, 0, visible_frame_y, frame_x,
                visible_frame_height, 0, 0x000000, 150);
        if(frame_x + frame_width < LCD_WIDTH &&
           visible_frame_height > 0)
            crazypod_ui_widget_box(
                viewport, frame_x + frame_width, visible_frame_y,
                LCD_WIDTH - frame_x - frame_width,
                visible_frame_height, 0, 0x000000, 150);
        ring = crazypod_ui_widget_box(
            viewport, frame_x, frame_y, frame_width, frame_height,
            6, white_color, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_set_style_border_color(
            ring, lv_color_hex(white_color), 0);
        lv_obj_set_style_border_opa(ring, 235, 0);
    }
    else {
        int progress = crazypod_photo_view_progress(model->photo_index);
        char loading_text[40];
        lv_obj_t *track;
        int fill_width;

        (void)crazypod_wallpaper_crop_controller_note_load_progress(progress);
        label = crazypod_ui_widget_label(
            viewport, LV_SYMBOL_REFRESH, &lv_font_montserrat_24,
            white_color, 130);
        lv_obj_set_pos(label, 148, 55);
        if(progress < 0)
            snprintf(loading_text, sizeof(loading_text),
                     "Could not load picture");
        else
            snprintf(loading_text, sizeof(loading_text),
                     "Loading picture  %d%%",
                     progress > 100 ? 100 : progress);
        view->progress_label = crazypod_ui_widget_label(
            viewport, loading_text, &lv_font_montserrat_10,
            white_color, 185);
        lv_obj_set_width(view->progress_label, 220);
        lv_obj_set_style_text_align(
            view->progress_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(view->progress_label, 50, 91);
        track = crazypod_ui_widget_box(
            viewport, 60, 112, 200, 6,
            LV_RADIUS_CIRCLE, white_color, 35);
        fill_width = progress < 0 ? 200 : progress * 2;
        if(fill_width < 2)
            fill_width = 2;
        if(fill_width > 200)
            fill_width = 200;
        view->progress_fill = crazypod_ui_widget_box(
            track, 0, 0, fill_width, 6, LV_RADIUS_CIRCLE,
            progress < 0 ? 0xFF453A : cyan_color, LV_OPA_COVER);
    }
    {
        lv_obj_t *hint = make_panel(
            CRAZYPOD_CROP_PANEL_HINT, viewport,
            6, 5, LCD_WIDTH - 12, 34, 10);
        label = crazypod_ui_widget_label(
            hint,
            "Click Arrows: Move  \xe2\x80\xa2  Rotate: Zoom  "
            "\xe2\x80\xa2  SELECT: Apply",
            &lv_font_montserrat_8, white_color, 230);
        lv_obj_set_width(label, LCD_WIDTH - 24);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 6, 4);
        label = crazypod_ui_widget_label(
            hint,
            "Hold MENU 0.5s: Cancel  \xe2\x80\xa2  "
            "Hold PLAY 0.5s: Reset",
            &lv_font_montserrat_8, 0xFFD60A, 235);
        lv_obj_set_width(label, LCD_WIDTH - 24);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 6, 18);
    }
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLYING) {
        lv_obj_t *panel = make_panel(
            CRAZYPOD_CROP_PANEL_APPLY, viewport,
            45, 67, 230, 50, 12);
        lv_obj_t *track;
        char progress_text[40];
        int fill_width = model->apply_progress * 2;

        if(fill_width < 2)
            fill_width = 2;
        if(fill_width > 200)
            fill_width = 200;
        snprintf(progress_text, sizeof(progress_text),
                 "Applying wallpaper  %d%%", model->apply_progress);
        view->progress_label = crazypod_ui_widget_label(
            panel, progress_text, &lv_font_montserrat_10,
            white_color, 230);
        lv_obj_set_width(view->progress_label, 210);
        lv_obj_set_style_text_align(
            view->progress_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(view->progress_label, 10, 9);
        track = crazypod_ui_widget_box(
            panel, 15, 31, 200, 7, LV_RADIUS_CIRCLE,
            white_color, 38);
        view->progress_fill = crazypod_ui_widget_box(
            track, 0, 0, fill_width, 7, LV_RADIUS_CIRCLE,
            cyan_color, LV_OPA_COVER);
    }
    (void)make_panel(
        CRAZYPOD_CROP_PANEL_BOTTOM, viewport,
        0, 168, LCD_WIDTH, 32, 0);
    label = crazypod_ui_widget_label(
        viewport, crazypod_photo_name(model->photo_index),
        &lv_font_montserrat_8, white_color, 225);
    lv_obj_set_pos(label, 9, 173);
    lv_obj_set_width(label, 156);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    {
        char zoom[20];
        snprintf(zoom, sizeof(zoom), "%d.%dx",
                 model->zoom_percent / 100,
                 model->zoom_percent % 100 / 10);
        label = crazypod_ui_widget_label(
            viewport, zoom, &lv_font_montserrat_8,
            white_color, 220);
        lv_obj_set_width(label, 42);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(label, 269, 173);
    }
    instruction = instruction_text(model);
    label = crazypod_ui_widget_label(
        viewport, instruction, &lv_font_montserrat_8,
        white_color,
        model->phase == CRAZYPOD_WALLPAPER_CROP_EDITING ? 145 : 220);
    lv_obj_set_width(label, 302);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 9, 187);
}

void crazypod_wallpaper_crop_screen_reset(void)
{
    crop_view.progress_fill = NULL;
    crop_view.progress_label = NULL;
}

bool crazypod_wallpaper_crop_screen_progress_ready(void)
{
    return crop_view.progress_fill != NULL &&
        crop_view.progress_label != NULL;
}

void crazypod_wallpaper_crop_screen_update_progress(
    int fill_width, const char *text)
{
    if(!crazypod_wallpaper_crop_screen_progress_ready())
        return;
    lv_obj_set_width(crop_view.progress_fill, fill_width);
    lv_label_set_text(crop_view.progress_label, text);
}

void crazypod_wallpaper_crop_screen_set_progress_error(bool error)
{
    if(crop_view.progress_fill == NULL)
        return;
    lv_obj_set_style_bg_color(
        crop_view.progress_fill,
        lv_color_hex(error ? 0xFF453A : 0x26CFF5), 0);
}

#endif
