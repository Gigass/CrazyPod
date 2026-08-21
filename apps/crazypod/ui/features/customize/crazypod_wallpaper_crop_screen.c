#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "../../../crazypod_photos.h"
#include "../../../crazypod_wallpaper.h"
#include "../../presentation/crazypod_glass_slots.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_wallpaper_crop_controller.h"
#include "crazypod_wallpaper_crop_screen.h"

static struct crazypod_wallpaper_crop_view crop_view;

#define CROP_FOOTER_HEIGHT 34
#define CROP_FOOTER_Y (LCD_HEIGHT - CROP_FOOTER_HEIGHT)
#define CROP_PROGRESS_WIDTH 200

static lv_obj_t *make_footer(lv_obj_t *parent)
{
    return crazypod_glass_slot_panel(
        CRAZYPOD_GLASS_SLOT_INFO_BAR,
        crazypod_glass_slot_prepare_frame(
            CRAZYPOD_GLASS_SLOT_INFO_BAR,
            0, CROP_FOOTER_Y, LCD_WIDTH, CROP_FOOTER_HEIGHT,
            CRAZYPOD_GLASS_HOME_CAPSULE),
        parent, 0, CROP_FOOTER_Y,
        LCD_WIDTH, CROP_FOOTER_HEIGHT, 0,
        CRAZYPOD_GLASS_HOME_CAPSULE);
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
        return CP_TR("Applying wallpaper...");
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLIED) {
        if(model->target == CRAZYPOD_APPEARANCE_HOME_BACKGROUND)
            return CP_TR("Applied to Home");
        if(model->target == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
            return CP_TR("Applied to Menu");
        return CP_TR("Applied to Lock Screen");
    }
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_ERROR) {
        if(model->error_loading)
            return CP_TR("Picture is still loading");
        switch(model->apply_result) {
        case CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE:
            return CP_TR("Invalid crop area");
        case CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED:
            return CP_TR("Not enough image workspace");
        case CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED:
            return CP_TR("Could not decode full picture");
        case CRAZYPOD_WALLPAPER_APPLY_CACHE_OPEN_FAILED:
            return CP_TR("Could not open wallpaper cache");
        case CRAZYPOD_WALLPAPER_APPLY_CACHE_WRITE_FAILED:
            return CP_TR("Could not write wallpaper cache");
        case CRAZYPOD_WALLPAPER_APPLY_CACHE_PUBLISH_FAILED:
            return CP_TR("Could not publish wallpaper cache");
        case CRAZYPOD_WALLPAPER_APPLY_SETTINGS_FAILED:
            return CP_TR("Could not save wallpaper setting");
        case CRAZYPOD_WALLPAPER_APPLY_ACTIVATE_FAILED:
            return CP_TR("Could not activate wallpaper");
        default:
            return CP_TR("Could not apply wallpaper");
        }
    }
    if(model->menu_armed)
        return CP_TR("Release MENU to Cancel");
    if(model->play_armed)
        return CP_TR("Release PLAY to Reset");
    return CP_TR("SELECT Apply  \xe2\x80\xa2  Wheel Zoom  "
                 "\xe2\x80\xa2  Hold MENU Cancel");
}

void crazypod_wallpaper_crop_screen_render(
    lv_obj_t *parent, uint32_t white_color, uint32_t cyan_color)
{
    struct crazypod_wallpaper_crop_view *view = &crop_view;
    const struct crazypod_wallpaper_crop_model *model =
        crazypod_wallpaper_crop_controller_model();
    const lv_image_dsc_t *source = crazypod_photo_view(model->photo_index);
    const lv_image_dsc_t *preview;
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    bool crop_valid = crop_rect(
        source, &crop_x, &crop_y, &crop_width, &crop_height);
    lv_obj_t *viewport;
    lv_obj_t *footer;
    lv_obj_t *label;
    const char *instruction = instruction_text(model);
    char status_text[48];
    int progress = -1;
    int fill_width = 0;
    bool show_progress = false;
    bool progress_error = false;

    preview = crop_valid
        ? crazypod_photo_render_crop_preview(
              model->photo_index, model->center_x, model->center_y)
        : NULL;
    viewport = crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_COVER);
    view->progress_fill = NULL;
    view->progress_label = NULL;
    if(source != NULL && preview != NULL) {
        int display_width;
        int display_height;
        int display_x;
        int display_y;
        int frame_x;
        int frame_y;
        int frame_width;
        int frame_height;
        lv_obj_t *image;
        lv_obj_t *ring;

        if(source->header.w * LCD_HEIGHT >
           source->header.h * LCD_WIDTH) {
            display_height = LCD_HEIGHT;
            display_width =
                source->header.w * LCD_HEIGHT / source->header.h;
        }
        else {
            display_width = LCD_WIDTH;
            display_height =
                source->header.h * LCD_WIDTH / source->header.w;
        }
        display_x = LCD_WIDTH / 2 -
            model->center_x * display_width / source->header.w;
        display_y = LCD_HEIGHT / 2 -
            model->center_y * display_height / source->header.h;
        if(display_x > 0)
            display_x = 0;
        if(display_x < LCD_WIDTH - display_width)
            display_x = LCD_WIDTH - display_width;
        if(display_y > 0)
            display_y = 0;
        if(display_y < LCD_HEIGHT - display_height)
            display_y = LCD_HEIGHT - display_height;
        image = lv_image_create(viewport);
        lv_image_set_src(image, preview);
        lv_obj_set_pos(image, 0, 0);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
        frame_x = display_x +
            crop_x * display_width / source->header.w;
        frame_y = display_y + crop_y * display_height / source->header.h;
        frame_width = crop_width * display_width / source->header.w;
        frame_height = crop_height * display_height / source->header.h;
        if(frame_y > 0)
            crazypod_ui_widget_box(
                viewport, 0, 0, LCD_WIDTH, frame_y,
                0, 0x000000, 150);
        if(frame_y + frame_height < LCD_HEIGHT)
            crazypod_ui_widget_box(
                viewport, 0, frame_y + frame_height, LCD_WIDTH,
                LCD_HEIGHT - frame_y - frame_height,
                0, 0x000000, 150);
        if(frame_x > 0 && frame_height > 0)
            crazypod_ui_widget_box(
                viewport, 0, frame_y, frame_x,
                frame_height, 0, 0x000000, 150);
        if(frame_x + frame_width < LCD_WIDTH &&
           frame_height > 0)
            crazypod_ui_widget_box(
                viewport, frame_x + frame_width, frame_y,
                LCD_WIDTH - frame_x - frame_width,
                frame_height, 0, 0x000000, 150);
        ring = crazypod_ui_widget_box(
            viewport, frame_x, frame_y, frame_width, frame_height,
            2, white_color, LV_OPA_TRANSP);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_set_style_border_color(
            ring, lv_color_hex(white_color), 0);
        lv_obj_set_style_border_opa(ring, 220, 0);
        if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLYING) {
            progress = model->apply_progress;
            show_progress = true;
        }
    }
    else {
        progress = crazypod_photo_view_progress(model->photo_index);
        (void)crazypod_wallpaper_crop_controller_note_load_progress(progress);
        label = crazypod_ui_widget_label(
            viewport, LV_SYMBOL_REFRESH, &lv_font_montserrat_24,
            white_color, 115);
        lv_obj_center(label);
        show_progress = true;
        progress_error = progress < 0 ||
            model->phase == CRAZYPOD_WALLPAPER_CROP_ERROR;
        if(model->phase == CRAZYPOD_WALLPAPER_CROP_ERROR)
            instruction = instruction_text(model);
        else if(progress_error)
            instruction = CP_TR("Could not load picture");
        else {
            if(progress > 100)
                progress = 100;
            snprintf(status_text, sizeof(status_text),
                     CP_FMT("Loading picture  %d%%"), progress);
            instruction = status_text;
        }
    }
    if(model->phase == CRAZYPOD_WALLPAPER_CROP_APPLYING) {
        snprintf(status_text, sizeof(status_text),
                 CP_FMT("Applying wallpaper  %d%%"),
                 model->apply_progress);
        instruction = status_text;
    }
    footer = make_footer(viewport);
    label = crazypod_ui_widget_label(
        footer, instruction, &lv_font_montserrat_8,
        white_color, 230);
    lv_obj_set_width(label, LCD_WIDTH - 16);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 8, show_progress ? 5 : 10);
    if(show_progress) {
        lv_obj_t *track = crazypod_ui_widget_box(
            footer, 60, 27, CROP_PROGRESS_WIDTH, 3,
            LV_RADIUS_CIRCLE, white_color, 38);

        if(progress < 0)
            progress = 100;
        if(progress > 100)
            progress = 100;
        fill_width = progress * CROP_PROGRESS_WIDTH / 100;
        if(fill_width < 2)
            fill_width = 2;
        view->progress_label = label;
        view->progress_fill = crazypod_ui_widget_box(
            track, 0, 0, fill_width, 3, LV_RADIUS_CIRCLE,
            progress_error ? 0xFF453A : cyan_color,
            LV_OPA_COVER);
    }
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
    CP_LV_LABEL_SET_TEXT(crop_view.progress_label, text);
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
