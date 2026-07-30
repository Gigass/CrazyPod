#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_image.h"
#include "../../../crazypod_miniapp_font.h"
#include "../../../crazypod_miniapps.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_miniapp_screen.h"

#define COLOR_DETAIL 0x08080D
#define COLOR_PANEL 0x1B1B22
#define COLOR_WHITE 0xFFFFFF
#define COLOR_MUTED 0x9A9AA4
#define COLOR_CYAN 0x26CFF5
#define COLOR_GREEN 0x30D158
#define COLOR_AMBER 0xFFD166

struct miniapp_screen_state {
    struct cp_scene scene;
    fb_data bitmap_pixels[160 * 160];
    lv_image_dsc_t bitmap_descriptor;
    char bitmap_id[CP_MINIAPP_RESOURCE_ID_SIZE];
    int bitmap_app_index;
    uint32_t bitmap_crc;
    uint16_t bitmap_width;
    uint16_t bitmap_height;
    uint32_t accent;
    lv_obj_t *parent;
};

static struct miniapp_screen_state screen = {
    .bitmap_app_index = -1,
};

static uint32_t accent_foreground(uint32_t accent)
{
    uint32_t red = (accent >> 16) & 0xffu;
    uint32_t green = (accent >> 8) & 0xffu;
    uint32_t blue = accent & 0xffu;
    uint32_t luminance =
        2126u * red * red + 7152u * green * green + 722u * blue * blue;

    return luminance >= 130050000u ? 0x000000 : COLOR_WHITE;
}

static uint32_t color(enum cp_color_token token)
{
    switch(token) {
    case CP_COLOR_BACKGROUND: return COLOR_DETAIL;
    case CP_COLOR_SURFACE: return COLOR_PANEL;
    case CP_COLOR_SURFACE_RAISED: return 0x292932;
    case CP_COLOR_WHITE: return COLOR_WHITE;
    case CP_COLOR_MUTED: return COLOR_MUTED;
    case CP_COLOR_ACCENT: return screen.accent;
    case CP_COLOR_ACCENT_FOREGROUND:
        return accent_foreground(screen.accent);
    case CP_COLOR_ROSE: return 0xFF4568;
    case CP_COLOR_GREEN: return COLOR_GREEN;
    case CP_COLOR_CYAN: return COLOR_CYAN;
    case CP_COLOR_AMBER: return COLOR_AMBER;
    case CP_COLOR_ERROR: return 0xFF453A;
    default: return COLOR_WHITE;
    }
}

static const lv_font_t *font(enum cp_font_token token)
{
    switch(token) {
    case CP_FONT_CAPTION: return &lv_font_montserrat_8;
    case CP_FONT_LABEL: return &crazypod_miniapp_symbol_font;
    case CP_FONT_BODY: return &lv_font_montserrat_12;
    case CP_FONT_CJK: return &lv_font_source_han_sans_sc_14_cjk;
    case CP_FONT_TITLE: return &lv_font_source_han_sans_sc_16_cjk;
    case CP_FONT_NUMBER: return &lv_font_montserrat_24;
    case CP_FONT_DISPLAY: return &lv_font_montserrat_48;
    default: return &lv_font_montserrat_10;
    }
}

static void render_rect(const struct cp_draw_command *command)
{
    bool focused = (command->flags & CP_DRAW_FOCUSED) != 0;
    int border_width = focused && command->border_width < 2
        ? 2 : command->border_width;
    lv_obj_t *box = crazypod_ui_widget_box(
        screen.parent, command->x, command->y, command->width,
        command->height,
        (command->flags & CP_DRAW_CIRCLE) != 0
            ? LV_RADIUS_CIRCLE : command->radius,
        color((enum cp_color_token)command->background), command->opacity);

    if(border_width > 0 && (focused ? 255 : command->border_opacity) > 0) {
        lv_obj_set_style_border_width(box, border_width, 0);
        lv_obj_set_style_border_color(
            box, lv_color_hex(focused ? COLOR_WHITE :
                color((enum cp_color_token)command->border)), 0);
        lv_obj_set_style_border_opa(
            box, focused ? 255 : command->border_opacity, 0);
    }
    lv_obj_set_style_clip_corner(box, true, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
}

static void render_text(const struct cp_draw_command *command)
{
    const lv_font_t *text_font = font((enum cp_font_token)command->font);
    int y = command->y;
    int height = command->height;
    lv_obj_t *label;

    if(height > text_font->line_height) {
        y += (height - text_font->line_height) / 2;
        height = text_font->line_height;
    }
    label = crazypod_ui_widget_label(
        screen.parent, command->text, text_font,
        color((enum cp_color_token)command->foreground), command->opacity);
    lv_obj_set_pos(label, command->x, y);
    lv_obj_set_size(label, command->width, height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(
        label, command->align == CP_ALIGN_RIGHT ? LV_TEXT_ALIGN_RIGHT :
        command->align == CP_ALIGN_CENTER ? LV_TEXT_ALIGN_CENTER :
        LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
}

static void render_ring(const struct cp_draw_command *command)
{
    int maximum = command->maximum > 0 ? command->maximum : 1;
    int value = command->value < 0 ? 0 : command->value;
    lv_obj_t *ring;

    if(value > maximum)
        value = maximum;
    ring = lv_arc_create(screen.parent);
    lv_obj_remove_style_all(ring);
    lv_obj_set_pos(ring, command->x, command->y);
    lv_obj_set_size(ring, command->width, command->height);
    lv_arc_set_range(ring, 0, maximum);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_rotation(ring, 270);
    lv_arc_set_value(ring, value);
    lv_obj_set_style_arc_color(ring, lv_color_hex(
        color((enum cp_color_token)command->track_color)), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ring, command->opacity, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, command->track_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(
        color((enum cp_color_token)command->progress_color)),
        LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(ring, command->opacity, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(
        ring, command->progress_width, LV_PART_INDICATOR);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
}

static void render_bar(const struct cp_draw_command *command)
{
    int maximum = command->maximum > 0 ? command->maximum : 1;
    int value = command->value < 0 ? 0 : command->value;
    int fill_width;
    lv_obj_t *track;

    if(value > maximum)
        value = maximum;
    fill_width = (int)((int64_t)value * command->width / maximum);
    track = crazypod_ui_widget_box(
        screen.parent, command->x, command->y, command->width,
        command->height, command->radius,
        color((enum cp_color_token)command->track_color), command->opacity);
    if(fill_width > 0) {
        lv_obj_t *fill = crazypod_ui_widget_box(
            track, 0, 0, fill_width, command->height, command->radius,
            color((enum cp_color_token)command->progress_color),
            command->opacity);
        lv_obj_remove_flag(fill, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
}

static void render_divider(const struct cp_draw_command *command)
{
    lv_obj_t *divider = crazypod_ui_widget_box(
        screen.parent, command->x, command->y, command->width,
        command->height, command->radius,
        color((enum cp_color_token)command->background), command->opacity);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_CLICKABLE);
}

static void render_bitmap(const struct cp_draw_command *command)
{
    struct cp_resource_info info = { .struct_size = sizeof(info) };
    int app_index = crazypod_miniapps_current();
    lv_obj_t *image;

    if(crazypod_miniapps_resource_stat(command->text, &info) !=
           CRAZYPOD_MINIAPP_OK ||
       info.type != CP_RESOURCE_BITMAP_RGB565 ||
       info.width == 0 || info.height == 0 ||
       info.width > 160 || info.height > 160 ||
       info.size > sizeof(screen.bitmap_pixels))
        return;
    if(app_index != screen.bitmap_app_index ||
       info.crc32 != screen.bitmap_crc ||
       info.width != screen.bitmap_width ||
       info.height != screen.bitmap_height ||
       strcmp(command->text, screen.bitmap_id) != 0) {
        if(crazypod_miniapps_resource_read(
               command->text, 0, screen.bitmap_pixels, info.size) !=
               (int)info.size ||
           !crazypod_image_configure_rgb565(
               &screen.bitmap_descriptor, screen.bitmap_pixels,
               info.width, info.height))
            return;
        snprintf(screen.bitmap_id, sizeof(screen.bitmap_id),
                 "%s", command->text);
        screen.bitmap_app_index = app_index;
        screen.bitmap_crc = info.crc32;
        screen.bitmap_width = info.width;
        screen.bitmap_height = info.height;
    }
    image = lv_image_create(screen.parent);
    lv_image_set_src(image, &screen.bitmap_descriptor);
    lv_obj_set_pos(image, command->x, command->y);
    if(command->width > 0 && command->height > 0) {
        int scale_x = command->width * LV_SCALE_NONE / info.width;
        int scale_y = command->height * LV_SCALE_NONE / info.height;
        int scale = scale_x < scale_y ? scale_x : scale_y;
        lv_image_set_scale(image, scale > 0 ? scale : 1);
    }
    lv_obj_set_style_opa(image, command->opacity, 0);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

static void render_toast(void)
{
    char text[CP_MINIAPP_TOAST_TEXT_SIZE];
    lv_obj_t *panel;
    lv_obj_t *label;

    if(!crazypod_miniapps_toast(text, sizeof(text)))
        return;
    panel = crazypod_ui_widget_box(
        screen.parent, 30, 198, 260, 30, 12, 0x292932, 245);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(panel, 45, 0);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_source_han_sans_sc_14_cjk,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 10, 7);
    lv_obj_set_size(label, 240, 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

void crazypod_miniapp_screen_reset(void)
{
    screen.bitmap_app_index = -1;
    screen.bitmap_id[0] = '\0';
}

void crazypod_miniapp_screen_render(lv_obj_t *parent, uint32_t accent)
{
    int index;

    screen.parent = parent;
    screen.accent = accent;
    if(!crazypod_miniapps_render(&screen.scene)) {
        lv_obj_set_style_bg_color(parent, lv_color_hex(COLOR_DETAIL), 0);
        crazypod_ui_widget_box(
            parent, 10, 40, 300, 188, 12, COLOR_PANEL, LV_OPA_COVER);
        {
            lv_obj_t *label = crazypod_ui_widget_label(
                parent, CP_TR("APP RENDER ERROR"), &lv_font_montserrat_12,
                0xFF453A, LV_OPA_COVER);
            lv_obj_set_pos(label, 30, 126);
            lv_obj_set_width(label, 260);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        }
        return;
    }
    lv_obj_set_style_bg_color(parent, lv_color_hex(color(
        (enum cp_color_token)screen.scene.background)), 0);
    crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
        color((enum cp_color_token)screen.scene.background), LV_OPA_COVER);
    for(index = 0; index < screen.scene.command_count; ++index) {
        const struct cp_draw_command *draw = &screen.scene.commands[index];
        switch(draw->type) {
        case CP_DRAW_RECT: render_rect(draw); break;
        case CP_DRAW_TEXT: render_text(draw); break;
        case CP_DRAW_RING: render_ring(draw); break;
        case CP_DRAW_DIVIDER: render_divider(draw); break;
        case CP_DRAW_PROGRESS: render_bar(draw); break;
        case CP_DRAW_BITMAP: render_bitmap(draw); break;
        default: break;
        }
    }
    render_toast();
}

#endif
