#include "crazypod_ui_widgets.h"

#include "../../crazypod_l10n.h"

static bool text_needs_i18n_font(const char *source, const char *resolved)
{
    const unsigned char *cursor = (const unsigned char *)resolved;

    if(source != NULL && source[0] == CRAZYPOD_L10N_MARKER[0])
        return true;
    if(cursor == NULL || (cursor[0] == 0xef && cursor[1] >= 0x80))
        return false;
    while(*cursor != '\0') {
        if(*cursor >= 0x80)
            return true;
        ++cursor;
    }
    return false;
}

static const lv_font_t *localized_font(const lv_font_t *font)
{
    if(font == &lv_font_montserrat_8)
        return &lv_font_crazypod_i18n_8;
    if(font == &lv_font_montserrat_10)
        return &lv_font_crazypod_i18n_10;
    if(font == &lv_font_montserrat_12)
        return &lv_font_crazypod_i18n_12;
    if(font == &lv_font_montserrat_16)
        return &lv_font_source_han_sans_sc_16_cjk;
    if(font == &lv_font_montserrat_24 || font == &lv_font_montserrat_48)
        return &lv_font_source_han_sans_sc_16_cjk;
    return font;
}

void crazypod_ui_widget_make_plain(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
}

lv_obj_t *crazypod_ui_widget_label(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color,
                                   lv_opa_t opacity)
{
    lv_obj_t *label = lv_label_create(parent);
    const char *resolved = crazypod_l10n_text(text);

    lv_obj_remove_style_all(label);
    lv_label_set_text(label, resolved);
    if(text_needs_i18n_font(text, resolved))
        font = localized_font(font);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_opa(label, opacity, 0);
    return label;
}

void crazypod_ui_widget_set_label_text(lv_obj_t *label, const char *text)
{
    const char *resolved = crazypod_l10n_text(text);

    lv_label_set_text(label, resolved);
    if(text_needs_i18n_font(text, resolved)) {
        const lv_font_t *font = lv_obj_get_style_text_font(
            label, LV_PART_MAIN);

        lv_obj_set_style_text_font(label, localized_font(font), 0);
    }
}

void crazypod_ui_widget_align_row_label(lv_obj_t *label, int x)
{
    if(label == NULL)
        return;
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, x, 0);
}

void crazypod_l10n_label_set_text(void *label, const char *text)
{
    crazypod_ui_widget_set_label_text((lv_obj_t *)label, text);
}

lv_obj_t *crazypod_ui_widget_box(lv_obj_t *parent, int x, int y,
                                 int width, int height, int radius,
                                 uint32_t color, lv_opa_t opacity)
{
    lv_obj_t *box = lv_obj_create(parent);

    crazypod_ui_widget_make_plain(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(box, opacity, 0);
    return box;
}

void crazypod_ui_widget_pixel_heart(lv_obj_t *parent, int x, int y,
                                    int unit, uint32_t color,
                                    lv_opa_t opacity)
{
    if(unit < 1)
        unit = 1;
    crazypod_ui_widget_box(parent, x + unit, y, 2 * unit, unit, 0,
                           color, opacity);
    crazypod_ui_widget_box(parent, x + 5 * unit, y, 2 * unit, unit, 0,
                           color, opacity);
    crazypod_ui_widget_box(parent, x, y + unit, 8 * unit, 2 * unit, 0,
                           color, opacity);
    crazypod_ui_widget_box(parent, x + unit, y + 3 * unit,
                           6 * unit, unit, 0, color, opacity);
    crazypod_ui_widget_box(parent, x + 2 * unit, y + 4 * unit,
                           4 * unit, unit, 0, color, opacity);
    crazypod_ui_widget_box(parent, x + 3 * unit, y + 5 * unit,
                           2 * unit, unit, 0, color, opacity);
}

static void make_normalized_heart(lv_obj_t *parent, uint32_t color)
{
    crazypod_ui_widget_box(parent, 2, 2, 3, 1, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 7, 2, 3, 1, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 1, 3, 10, 1, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 0, 4, 12, 3, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 1, 7, 10, 2, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 2, 9, 8, 1, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 3, 10, 6, 1, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 4, 11, 4, 1, 0,
                           color, LV_OPA_COVER);
    crazypod_ui_widget_box(parent, 5, 12, 2, 1, 0,
                           color, LV_OPA_COVER);
}

static void make_normalized_glyph(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, int x_offset, int y_offset)
{
    lv_obj_t *label = crazypod_ui_widget_label(
        parent, text, font, color, LV_OPA_COVER);

    lv_obj_align(label, LV_ALIGN_CENTER, x_offset, y_offset);
}

void crazypod_ui_widget_icon_set(lv_obj_t *obj,
                                 enum crazypod_ui_icon icon)
{
    uint32_t color;

    if(obj == NULL)
        return;
    color = lv_color_to_u32(
        lv_obj_get_style_text_color(obj, LV_PART_MAIN));
    lv_obj_clean(obj);
    switch(icon) {
    case CRAZYPOD_UI_ICON_HEART:
        make_normalized_heart(obj, color);
        break;
    case CRAZYPOD_UI_ICON_PLAY:
        make_normalized_glyph(
            obj, LV_SYMBOL_PLAY, &lv_font_montserrat_12,
            color, 1, 0);
        break;
    case CRAZYPOD_UI_ICON_SHUFFLE:
        make_normalized_glyph(
            obj, LV_SYMBOL_SHUFFLE, &lv_font_montserrat_12,
            color, 0, 0);
        break;
    case CRAZYPOD_UI_ICON_REPEAT:
        make_normalized_glyph(
            obj, LV_SYMBOL_LOOP, &lv_font_montserrat_10,
            color, 0, 0);
        break;
    case CRAZYPOD_UI_ICON_REPEAT_ONE:
        make_normalized_glyph(
            obj, LV_SYMBOL_LOOP, &lv_font_montserrat_10,
            color, 0, 0);
        make_normalized_glyph(
            obj, "1", &lv_font_montserrat_8,
            color, 0, 0);
        break;
    case CRAZYPOD_UI_ICON_FILE:
        make_normalized_glyph(
            obj, LV_SYMBOL_FILE, &lv_font_montserrat_12,
            color, 0, 0);
        break;
    case CRAZYPOD_UI_ICON_BARS:
        make_normalized_glyph(
            obj, LV_SYMBOL_BARS, &lv_font_montserrat_12,
            color, 0, 0);
        break;
    case CRAZYPOD_UI_ICON_LIST:
        make_normalized_glyph(
            obj, LV_SYMBOL_LIST, &lv_font_montserrat_12,
            color, 0, 0);
        break;
    case CRAZYPOD_UI_ICON_AUDIO:
        make_normalized_glyph(
            obj, LV_SYMBOL_AUDIO, &lv_font_montserrat_10,
            color, 0, 0);
        break;
    }
}

lv_obj_t *crazypod_ui_widget_icon(lv_obj_t *parent, int x, int y,
                                  enum crazypod_ui_icon icon,
                                  uint32_t color, lv_opa_t opacity)
{
    lv_obj_t *obj = crazypod_ui_widget_box(
        parent, x, y, 16, 16, 0, color, LV_OPA_TRANSP);

    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_opa(obj, opacity, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    crazypod_ui_widget_icon_set(obj, icon);
    return obj;
}

void crazypod_ui_widget_icon_set_color(lv_obj_t *obj, uint32_t color)
{
    uint32_t child_count;
    uint32_t child_index;

    if(obj == NULL)
        return;
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    child_count = lv_obj_get_child_count(obj);
    for(child_index = 0; child_index < child_count; ++child_index) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)child_index);

        lv_obj_set_style_bg_color(child, lv_color_hex(color), 0);
        lv_obj_set_style_text_color(child, lv_color_hex(color), 0);
    }
}
