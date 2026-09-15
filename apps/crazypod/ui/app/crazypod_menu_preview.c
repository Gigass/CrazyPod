#include "config.h"

#include <string.h>

#include "../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include "settings.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_artwork.h"
#include "../../crazypod_music.h"
#include "../../crazypod_playlist.h"
#include "../../crazypod_state.h"
#include "../navigation/crazypod_route_query.h"
#include "../presentation/crazypod_menu_icon_assets.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/settings/crazypod_settings_feature.h"
#include "../navigation/crazypod_route_registry.h"
#include "../presentation/crazypod_glass_slots.h"
#include "../presentation/crazypod_marquee.h"
#include "../presentation/crazypod_preview_motion.h"
#include "../presentation/crazypod_preview_primitives.h"
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

/*
 * The Reduce Effects preview is rebuilt on every wheel step: a badge, its
 * icon, and a caption panel that is itself a box, a bevel, two fasteners
 * and a marquee label. That is a dozen objects created and destroyed per
 * step, which on the PP5022 is worth more than the drawing it replaced.
 * Keep them alive for as long as one route owns the pane and retitle them
 * instead.
 *
 * Lifetime: the pane is cleaned wholesale on every route change, and
 * crazypod_menu_preview_reset() runs just before that clean, so it drops
 * these pointers itself. lv_obj_null_on_delete() is the backstop for any
 * other path that deletes the tree from underneath us -- a stale object
 * pointer here would be invalidated later through a freed parent chain.
 */
static struct {
    lv_obj_t *root;
    lv_obj_t *badge;
    lv_obj_t *icon;
    lv_obj_t *caption;
    lv_obj_t *caption_label;
    const lv_obj_t *parent;
    const lv_image_dsc_t *asset;
} simple;

static void forget_simple(void)
{
    if(simple.root != NULL && lv_obj_is_valid(simple.root))
        lv_obj_delete(simple.root);
    memset(&simple, 0, sizeof(simple));
}

static void hide_simple(void)
{
    if(simple.root != NULL && lv_obj_is_valid(simple.root))
        lv_obj_add_flag(simple.root, LV_OBJ_FLAG_HIDDEN);
}

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

static bool copy_current_track(struct crazypod_track *track)
{
    char path[MAX_PATH];

    return crazypod_queue_copy_path(
            crazypod_queue_index(), path, sizeof(path)) &&
        crazypod_music_copy_track(crazypod_music_find_track(path), track);
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
        parent, crazypod_preview_centered_x(118),
        crazypod_preview_visual_y(64),
        118, 64, 14,
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
    lv_obj_set_size(label, 98, 23);
    crazypod_marquee_configure_centered(label, true);

    crazypod_preview_make_caption(
        parent, detail, &lv_font_montserrat_8,
        "", &lv_font_montserrat_8);
}

static void render_photos(
    const struct route_state *state, bool videos)
{
    crazypod_photos_feature_render_preview(
        state, preview_parent(), videos,
        preview.defer_media,
        crazypod_preview_motion_media_deferred_flag());
}

static void render_utility(
    const struct route_state *state)
{
    if(crazypod_miniapps_feature_render_gameboy_preview(
           state, preview_parent()))
        return;
    crazypod_organizer_feature_render_preview(
        preview_parent(), state,
        preview.host.item_title(state, state->selected),
        crazypod_miniapps_feature_last_error(),
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
    /* The caller cleans the pane immediately after this, taking the
     * persistent preview with it. Drop the pointers before they dangle. */
    memset(&simple, 0, sizeof(simple));
}

static void build_simple(lv_obj_t *parent)
{
    simple.root = lv_obj_create(parent);
    crazypod_ui_widget_make_plain(simple.root);
    lv_obj_set_pos(simple.root, 0, 0);
    lv_obj_set_size(simple.root, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(simple.root, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(simple.root, LV_OBJ_FLAG_CLICKABLE);

    simple.badge = crazypod_ui_widget_box(
        simple.root, 212, 74, 56, 56, LV_RADIUS_CIRCLE,
        primary_color(), 200);
    lv_obj_set_style_border_width(simple.badge, 1, 0);
    lv_obj_set_style_border_color(
        simple.badge, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(simple.badge, 90, 0);

    simple.icon = lv_image_create(simple.badge);
    lv_image_set_scale(simple.icon, 512);
    lv_obj_set_style_image_recolor(
        simple.icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_image_recolor_opa(
        simple.icon, LV_OPA_COVER, 0);
    lv_obj_remove_flag(simple.icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(simple.icon, LV_OBJ_FLAG_HIDDEN);

    simple.caption = crazypod_preview_make_caption_labels(
        simple.root, "", preview.host.metadata_font,
        "", &lv_font_montserrat_8, &simple.caption_label, NULL);

    simple.parent = parent;
    lv_obj_null_on_delete(&simple.root);
    lv_obj_null_on_delete(&simple.badge);
    lv_obj_null_on_delete(&simple.icon);
    lv_obj_null_on_delete(&simple.caption);
    lv_obj_null_on_delete(&simple.caption_label);
}

static void render_simple(const struct route_state *state)
{
    const char *title = preview.host.item_title(state, state->selected);
    const lv_image_dsc_t *asset = crazypod_menu_icon_asset(
        crazypod_route_query_item_icon(state, state->selected));
    lv_obj_t *parent = preview.host.parent;

    if(parent == NULL)
        return;
    /* The pane is rebuilt under a different parent, or something deleted
     * the tree without going through reset(): start over rather than
     * touch anything that may already be gone. */
    if(simple.root != NULL &&
       (simple.parent != parent || !lv_obj_is_valid(simple.root) ||
        simple.badge == NULL || simple.icon == NULL ||
        simple.caption_label == NULL))
        forget_simple();
    if(simple.root == NULL)
        build_simple(parent);
    if(simple.root == NULL)
        return;

    lv_obj_set_style_bg_color(
        simple.badge, lv_color_hex(primary_color()), 0);
    if(simple.asset != asset) {
        simple.asset = asset;
        if(asset != NULL) {
            lv_image_set_src(simple.icon, asset);
            lv_obj_center(simple.icon);
            lv_obj_remove_flag(simple.icon, LV_OBJ_FLAG_HIDDEN);
        }
        else
            lv_obj_add_flag(simple.icon, LV_OBJ_FLAG_HIDDEN);
    }
    crazypod_marquee_set_text_centered(
        simple.caption_label, title != NULL ? title : "", true);
    lv_obj_remove_flag(simple.root, LV_OBJ_FLAG_HIDDEN);
}

void crazypod_menu_preview_render(
    const struct route_state *state, bool animated)
{
    struct crazypod_track track;
    bool have_track;
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
    if(crazypod_state_reduce_effects() &&
       state->route != MUSIC_ROUTE_SEARCH &&
       state->route != NOTES_ROUTE_SEARCH &&
       state->route != CALENDAR_ROUTE_TITLE_EDITOR) {
        /*
         * A skeuomorphic preview is twenty to forty objects with bevels,
         * fasteners and motion parts, rebuilt on every wheel step; on the
         * PP5022 that rebuild is most of the frame. Reduce Effects trades
         * it for the item's icon and a caption.
         */
        render_simple(state);
        preview.defer_media = false;
        /* The simple preview lives beside the motion content, not inside
         * it, so there is nothing for an exit animation to carry off --
         * and reporting motion_ready would make the scheduler wait out
         * that animation before retitling. Take the direct path. */
        preview.motion_ready = false;
        return;
    }
    hide_simple();
    if(state->route == NOTES_ROUTE_SEARCH)
        return;
    if(state->route == MUSIC_ROUTE_SEARCH) {
        render_editor(
            crazypod_music_search_query(), CP_TR("Any track"),
            CP_TR("Searches title, artist and album."));
    }
    else if(state->route == CALENDAR_ROUTE_TITLE_EDITOR) {
        static char cursor_text[98];

        render_editor(
            crazypod_organizer_feature_editor_title(
                cursor_text, sizeof(cursor_text)),
            CP_TR("Event title"),
            CP_TR("Center inserts · Left/Right moves cursor."));
    }
    else if(state->route == MUSIC_ROUTE_MENU) {
        crazypod_music_feature_render_root_preview(
            preview_parent(), state->selected,
            preview.defer_media);
    }
    else if(state->route == PHOTOS_ROUTE_MENU)
        render_photos(state, false);
    else if(state->route == PHOTOS_ROUTE_DELETE_MENU)
        render_photos(state, state->selected == 1);
    else if(state->route == PHOTOS_ROUTE_VIDEOS ||
            state->route == PHOTOS_ROUTE_DELETE_VIDEOS)
        render_photos(state, true);
    else if(state->route == EXTRAS_ROUTE_MENU) {
        crazypod_extras_preview_render(
            preview_parent(), state,
            preview.host.metadata_font);
    }
    else if(state->route >= NOTES_ROUTE_MENU &&
            state->route <= NOTES_ROUTE_EMPTY_TRASH_CONFIRM) {
        crazypod_notes_feature_render_preview(
            preview_parent(), state,
            preview.host.metadata_font);
    }
    else if(state->route >= BOOKS_ROUTE_MENU &&
            state->route <= BOOKS_ROUTE_INFO) {
        crazypod_books_feature_render_preview(
            preview_parent(), state,
            preview.host.metadata_font);
    }
    else if(state->route == UTILITIES_ROUTE_MENU ||
            state->route == GAMEBOY_ROUTE_LIBRARY ||
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
            crazypod_customize_feature_preset_editor_value());
    }
    else {
        have_track = crazypod_music_feature_copy_route_track(
            state, state->selected, &track);
        crazypod_music_feature_render_item_preview(
            preview_parent(), state, have_track ? &track : NULL,
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
    struct crazypod_track track;
    bool have_track = false;
    int i;
    int count;

    if(state == NULL)
        return;
    if(state->route >= PHOTOS_ROUTE_MENU &&
       state->route <= PHOTOS_ROUTE_DETAIL) {
        crazypod_photos_feature_prefetch_preview(state);
        return;
    }
    if(state->route == MUSIC_ROUTE_MENU) {
        if(state->selected == 0) {
            have_track = copy_current_track(&track);
            if(have_track)
                (void)crazypod_artwork_load_priority(
                    CRAZYPOD_PREVIEW_ARTWORK_SLOT, &track,
                    NOW_ARTWORK_SIZE, ARTWORK_PRIORITY);
        }
        else if(state->selected == 1) {
            count = crazypod_music_album_count();
            for(i = 0; i < 3 && i < count; ++i) {
                have_track = crazypod_music_copy_album_track(i, 0, &track);
                if(have_track)
                    (void)crazypod_artwork_load_priority(
                        CRAZYPOD_MENU_PREVIEW_FLOW_SLOT_BASE + i,
                        &track, FLOW_ARTWORK_SIZE,
                        ARTWORK_PRIORITY);
            }
        }
        else if(state->selected == 5 &&
                crazypod_music_album_count() > 0) {
            have_track = crazypod_music_copy_album_track(0, 0, &track);
            if(have_track)
                (void)crazypod_artwork_load_priority(
                    CRAZYPOD_PREVIEW_ARTWORK_SLOT, &track,
                    ALBUM_ARTWORK_SIZE, ARTWORK_PRIORITY);
        }
        return;
    }
    if(state->route == MUSIC_ROUTE_ALBUMS)
        have_track = crazypod_music_copy_album_track(
            state->selected, 0, &track);
    else if(state->route == MUSIC_ROUTE_ARTISTS ||
            state->route == MUSIC_ROUTE_PLAYLISTS)
        return;
    else if(crazypod_menu_preview_is_music_route(
                state->route))
        have_track = crazypod_music_feature_copy_route_track(
            state, state->selected, &track);
    if(have_track)
        (void)crazypod_artwork_load_priority(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT, &track,
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
