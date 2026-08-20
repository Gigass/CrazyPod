#ifndef CRAZYPOD_MUSIC_FEATURE_H
#define CRAZYPOD_MUSIC_FEATURE_H

#include "lvgl.h"

#include "../../../crazypod_music.h"
#include "../../crazypod_menu_icon.h"
#include "../../navigation/crazypod_ui_routes.h"
#include "../crazypod_feature.h"
#include "../../presentation/crazypod_glass_slots.h"

struct crazypod_music_library_host {
    lv_obj_t *parent;
    void (*prepare_loading_surface)(void);
    void (*render_route)(bool transition);
    bool (*route_visible)(void);
};

struct crazypod_music_activation_host {
    void (*render)(bool transition);
    void (*push)(enum crazypod_route route, int group);
    void (*push_selected)(
        enum crazypod_route route, int group, int selected);
    int (*initial_album_index)(void);
    void (*request_now_playing)(void);
    void (*show_now_actions)(void);
};

typedef lv_obj_t *(*crazypod_music_panel_factory)(
    lv_obj_t *parent, enum crazypod_glass_slot slot,
    int x, int y, int width, int height, int radius);

int crazypod_music_feature_item_count(
    const struct route_state *state, const char *search_query);
const char *crazypod_music_feature_title(
    const struct route_state *state);
bool crazypod_music_feature_item_title(
    const struct route_state *state, int index,
    const char *search_query, const char **title);
enum crazypod_menu_icon crazypod_music_feature_item_icon(
    const struct route_state *state, int index);
bool crazypod_music_feature_alpha_jump_available(
    const struct route_state *state);
bool crazypod_music_feature_alpha_jump_target(
    const struct route_state *state, int direction,
    int *target, char *key);
void crazypod_music_library_configure(
    const struct crazypod_music_library_host *host);
void crazypod_music_library_initialize(long now);
void crazypod_music_library_begin(long now);
void crazypod_music_library_leave(long now);
void crazypod_music_library_service(long now, bool storage_active);
bool crazypod_music_library_update(void);
void crazypod_music_library_schedule_rescan(long not_before);
bool crazypod_music_library_loaded(void);
bool crazypod_music_library_loading(void);
bool crazypod_music_library_preparing_artwork(void);
bool crazypod_music_feature_activate(
    const struct route_state *state,
    const struct crazypod_music_activation_host *host);
const struct crazypod_track *crazypod_music_feature_route_track(
    const struct route_state *state, int index);
void crazypod_music_feature_render_album_flow(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font);
void crazypod_music_feature_reset_view(void);
int crazypod_music_feature_sync_album_flow(void);
bool crazypod_music_feature_render_special(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font, int item_count,
    const char *(*item_title)(
        const struct route_state *state, int index),
    uint32_t primary_color, uint32_t secondary_color,
    uint32_t panel_color, bool gradient_highlight,
    crazypod_music_panel_factory make_panel);
const char *crazypod_music_search_query(void);
void crazypod_music_search_backspace(void);
int crazypod_music_podcast_track_index(int position);
bool crazypod_music_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context);
void crazypod_music_feature_render_root_preview(
    lv_obj_t *parent, int selected, bool defer_media);
void crazypod_music_feature_render_item_preview(
    lv_obj_t *parent, const struct route_state *state,
    const struct crazypod_track *track,
    const lv_font_t *metadata_font);

#endif
