#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_photos.h"
#include "../../../crazypod_videos.h"
#include "../../presentation/crazypod_glass_slots.h"
#include "crazypod_media_activation.h"
#include "crazypod_photo_controller.h"
#include "crazypod_photo_screen.h"
#include "crazypod_photos_feature.h"

static unsigned photo_generation_seen;
static unsigned photo_view_generation_seen;
static unsigned video_generation_seen;

int crazypod_photos_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case PHOTOS_ROUTE_MENU:
        return 3;
    case PHOTOS_ROUTE_LIBRARY:
        return crazypod_photo_count();
    case PHOTOS_ROUTE_VIDEOS:
        return crazypod_video_count();
    case PHOTOS_ROUTE_FAVORITES:
        return crazypod_photo_favorite_count();
    case PHOTOS_ROUTE_DETAIL:
        return 2;
    default:
        return 0;
    }
}

const char *crazypod_photos_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case PHOTOS_ROUTE_MENU:
        return "MEDIA";
    case PHOTOS_ROUTE_LIBRARY:
        return "PHOTOS";
    case PHOTOS_ROUTE_VIDEOS:
        return "VIDEOS";
    case PHOTOS_ROUTE_FAVORITES:
        return "FAVORITES";
    case PHOTOS_ROUTE_DETAIL:
        return "PHOTO";
    default:
        return "";
    }
}

bool crazypod_photos_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    switch(state->route) {
    case PHOTOS_ROUTE_MENU: {
        static const char *const titles[] = {
            "Photos", "Videos", "Favorites"
        };

        *title = index >= 0 && index < 3 ? titles[index] : "";
        return true;
    }
    case PHOTOS_ROUTE_LIBRARY:
        *title = crazypod_photo_name(index);
        return true;
    case PHOTOS_ROUTE_VIDEOS:
        *title = crazypod_video_name(index);
        return true;
    case PHOTOS_ROUTE_FAVORITES:
        *title = crazypod_photo_name(
            crazypod_photo_favorite_index(index));
        return true;
    case PHOTOS_ROUTE_DETAIL:
        *title = index == 0 ? "Fit" : "2x";
        return true;
    default:
        return false;
    }
}

int crazypod_photos_feature_route_index(
    const struct route_state *state, int position)
{
    if(state->route == PHOTOS_ROUTE_FAVORITES)
        return crazypod_photo_favorite_index(position);
    if(state->route == PHOTOS_ROUTE_LIBRARY)
        return position;
    return -1;
}

void crazypod_photos_feature_initialize_media(void)
{
    photo_generation_seen = crazypod_photo_generation();
    photo_view_generation_seen =
        crazypod_photo_view_generation();
    video_generation_seen = crazypod_video_generation();
}

void crazypod_photos_feature_note_video_generation(
    unsigned generation)
{
    video_generation_seen = generation;
}

enum crazypod_feature_media_update
crazypod_photos_feature_poll_media(
    enum crazypod_route route, bool blocked,
    bool preview_motion_active)
{
    enum crazypod_feature_media_update update =
        CRAZYPOD_FEATURE_MEDIA_NONE;
    unsigned generation;

    if(blocked)
        return update;
    if(route == PHOTOS_ROUTE_DETAIL) {
        generation = crazypod_photo_view_generation();
        if(generation != photo_view_generation_seen) {
            photo_view_generation_seen = generation;
            update = CRAZYPOD_FEATURE_MEDIA_ROUTE;
        }
    }
    else {
        generation = crazypod_photo_generation();
        if(generation != photo_generation_seen) {
            if(route == PHOTOS_ROUTE_MENU &&
               preview_motion_active)
                return update;
            photo_generation_seen = generation;
            if(route == PHOTOS_ROUTE_MENU)
                update = CRAZYPOD_FEATURE_MEDIA_PREVIEW;
            else if(route == PHOTOS_ROUTE_LIBRARY ||
                    route == PHOTOS_ROUTE_FAVORITES)
                update = CRAZYPOD_FEATURE_MEDIA_ROUTE;
        }
    }

    generation = crazypod_video_generation();
    if(generation != video_generation_seen) {
        if((route == PHOTOS_ROUTE_MENU ||
            route == PHOTOS_ROUTE_VIDEOS) &&
           preview_motion_active)
            return update;
        video_generation_seen = generation;
        if(route == PHOTOS_ROUTE_MENU ||
           route == PHOTOS_ROUTE_VIDEOS)
            update = CRAZYPOD_FEATURE_MEDIA_PREVIEW;
    }
    return update;
}

bool crazypod_photos_feature_activate(
    struct route_state *state,
    const struct crazypod_photos_activation_host *host)
{
    const struct crazypod_media_activation_result action =
        crazypod_media_activation_execute(state);

    if(action.kind == CRAZYPOD_MEDIA_ACTIVATION_UNHANDLED)
        return false;
    if(action.kind == CRAZYPOD_MEDIA_ACTIVATION_PUSH)
        host->push(action.route, action.group);
    else if(action.kind ==
            CRAZYPOD_MEDIA_ACTIVATION_PUSH_SELECTED)
        host->push_selected(
            action.route, action.group, action.selected);
    else if(action.kind == CRAZYPOD_MEDIA_ACTIVATION_RENDER) {
        if(action.video_generation != 0)
            crazypod_photos_feature_note_video_generation(
                action.video_generation);
        host->render(false);
    }
    return true;
}

static lv_obj_t *create_detail_info_panel(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, void *context)
{
    (void)context;
    return crazypod_glass_slot_panel(
        CRAZYPOD_GLASS_SLOT_INFO_BAR,
        crazypod_glass_slot_prepare_frame(
            CRAZYPOD_GLASS_SLOT_INFO_BAR,
            x, y, width, height,
            CRAZYPOD_GLASS_TEXT_PANEL),
        parent, x, y, width, height, radius,
        CRAZYPOD_GLASS_TEXT_PANEL);
}

bool crazypod_photos_feature_render(
    const struct route_state *state,
    const struct crazypod_photos_render_context *context)
{
    enum crazypod_photo_grid_mode mode;
    int photo_index;

    if(state->route == PHOTOS_ROUTE_LIBRARY ||
       state->route == PHOTOS_ROUTE_FAVORITES) {
        mode = state->route == PHOTOS_ROUTE_FAVORITES
            ? CRAZYPOD_PHOTO_GRID_FAVORITES
            : CRAZYPOD_PHOTO_GRID_LIBRARY;
        crazypod_photo_screen_render_grid(
            context->parent, mode, state->selected,
            crazypod_photos_feature_title(state),
            context->metadata_font, context->primary_color,
            context->panel_color);
        photo_index = crazypod_photo_screen_grid_index(
            mode, state->selected);
        crazypod_photo_screen_render_favorite_status(
            context->parent, photo_index, context->now,
            context->foreground_color, context->muted_color);
        return true;
    }
    if(state->route != PHOTOS_ROUTE_DETAIL)
        return false;
    crazypod_photo_screen_render_detail(
        context->parent, state->group,
        context->foreground_color,
        create_detail_info_panel, NULL);
    crazypod_photo_screen_render_favorite_status(
        context->parent, state->group, context->now,
        context->foreground_color, context->muted_color);
    return true;
}

lv_obj_t *crazypod_photos_feature_render_image(
    lv_obj_t *parent, const lv_image_dsc_t *descriptor,
    int x, int y, int width, int height)
{
    return crazypod_photo_screen_render_image(
        parent, descriptor, x, y, width, height);
}

void crazypod_photos_feature_render_wallpaper_grid(
    lv_obj_t *parent, int selected, const char *title,
    const lv_font_t *title_font,
    uint32_t primary_color, uint32_t panel_color)
{
    crazypod_photo_screen_render_grid(
        parent, CRAZYPOD_PHOTO_GRID_WALLPAPER,
        selected, title, title_font,
        primary_color, panel_color);
}

void crazypod_photos_feature_note_direction(long now)
{
    crazypod_photo_controller_note_direction(now);
}

void crazypod_photos_feature_reset_view(void)
{
    crazypod_photo_screen_reset_transient();
}

#endif
