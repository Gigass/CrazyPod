#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_photos.h"
#include "../../../crazypod_videos.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_photo_screen.h"
#include "../../presentation/crazypod_menu_preview_motion.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "crazypod_photos_preview.h"

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

static void render_procedural_photo(
    lv_obj_t *parent, int x, int y,
    int width, int height, int seed)
{
    static const uint32_t sky_colors[] = {
        0x8EC5D8, 0xD7A7B8, 0x8AB89B, 0xC8A66A
    };
    static const uint32_t ground_colors[] = {
        0x385869, 0x744A61, 0x46654F, 0x725537
    };
    lv_obj_t *scene = make_box(
        parent, x, y, width, height, 2,
        sky_colors[seed % 4], LV_OPA_COVER);
    int horizon = height * 2 / 3;

    lv_obj_set_style_bg_grad_color(
        scene, lv_color_hex(sky_colors[(seed + 1) % 4]), 0);
    lv_obj_set_style_bg_grad_dir(scene, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(scene, 1, 0);
    lv_obj_set_style_border_color(
        scene, lv_color_hex(0xF8FAF8), 0);
    lv_obj_set_style_border_opa(scene, 62, 0);
    make_box(
        scene, width * 2 / 3, height / 7,
        width / 7, width / 7,
        LV_RADIUS_CIRCLE, 0xF7DE8B, 210);
    make_box(
        scene, 0, horizon, width, height - horizon,
        0, ground_colors[seed % 4], LV_OPA_COVER);
    make_box(
        scene, width / 8, horizon - height / 5,
        width * 3 / 5, height / 4,
        height / 8, ground_colors[(seed + 1) % 4], 210);
    make_box(
        scene, width * 3 / 4, horizon - height / 9,
        width / 9, height / 3, 1, 0x253B35, 145);
}

static void format_media_duration(
    char *buffer, size_t size, uint32_t seconds)
{
    uint32_t hours = seconds / 3600u;
    uint32_t minutes = seconds / 60u % 60u;
    uint32_t remainder = seconds % 60u;

    if(hours > 0)
        snprintf(buffer, size, "%lu:%02lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes,
                 (unsigned long)remainder);
    else
        snprintf(buffer, size, "%lu:%02lu",
                 (unsigned long)minutes, (unsigned long)remainder);
}

static lv_obj_t *render_video_card(
    const struct crazypod_photos_preview_context *context,
    int video_index, int x, int y, int width, int height)
{
    const lv_image_dsc_t *poster = NULL;
    lv_obj_t *card = make_box(
        context->parent, x, y, width, height, 6,
        0x0B0D12, LV_OPA_COVER);
    lv_obj_t *play;

    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(
        card, lv_color_hex(0xAEB7C7), 0);
    lv_obj_set_style_border_opa(card, 120, 0);
    if(video_index >= 0) {
        if(context->defer_media) {
            if(context->media_deferred != NULL)
                *context->media_deferred = true;
        }
        else
            poster = crazypod_video_poster(video_index);
    }
    if(poster != NULL)
        (void)crazypod_photo_screen_render_image(
            card, poster, 3, 3, width - 6, height - 6);
    else {
        lv_obj_t *empty = make_label(
            card, LV_SYMBOL_IMAGE, &lv_font_montserrat_24,
            COLOR_WHITE, 55);
        lv_obj_center(empty);
    }
    play = make_box(
        card, width / 2 - 15, height / 2 - 15,
        30, 30, LV_RADIUS_CIRCLE, 0x05070A, 190);
    {
        lv_obj_t *symbol = make_label(
            play, LV_SYMBOL_PLAY, &lv_font_montserrat_12,
            COLOR_WHITE, LV_OPA_COVER);
        lv_obj_center(symbol);
    }
    return card;
}

void crazypod_videos_preview_render(
    const struct route_state *state,
    const struct crazypod_photos_preview_context *context)
{
    int count = crazypod_video_count();
    int index = count > 0 ? state->selected : -1;
    const char *name =
        index >= 0 ? crazypod_video_name(index) : CP_TR("No Videos");
    uint32_t resume = index >= 0
        ? crazypod_video_resume_seconds(index) : 0;
    uint32_t duration = index >= 0
        ? crazypod_video_duration_seconds(index) : 0;
    lv_obj_t *text_panel;
    lv_obj_t *label;
    char time[24];
    char detail[64];

    crazypod_preview_make_plinth(
        context->parent, 173, 154, 134, 0x8892A2, 0x161A21);
    render_video_card(context, index, 173, 48, 134, 102);
    format_media_duration(time, sizeof(time), duration);
    if(resume > 0)
        snprintf(detail, sizeof(detail), CP_FMT("Resume %lu:%02lu  ·  %s"),
                 (unsigned long)(resume / 60u),
                 (unsigned long)(resume % 60u), time);
    else
        snprintf(detail, sizeof(detail), CP_FMT("%s  ·  MPEG"), time);
    text_panel = crazypod_preview_make_text_panel(
        context->parent, 160, 52);
    label = make_label(
        text_panel, name, &lv_font_montserrat_10,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 128);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 6, 6);
    label = make_label(
        text_panel, detail, &lv_font_montserrat_8,
        COLOR_WHITE, 120);
    lv_obj_set_width(label, 128);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 6, 28);
}

void crazypod_photos_preview_render(
    const struct route_state *state,
    const struct crazypod_photos_preview_context *context)
{
    int count = state->selected == 0
        ? crazypod_photo_count()
        : state->selected == 1
            ? crazypod_video_count()
            : crazypod_photo_favorite_count();
    lv_obj_t *parent = context->parent;
    lv_obj_t *preview = NULL;
    lv_obj_t *label;
    lv_obj_t *text_panel;
    char detail[48];
    int i;

    crazypod_preview_make_plinth(
        parent, 180, 149, 120, 0xAEB6B9, 0x252A2C);
    if(state->selected == 0) {
        static const int x[] = { 181, 213, 244 };
        static const int y[] = { 70, 57, 72 };
        static const int angle[] = { -75, 15, 80 };
        for(i = 0; i < 3; ++i) {
            const lv_image_dsc_t *descriptor = NULL;

            if(i < count) {
                if(context->defer_media) {
                    if(context->media_deferred != NULL)
                        *context->media_deferred = true;
                }
                else {
                    descriptor = crazypod_photo_thumbnail(
                        CRAZYPOD_PHOTO_THUMB_SLOTS - 1 - i, i);
                }
            }
            preview = make_box(
                parent, x[i], y[i], 57, 76, 3,
                0xF0E9DB, LV_OPA_COVER);
            lv_obj_set_style_border_width(preview, 1, 0);
            lv_obj_set_style_border_color(
                preview, lv_color_hex(0xB8AE9F), 0);
            lv_obj_set_style_border_opa(preview, 155, 0);
            crazypod_preview_add_bevel(
                preview, 57, 76, 0xFFFFFF, 0x867C6C);
            render_procedural_photo(
                preview, 4, 4, 49, 54, i);
            if(descriptor != NULL)
                (void)crazypod_photo_screen_render_image(
                    preview, descriptor, 4, 4, 49, 54);
            make_box(preview, 12, 65, 33, 2, 1, 0x6C6258, 82);
            make_box(preview, 17, 70, 23, 1, 0, 0x6C6258, 48);
            if(i == 1)
                make_box(preview, 22, 0, 14, 4, 1, 0xD2B879, 185);
            lv_obj_set_style_transform_rotation(preview, angle[i], 0);
            crazypod_menu_preview_motion_register(
                preview, (i - 1) * 28, 25 + i * 4, 194,
                angle[i] + (i - 1) * 110, 0,
                i * 45, 280, (i - 1) * 23, -16, 194,
                angle[i] + (i - 1) * 80);
        }
    }
    else if(state->selected == 1) {
        int video_index = count > 0 ? 0 : -1;

        preview = render_video_card(
            context, video_index, 173, 48, 134, 102);
        make_box(parent, 209, 146, 60, 6, 3, 0x252A31, 210);
        if(preview != NULL)
            crazypod_menu_preview_motion_register(
                preview, 18, 0, 205, 55, 0,
                0, 280, 18, -4, 205, 80);
    }
    else {
        int photo_index = crazypod_photo_favorite_index(0);
        const lv_image_dsc_t *descriptor = NULL;

        if(photo_index >= 0) {
            if(context->defer_media) {
                if(context->media_deferred != NULL)
                    *context->media_deferred = true;
            }
            else {
                descriptor = crazypod_photo_thumbnail(
                    CRAZYPOD_PHOTO_THUMB_SLOTS - 1, photo_index);
            }
        }
        preview = make_box(
            parent, 188, 56, 104, 96, 7,
            0x6B4429, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            preview, lv_color_hex(0x2D1B12), 0);
        lv_obj_set_style_bg_grad_dir(preview, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_border_width(preview, 3, 0);
        lv_obj_set_style_border_color(
            preview, lv_color_hex(0xB7834D), 0);
        lv_obj_set_style_border_opa(preview, 190, 0);
        crazypod_preview_add_bevel(
            preview, 104, 96, 0xD1A36A, 0x160B06);
        make_box(preview, 5, 5, 1, 86, 0, 0xD8A76B, 70);
        make_box(preview, 98, 5, 1, 86, 0, 0x160B06, 120);
        {
            lv_obj_t *mat = make_box(
                preview, 9, 9, 86, 77, 3,
                0xE7DDC9, LV_OPA_COVER);
            lv_obj_set_style_border_width(mat, 1, 0);
            lv_obj_set_style_border_color(
                mat, lv_color_hex(0xBEB39F), 0);
            lv_obj_set_style_border_opa(mat, 135, 0);
            if(count > 0) {
                render_procedural_photo(
                    mat, 5, 5, 76, 67, 3);
                (void)crazypod_photo_screen_render_image(
                    mat, descriptor, 5, 5, 76, 67);
            }
            else {
                make_box(mat, 5, 5, 76, 67, 2,
                         0xA8B0B2, LV_OPA_COVER);
                label = make_label(
                    mat, LV_SYMBOL_IMAGE, &lv_font_montserrat_24,
                    COLOR_WHITE, 82);
                lv_obj_center(label);
            }
        }
        make_box(parent, 228, 151, 24, 5, 2, 0x6E452A, 225);
        {
            lv_obj_t *pin = make_box(
                parent, 257, 48, 29, 29,
                LV_RADIUS_CIRCLE, 0x8D243A, LV_OPA_COVER);
            lv_obj_set_style_border_width(pin, 1, 0);
            lv_obj_set_style_border_color(
                pin, lv_color_hex(0xE1B46E), 0);
            lv_obj_set_style_border_opa(pin, 130, 0);
            crazypod_preview_add_bevel(
                pin, 29, 29, 0xF0CE91, 0x310914);
            crazypod_ui_widget_pixel_heart(
                pin, 6, 8, 2, 0xFFE2A8, LV_OPA_COVER);
            crazypod_menu_preview_motion_register(
                pin, 13, -14, 170, 120, 0,
                80, 240, 11, -11, 170, 180);
        }
        crazypod_menu_preview_motion_register(
            preview, 17, 0, 214, 65, 0,
            0, 280, 18, -5, 214, 95);
    }

    snprintf(detail, sizeof(detail),
             state->selected == 1
                 ? (count == 1 ? CP_FMT("%d video") : CP_FMT("%d videos"))
                 : (count == 1 ? CP_FMT("%d photo") : CP_FMT("%d photos")),
             count);
    text_panel = crazypod_preview_make_text_panel(parent, 166, 52);
    label = make_label(
        text_panel, detail, &lv_font_montserrat_10,
        COLOR_WHITE, 190);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 7, 6);
    label = make_label(
        text_panel,
        state->selected == 0 ? CP_TR("All pictures in /Pictures")
        : state->selected == 1 ? CP_TR("Converted MPEG files in /Videos")
                               : CP_TR("Saved favorites"),
        &lv_font_montserrat_8, COLOR_WHITE, 100);
    lv_obj_set_width(label, 132);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 4, 25);
    crazypod_menu_preview_motion_register(
        text_panel, 0, 9, 246, 0, 0, 70, 220,
        0, 6, 246, 0);
}

#endif
