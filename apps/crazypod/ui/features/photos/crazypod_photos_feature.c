#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "kernel.h"

#include "../../../crazypod_photos.h"
#include "../../../crazypod_videos.h"
#include "../../presentation/crazypod_glass_slots.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_media_activation.h"
#include "crazypod_photo_controller.h"
#include "crazypod_photo_screen.h"
#include "crazypod_photos_feature.h"
#include "crazypod_photos_preview.h"

static unsigned photo_generation_seen;
static unsigned photo_view_generation_seen;
static unsigned video_generation_seen;
static long delete_feedback_until;
static enum crazypod_route delete_feedback_route;
static bool delete_feedback_success;

int crazypod_photos_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case PHOTOS_ROUTE_MENU:
        return 4;
    case PHOTOS_ROUTE_LIBRARY:
    case PHOTOS_ROUTE_DELETE_PHOTOS:
        return crazypod_photo_count();
    case PHOTOS_ROUTE_VIDEOS:
    case PHOTOS_ROUTE_DELETE_VIDEOS:
        return crazypod_video_count();
    case PHOTOS_ROUTE_FAVORITES:
        return crazypod_photo_favorite_count();
    case PHOTOS_ROUTE_DELETE_MENU:
        return 2;
    case PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM:
    case PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM:
        return 1;
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
        return CP_TR("MEDIA");
    case PHOTOS_ROUTE_LIBRARY:
        return CP_TR("PHOTOS");
    case PHOTOS_ROUTE_VIDEOS:
        return CP_TR("VIDEOS");
    case PHOTOS_ROUTE_FAVORITES:
        return CP_TR("FAVORITES");
    case PHOTOS_ROUTE_DELETE_MENU:
    case PHOTOS_ROUTE_DELETE_PHOTOS:
    case PHOTOS_ROUTE_DELETE_VIDEOS:
    case PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM:
    case PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM:
        return CP_TR("Delete");
    case PHOTOS_ROUTE_DETAIL:
        return CP_TR("PHOTO");
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
            CP_TR("Photos"), CP_TR("Videos"), CP_TR("Favorites"),
            CP_TR("Delete")
        };

        *title = index >= 0 && index < 4 ? titles[index] : "";
        return true;
    }
    case PHOTOS_ROUTE_LIBRARY:
    case PHOTOS_ROUTE_DELETE_PHOTOS:
        *title = crazypod_photo_name(index);
        return true;
    case PHOTOS_ROUTE_VIDEOS:
    case PHOTOS_ROUTE_DELETE_VIDEOS:
        *title = crazypod_video_name(index);
        return true;
    case PHOTOS_ROUTE_FAVORITES:
        *title = crazypod_photo_name(
            crazypod_photo_favorite_index(index));
        return true;
    case PHOTOS_ROUTE_DETAIL:
        *title = index == 0 ? CP_TR("Fit") : "2x";
        return true;
    case PHOTOS_ROUTE_DELETE_MENU:
        *title = index == 0 ? CP_TR("Photos") : CP_TR("Videos");
        return true;
    case PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM:
    case PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM:
        *title = CP_TR("Hold Center to Delete");
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
    if(state->route == PHOTOS_ROUTE_LIBRARY ||
       state->route == PHOTOS_ROUTE_DELETE_PHOTOS)
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
                    route == PHOTOS_ROUTE_FAVORITES ||
                    route == PHOTOS_ROUTE_DELETE_PHOTOS ||
                    route == PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM)
                update = CRAZYPOD_FEATURE_MEDIA_ROUTE;
        }
    }

    generation = crazypod_video_generation();
    if(generation != video_generation_seen) {
        if((route == PHOTOS_ROUTE_MENU ||
            route == PHOTOS_ROUTE_VIDEOS ||
            route == PHOTOS_ROUTE_DELETE_MENU ||
            route == PHOTOS_ROUTE_DELETE_VIDEOS) &&
           preview_motion_active)
            return update;
        video_generation_seen = generation;
        if(route == PHOTOS_ROUTE_MENU ||
           route == PHOTOS_ROUTE_VIDEOS ||
           route == PHOTOS_ROUTE_DELETE_MENU ||
           route == PHOTOS_ROUTE_DELETE_VIDEOS)
            update = CRAZYPOD_FEATURE_MEDIA_PREVIEW;
        else if(route == PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM)
            update = CRAZYPOD_FEATURE_MEDIA_ROUTE;
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
       state->route == PHOTOS_ROUTE_FAVORITES ||
       state->route == PHOTOS_ROUTE_DELETE_PHOTOS) {
        mode = state->route == PHOTOS_ROUTE_FAVORITES
            ? CRAZYPOD_PHOTO_GRID_FAVORITES
            : state->route == PHOTOS_ROUTE_DELETE_PHOTOS
                ? CRAZYPOD_PHOTO_GRID_DELETE
            : CRAZYPOD_PHOTO_GRID_LIBRARY;
        crazypod_photo_screen_render_grid(
            context->parent, mode, state->selected,
            crazypod_photos_feature_title(state),
            context->metadata_font, context->primary_color,
            context->panel_color);
        photo_index = crazypod_photo_screen_grid_index(
            mode, state->selected);
        if(state->route != PHOTOS_ROUTE_DELETE_PHOTOS)
            crazypod_photo_screen_render_favorite_status(
                context->parent, photo_index, context->now,
                context->foreground_color, context->muted_color);
        return true;
    }
    if(state->route == PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM ||
       state->route == PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM) {
        bool video = state->route ==
            PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM;
        const lv_image_dsc_t *descriptor = video
            ? crazypod_video_poster(state->group)
            : crazypod_photo_thumbnail(0, state->group);

        crazypod_photo_screen_render_delete_confirmation(
            context->parent,
            video ? crazypod_video_name(state->group)
                  : crazypod_photo_name(state->group),
            descriptor, video,
            context->foreground_color,
            context->muted_color);
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

struct crazypod_photos_confirmation_result
crazypod_photos_feature_confirm(
    const struct route_state *state, long now,
    long feedback_ticks)
{
    struct crazypod_photos_confirmation_result result = { 0 };
    int count;

    if(state->route != PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM &&
       state->route != PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM)
        return result;
    result.handled = true;
    result.return_route = state->route ==
        PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM
            ? PHOTOS_ROUTE_DELETE_PHOTOS
            : PHOTOS_ROUTE_DELETE_VIDEOS;
    result.deleted = state->route ==
        PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM
            ? crazypod_photo_delete(state->group)
            : crazypod_video_delete(state->group);
    count = result.return_route == PHOTOS_ROUTE_DELETE_PHOTOS
        ? crazypod_photo_count() : crazypod_video_count();
    result.selected = state->group;
    if(result.selected >= count)
        result.selected = count > 0 ? count - 1 : 0;
    delete_feedback_success = result.deleted;
    delete_feedback_route = result.deleted
        ? result.return_route : state->route;
    delete_feedback_until = now +
        (feedback_ticks > 0 ? feedback_ticks : 1);
    return result;
}

bool crazypod_photos_feature_service_feedback(long now)
{
    if(delete_feedback_until == 0 ||
       TIME_BEFORE(now, delete_feedback_until))
        return false;
    delete_feedback_until = 0;
    return true;
}

void crazypod_photos_feature_render_feedback(
    const struct route_state *state, lv_obj_t *parent,
    uint32_t foreground_color, uint32_t muted_color,
    long now)
{
    lv_obj_t *panel;
    lv_obj_t *label;

    if(delete_feedback_until == 0 ||
       state->route != delete_feedback_route ||
       !TIME_BEFORE(now, delete_feedback_until))
        return;
    panel = crazypod_glass_slot_panel(
        CRAZYPOD_GLASS_SLOT_INFO_TOAST,
        crazypod_glass_slot_prepare_frame(
            CRAZYPOD_GLASS_SLOT_INFO_TOAST,
            64, 172, 192, 34,
            CRAZYPOD_GLASS_INFO_TOAST),
        parent, 64, 172, 192, 34, 12,
        CRAZYPOD_GLASS_INFO_TOAST);
    label = crazypod_ui_widget_label(
        panel, LV_SYMBOL_TRASH, &lv_font_montserrat_12,
        delete_feedback_success ? 0xFF453A : muted_color,
        LV_OPA_COVER);
    lv_obj_set_pos(label, 12, 9);
    label = crazypod_ui_widget_label(
        panel,
        delete_feedback_success
            ? CP_TR("Deleted") : CP_TR("Delete Failed"),
        &lv_font_montserrat_10,
        foreground_color, LV_OPA_COVER);
    lv_obj_set_pos(label, 39, 10);
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

void crazypod_photos_feature_render_preview(
    const struct route_state *state, lv_obj_t *parent,
    bool videos, bool defer_media,
    bool *media_deferred)
{
    const struct crazypod_photos_preview_context context = {
        .parent = parent,
        .defer_media = defer_media,
        .media_deferred = media_deferred,
    };

    if(videos)
        crazypod_videos_preview_render(state, &context);
    else
        crazypod_photos_preview_render(state, &context);
}

void crazypod_photos_feature_prefetch_preview(
    const struct route_state *state)
{
    int count;
    int index;

    if(state == NULL)
        return;
    if(state->route == PHOTOS_ROUTE_MENU) {
        if(state->selected == 0) {
            count = crazypod_photo_count();
            for(index = 0; index < count && index < 3; ++index)
                (void)crazypod_photo_thumbnail(index, index);
        }
        else if(state->selected == 1 && crazypod_video_count() > 0)
            (void)crazypod_video_poster(0);
        else if(state->selected == 2) {
            index = crazypod_photo_favorite_index(0);
            if(index >= 0)
                (void)crazypod_photo_thumbnail(0, index);
        }
        return;
    }
    if(state->route == PHOTOS_ROUTE_DELETE_MENU) {
        if(state->selected == 0 && crazypod_photo_count() > 0)
            (void)crazypod_photo_thumbnail(0, 0);
        else if(state->selected == 1 && crazypod_video_count() > 0)
            (void)crazypod_video_poster(0);
        return;
    }
    if((state->route == PHOTOS_ROUTE_VIDEOS ||
        state->route == PHOTOS_ROUTE_DELETE_VIDEOS) &&
       state->selected >= 0 &&
       state->selected < crazypod_video_count())
        (void)crazypod_video_poster(state->selected);
}

void crazypod_photos_feature_reset_controller(void)
{
    crazypod_photo_controller_reset();
}

void crazypod_photos_feature_open_detail(int zoom_percent)
{
    crazypod_photo_controller_open_detail(zoom_percent);
}

void crazypod_photos_feature_reset_view(void)
{
    crazypod_photo_screen_reset_transient();
}

#endif
