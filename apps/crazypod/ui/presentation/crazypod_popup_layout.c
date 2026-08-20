#include "config.h"

#ifdef IPOD_6G

#include "../../crazypod_l10n.h"
#include "crazypod_popup_layout.h"
#include "crazypod_ui_widgets.h"

int crazypod_popup_text_width(
    const char *text, const lv_font_t *font)
{
    lv_point_t size = { 0, 0 };
    const char *resolved;

    if(text == NULL || text[0] == '\0' || font == NULL)
        return 0;
    resolved = crazypod_l10n_text(text);
    font = crazypod_ui_widget_resolve_font(text, font);
    if(font == NULL)
        return 0;
    lv_text_get_size(
        &size, resolved, font, 0, 0,
        LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
    return size.x;
}

int crazypod_popup_wrapped_text_height(
    const char *text, const lv_font_t *font,
    int width, int line_space)
{
    lv_point_t size = { 0, 0 };
    const char *resolved;
    int line_height;

    if(font == NULL || width <= 0)
        return 0;
    resolved = crazypod_l10n_text(text);
    font = crazypod_ui_widget_resolve_font(text, font);
    if(font == NULL)
        return 0;
    line_height = lv_font_get_line_height(font);
    if(resolved == NULL || resolved[0] == '\0')
        return line_height;
    lv_text_get_size(
        &size, resolved, font, 0, line_space,
        width, LV_TEXT_FLAG_NONE);
    return size.y < line_height ? line_height : size.y;
}

int crazypod_popup_clamp_width(
    int content_width, int horizontal_padding,
    int minimum_width, int maximum_width)
{
    int width = content_width + horizontal_padding * 2;

    if(width < minimum_width)
        width = minimum_width;
    if(width > maximum_width)
        width = maximum_width;
    return width;
}

struct crazypod_popup_geometry crazypod_popup_centered_geometry(
    int width, int height)
{
    struct crazypod_popup_geometry geometry;
    const int maximum_width = LCD_WIDTH - 24;
    const int maximum_height = LCD_HEIGHT - 16;

    if(width < 1)
        width = 1;
    if(height < 1)
        height = 1;
    if(width > maximum_width)
        width = maximum_width;
    if(height > maximum_height)
        height = maximum_height;
    geometry.width = width;
    geometry.height = height;
    geometry.x = (LCD_WIDTH - width) / 2;
    geometry.y = (LCD_HEIGHT - height) / 2;
    return geometry;
}

#endif
