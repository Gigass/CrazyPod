#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_photos.h"
#include "../../../crazypod_videos.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_photo_screen.h"
#include "../../presentation/crazypod_preview_motion.h"
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

static void set_rotation(void *target, int32_t rotation)
{
    lv_obj_set_style_transform_rotation(target, rotation, 0);
}

static void start_rotation_loop(
    lv_obj_t *object, int start, int end,
    int duration, int delay, bool reverse)
{
    lv_anim_t animation;

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_exec_cb(&animation, set_rotation);
    lv_anim_set_values(&animation, start, end);
    lv_anim_set_duration(&animation, duration);
    lv_anim_set_delay(&animation, delay);
    lv_anim_set_path_cb(
        &animation,
        reverse ? lv_anim_path_ease_in_out : lv_anim_path_linear);
    if(reverse)
        lv_anim_set_reverse_duration(&animation, duration);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    (void)lv_anim_start(&animation);
}

static lv_obj_t *render_memory_aperture(
    lv_obj_t *parent, bool favorite)
{
    static const int blade_x[] = { 21, 39, 32, 10, 3 };
    static const int blade_y[] = { 4, 20, 43, 43, 20 };
    static const int blade_angle[] = { 0, 720, 1440, 2160, 2880 };
    uint32_t surface = favorite ? 0x190A10 : 0x071416;
    uint32_t surface_deep = favorite ? 0x070306 : 0x020708;
    uint32_t primary = favorite ? 0xEA7E98 : 0x64D8CB;
    uint32_t secondary = favorite ? 0xD7A45B : 0x6F84E8;
    uint32_t warm = favorite ? 0xFFE0A3 : 0xF1C66D;
    lv_obj_t *stage = make_box(
        parent, 186, 44, 108, 108, 0,
        surface_deep, LV_OPA_TRANSP);
    lv_obj_t *lens = make_box(
        stage, 8, 8, 92, 92, LV_RADIUS_CIRCLE,
        surface, LV_OPA_COVER);
    lv_obj_t *orbit;
    lv_obj_t *blades;
    int i;

    lv_obj_set_style_bg_grad_color(
        lens, lv_color_hex(surface_deep), 0);
    lv_obj_set_style_bg_grad_dir(lens, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(lens, 2, 0);
    lv_obj_set_style_border_color(
        lens, lv_color_hex(primary), 0);
    lv_obj_set_style_border_opa(lens, 150, 0);
    crazypod_preview_add_bevel(
        lens, 92, 92, secondary, surface_deep);

    orbit = make_box(
        lens, 3, 3, 86, 86, 0,
        surface_deep, LV_OPA_TRANSP);
    lv_obj_set_style_transform_pivot_x(orbit, 43, 0);
    lv_obj_set_style_transform_pivot_y(orbit, 43, 0);
    make_box(
        orbit, 40, 0, 6, 6, LV_RADIUS_CIRCLE,
        warm, LV_OPA_COVER);
    make_box(
        orbit, 76, 57, 4, 4, LV_RADIUS_CIRCLE,
        primary, 210);

    blades = make_box(
        lens, 11, 11, 70, 70, 0,
        surface_deep, LV_OPA_TRANSP);
    lv_obj_set_style_transform_pivot_x(blades, 35, 0);
    lv_obj_set_style_transform_pivot_y(blades, 35, 0);
    for(i = 0; i < 5; ++i) {
        lv_obj_t *blade = make_box(
            blades, blade_x[i], blade_y[i], 27, 11, 6,
            (i & 1) != 0 ? secondary : primary,
            (lv_opa_t)(205 - i * 9));

        lv_obj_set_style_transform_pivot_x(blade, 13, 0);
        lv_obj_set_style_transform_pivot_y(blade, 5, 0);
        lv_obj_set_style_transform_rotation(
            blade, blade_angle[i], 0);
    }
    make_box(
        lens, 40, 40, 12, 12, LV_RADIUS_CIRCLE,
        warm, LV_OPA_COVER);

    if(!crazypod_preview_motion_reduced()) {
        start_rotation_loop(orbit, 0, 3600, 7200, 520, false);
        start_rotation_loop(blades, -35, 45, 1800, 520, true);
    }
    return stage;
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
        poster = crazypod_video_poster(video_index);
        if(context->defer_media) {
            if(context->media_deferred != NULL)
                *context->media_deferred = true;
            poster = NULL;
        }
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
    int index = count > 0
        ? state->route == PHOTOS_ROUTE_DELETE_MENU
            ? 0 : state->selected
        : -1;
    const char *name =
        index >= 0 ? crazypod_video_name(index) : CP_TR("No Videos");
    uint32_t resume = index >= 0
        ? crazypod_video_resume_seconds(index) : 0;
    uint32_t duration = index >= 0
        ? crazypod_video_duration_seconds(index) : 0;
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
    crazypod_preview_make_caption(
        context->parent, name, &lv_font_montserrat_10,
        detail, &lv_font_montserrat_8);
}

void crazypod_photos_preview_render(
    const struct route_state *state,
    const struct crazypod_photos_preview_context *context)
{
    int count = state->selected == 0
        ? crazypod_photo_count()
        : state->selected == 1
            ? crazypod_video_count()
            : state->selected == 2
                ? crazypod_photo_favorite_count()
                : crazypod_photo_count() + crazypod_video_count();
    lv_obj_t *parent = context->parent;
    lv_obj_t *preview = NULL;
    lv_obj_t *label;
    lv_obj_t *text_panel;
    char detail[48];

    crazypod_preview_make_plinth(
        parent, 180, 149, 120, 0xAEB6B9, 0x252A2C);
    if(state->selected == 0) {
        preview = render_memory_aperture(parent, false);
        crazypod_preview_motion_register(
            preview, 0, 22, 194, -80, 0,
            0, 340, 0, -13, 194, 80);
    }
    else if(state->selected == 1) {
        int video_index = count > 0 ? 0 : -1;

        preview = render_video_card(
            context, video_index, 173, 48, 134, 102);
        make_box(parent, 209, 146, 60, 6, 3, 0x252A31, 210);
        if(preview != NULL)
            crazypod_preview_motion_register(
                preview, 18, 0, 205, 55, 0,
                0, 280, 18, -4, 205, 80);
    }
    else if(state->selected == 2) {
        preview = render_memory_aperture(parent, true);
        crazypod_preview_motion_register(
            preview, 0, 22, 194, 80, 0,
            0, 340, 0, -13, 194, -80);
    }
    else {
        preview = make_box(
            parent, 195, 64, 90, 90, 18,
            0x5A1417, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            preview, lv_color_hex(0x18090A), 0);
        lv_obj_set_style_bg_grad_dir(
            preview, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(preview, 2, 0);
        lv_obj_set_style_border_color(
            preview, lv_color_hex(0xFF6B63), 0);
        lv_obj_set_style_border_opa(preview, 160, 0);
        label = make_label(
            preview, LV_SYMBOL_TRASH,
            &lv_font_montserrat_24,
            0xFF8A84, LV_OPA_COVER);
        lv_obj_center(label);
        crazypod_preview_motion_register(
            preview, 12, 6, 210, 35, 0,
            0, 260, 12, -5, 210, 65);
    }

    if(state->selected == 3)
        snprintf(detail, sizeof(detail), CP_FMT("%d"), count);
    else
        snprintf(detail, sizeof(detail),
                 state->selected == 1
                     ? (count == 1 ? CP_FMT("%d video") : CP_FMT("%d videos"))
                     : (count == 1 ? CP_FMT("%d photo") : CP_FMT("%d photos")),
                 count);
    text_panel = crazypod_preview_make_caption(
        parent, detail, &lv_font_montserrat_10,
        state->selected == 0 ? CP_TR("All pictures in /Pictures")
        : state->selected == 1 ? CP_TR("Converted MPEG files in /Videos")
        : state->selected == 2 ? CP_TR("Saved favorites")
                               : CP_TR("Erase Forever"),
        &lv_font_montserrat_8);
    crazypod_preview_motion_register(
        text_panel, 0, 9, 246, 0, 0, 70, 220,
        0, 6, 246, 0);
}

#endif
