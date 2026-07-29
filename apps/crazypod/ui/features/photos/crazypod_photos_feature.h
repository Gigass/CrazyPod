#ifndef CRAZYPOD_PHOTOS_FEATURE_H
#define CRAZYPOD_PHOTOS_FEATURE_H

#include "lvgl.h"

#include "../../navigation/crazypod_ui_routes.h"
#include "../../navigation/crazypod_input_event.h"
#include "../crazypod_feature.h"

struct crazypod_photos_activation_host {
    void (*push)(enum crazypod_route route, int group);
    void (*push_selected)(
        enum crazypod_route route, int group, int selected);
    void (*render)(bool transition);
};

struct crazypod_photos_runtime_host {
    void (*render)(bool transition);
    void (*move_selection)(int direction);
    void (*activate)(void);
    void (*pop)(void);
    void (*appearance_changed)(void);
};

struct crazypod_photos_render_context {
    lv_obj_t *parent;
    const lv_font_t *metadata_font;
    uint32_t primary_color;
    uint32_t panel_color;
    uint32_t foreground_color;
    uint32_t muted_color;
    long now;
};

int crazypod_photos_feature_item_count(
    const struct route_state *state);
const char *crazypod_photos_feature_title(
    const struct route_state *state);
bool crazypod_photos_feature_item_title(
    const struct route_state *state, int index,
    const char **title);
int crazypod_photos_feature_route_index(
    const struct route_state *state, int position);
void crazypod_photos_feature_initialize_media(void);
void crazypod_photos_feature_note_video_generation(
    unsigned generation);
enum crazypod_feature_media_update
crazypod_photos_feature_poll_media(
    enum crazypod_route route, bool blocked,
    bool preview_motion_active);
bool crazypod_photos_feature_activate(
    struct route_state *state,
    const struct crazypod_photos_activation_host *host);
bool crazypod_photos_feature_render(
    const struct route_state *state,
    const struct crazypod_photos_render_context *context);
void crazypod_photos_feature_reset_view(void);
void crazypod_photos_runtime_configure(
    const struct crazypod_photos_runtime_host *host);
bool crazypod_photos_runtime_handle_input(
    struct route_state *state,
    const struct crazypod_input_event *event, long now);
void crazypod_photos_runtime_service(
    struct route_state *state, long now);
lv_obj_t *crazypod_photos_feature_render_image(
    lv_obj_t *parent, const lv_image_dsc_t *descriptor,
    int x, int y, int width, int height);
void crazypod_photos_feature_render_wallpaper_grid(
    lv_obj_t *parent, int selected, const char *title,
    const lv_font_t *title_font,
    uint32_t primary_color, uint32_t panel_color);
void crazypod_photos_feature_note_direction(long now);
void crazypod_photos_feature_render_preview(
    const struct route_state *state, lv_obj_t *parent,
    bool videos, bool defer_media,
    bool *media_deferred);
void crazypod_photos_feature_reset_controller(void);
void crazypod_photos_feature_open_detail(int zoom_percent);

#endif
