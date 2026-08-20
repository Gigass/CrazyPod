#include "config.h"

#ifdef IPOD_6G

#include "../../crazypod_appearance.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../navigation/crazypod_route_query.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../presentation/crazypod_menu_list.h"
#include "../presentation/crazypod_ui_menu_layout.h"
#include "crazypod_menu_rows.h"

#define VISIBLE_ROWS 6
#define SCROLL_Y 66
#define SCROLL_HEIGHT 164
#define COLOR_PANEL 0x1B1B22

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
    start = crazypod_ui_menu_window_start(
        count, state->selected, VISIBLE_ROWS);

    for(row = 0; row < VISIBLE_ROWS; ++row) {
        int index = start + row;
        bool visible = index < count;
        bool selected = visible && index == state->selected;
        enum crazypod_menu_icon icon = visible
            ? crazypod_route_query_item_icon(state, index)
            : CRAZYPOD_MENU_ICON_NONE;
        const char *marker_text =
            visible && crazypod_route_query_item_is_current(state, index)
                ? LV_SYMBOL_OK :
            selected ? LV_SYMBOL_PLAY :
            state->route == MUSIC_ROUTE_SEARCH ? "" : LV_SYMBOL_BULLET;

        crazypod_menu_list_refresh_row(
            row, visible,
            visible ? crazypod_route_query_item_title(
                state, index, crazypod_music_search_query(),
                crazypod_organizer_feature_stopwatch_running(),
                crazypod_organizer_feature_workout_running()) : "",
            selected,
            selected ? 255 :
                state->route == MUSIC_ROUTE_SEARCH ? 150 : 195,
            COLOR_PANEL,
            highlight_color(crazypod_appearance_get()->primary_color),
            highlight_color(crazypod_appearance_get()->secondary_color),
            crazypod_appearance_get()->highlight_style != 0,
            icon, selected ? 255 : 170,
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
