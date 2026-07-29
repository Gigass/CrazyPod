#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "../../crazypod_appearance.h"
#include "../../crazypod_books.h"
#include "../../crazypod_icons.h"
#include "../../crazypod_photos.h"
#include "../../crazypod_wallpaper.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/settings/crazypod_settings_feature.h"
#include "../presentation/crazypod_choice_overlay.h"
#include "../presentation/crazypod_overlay_glass.h"
#include "../presentation/crazypod_popup_motion.h"
#include "crazypod_choice_coordinator.h"

static struct crazypod_choice_coordinator_host host;

static int choice_count(int kind_value, int id, void *context)
{
    enum crazypod_choice_kind kind =
        (enum crazypod_choice_kind)kind_value;

    (void)context;
    switch(kind) {
    case CRAZYPOD_CHOICE_ICON_THEME:
        return CRAZYPOD_ICON_THEME_COUNT;
    case CRAZYPOD_CHOICE_APPEARANCE:
        return crazypod_customize_feature_choice_count(
            (enum crazypod_appearance_field)id);
    case CRAZYPOD_CHOICE_BACKGROUND:
        return CRAZYPOD_APPEARANCE_COLOR_COUNT + 2;
    case CRAZYPOD_CHOICE_SETTING:
        return crazypod_settings_feature_choice_count(id);
    case CRAZYPOD_CHOICE_BOOK_FONT_SIZE:
        return 3;
    case CRAZYPOD_CHOICE_BOOK_THEME:
        return 4;
    default:
        return 0;
    }
}

static int current_index(int kind_value, int id, void *context)
{
    enum crazypod_choice_kind kind =
        (enum crazypod_choice_kind)kind_value;
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();

    (void)context;
    switch(kind) {
    case CRAZYPOD_CHOICE_ICON_THEME:
        return appearance->icon_theme;
    case CRAZYPOD_CHOICE_APPEARANCE:
        return crazypod_customize_feature_choice_index(
            (enum crazypod_appearance_field)id);
    case CRAZYPOD_CHOICE_BACKGROUND: {
        const char *path =
            crazypod_customize_feature_background_wallpaper(id);

        return path[0] != '\0'
            ? CRAZYPOD_APPEARANCE_COLOR_COUNT + 1
            : crazypod_customize_feature_field_value(id);
    }
    case CRAZYPOD_CHOICE_SETTING:
        return crazypod_settings_feature_choice_index(id);
    case CRAZYPOD_CHOICE_BOOK_FONT_SIZE:
        return crazypod_books_font_size();
    case CRAZYPOD_CHOICE_BOOK_THEME:
        return crazypod_books_theme();
    default:
        return 0;
    }
}

static const char *choice_title(
    int kind_value, int id, void *context)
{
    enum crazypod_choice_kind kind =
        (enum crazypod_choice_kind)kind_value;

    (void)context;
    switch(kind) {
    case CRAZYPOD_CHOICE_ICON_THEME:
        return "ICON THEME";
    case CRAZYPOD_CHOICE_APPEARANCE:
        return crazypod_customize_feature_field_title(id);
    case CRAZYPOD_CHOICE_BACKGROUND:
        return crazypod_customize_feature_background_title(id);
    case CRAZYPOD_CHOICE_SETTING:
        return crazypod_settings_feature_choice_item_title(id);
    case CRAZYPOD_CHOICE_BOOK_FONT_SIZE:
        return "TEXT SIZE";
    case CRAZYPOD_CHOICE_BOOK_THEME:
        return "PAGE THEME";
    default:
        return "";
    }
}

static const char *item_title(
    int kind_value, int id, int index, void *context)
{
    enum crazypod_choice_kind kind =
        (enum crazypod_choice_kind)kind_value;

    (void)context;
    switch(kind) {
    case CRAZYPOD_CHOICE_ICON_THEME:
        return crazypod_icon_theme_name(index);
    case CRAZYPOD_CHOICE_APPEARANCE:
        return crazypod_customize_feature_choice_title(
            id, index);
    case CRAZYPOD_CHOICE_BACKGROUND:
        if(index == 0)
            return id == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND
                ? "Follow Home" : "Default";
        return index <= CRAZYPOD_APPEARANCE_COLOR_COUNT
            ? crazypod_appearance_color_name(index - 1)
            : "Choose Picture";
    case CRAZYPOD_CHOICE_SETTING:
        return crazypod_settings_feature_choice_title(id, index);
    case CRAZYPOD_CHOICE_BOOK_FONT_SIZE: {
        static const char *const sizes[] = {
            "Small  ·  12 pt", "Medium  ·  14 pt",
            "Large  ·  16 pt"
        };

        return index >= 0 && index < 3 ? sizes[index] : "";
    }
    case CRAZYPOD_CHOICE_BOOK_THEME: {
        static const char *const themes[] = {
            "Parchment", "Light", "Mint", "Dark"
        };

        return index >= 0 && index < 4 ? themes[index] : "";
    }
    default:
        return "";
    }
}

static bool item_color(
    int kind_value, int id, int index,
    uint32_t *color, void *context)
{
    enum crazypod_choice_kind kind =
        (enum crazypod_choice_kind)kind_value;

    (void)context;
    if(color == NULL)
        return false;
    if(kind == CRAZYPOD_CHOICE_APPEARANCE) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)id;

        if(field == CRAZYPOD_APPEARANCE_PRIMARY ||
           field == CRAZYPOD_APPEARANCE_SECONDARY) {
            *color = crazypod_appearance_color(
                crazypod_customize_feature_choice_value(
                    field, index));
            return true;
        }
    }
    if(kind == CRAZYPOD_CHOICE_BACKGROUND) {
        *color = index > 0 &&
            index <= CRAZYPOD_APPEARANCE_COLOR_COUNT
                ? crazypod_appearance_color(index - 1)
                : crazypod_customize_feature_background_color(id);
        return true;
    }
    if(kind == CRAZYPOD_CHOICE_BOOK_THEME &&
       index >= 0 && index < 4) {
        *color =
            crazypod_books_feature_page_colors()[index];
        return true;
    }
    return false;
}

static lv_obj_t *create_panel(
    lv_obj_t *parent, int x, int y,
    int width, int height, void *context)
{
    (void)context;
    return crazypod_overlay_glass_panel(
        parent, x, y, width, height);
}

static void animate_panel(
    lv_obj_t *panel, int target_y, void *context)
{
    (void)context;
    crazypod_popup_animate(panel, target_y);
}

static int photo_index_for_path(const char *path)
{
    int index;

    if(path == NULL || path[0] == '\0')
        return 0;
    for(index = 0; index < crazypod_photo_count(); ++index) {
        if(strcmp(path, crazypod_photo_path(index)) == 0)
            return index;
    }
    return 0;
}

void crazypod_choice_coordinator_configure(
    const struct crazypod_choice_coordinator_host *new_host)
{
    if(new_host != NULL)
        host = *new_host;
}

void crazypod_choice_coordinator_reset(void)
{
    crazypod_choice_overlay_reset();
}

bool crazypod_choice_coordinator_visible(void)
{
    return crazypod_choice_overlay_visible();
}

void crazypod_choice_coordinator_show(
    enum crazypod_choice_kind kind, int id, int selected)
{
    const struct crazypod_choice_overlay_callbacks callbacks = {
        .count = choice_count,
        .current_index = current_index,
        .title = choice_title,
        .item_title = item_title,
        .item_color = item_color,
        .create_panel = create_panel,
        .animate_panel = animate_panel,
    };

    if(crazypod_now_playing_overlay_visible())
        crazypod_now_playing_overlay_dismiss(false);
    crazypod_overlay_glass_prepare(true);
    crazypod_choice_overlay_show(
        host.parent, kind, id, selected,
        host.metadata_font, &callbacks);
}

void crazypod_choice_coordinator_dismiss(bool refresh_route)
{
    crazypod_choice_overlay_dismiss();
    if(refresh_route && host.route_available())
        host.render(false);
}

void crazypod_choice_coordinator_move(int direction)
{
    crazypod_choice_overlay_move(direction);
}

void crazypod_choice_coordinator_activate(void)
{
    enum crazypod_choice_kind kind =
        (enum crazypod_choice_kind)
            crazypod_choice_overlay_kind();
    int id = crazypod_choice_overlay_id();
    int selected = crazypod_choice_overlay_selected();

    if(kind == CRAZYPOD_CHOICE_NONE)
        return;
    if(kind == CRAZYPOD_CHOICE_ICON_THEME) {
        crazypod_appearance_set_icon_theme(selected);
        host.appearance_changed();
        crazypod_choice_coordinator_dismiss(true);
    }
    else if(kind == CRAZYPOD_CHOICE_APPEARANCE) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)id;

        crazypod_appearance_set_value(
            field,
            crazypod_customize_feature_choice_value(
                field, selected));
        host.appearance_changed();
        crazypod_choice_coordinator_dismiss(true);
    }
    else if(kind == CRAZYPOD_CHOICE_BACKGROUND) {
        enum crazypod_appearance_field field =
            (enum crazypod_appearance_field)id;
        enum crazypod_wallpaper_target target =
            crazypod_customize_feature_background_target(field);

        if(selected == CRAZYPOD_APPEARANCE_COLOR_COUNT + 1) {
            const char *path =
                crazypod_customize_feature_background_wallpaper(field);

            crazypod_photos_set_route_suspended(false);
            crazypod_photos_ensure_catalog();
            crazypod_choice_coordinator_dismiss(false);
            host.push_selected(
                DIY_ROUTE_WALLPAPER_FILES, field,
                photo_index_for_path(path));
        }
        else {
            crazypod_wallpaper_clear(target);
            crazypod_appearance_set_value(field, selected);
            host.appearance_changed();
            crazypod_choice_coordinator_dismiss(true);
        }
    }
    else if(kind == CRAZYPOD_CHOICE_SETTING) {
        crazypod_settings_feature_apply_choice(id, selected);
        crazypod_choice_coordinator_dismiss(true);
    }
    else if(kind == CRAZYPOD_CHOICE_BOOK_FONT_SIZE) {
        crazypod_choice_coordinator_dismiss(false);
        crazypod_books_feature_apply_font_size(selected);
    }
    else if(kind == CRAZYPOD_CHOICE_BOOK_THEME) {
        crazypod_books_set_theme(selected);
        crazypod_choice_coordinator_dismiss(true);
    }
}

#endif
