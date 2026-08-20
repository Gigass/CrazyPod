#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_soundwave.h"
#include "crazypod_customize_catalog.h"

const char *const crazypod_customize_menu_titles[] = {
    CP_TR("Presets"), CP_TR("Icons"), CP_TR("Details"),
    CP_TR("Backgrounds"), CP_TR("Themes"),
    CP_TR("Headphones"), CP_TR("Layout")
};

const char *const crazypod_customize_menu_symbols[] = {
    LV_SYMBOL_SAVE, LV_SYMBOL_IMAGE, LV_SYMBOL_SETTINGS,
    LV_SYMBOL_DIRECTORY, LV_SYMBOL_AUDIO, LV_SYMBOL_AUDIO,
    LV_SYMBOL_SHUFFLE
};

const char *const crazypod_customize_preset_actions[] = {
    CP_TR("Apply"), CP_TR("Export"), CP_TR("Edit")
};

const char *const crazypod_customize_preset_edit_actions[] = {
    CP_TR("Rename"), CP_TR("Update from Current"), CP_TR("Delete")
};

const char *const crazypod_customize_detail_titles[] = {
    CP_TR("Icon Size"), CP_TR("Wave Style"), CP_TR("Glow"), CP_TR("Highlight"),
    CP_TR("Primary"), CP_TR("Secondary")
};

const enum crazypod_appearance_field
crazypod_customize_detail_fields[] = {
    CRAZYPOD_APPEARANCE_ICON_SCALE,
    CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE,
    CRAZYPOD_APPEARANCE_GLOW,
    CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE,
    CRAZYPOD_APPEARANCE_PRIMARY,
    CRAZYPOD_APPEARANCE_SECONDARY,
};

const char *const crazypod_customize_layout_titles[] = {
    CP_TR("Screen Top"), CP_TR("Screen Bottom")
};

const enum crazypod_appearance_field
crazypod_customize_layout_fields[] = {
    CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS,
    CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS,
};

const int crazypod_customize_radius_values[] = {
    0, 4, 8, 12, 16, 20, 24, 32
};

const char *const crazypod_customize_background_titles[] = {
    CP_TR("Home"), CP_TR("Menu"), CP_TR("Lock Screen")
};

enum crazypod_appearance_field
crazypod_customize_background_field(int index)
{
    if(index == 1)
        return CRAZYPOD_APPEARANCE_MENU_BACKGROUND;
    if(index == 2)
        return CRAZYPOD_APPEARANCE_LOCK_BACKGROUND;
    return CRAZYPOD_APPEARANCE_HOME_BACKGROUND;
}

const char *crazypod_customize_background_title(
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return CP_TR("MENU");
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return CP_TR("LOCK SCREEN");
    return CP_TR("HOME");
}

const char *crazypod_customize_background_wallpaper(
    const struct crazypod_appearance *appearance,
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return appearance->menu_wallpaper;
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return appearance->lock_wallpaper;
    return appearance->home_wallpaper;
}

enum crazypod_wallpaper_target
crazypod_customize_background_target(
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return CRAZYPOD_WALLPAPER_MENU;
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return CRAZYPOD_WALLPAPER_LOCK;
    return CRAZYPOD_WALLPAPER_HOME;
}

uint32_t crazypod_customize_background_default_color(
    enum crazypod_appearance_field field)
{
    if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        return 0x08080D;
    if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        return 0x07090D;
    return 0x141419;
}

int crazypod_customize_field_value(
    enum crazypod_appearance_field field)
{
    const struct crazypod_appearance *value = crazypod_appearance_get();

    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_THEME: return value->icon_theme;
    case CRAZYPOD_APPEARANCE_ICON_SCALE: return value->icon_scale;
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE:
        return value->sound_wave_style;
    case CRAZYPOD_APPEARANCE_GLOW: return value->glow;
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE:
        return value->highlight_style;
    case CRAZYPOD_APPEARANCE_PRIMARY: return value->primary_color;
    case CRAZYPOD_APPEARANCE_SECONDARY: return value->secondary_color;
    case CRAZYPOD_APPEARANCE_HOME_BACKGROUND:
        return value->home_background;
    case CRAZYPOD_APPEARANCE_MENU_BACKGROUND:
        return value->menu_background;
    case CRAZYPOD_APPEARANCE_LOCK_BACKGROUND:
        return value->lock_background;
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS:
        return value->screen_top_radius;
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS:
        return value->screen_bottom_radius;
    }
    return 0;
}

int crazypod_customize_choice_count(
    enum crazypod_appearance_field field)
{
    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_SCALE: return 5;
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE:
        return CRAZYPOD_SOUND_WAVE_STYLE_COUNT;
    case CRAZYPOD_APPEARANCE_GLOW: return 4;
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE: return 2;
    case CRAZYPOD_APPEARANCE_PRIMARY:
    case CRAZYPOD_APPEARANCE_SECONDARY:
        return CRAZYPOD_APPEARANCE_COLOR_COUNT;
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS:
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS:
        return CRAZYPOD_CUSTOMIZE_RADIUS_COUNT;
    default:
        return 0;
    }
}

int crazypod_customize_choice_value(
    enum crazypod_appearance_field field, int index)
{
    if(field == CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS ||
       field == CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS)
        return index >= 0 && index < CRAZYPOD_CUSTOMIZE_RADIUS_COUNT
            ? crazypod_customize_radius_values[index] : 0;
    return index;
}

int crazypod_customize_choice_index(
    enum crazypod_appearance_field field)
{
    int current = crazypod_customize_field_value(field);
    int count = crazypod_customize_choice_count(field);
    int index;

    for(index = 0; index < count; ++index) {
        if(crazypod_customize_choice_value(field, index) == current)
            return index;
    }
    return 0;
}

const char *crazypod_customize_choice_title(
    enum crazypod_appearance_field field, int index)
{
    static char radius_text[16];
    static const char *const icon_sizes[] = {
        "80%", "90%", "100%", "110%", "120%"
    };
    static const char *const glows[] = {
        CP_TR("Off"), CP_TR("Low"), CP_TR("Medium"), CP_TR("High")
    };
    static const char *const highlights[] = {
        CP_TR("Solid"), CP_TR("Gradient")
    };

    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_SCALE:
        return index >= 0 && index < 5 ? icon_sizes[index] : "";
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE:
        return crazypod_sound_wave_style_name(index);
    case CRAZYPOD_APPEARANCE_GLOW:
        return index >= 0 && index < 4 ? glows[index] : "";
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE:
        return index >= 0 && index < 2 ? highlights[index] : "";
    case CRAZYPOD_APPEARANCE_PRIMARY:
    case CRAZYPOD_APPEARANCE_SECONDARY:
        return crazypod_appearance_color_name(index);
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS:
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS:
        snprintf(radius_text, sizeof(radius_text), CP_FMT("%d px"),
                 crazypod_customize_choice_value(field, index));
        return radius_text;
    default:
        return "";
    }
}

const char *crazypod_customize_field_title(
    enum crazypod_appearance_field field)
{
    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_SCALE: return CP_TR("ICON SIZE");
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE: return CP_TR("WAVE STYLE");
    case CRAZYPOD_APPEARANCE_GLOW: return CP_TR("GLOW");
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE: return CP_TR("HIGHLIGHT");
    case CRAZYPOD_APPEARANCE_PRIMARY: return CP_TR("PRIMARY");
    case CRAZYPOD_APPEARANCE_SECONDARY: return CP_TR("SECONDARY");
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS: return CP_TR("SCREEN TOP");
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS: return CP_TR("SCREEN BOTTOM");
    default: return CP_TR("OPTIONS");
    }
}

#endif
