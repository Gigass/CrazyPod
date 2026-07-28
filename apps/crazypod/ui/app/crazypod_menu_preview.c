#include "config.h"

#ifdef IPOD_6G

#include "settings.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_artwork.h"
#include "../../crazypod_music.h"
#include "../../crazypod_playlist.h"
#include "../features/books/crazypod_books_preview.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/customize/crazypod_preset_editor_controller.h"
#include "../features/miniapps/crazypod_miniapp_runtime_controller.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/music/crazypod_music_item_preview.h"
#include "../features/music/crazypod_music_root_preview.h"
#include "../features/notes/crazypod_notes_preview.h"
#include "../features/organizer/crazypod_calendar_controller.h"
#include "../features/organizer/crazypod_utility_preview.h"
#include "../features/photos/crazypod_photos_preview.h"
#include "../features/settings/crazypod_settings_feature.h"
#include "../navigation/crazypod_route_registry.h"
#include "../presentation/crazypod_glass_slots.h"
#include "../presentation/crazypod_preview_motion.h"
#include "../presentation/crazypod_preview_primitives.h"
#include "../presentation/crazypod_ui_text.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "../shell/crazypod_extras_preview.h"
#include "crazypod_menu_preview.h"

#define ARTWORK_CACHE_SIZE 72
#define NOW_ARTWORK_SIZE 68
#define FLOW_ARTWORK_SIZE 58
#define ALBUM_ARTWORK_SIZE 67
#define ARTWORK_PRIORITY 20
#define COLOR_WHITE 0xFFFFFF

static struct {
    struct crazypod_menu_preview_host host;
    bool motion_ready;
    bool defer_media;
} preview;

static uint32_t primary_color(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->primary_color);
}

static uint32_t secondary_color(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->secondary_color);
}

static lv_obj_t *preview_parent(void)
{
    return crazypod_preview_motion_parent(preview.host.parent);
}

static const struct crazypod_track *current_track(void)
{
    const char *path =
        crazypod_queue_path(crazypod_queue_index());

    return crazypod_music_track(
        crazypod_music_find_track(path));
}

bool crazypod_menu_preview_is_music_route(
    enum crazypod_route route)
{
    return crazypod_route_registry_has_flag(
        route, CRAZYPOD_ROUTE_FLAG_PREVIEW);
}

bool crazypod_menu_preview_is_skeuomorphic_route(
    enum crazypod_route route)
{
    return crazypod_route_registry_has_flag(
        route,
        CRAZYPOD_ROUTE_FLAG_SKEUOMORPHIC_PREVIEW);
}

static bool is_settings_route(enum crazypod_route route)
{
    return route >= SETTINGS_ROUTE_MENU &&
        route <= SETTINGS_ROUTE_MAIN_MENU_ACTIONS;
}

static void render_editor(
    const char *value, const char *empty_text,
    const char *detail)
{
    lv_obj_t *parent = preview_parent();
    lv_obj_t *card;
    lv_obj_t *symbol;
    lv_obj_t *label;

    card = crazypod_ui_widget_box(
        parent, 181, 78, 118, 64, 14,
        primary_color(), 210);
    lv_obj_set_style_bg_grad_color(
        card, lv_color_hex(secondary_color()), 0);
    lv_obj_set_style_bg_grad_dir(
        card, LV_GRAD_DIR_HOR, 0);
    symbol = crazypod_ui_widget_label(
        card, LV_SYMBOL_KEYBOARD,
        &lv_font_montserrat_16, COLOR_WHITE, 225);
    lv_obj_set_pos(symbol, 10, 9);
    label = crazypod_ui_widget_label(
        card,
        value != NULL && value[0] != '\0'
            ? value : empty_text,
        &lv_font_montserrat_12, COLOR_WHITE,
        value != NULL && value[0] != '\0' ? 255 : 130);
    lv_obj_set_pos(label, 10, 35);
    lv_obj_set_width(label, 98);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    card = crazypod_preview_make_text_panel(
        parent, 154, 46);
    label = crazypod_ui_widget_label(
        card, detail, &lv_font_montserrat_8,
        COLOR_WHITE, 125);
    lv_obj_set_pos(label, 11, 8);
    lv_obj_set_width(label, 118);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(
        label, LV_LABEL_LONG_MODE_WRAP);
}

static void render_photos(
    const struct route_state *state, bool videos)
{
    const struct crazypod_photos_preview_context context = {
        .parent = preview_parent(),
        .defer_media = preview.defer_media,
        .media_deferred =
            crazypod_preview_motion_media_deferred_flag(),
    };

    if(videos)
        crazypod_videos_preview_render(state, &context);
    else
        crazypod_photos_preview_render(state, &context);
}

static void render_utility(
    const struct route_state *state)
{
    crazypod_utility_preview_render(
        preview_parent(), state,
        preview.host.item_title(state, state->selected),
        crazypod_miniapp_runtime_last_error(),
        preview.host.metadata_font);
}

void crazypod_menu_preview_configure(
    const struct crazypod_menu_preview_host *host)
{
    if(host != NULL)
        preview.host = *host;
}

void crazypod_menu_preview_reset(void)
{
    preview.motion_ready = false;
    preview.defer_media = false;
}

void crazypod_menu_preview_render(
    const struct route_state *state, bool animated)
{
    bool animate = animated && preview.motion_ready &&
        crazypod_menu_preview_is_skeuomorphic_route(
            state->route);

    if(state->route == MUSIC_ROUTE_MENU)
        crazypod_preview_motion_set_profile(
            CRAZYPOD_PREVIEW_PROFILE_MUSIC);
    else if(state->route == PHOTOS_ROUTE_MENU)
        crazypod_preview_motion_set_profile(
            CRAZYPOD_PREVIEW_PROFILE_PHOTOS);
    else if(state->route >= NOTES_ROUTE_MENU &&
            state->route <= NOTES_ROUTE_EMPTY_TRASH_CONFIRM)
        crazypod_preview_motion_set_profile(
            CRAZYPOD_PREVIEW_PROFILE_NOTES);
    else if(state->route >= BOOKS_ROUTE_MENU &&
            state->route <= BOOKS_ROUTE_INFO)
        crazypod_preview_motion_set_profile(
            CRAZYPOD_PREVIEW_PROFILE_BOOKS);
    else
        crazypod_preview_motion_set_profile(
            CRAZYPOD_PREVIEW_PROFILE_DEFAULT);

    preview.defer_media = animate;
    *crazypod_preview_motion_media_deferred_flag() = false;
    crazypod_preview_motion_reset_root(preview.host.parent);
    if(state->route == MUSIC_ROUTE_SEARCH) {
        render_editor(
            crazypod_music_search_query(), "Any track",
            "Searches title, artist and album.");
    }
    else if(state->route == CALENDAR_ROUTE_TITLE_EDITOR) {
        const struct crazypod_calendar_editor_model editor =
            crazypod_calendar_controller_editor();
        static char cursor_text[98];

        render_editor(
            crazypod_ui_text_with_cursor(
                editor.summary, editor.cursor,
                cursor_text, sizeof(cursor_text)),
            "Event title",
            "Center inserts · Left/Right moves cursor.");
    }
    else if(state->route == MUSIC_ROUTE_MENU) {
        crazypod_music_root_preview_render(
            preview_parent(), state->selected,
            preview.defer_media);
    }
    else if(state->route == PHOTOS_ROUTE_MENU)
        render_photos(state, false);
    else if(state->route == PHOTOS_ROUTE_VIDEOS)
        render_photos(state, true);
    else if(state->route == EXTRAS_ROUTE_MENU) {
        crazypod_extras_preview_render(
            preview_parent(), state,
            preview.host.metadata_font);
    }
    else if(state->route >= NOTES_ROUTE_MENU &&
            state->route <= NOTES_ROUTE_EMPTY_TRASH_CONFIRM) {
        crazypod_notes_preview_render(
            preview_parent(), state,
            preview.host.metadata_font);
    }
    else if(state->route >= BOOKS_ROUTE_MENU &&
            state->route <= BOOKS_ROUTE_INFO) {
        crazypod_books_preview_render(
            preview_parent(), state,
            preview.host.metadata_font);
    }
    else if(state->route == UTILITIES_ROUTE_MENU ||
            state->route == CLOCK_ROUTE_MENU ||
            state->route == CLOCK_ROUTE_SLEEP_TIMER ||
            state->route == WORKOUT_ROUTE_MENU ||
            state->route == WORKOUT_ROUTE_TYPES ||
            state->route == WORKOUT_ROUTE_HISTORY ||
            state->route == WORKOUT_ROUTE_FINISH_CONFIRM ||
            state->route == WORKOUT_ROUTE_DELETE_CONFIRM ||
            state->route == CALENDAR_ROUTE_MENU ||
            state->route == CALENDAR_ROUTE_TODAY ||
            state->route == CALENDAR_ROUTE_UPCOMING ||
            state->route == CALENDAR_ROUTE_DAY_EVENTS ||
            state->route == CALENDAR_ROUTE_EDITOR ||
            state->route == CALENDAR_ROUTE_ACTIONS ||
            state->route == CALENDAR_ROUTE_DELETE_CONFIRM ||
            state->route == CONTACTS_ROUTE_LIST)
        render_utility(state);
    else if(is_settings_route(state->route)) {
        crazypod_settings_feature_render_preview(
            preview_parent(), state,
            preview.host.item_title(
                state, state->selected),
            primary_color(), secondary_color(),
            global_settings.eq_enabled,
            global_settings.playlist_shuffle,
            crazypod_queue_repeat() != REPEAT_OFF);
    }
    else if(state->route >= DIY_ROUTE_MENU) {
        crazypod_customize_feature_render_preview(
            preview_parent(), state,
            preview.host.item_title(
                state, state->selected),
            primary_color(), secondary_color(),
            crazypod_preset_editor_value());
    }
    else {
        crazypod_music_item_preview_render(
            preview_parent(), state,
            crazypod_music_feature_route_track(
                state, state->selected),
            preview.host.metadata_font);
    }
    preview.defer_media = false;
    preview.motion_ready = true;
    if(animate)
        crazypod_preview_motion_start_entrance();
}

void crazypod_menu_preview_prefetch(
    const struct route_state *state)
{
    const struct crazypod_track *track = NULL;
    int i;
    int count;

    if(state == NULL)
        return;
    if(state->route == MUSIC_ROUTE_MENU) {
        if(state->selected == 0) {
            track = current_track();
            if(track != NULL)
                (void)crazypod_artwork_load_priority(
                    CRAZYPOD_PREVIEW_ARTWORK_SLOT, track,
                    NOW_ARTWORK_SIZE, ARTWORK_PRIORITY);
        }
        else if(state->selected == 1) {
            count = crazypod_music_album_count();
            for(i = 0; i < 3 && i < count; ++i) {
                track = crazypod_music_album_track(i, 0);
                if(track != NULL)
                    (void)crazypod_artwork_load_priority(
                        CRAZYPOD_MENU_PREVIEW_FLOW_SLOT_BASE + i,
                        track, FLOW_ARTWORK_SIZE,
                        ARTWORK_PRIORITY);
            }
        }
        else if(state->selected == 5 &&
                crazypod_music_album_count() > 0) {
            track = crazypod_music_album_track(0, 0);
            if(track != NULL)
                (void)crazypod_artwork_load_priority(
                    CRAZYPOD_PREVIEW_ARTWORK_SLOT, track,
                    ALBUM_ARTWORK_SIZE, ARTWORK_PRIORITY);
        }
        return;
    }
    if(state->route == MUSIC_ROUTE_ALBUMS)
        track = crazypod_music_album_track(
            state->selected, 0);
    else if(state->route == MUSIC_ROUTE_ARTISTS ||
            state->route == MUSIC_ROUTE_PLAYLISTS)
        return;
    else if(crazypod_menu_preview_is_music_route(
                state->route))
        track = crazypod_music_feature_route_track(
            state, state->selected);
    if(track != NULL)
        (void)crazypod_artwork_load_priority(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT, track,
            ARTWORK_CACHE_SIZE, ARTWORK_PRIORITY);
}

void crazypod_menu_preview_settle(void)
{
    crazypod_preview_motion_settle();
}

bool crazypod_menu_preview_motion_ready(void)
{
    return preview.motion_ready;
}

#endif
