#include "config.h"

#ifdef IPOD_6G

#include "../../crazypod_appearance.h"
#include "../../crazypod_apps.h"
#include "../../crazypod_miniapps.h"
#include "../features/customize/crazypod_customize_catalog.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/organizer/crazypod_activity_controller.h"
#include "../features/settings/crazypod_settings_catalog.h"
#include "../navigation/crazypod_route_query.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../presentation/crazypod_menu_list.h"
#include "../shell/crazypod_app_catalog.h"
#include "crazypod_menu_rows.h"

#define VISIBLE_ROWS 7
#define SCROLL_Y 66
#define SCROLL_HEIGHT 164
#define COLOR_PANEL 0x1B1B22

static const char *const music_symbols[] = {
    LV_SYMBOL_AUDIO, LV_SYMBOL_IMAGE, LV_SYMBOL_LOOP, LV_SYMBOL_LIST,
    LV_SYMBOL_HOME, LV_SYMBOL_DIRECTORY, LV_SYMBOL_AUDIO, LV_SYMBOL_EYE_OPEN
};

static const char *const photo_symbols[] = {
    LV_SYMBOL_IMAGE, LV_SYMBOL_PLAY, LV_SYMBOL_OK
};

static const struct crazypod_app_descriptor *route_app(
    const struct route_state *state, int index)
{
    enum crazypod_app_id id;

    if(state->route == SETTINGS_ROUTE_MAIN_MENU)
        id = crazypod_apps_ordered_id(index);
    else if(state->route == EXTRAS_ROUTE_MENU)
        id = crazypod_apps_hidden_id(index);
    else if(state->route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS)
        id = (enum crazypod_app_id)state->group;
    else
        return NULL;
    return crazypod_app_catalog_find(id);
}

static const char *miniapp_symbol(int index)
{
    const struct crazypod_miniapp_metadata *metadata =
        crazypod_miniapps_metadata(index);

    return metadata != NULL && metadata->symbol[0] != '\0'
        ? metadata->symbol : LV_SYMBOL_FILE;
}

static uint32_t highlight_color(int color)
{
    return crazypod_appearance_color(color);
}

void crazypod_menu_rows_refresh(const struct route_state *state)
{
    int count = crazypod_route_query_item_count(
        state, crazypod_music_search_query());
    int start;
    int row;

    if(!crazypod_menu_list_matches(state->route) || count <= 0)
        return;
    start = count <= VISIBLE_ROWS ? 0 :
        state->selected - VISIBLE_ROWS / 2;
    if(start < 0)
        start = 0;
    if(start > count - VISIBLE_ROWS)
        start = count - VISIBLE_ROWS;

    for(row = 0; row < VISIBLE_ROWS; ++row) {
        int index = start + row;
        bool visible = index < count;
        bool selected = visible && index == state->selected;
        const struct crazypod_app_descriptor *app =
            visible ? route_app(state, index) : NULL;
        const char *icon_text =
            app != NULL ? app->symbol :
            state->route == MUSIC_ROUTE_MENU && visible
                ? music_symbols[index] :
            state->route == PHOTOS_ROUTE_MENU && visible
                ? photo_symbols[index] :
            state->route == SETTINGS_ROUTE_MENU && visible
                ? crazypod_settings_menu_symbols[index] :
            state->route == UTILITIES_ROUTE_MENU && visible
                ? miniapp_symbol(index) :
            visible ? crazypod_customize_menu_symbols[index] : "";
        const char *marker_text =
            visible && crazypod_route_query_item_is_current(state, index)
                ? LV_SYMBOL_OK :
            selected ? LV_SYMBOL_PLAY :
            state->route == MUSIC_ROUTE_SEARCH ? "" : LV_SYMBOL_BULLET;

        crazypod_menu_list_refresh_row(
            row, visible,
            visible ? crazypod_route_query_item_title(
                state, index, crazypod_music_search_query(),
                crazypod_activity_stopwatch_running(),
                crazypod_activity_workout_running()) : "",
            selected,
            selected ? 255 :
                state->route == MUSIC_ROUTE_SEARCH ? 150 : 195,
            COLOR_PANEL,
            highlight_color(crazypod_appearance_get()->primary_color),
            highlight_color(crazypod_appearance_get()->secondary_color),
            crazypod_appearance_get()->highlight_style != 0,
            icon_text, selected ? 255 : 170,
            marker_text, selected ? 205 : 90);
    }
    if(count > 1) {
        int thumb_height = SCROLL_HEIGHT * VISIBLE_ROWS / count;
        int thumb_y;

        if(thumb_height < 12)
            thumb_height = 12;
        thumb_y = SCROLL_Y + (SCROLL_HEIGHT - thumb_height) *
            state->selected / (count - 1);
        crazypod_menu_list_refresh_scroll(thumb_y, thumb_height);
    }
}

#endif
