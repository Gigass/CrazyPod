#ifndef CRAZYPOD_NOW_PLAYING_FEATURE_H
#define CRAZYPOD_NOW_PLAYING_FEATURE_H

#include "lvgl.h"

#include "../../../crazypod_music.h"
#include "../../crazypod_menu_icon.h"
#include "../../navigation/crazypod_input_event.h"
#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_now_playing_overlay {
    CRAZYPOD_NOW_OVERLAY_NONE = 0,
    CRAZYPOD_NOW_OVERLAY_ACTIONS,
    CRAZYPOD_NOW_OVERLAY_QUEUE,
    CRAZYPOD_NOW_OVERLAY_PROGRESS,
};

struct crazypod_now_playing_overlay_host {
    lv_obj_t *parent;
    void (*prepare_glass)(bool refresh, void *context);
    void (*preserve_modal_underlay)(void *context);
    lv_obj_t *(*create_modal_underlay)(
        lv_obj_t *parent, void *context);
    lv_obj_t *(*create_panel)(
        lv_obj_t *parent, int x, int y,
        int width, int height, void *context);
    void (*render)(void *context);
    void *context;
};

struct crazypod_now_playing_navigation_host {
    void (*push_now_playing)(void);
    void (*boost)(int ticks);
};

typedef lv_obj_t *(*crazypod_now_playing_artwork_renderer)(
    lv_obj_t *parent, const struct crazypod_track *track,
    int x, int y, int display_size,
    const lv_image_dsc_t *descriptor, bool scale_descriptor);

struct crazypod_now_playing_render_context {
    lv_obj_t *parent;
    const lv_font_t *metadata_font;
    crazypod_now_playing_artwork_renderer render_artwork;
    void (*boost)(int ticks);
};

int crazypod_now_playing_feature_item_count(
    const struct route_state *state);
const char *crazypod_now_playing_feature_title(
    const struct route_state *state);
bool crazypod_now_playing_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
enum crazypod_menu_icon crazypod_now_playing_feature_item_icon(
    const struct route_state *state, int index);

void crazypod_now_playing_overlay_configure(
    const struct crazypod_now_playing_overlay_host *host);
void crazypod_now_playing_overlay_reset(void);
bool crazypod_now_playing_overlay_visible(void);
enum crazypod_now_playing_overlay
crazypod_now_playing_overlay_kind(void);
bool crazypod_now_playing_lyrics_mode(void);
void crazypod_now_playing_overlay_show_actions(void);
void crazypod_now_playing_overlay_show_queue(void);
void crazypod_now_playing_overlay_show_progress(void);
void crazypod_now_playing_overlay_restore(
    enum crazypod_now_playing_overlay overlay);
void crazypod_now_playing_overlay_dismiss(bool refresh);
void crazypod_now_playing_overlay_activate(void);
void crazypod_now_playing_overlay_move(int direction);
void crazypod_now_playing_adjust_volume(int direction);
void crazypod_now_playing_overlay_cycle_playback_mode(void);
void crazypod_now_playing_overlay_refresh_queue(void);
void crazypod_now_playing_overlay_refresh_after_playback(void);
void crazypod_now_playing_overlay_refresh_tick(void);
void crazypod_now_playing_navigation_configure(
    const struct crazypod_now_playing_navigation_host *host);
void crazypod_now_playing_navigation_initialize(void);
void crazypod_now_playing_request_open(void);
int crazypod_now_playing_artwork_slot(
    const struct crazypod_track *track);
void crazypod_now_playing_artwork_sync(void);
void crazypod_now_playing_artwork_set_source_size(int source_size);
const lv_image_dsc_t *crazypod_now_playing_artwork_committed(
    const char **track_path, unsigned *generation);
unsigned crazypod_now_playing_artwork_committed_generation(void);
bool crazypod_now_playing_artwork_changed(void);
void crazypod_now_playing_feature_render(
    const struct crazypod_now_playing_render_context *context);
void crazypod_now_playing_feature_tick_wave(
    long now, bool active);
void crazypod_now_playing_feature_reset_screen(void);
const char *crazypod_now_playing_feature_rendered_track_path(void);
void crazypod_now_playing_feature_update_playback(
    uint32_t elapsed_ms, uint32_t length_ms);

void crazypod_now_playing_theme_prepare(void);
int crazypod_now_playing_theme_choice_count(void);
const char *crazypod_now_playing_theme_choice_title(int index);
bool crazypod_now_playing_theme_choice_current(int index);
bool crazypod_now_playing_theme_select(int index);
bool crazypod_now_playing_theme_enabled(void);
bool crazypod_now_playing_theme_open(void);
bool crazypod_now_playing_theme_modal_visible(void);
bool crazypod_now_playing_theme_owns_status_bar(void);
int crazypod_now_playing_theme_last_error(void);
bool crazypod_now_playing_theme_render(
    lv_obj_t *parent, uint32_t accent);
bool crazypod_now_playing_theme_handle_input(
    const struct crazypod_input_event *event);
void crazypod_now_playing_theme_close(void);

#endif
