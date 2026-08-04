#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "lvgl.h"

#include "../../crazypod_apps.h"
#include "../../crazypod_miniapps.h"
#include "crazypod_ui_menu_layout.h"
#include "crazypod_ui_widgets.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../shell/crazypod_app_catalog.h"
#include "crazypod_empty_state.h"
#include "crazypod_menu_list.h"
#include "../navigation/crazypod_route_query.h"
#include "../features/settings/crazypod_settings_feature.h"
#include "crazypod_marquee.h"
#include "crazypod_menu_screen.h"

#define COLOR_WHITE 0xFFFFFF
#define CRAZYPOD_VISIBLE_ROWS 7
#define CRAZYPOD_MENU_HEADER_X 16
#define CRAZYPOD_MENU_HEADER_Y 42
#define CRAZYPOD_MENU_HEADER_WIDTH 128
#define CRAZYPOD_MENU_HEADER_HEIGHT 20
#define CRAZYPOD_MENU_ROW_X 8
#define CRAZYPOD_MENU_ROW_Y 64
#define CRAZYPOD_MENU_ROW_WIDTH 140
#define CRAZYPOD_MENU_ROW_HEIGHT 24
#define CRAZYPOD_MENU_ROW_STEP 24
#define CRAZYPOD_MENU_SCROLL_X 153
#define CRAZYPOD_MENU_SCROLL_Y 66
#define CRAZYPOD_MENU_SCROLL_HEIGHT 164

static const char *const music_menu_symbols[] = {
    LV_SYMBOL_AUDIO, LV_SYMBOL_IMAGE, LV_SYMBOL_LOOP, LV_SYMBOL_LIST,
    LV_SYMBOL_HOME, LV_SYMBOL_DIRECTORY, LV_SYMBOL_AUDIO,
    LV_SYMBOL_EYE_OPEN
};
static const char *const photos_menu_symbols[] = {
    LV_SYMBOL_IMAGE, LV_SYMBOL_PLAY, LV_SYMBOL_OK, LV_SYMBOL_TRASH
};

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

static void make_pixel_heart(
    lv_obj_t *parent, int x, int y, int unit,
    uint32_t color, lv_opa_t opacity)
{
    crazypod_ui_widget_pixel_heart(
        parent, x, y, unit, color, opacity);
}

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

void crazypod_menu_screen_render(
    const struct route_state *state,
    const struct crazypod_menu_screen_context *context)
{
    int count = context->item_count;
    int start;
    int row;
    lv_obj_t *header;

    crazypod_menu_list_reset(state->route);
    header = make_label(context->parent, crazypod_route_query_title(state),
                        context->metadata_font,
                        COLOR_WHITE, 85);
    lv_obj_set_pos(header, CRAZYPOD_MENU_HEADER_X,
                   CRAZYPOD_MENU_HEADER_Y);
    lv_obj_set_width(header, CRAZYPOD_MENU_HEADER_WIDTH);
    lv_obj_set_height(header, CRAZYPOD_MENU_HEADER_HEIGHT);
    lv_label_set_long_mode(header, LV_LABEL_LONG_MODE_DOTS);

    if(count <= 0) {
        if(state->route == DIY_ROUTE_WALLPAPER_FILES)
            crazypod_empty_state_render(
                context->parent, LV_SYMBOL_IMAGE, CP_TR("No Pictures"),
                CP_TR("Add JPG or BMP files to /Pictures."));
        else if(state->route == PHOTOS_ROUTE_LIBRARY)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_IMAGE,
                CP_TR("No Pictures"),
                CP_TR("Add JPG, JPEG or BMP files to /Pictures."));
        else if(state->route == PHOTOS_ROUTE_VIDEOS ||
                state->route == PHOTOS_ROUTE_DELETE_VIDEOS)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_PLAY,
                CP_TR("No Videos"),
                CP_TR("Convert MPG or MPEG files into /Videos."));
        else if(state->route == PHOTOS_ROUTE_FAVORITES)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_IMAGE,
                CP_TR("No Favorites"),
                               CP_TR("Hold Select on a photo to save it here."));
        else if(state->route == EXTRAS_ROUTE_MENU)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_DIRECTORY,
                CP_TR("Nothing Hidden"),
                CP_TR("Hide apps in Settings > Main Menu."));
        else if(state->route == UTILITIES_ROUTE_MENU)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Mini Apps"),
                CP_TR("Copy a CPK directly to /MiniApps."));
        else if(state->route == NOTES_ROUTE_DELETED)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_EDIT,
                CP_TR("Deleted Is Empty"),
                CP_TR("Deleted notes can be restored from here."));
        else if(state->route == BOOKS_ROUTE_LIBRARY)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Books"),
                CP_TR("Add EPUB, TXT or Markdown files to /Books."));
        else if(state->route == BOOKS_ROUTE_RECENTS)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Recent Books"),
                CP_TR("Open a book to add it here."));
        else if(state->route == BOOKS_ROUTE_FAVORITES)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Favorites"),
                CP_TR("Favorite a book from Book Actions."));
        else if(state->route == BOOKS_ROUTE_BOOKMARKS)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Bookmark"),
                CP_TR("Press PLAY while reading to save this page."));
        else if(state->route == PODCASTS_ROUTE_MENU)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_AUDIO,
                CP_TR("No Podcasts"),
                CP_TR("Add audio files under /Podcasts and rescan."));
        else if(state->route == CONTACTS_ROUTE_LIST)
            crazypod_empty_state_render(context->parent,
                NULL,
                CP_TR("No Contacts"),
                CP_TR("Add VCF files to /Contacts."));
        else if(state->route == WORKOUT_ROUTE_HISTORY)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_PLAY,
                CP_TR("No Workouts"),
                CP_TR("Start a time-only workout first."));
        else
            crazypod_empty_state_render(
                context->parent, LV_SYMBOL_AUDIO, CP_TR("Nothing Here"),
                CP_TR("Add local music and rescan."));
        return;
    }

    start = crazypod_ui_menu_window_start(
        count, state->selected, CRAZYPOD_VISIBLE_ROWS);

    for(row = 0; row < CRAZYPOD_VISIBLE_ROWS; ++row) {
        int index = start + row;
        int y = CRAZYPOD_MENU_ROW_Y + row * CRAZYPOD_MENU_ROW_STEP;
        bool selected = index == state->selected;
        lv_obj_t *row_box;
        lv_obj_t *label;
        lv_obj_t *marker;
        const char *title;
        int text_x = 12;
        int text_width = 120;

        if(index >= count)
            break;
        row_box = make_box(context->parent,
                           CRAZYPOD_MENU_ROW_X, y,
                           CRAZYPOD_MENU_ROW_WIDTH,
                           CRAZYPOD_MENU_ROW_HEIGHT, 8,
                           selected ? context->primary_color : context->panel_color,
                           selected ? 220 : LV_OPA_TRANSP);
        if(selected) {
            if(context->gradient_highlight) {
                lv_obj_set_style_bg_grad_color(
                    row_box, lv_color_hex(context->secondary_color), 0);
                lv_obj_set_style_bg_grad_dir(row_box, LV_GRAD_DIR_HOR, 0);
            }
            lv_obj_set_style_border_width(row_box, 1, 0);
            lv_obj_set_style_border_color(row_box,
                                           lv_color_hex(COLOR_WHITE), 0);
            lv_obj_set_style_border_opa(row_box, 90, 0);
        }

        if(state->route == MUSIC_ROUTE_MENU ||
           state->route == PHOTOS_ROUTE_MENU ||
           state->route == SETTINGS_ROUTE_MENU ||
           state->route == SETTINGS_ROUTE_MAIN_MENU ||
           state->route == EXTRAS_ROUTE_MENU ||
           state->route == UTILITIES_ROUTE_MENU ||
           state->route == DIY_ROUTE_MENU) {
            const struct crazypod_app_descriptor *route_app_item =
                route_app(state, index);
            const char *icon_text = route_app_item != NULL
                ? route_app_item->symbol :
                state->route == MUSIC_ROUTE_MENU
                    ? music_menu_symbols[index]
                : state->route == PHOTOS_ROUTE_MENU
                    ? photos_menu_symbols[index]
                : state->route == SETTINGS_ROUTE_MENU
                    ? crazypod_settings_feature_menu_symbol(index)
                : state->route == UTILITIES_ROUTE_MENU
                    ? miniapp_symbol(index)
                    : crazypod_customize_feature_menu_symbol(index);
            lv_obj_t *circle = make_box(row_box, 0, 0, 21, 21,
                                        LV_RADIUS_CIRCLE, COLOR_WHITE,
                                        selected ? 45 : 18);
            lv_obj_t *icon;

            lv_obj_align(circle, LV_ALIGN_LEFT_MID, 6, 0);
            if(state->route == PHOTOS_ROUTE_MENU && index == 2) {
                icon = make_box(circle, 0, 0, 8, 6, 0,
                                COLOR_WHITE, LV_OPA_TRANSP);
                make_pixel_heart(icon, 0, 0, 1,
                                 0xFF375F, LV_OPA_COVER);
                lv_obj_center(icon);
                lv_obj_set_style_opa(
                    icon, selected ? 255 : 170, 0);
            }
            else {
                icon = make_label(circle, icon_text,
                                  &lv_font_montserrat_10,
                                  COLOR_WHITE,
                                  selected ? 255 : 170);
                lv_obj_center(icon);
            }
            crazypod_menu_list_bind_icon(
                row, circle, icon,
                !(state->route == PHOTOS_ROUTE_MENU && index == 2));
            text_x = 34;
            text_width = 88;
        }
        title = context->item_title(state, index);
        if(title == NULL)
            title = "";
        label = make_label(row_box, title, context->metadata_font,
                           COLOR_WHITE, selected ? 255 : 195);
        lv_obj_set_width(label, text_width);
        crazypod_ui_widget_align_row_label(label, text_x);
        crazypod_marquee_configure(label, selected);

        marker = make_label(row_box,
                            context->item_is_current(state, index)
                                ? LV_SYMBOL_OK :
                            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET,
                            &lv_font_montserrat_8,
                            COLOR_WHITE, selected ? 205 : 90);
        crazypod_ui_widget_align_row_label(marker, 128);
        crazypod_menu_list_bind_row(row, row_box, label, marker);
    }

    if(count > CRAZYPOD_VISIBLE_ROWS) {
        int track_height = CRAZYPOD_MENU_SCROLL_HEIGHT;
        int thumb_height;
        int thumb_y;
        lv_obj_t *bar;
        crazypod_ui_menu_scroll_thumb(
            count, state->selected, CRAZYPOD_VISIBLE_ROWS,
            CRAZYPOD_MENU_SCROLL_Y, track_height, 12,
            &thumb_y, &thumb_height);
        bar = make_box(context->parent, CRAZYPOD_MENU_SCROLL_X,
                       CRAZYPOD_MENU_SCROLL_Y, 2, track_height, 1,
                       COLOR_WHITE, 25);
        (void)bar;
        bar = make_box(context->parent, CRAZYPOD_MENU_SCROLL_X,
                       thumb_y, 2, thumb_height, 1, COLOR_WHITE, 155);
        crazypod_menu_list_bind_scroll_thumb(bar);
    }

}



#endif
