#include "config.h"

#ifdef IPOD_6G

#include "kernel.h"
#include "playlist.h"
#include "timefuncs.h"

#include "../../crazypod_l10n.h"
#include "../../crazypod_apps.h"
#include "../../crazypod_coverflow.h"
#include "../../crazypod_music.h"
#include "../../crazypod_playlist.h"
#include "../../crazypod_presets.h"
#include "../../crazypod_state.h"
#include "../features/books/crazypod_books_feature.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/notes/crazypod_notes_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/organizer/crazypod_organizer_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../features/settings/crazypod_settings_feature.h"
#include "../navigation/crazypod_render_scheduler.h"
#include "../navigation/crazypod_route_registry.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../presentation/crazypod_alpha_jump_hud.h"
#include "../presentation/crazypod_menu_list.h"
#include "../presentation/crazypod_preview_motion.h"
#include "../shell/crazypod_app_catalog.h"
#include "../shell/crazypod_desktop.h"
#include "../shell/crazypod_notification.h"
#include "crazypod_app_launcher.h"
#include "crazypod_choice_coordinator.h"
#include "crazypod_menu_preview.h"
#include "crazypod_playback.h"
#include "crazypod_route_actions.h"

#define PREVIEW_SETTLE_TICKS \
    ((HZ * 120 / 1000) > 0 ? (HZ * 120 / 1000) : 1)
#define ALPHA_JUMP_HUD_TICKS \
    ((HZ * 760 / 1000) > 0 ? (HZ * 760 / 1000) : 1)

static struct crazypod_route_actions_host host;
static bool overlay_dispatch;
static bool overlay_changed;
static bool overlay_transition;
static bool overlay_failed;
static enum crazypod_app_id reorder_preferred =
    CRAZYPOD_APP_INVALID;

static bool queue_music_selection(
    enum crazypod_music_scope scope, int group_index,
    int selected_index, const char *query, bool *deferred);

static struct route_state *current_route(void)
{
    return crazypod_ui_routes_current();
}

static int today_date(void)
{
    struct tm *now = get_time();

    return (now->tm_year + 1900) * 10000 +
        (now->tm_mon + 1) * 100 + now->tm_mday;
}

void crazypod_route_actions_configure(
    const struct crazypod_route_actions_host *new_host)
{
    if(new_host != NULL)
        host = *new_host;
}

static void transaction_render(bool transition)
{
    if(overlay_dispatch) {
        overlay_changed = true;
        overlay_transition = overlay_transition || transition;
        return;
    }
    host.render(transition);
}

static void transaction_failed(void)
{
    if(overlay_dispatch) {
        overlay_failed = true;
        return;
    }
    host.render(false);
    crazypod_choice_coordinator_show_receipt(
        CP_TR("Failed"), false, current_tick, false);
}

static bool push_route_state(
    enum crazypod_route route, int group, int selected)
{
    if(crazypod_route_registry_get(route) == NULL ||
       !crazypod_ui_routes_push(route, group, selected))
        return false;
    crazypod_menu_preview_prefetch(current_route());
    return true;
}

void crazypod_route_actions_push_selected(
    enum crazypod_route route, int group, int selected)
{
    if(route == SETTINGS_ROUTE_MAIN_MENU_ACTIONS) {
        crazypod_choice_coordinator_show(
            CRAZYPOD_CHOICE_MAIN_MENU_ITEM_ACTIONS,
            group, selected);
        return;
    }
    if(route == DIY_ROUTE_NOW_PLAYING_THEMES) {
        crazypod_choice_coordinator_show(
            CRAZYPOD_CHOICE_NOW_PLAYING_THEME,
            0, selected);
        return;
    }
    if(crazypod_choice_coordinator_route_should_overlay(route)) {
        crazypod_choice_coordinator_show_route(
            route, group, selected);
        return;
    }
    if(!push_route_state(route, group, selected))
        return;
    transaction_render(true);
}

void crazypod_route_actions_push(
    enum crazypod_route route, int group)
{
    crazypod_route_actions_push_selected(
        route, group, 0);
}

void crazypod_route_actions_request_now_playing(void)
{
    crazypod_now_playing_request_open();
}

bool crazypod_route_actions_confirm_photos(
    const struct route_state *state, long now)
{
    bool overlay =
        crazypod_choice_coordinator_owns_route_state(state);
    struct crazypod_photos_confirmation_result result =
        crazypod_photos_feature_confirm(state);

    if(!result.handled)
        return false;
    if(overlay) {
        struct route_state *parent = current_route();

        if(parent != NULL &&
           parent->route == result.return_route)
            parent->selected = result.selected;
        crazypod_choice_coordinator_show_receipt(
            result.deleted
                ? CP_TR("Deleted") : CP_TR("Delete Failed"),
            result.deleted, now, true);
        return true;
    }
    if(result.deleted && crazypod_ui_routes_depth() > 1) {
        struct route_state *parent;

        crazypod_ui_routes_pop();
        parent = current_route();
        if(parent != NULL && parent->route == result.return_route)
            parent->selected = result.selected;
    }
    host.render(result.deleted);
    crazypod_notification_show(
        result.deleted ? CRAZYPOD_NOTIFICATION_SUCCESS
                       : CRAZYPOD_NOTIFICATION_ERROR,
        result.deleted
            ? CP_TR("Deleted") : CP_TR("Delete Failed"));
    return true;
}

void crazypod_route_actions_pop(void)
{
    if(crazypod_ui_routes_depth() > 0 &&
       current_route()->route == MUSIC_ROUTE_NOW_PLAYING)
        crazypod_now_playing_theme_close();
    if(crazypod_ui_routes_depth() > 0 &&
       current_route()->route == MINIAPP_ROUTE_VIEW &&
       crazypod_miniapps_feature_is_open()) {
        crazypod_miniapps_feature_reset_input();
        crazypod_miniapps_feature_close();
    }
    if(crazypod_ui_routes_depth() > 1) {
        crazypod_ui_routes_pop();
        transaction_render(true);
    }
    else
        host.close_product();
}

static void play_selected_track(struct route_state *state)
{
    bool started = false;
    bool deferred = false;

    switch(state->route) {
    case MUSIC_ROUTE_ALL:
    case MUSIC_ROUTE_SONGS:
        started = queue_music_selection(
            CRAZYPOD_SCOPE_ALL, 0, state->selected, NULL,
            &deferred);
        break;
    case MUSIC_ROUTE_PLAYLIST_SONGS:
        started = queue_music_selection(
            CRAZYPOD_SCOPE_PLAYLIST,
            state->group, state->selected, NULL, &deferred);
        break;
    case MUSIC_ROUTE_ARTIST_SONGS:
        started = queue_music_selection(
            CRAZYPOD_SCOPE_ARTIST,
            state->group, state->selected, NULL, &deferred);
        break;
    case MUSIC_ROUTE_ALBUM_SONGS:
        started = queue_music_selection(
            CRAZYPOD_SCOPE_ALBUM,
            state->group, state->selected, NULL, &deferred);
        break;
    case MUSIC_ROUTE_QUEUE:
        if(state->selected >= 0 &&
           state->selected < crazypod_queue_count()) {
            if(crazypod_playback_commands_ready()) {
                crazypod_playback_select_async(state->selected);
                deferred = true;
            }
            else
                playlist_start(state->selected, 0, 0);
            started = true;
        }
        break;
    case MUSIC_ROUTE_SEARCH_RESULTS:
        started = queue_music_selection(
            CRAZYPOD_SCOPE_SEARCH, 0, state->selected,
            crazypod_music_search_query(), &deferred);
        break;
    default:
        break;
    }
    if(started && !deferred) {
        crazypod_state_forget_resume();
        crazypod_state_mark_dirty();
    }
    if(started) {
        crazypod_route_actions_request_now_playing();
    }
    else {
        crazypod_notification_show(
            CRAZYPOD_NOTIFICATION_ERROR,
            CP_TR("No track available"));
    }
}

void crazypod_route_actions_begin_note(
    uint32_t id, bool resume_draft)
{
    crazypod_notes_feature_begin_editor(id, resume_draft);
    crazypod_route_actions_push_selected(
        NOTES_ROUTE_COMPOSER, -1, 0);
}

static void open_note_reader(uint32_t id)
{
    crazypod_notes_feature_load_reader(id);
    crazypod_route_actions_push_selected(
        NOTES_ROUTE_READER, (int)id, 0);
}

bool crazypod_route_actions_note_dirty(void)
{
    return crazypod_notes_feature_editor_dirty();
}

void crazypod_route_actions_service_notes(void)
{
    crazypod_notes_feature_service_editor();
}

static void commit_note_editor(void)
{
    bool transaction = overlay_dispatch;
    uint32_t id = crazypod_notes_feature_commit_editor();

    if(id == 0) {
        transaction_failed();
        return;
    }
    crazypod_ui_routes_reset(NOTES_ROUTE_MENU, -1, 0);
    open_note_reader(id);
    if(!transaction)
        crazypod_choice_coordinator_show_receipt(
            CP_TR("Saved"), true, current_tick, false);
}

static void begin_calendar_editor(uint32_t id, int date)
{
    crazypod_organizer_feature_begin_editor(
        id, date > 0 ? date : today_date());
    crazypod_route_actions_push(
        CALENDAR_ROUTE_EDITOR, -1);
}

void crazypod_route_actions_show_calendar_day(int date)
{
    int root = -1;
    int i;

    crazypod_organizer_feature_set_focus_date(date);
    for(i = crazypod_ui_routes_depth() - 1; i >= 0; --i) {
        if(crazypod_ui_routes_at(i)->route ==
           CALENDAR_ROUTE_MENU) {
            root = i;
            break;
        }
    }
    if(root < 0) {
        root = 0;
        crazypod_ui_routes_reset(
            CALENDAR_ROUTE_MENU, -1, 0);
    }
    crazypod_ui_routes_truncate(root + 1);
    crazypod_ui_routes_push(
        CALENDAR_ROUTE_DAY_EVENTS, -1, 0);
}

static bool commit_calendar_editor(void)
{
    bool transaction = overlay_dispatch;
    int date;
    uint32_t id =
        crazypod_organizer_feature_commit_editor(&date);

    if(id == 0) {
        transaction_failed();
        return false;
    }
    crazypod_route_actions_show_calendar_day(date);
    transaction_render(true);
    if(!transaction)
        crazypod_choice_coordinator_show_receipt(
            CP_TR("Saved"), true, current_tick, false);
    return true;
}

static void persist_main_menu(
    enum crazypod_app_id preferred)
{
    int next = crazypod_apps_visible_index(preferred);

    if(next < 0)
        next = crazypod_desktop_selected();
    if(next >= crazypod_apps_visible_count())
        next = crazypod_apps_visible_count() - 1;
    if(next < 0)
        next = 0;
    crazypod_desktop_set_selected(next, false);
    crazypod_state_mark_dirty();
    crazypod_state_save(false);
}

enum crazypod_app_id crazypod_route_actions_selected_app(void)
{
    return crazypod_apps_visible_id(
        crazypod_desktop_selected());
}

void crazypod_route_actions_main_menu_changed(
    enum crazypod_app_id preferred,
    enum crazypod_app_id changed)
{
    struct route_state *parent = current_route();

    if(parent != NULL && parent->route ==
       SETTINGS_ROUTE_MAIN_MENU)
        parent->selected =
            crazypod_apps_order_index(changed);
    persist_main_menu(preferred);
}

void crazypod_route_actions_begin_main_menu_reorder(
    enum crazypod_app_id id)
{
    struct route_state *state = current_route();

    if(state == NULL ||
       state->route != SETTINGS_ROUTE_MAIN_MENU ||
       !crazypod_apps_is_known(id))
        return;
    reorder_preferred =
        crazypod_route_actions_selected_app();
    crazypod_settings_feature_begin_main_menu_reorder(id);
    state->selected = crazypod_apps_order_index(id);
    host.render(false);
}

void crazypod_route_actions_commit_pending_main_menu_reorder(void)
{
    enum crazypod_app_id id;

    if(!crazypod_settings_feature_main_menu_reordering())
        return;
    id = crazypod_settings_feature_finish_main_menu_reorder();
    if(crazypod_apps_is_known(id))
        persist_main_menu(reorder_preferred);
    reorder_preferred = CRAZYPOD_APP_INVALID;
    if(crazypod_apps_is_known(id))
        crazypod_choice_coordinator_show_receipt(
            CP_TR("Saved"), true, current_tick, false);
}

static void show_main_menu_actions(enum crazypod_app_id id)
{
    crazypod_choice_coordinator_show(
        CRAZYPOD_CHOICE_MAIN_MENU_ITEM_ACTIONS,
        id, 0);
}

static void show_setting_choices(int item, int selected)
{
    crazypod_choice_coordinator_show(
        CRAZYPOD_CHOICE_SETTING, item, selected);
}

static void appearance_changed(void)
{
    crazypod_desktop_refresh_appearance();
    transaction_render(false);
}

static void preset_deleted(void)
{
    int depth = overlay_dispatch
        ? crazypod_ui_routes_depth()
        : crazypod_ui_routes_depth() - 2;

    crazypod_ui_routes_truncate(depth < 1 ? 1 : depth);
    if(current_route()->route == DIY_ROUTE_PRESET_LIBRARY &&
       current_route()->selected >= crazypod_preset_count())
        current_route()->selected =
            crazypod_preset_count() > 0
                ? crazypod_preset_count() - 1 : 0;
    transaction_render(false);
}

static void pop_note_composer(void)
{
    crazypod_route_actions_pop();
    if(current_route()->route == NOTES_ROUTE_COMPOSER)
        crazypod_route_actions_pop();
}

static void action_pop(void)
{
    bool renamed = current_route() != NULL &&
        current_route()->route == DIY_ROUTE_PRESET_RENAME;

    if(overlay_dispatch) {
        overlay_changed = true;
        return;
    }
    crazypod_route_actions_pop();
    if(renamed)
        crazypod_choice_coordinator_show_receipt(
            CP_TR("Saved"), true, current_tick, false);
}

static bool transition_needs_receipt(
    const struct route_state *state)
{
    return state != NULL &&
        ((state->route == NOTES_ROUTE_EXIT_ACTIONS &&
          (state->selected == 0 || state->selected == 1)) ||
         (state->route == NOTES_ROUTE_ACTIONS &&
          state->selected == 2));
}

static void reset_open_note_reader(uint32_t id)
{
    crazypod_ui_routes_reset(NOTES_ROUTE_MENU, -1, 0);
    open_note_reader(id);
}

static bool queue_music_selection(
    enum crazypod_music_scope scope, int group_index,
    int selected_index, const char *query, bool *deferred)
{
    *deferred = false;
    if(crazypod_playback_commands_ready()) {
        *deferred = true;
        return crazypod_playback_select_music_async(
            scope, group_index, selected_index, query);
    }
    return scope == CRAZYPOD_SCOPE_SEARCH
        ? crazypod_music_play_search(query, selected_index)
        : crazypod_music_play(scope, group_index, selected_index);
}

static bool play_music_track(int library_index)
{
    bool started;
    bool deferred;

    started = queue_music_selection(
        CRAZYPOD_SCOPE_ALL, 0, library_index, NULL, &deferred);

    if(started && !deferred) {
        crazypod_state_forget_resume();
        crazypod_state_mark_dirty();
    }
    if(!started)
        crazypod_notification_show(
            CRAZYPOD_NOTIFICATION_ERROR,
            CP_TR("No track available"));
    return started;
}

static bool activate_music(
    struct route_state *state, long now)
{
    const struct crazypod_feature *feature =
        crazypod_route_registry_feature(state->route);

    if(feature == NULL)
        return false;
    (void)now;
    if(feature->id == CRAZYPOD_FEATURE_MUSIC ||
       feature->id == CRAZYPOD_FEATURE_NOW_PLAYING) {
        const struct crazypod_music_activation_host actions = {
            .render = transaction_render,
            .push = crazypod_route_actions_push,
            .push_selected =
                crazypod_route_actions_push_selected,
            .initial_album_index =
                host.initial_album_index,
            .request_now_playing =
                crazypod_route_actions_request_now_playing,
            .show_now_actions =
                crazypod_now_playing_overlay_show_actions,
            .play_track = play_music_track,
        };

        return crazypod_music_feature_activate(
            state, &actions);
    }
    return false;
}

static void show_book_font_size(int selected)
{
    crazypod_choice_coordinator_show(
        CRAZYPOD_CHOICE_BOOK_FONT_SIZE, 0, selected);
}

static void show_book_theme(int selected)
{
    crazypod_choice_coordinator_show(
        CRAZYPOD_CHOICE_BOOK_THEME, 0, selected);
}

static bool activate_domain(
    struct route_state *state, long now)
{
    const struct crazypod_feature *feature =
        crazypod_route_registry_feature(state->route);

    if(feature == NULL)
        return false;
    if(feature->id == CRAZYPOD_FEATURE_MUSIC ||
       feature->id == CRAZYPOD_FEATURE_NOW_PLAYING)
        return activate_music(state, now);
    if(feature->id == CRAZYPOD_FEATURE_BOOKS) {
        const struct crazypod_books_activation_host actions = {
            .render = transaction_render,
            .operation_failed = transaction_failed,
            .push = crazypod_route_actions_push,
            .pop = action_pop,
            .show_font_size = show_book_font_size,
            .show_theme = show_book_theme,
        };

        return crazypod_books_feature_activate(
            state, &actions);
    }
    if(feature->id == CRAZYPOD_FEATURE_NOTES) {
        const struct crazypod_notes_activation_host actions = {
            .render = transaction_render,
            .operation_failed = transaction_failed,
            .push = crazypod_route_actions_push,
            .pop = action_pop,
            .pop_composer = pop_note_composer,
            .open_composer =
                crazypod_route_actions_begin_note,
            .open_reader = open_note_reader,
            .reset_open_reader = reset_open_note_reader,
            .commit_editor = commit_note_editor,
        };

        return crazypod_notes_feature_activate(
            state, &actions);
    }
    if(feature->id == CRAZYPOD_FEATURE_PHOTOS) {
        const struct crazypod_photos_activation_host actions = {
            .push = crazypod_route_actions_push,
            .push_selected =
                crazypod_route_actions_push_selected,
            .render = transaction_render,
        };

        return crazypod_photos_feature_activate(
            state, &actions);
    }
    if(feature->id == CRAZYPOD_FEATURE_ORGANIZER) {
        const struct crazypod_organizer_activation_host actions = {
            .render = transaction_render,
            .push = crazypod_route_actions_push,
            .pop = action_pop,
            .begin_editor = begin_calendar_editor,
            .commit_editor = commit_calendar_editor,
        };

        return crazypod_organizer_feature_activate(
            state, now, &actions);
    }
    if(feature->id == CRAZYPOD_FEATURE_SETTINGS) {
        const struct crazypod_settings_activation_host actions = {
            .selected_app =
                crazypod_route_actions_selected_app,
            .push = crazypod_route_actions_push,
            .render = transaction_render,
            .main_menu_changed =
                crazypod_route_actions_main_menu_changed,
            .show_main_menu_actions =
                show_main_menu_actions,
            .show_choices = show_setting_choices,
        };

        return crazypod_settings_feature_activate(
            state, &actions);
    }
    if(feature->id == CRAZYPOD_FEATURE_MINIAPPS) {
        const struct crazypod_miniapps_activation_host actions = {
            .push = crazypod_route_actions_push,
            .render = transaction_render,
        };

        /*
         * Loading a native miniapp is synchronous. Boost before
         * entering it so a user who paused on the list does not run the
         * bootstrap watchdog at the iPod's idle clock.
         */
        host.boost(HZ / 2);
        return crazypod_miniapps_feature_activate(
            state, &actions);
    }
    return false;
}

static void show_icon_choices(int group, int selected)
{
    crazypod_choice_coordinator_show(
        CRAZYPOD_CHOICE_ICON_THEME, group, selected);
}

static void show_appearance_choices(int group, int selected)
{
    crazypod_choice_coordinator_show(
        CRAZYPOD_CHOICE_APPEARANCE, group, selected);
}

static void show_background_choices(int group, int selected)
{
    crazypod_choice_coordinator_show(
        CRAZYPOD_CHOICE_BACKGROUND, group, selected);
}

static bool activate_customize(struct route_state *state)
{
    const struct crazypod_customize_activation_host actions = {
        .render = transaction_render,
        .operation_failed = transaction_failed,
        .push = crazypod_route_actions_push,
        .push_selected =
            crazypod_route_actions_push_selected,
        .pop = action_pop,
        .appearance_changed = appearance_changed,
        .show_icon_choices = show_icon_choices,
        .show_appearance_choices = show_appearance_choices,
        .show_background_choices = show_background_choices,
        .preset_deleted = preset_deleted,
    };

    return crazypod_customize_feature_activate(
        state, &actions);
}

void crazypod_route_actions_activate(long now)
{
    struct route_state *state = current_route();
    const struct crazypod_feature *feature;

    if(state == NULL)
        return;
    if(state->route == SETTINGS_ROUTE_MAIN_MENU &&
       crazypod_settings_feature_main_menu_reordering()) {
        enum crazypod_app_id id =
            crazypod_settings_feature_finish_main_menu_reorder();

        if(crazypod_apps_is_known(id)) {
            state->selected = crazypod_apps_order_index(id);
            persist_main_menu(reorder_preferred);
            reorder_preferred = CRAZYPOD_APP_INVALID;
            host.render(false);
            crazypod_choice_coordinator_show_receipt(
                CP_TR("Saved"), true, now, false);
        }
        return;
    }
    if(state->route == MUSIC_ROUTE_NOW_PLAYING &&
       crazypod_now_playing_overlay_visible()) {
        crazypod_now_playing_overlay_activate();
        return;
    }
    if(crazypod_choice_coordinator_visible()) {
        crazypod_choice_coordinator_activate(now);
        return;
    }
    feature = crazypod_route_registry_feature(state->route);
    if(feature != NULL &&
       feature->id == CRAZYPOD_FEATURE_CUSTOMIZE) {
        if(activate_customize(state))
            return;
    }
    else if(activate_domain(state, now))
        return;
    if(crazypod_route_registry_is_shell(state->route)) {
        const struct crazypod_app_descriptor *app =
            crazypod_app_catalog_find(
                crazypod_apps_hidden_id(state->selected));

        if(app != NULL)
            crazypod_app_launcher_open(app->id);
        return;
    }
    play_selected_track(state);
}

void crazypod_route_actions_activate_overlay_state(
    const struct route_state *source, long now)
{
    struct route_state state;
    const struct crazypod_feature *feature;

    if(source == NULL)
        return;
    state = *source;
    overlay_dispatch = true;
    overlay_changed = false;
    overlay_transition = false;
    overlay_failed = false;
    feature = crazypod_route_registry_feature(state.route);
    if(feature != NULL &&
       feature->id == CRAZYPOD_FEATURE_CUSTOMIZE)
        (void)activate_customize(&state);
    else
        (void)activate_domain(&state, now);
    overlay_dispatch = false;

    if(overlay_failed) {
        crazypod_choice_coordinator_show_receipt(
            CP_TR("Failed"), false, now, false);
    }
    else if(overlay_transition &&
            transition_needs_receipt(&state)) {
        crazypod_choice_coordinator_show_receipt(
            CP_TR("Saved"), true, now, true);
    }
    else if(overlay_transition) {
        crazypod_choice_coordinator_dismiss(false);
        host.render(true);
    }
    else if(overlay_changed) {
        if(state.route == NOTES_ROUTE_DELETED_ACTIONS) {
            struct route_state *parent = current_route();
            int count = parent != NULL
                ? host.item_count(parent) : 0;

            if(parent != NULL &&
               parent->route == NOTES_ROUTE_DELETED &&
               parent->selected >= count)
                parent->selected = count > 0 ? count - 1 : 0;
        }
        crazypod_choice_coordinator_show_receipt(
            CP_TR("Saved"), true, now, true);
    }
}

void crazypod_route_actions_move(int direction, long now)
{
    struct route_state *state = current_route();
    int count;
    int next;

    if(state == NULL)
        return;
    if(state->route == SETTINGS_ROUTE_MAIN_MENU &&
       crazypod_settings_feature_main_menu_reordering()) {
        int step = direction < 0 ? -1 : 1;
        int remaining = direction < 0 ? -direction : direction;
        enum crazypod_app_id id =
            crazypod_settings_feature_main_menu_reorder_id();
        bool changed = false;

        while(remaining-- > 0)
            changed =
                crazypod_settings_feature_move_main_menu_item(step) ||
                changed;
        if(changed) {
            state->selected = crazypod_apps_order_index(id);
            host.render(false);
        }
        return;
    }
    count = host.item_count(state);
    if(crazypod_choice_coordinator_visible()) {
        crazypod_choice_coordinator_move(direction);
        return;
    }
    if(state->route == MUSIC_ROUTE_NOW_PLAYING) {
        if(crazypod_now_playing_overlay_visible())
            crazypod_now_playing_overlay_move(direction);
        else
            crazypod_now_playing_adjust_volume(direction);
        return;
    }
    if(state->route == PHOTOS_ROUTE_DETAIL) {
        if(crazypod_ui_routes_depth() > 1) {
            struct route_state *parent =
                crazypod_ui_routes_at(
                    crazypod_ui_routes_depth() - 2);
            int parent_count = host.item_count(parent);

            if(parent_count > 0) {
                next = parent->selected + direction;
                if(next < 0)
                    next = 0;
                if(next >= parent_count)
                    next = parent_count - 1;
                if(next != parent->selected) {
                    parent->selected = next;
                    state->group =
                        crazypod_photos_feature_route_index(
                            parent, parent->selected);
                    state->selected = 0;
                    crazypod_photos_feature_open_detail(100);
                }
            }
        }
        host.render(false);
        return;
    }
    if(count <= 0)
        return;
    host.boost(HZ / 3);
    if(state->route == MUSIC_ROUTE_ALBUM_FLOW) {
        next = crazypod_coverflow_step(direction);
        if(next != state->selected)
            state->selected = next;
        return;
    }
    next = state->selected + direction;
    if(next < 0)
        next = 0;
    if(next >= count)
        next = count - 1;
    if(next == state->selected)
        return;
    state->selected = next;
    crazypod_menu_preview_prefetch(state);
    if(crazypod_menu_preview_is_skeuomorphic_route(
           state->route))
        crazypod_preview_motion_set_direction(direction);
    if(state->route == MUSIC_ROUTE_SEARCH) {
        host.refresh_menu_rows(state);
        return;
    }
    if(crazypod_menu_list_matches(state->route)) {
        host.refresh_menu_rows(state);
        crazypod_render_scheduler_schedule_preview(
            now + PREVIEW_SETTLE_TICKS);
    }
    else
        crazypod_render_scheduler_schedule_route(now);
}

bool crazypod_route_actions_alpha_jump(
    int direction, long now)
{
    struct route_state *state = current_route();
    int target;
    char key;

    if(state == NULL ||
       !crazypod_music_feature_alpha_jump_target(
           state, direction, &target, &key) ||
       target == state->selected)
        return false;
    host.boost(HZ / 3);
    state->selected = target;
    if(crazypod_menu_preview_is_music_route(state->route))
        crazypod_menu_preview_prefetch(state);
    if(crazypod_menu_preview_is_skeuomorphic_route(
           state->route))
        crazypod_preview_motion_set_direction(direction);
    if(crazypod_menu_list_matches(state->route)) {
        host.refresh_menu_rows(state);
        crazypod_render_scheduler_schedule_preview(
            now + PREVIEW_SETTLE_TICKS);
    }
    else
        crazypod_render_scheduler_schedule_route(now);
    crazypod_alpha_jump_hud_show(
        key, now, ALPHA_JUMP_HUD_TICKS);
    return true;
}

#endif
