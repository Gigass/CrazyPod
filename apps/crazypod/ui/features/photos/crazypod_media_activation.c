#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_photos.h"
#include "../../../crazypod_videos.h"
#include "crazypod_media_activation.h"
#include "crazypod_photo_controller.h"
#include "crazypod_photo_screen.h"

static struct crazypod_media_activation_result result(
    enum crazypod_media_activation_kind kind,
    enum crazypod_route route, int group, int selected)
{
    const struct crazypod_media_activation_result value = {
        .kind = kind,
        .route = route,
        .group = group,
        .selected = selected,
    };

    return value;
}

struct crazypod_media_activation_result
crazypod_media_activation_execute(struct route_state *state)
{
    if(state->route == PHOTOS_ROUTE_MENU) {
        return result(
            CRAZYPOD_MEDIA_ACTIVATION_PUSH,
            state->selected == 0 ? PHOTOS_ROUTE_LIBRARY :
            state->selected == 1 ? PHOTOS_ROUTE_VIDEOS :
            state->selected == 2 ? PHOTOS_ROUTE_FAVORITES :
                                   PHOTOS_ROUTE_DELETE_MENU,
            -1, 0);
    }
    if(state->route == PHOTOS_ROUTE_DELETE_MENU) {
        return result(
            CRAZYPOD_MEDIA_ACTIVATION_PUSH,
            state->selected == 0
                ? PHOTOS_ROUTE_DELETE_PHOTOS
                : PHOTOS_ROUTE_DELETE_VIDEOS,
            -1, 0);
    }
    if(state->route == PHOTOS_ROUTE_DELETE_PHOTOS) {
        if(state->selected < 0 ||
           state->selected >= crazypod_photo_count())
            return result(
                CRAZYPOD_MEDIA_ACTIVATION_NONE,
                state->route, 0, 0);
        return result(
            CRAZYPOD_MEDIA_ACTIVATION_PUSH,
            PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM,
            state->selected, 0);
    }
    if(state->route == PHOTOS_ROUTE_DELETE_VIDEOS) {
        if(state->selected < 0 ||
           state->selected >= crazypod_video_count())
            return result(
                CRAZYPOD_MEDIA_ACTIVATION_NONE,
                state->route, 0, 0);
        return result(
            CRAZYPOD_MEDIA_ACTIVATION_PUSH,
            PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM,
            state->selected, 0);
    }
    if(state->route == PHOTOS_ROUTE_DELETE_PHOTO_CONFIRM ||
       state->route == PHOTOS_ROUTE_DELETE_VIDEO_CONFIRM)
        return result(
            CRAZYPOD_MEDIA_ACTIVATION_NONE,
            state->route, 0, 0);
    if(state->route == PHOTOS_ROUTE_VIDEOS) {
        struct crazypod_media_activation_result value =
            result(CRAZYPOD_MEDIA_ACTIVATION_NONE, state->route, 0, 0);

        if(state->selected >= 0 &&
           state->selected < crazypod_video_count()) {
            value.video_started =
                crazypod_video_play(state->selected);
            value.video_generation =
                crazypod_video_generation();
            value.kind = CRAZYPOD_MEDIA_ACTIVATION_RENDER;
        }
        return value;
    }
    if(state->route == PHOTOS_ROUTE_LIBRARY ||
       state->route == PHOTOS_ROUTE_FAVORITES) {
        enum crazypod_photo_grid_mode mode =
            state->route == PHOTOS_ROUTE_FAVORITES
                ? CRAZYPOD_PHOTO_GRID_FAVORITES
                : CRAZYPOD_PHOTO_GRID_LIBRARY;
        int photo_index = crazypod_photo_screen_grid_index(
            mode, state->selected);

        if(photo_index < 0)
            return result(
                CRAZYPOD_MEDIA_ACTIVATION_NONE, state->route, 0, 0);
        crazypod_photo_controller_open_detail(100);
        return result(
            CRAZYPOD_MEDIA_ACTIVATION_PUSH_SELECTED,
            PHOTOS_ROUTE_DETAIL, photo_index, 0);
    }
    if(state->route == PHOTOS_ROUTE_DETAIL) {
        state->selected = state->selected == 0 ? 1 : 0;
        crazypod_photo_controller_open_detail(
            state->selected > 0 ? 220 : 100);
        return result(
            CRAZYPOD_MEDIA_ACTIVATION_RENDER, state->route, 0, 0);
    }
    return result(
        CRAZYPOD_MEDIA_ACTIVATION_UNHANDLED, state->route, 0, 0);
}

#endif
