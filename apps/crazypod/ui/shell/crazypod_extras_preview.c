#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "lvgl.h"

#include "../../crazypod_apps.h"
#include "../../crazypod_icons.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_app_catalog.h"
#include "../features/photos/crazypod_photo_screen.h"
#include "../presentation/crazypod_preview_primitives.h"
#include "crazypod_extras_preview.h"

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

void crazypod_extras_preview_render(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font)
{
    static lv_image_dsc_t icon_descriptor;
    const struct crazypod_app_descriptor *app =
        crazypod_app_catalog_find(
            crazypod_apps_hidden_id(state->selected));
    int app_index =
        app != NULL ? crazypod_app_catalog_index(app->id) : -1;
    const struct crazypod_icon *icon =
        app_index >= 0 ? crazypod_icon_get(app_index) : NULL;
    lv_obj_t *text_panel;
    lv_obj_t *label;

    make_box(
        parent, 188, 153, 104, 10,
        LV_RADIUS_CIRCLE, 0x000000, 72);
    if(icon != NULL && icon->pixels != NULL) {
        memset(&icon_descriptor, 0, sizeof(icon_descriptor));
        icon_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        icon_descriptor.header.cf =
            LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
        icon_descriptor.header.w = icon->width;
        icon_descriptor.header.h = icon->height;
        icon_descriptor.header.stride = icon->stride;
        icon_descriptor.data_size = icon->stride * icon->height;
        icon_descriptor.data = icon->pixels;
        crazypod_photo_screen_render_image(
            parent, &icon_descriptor, 174, 37, 132, 132);
    }
    else {
        lv_obj_t *fallback = make_box(
            parent, 196, 57, 88, 88, 20,
            app != NULL ? app->color : 0x59606B,
            LV_OPA_COVER);
        label = make_label(
            fallback,
            app != NULL ? app->symbol : LV_SYMBOL_LIST,
            &lv_font_montserrat_24, COLOR_WHITE, 230);
        lv_obj_center(label);
    }
    text_panel =
        crazypod_preview_make_text_panel(parent, 168, 52);
    label = make_label(
        text_panel,
        app != NULL ? app->name : "More Features",
        metadata_font, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(
        text_panel,
        app != NULL
            ? "Hidden from Main Menu"
            : "No hidden applications",
        &lv_font_montserrat_8, COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 30);
}

#endif
