#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "audio.h"
#include "kernel.h"
#include "playlist.h"

#include "../../crazypod_artwork.h"
#include "../../crazypod_coverflow.h"
#include "../../crazypod_music.h"
#include "../../crazypod_playlist.h"
#include "../../crazypod_state.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../navigation/crazypod_render_scheduler.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../presentation/crazypod_menu_list.h"
#include "../presentation/crazypod_preview_motion.h"
#include "../shell/crazypod_now_capsule.h"
#include "../shell/crazypod_shell.h"
#include "crazypod_menu_preview.h"
#include "crazypod_playback.h"

#define NOW_ARTWORK_CACHE_SIZE \
    CRAZYPOD_COVERFLOW_ARTWORK_SIZE
#define PREVIOUS_RESTART_THRESHOLD_MS 3000

static struct {
    struct crazypod_playback_host host;
    unsigned preview_generation;
    unsigned menu_generation;
    long last_album_warm;
} playback;

static struct route_state *current_route(void)
{
    return crazypod_ui_routes_current();
}

static const struct crazypod_track *current_track(void)
{
    const char *path =
        crazypod_queue_path(crazypod_queue_index());

    return crazypod_music_track(
        crazypod_music_find_track(path));
}

static unsigned menu_artwork_signature(void)
{
    unsigned signature = 2166136261u;
    bool flow = crazypod_shell_product_active() &&
        crazypod_ui_routes_depth() > 0 &&
        current_route()->route == MUSIC_ROUTE_MENU &&
        current_route()->selected == 1;
    int first = flow
        ? CRAZYPOD_MENU_PREVIEW_FLOW_SLOT_BASE : 0;
    int count = flow ? 3 : 7;
    int slot;

    for(slot = first; slot < first + count; ++slot) {
        signature ^=
            crazypod_artwork_slot_generation(slot);
        signature *= 16777619u;
    }
    signature ^= crazypod_artwork_slot_generation(
        CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    return signature * 16777619u;
}

void crazypod_playback_configure(
    const struct crazypod_playback_host *host)
{
    if(host != NULL)
        playback.host = *host;
}

void crazypod_playback_initialize(void)
{
    playback.preview_generation =
        crazypod_artwork_slot_generation(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    playback.menu_generation =
        menu_artwork_signature();
    playback.last_album_warm = 0;
}

int crazypod_playback_initial_album_index(void)
{
    const struct crazypod_track *track = current_track();
    int count = crazypod_music_album_count();
    int i;

    if(track == NULL || count <= 0)
        return 0;
    for(i = 0; i < count; ++i) {
        const struct crazypod_album *album =
            crazypod_music_album(i);

        if(album != NULL &&
           strcmp(album->title, track->album) == 0 &&
           strcmp(album->artist, track->album_artist) == 0)
            return i;
    }
    return 0;
}

void crazypod_playback_toggle(void)
{
    int status = audio_status();

    if(crazypod_queue_count() <= 0) {
        if(crazypod_music_track_count() > 0)
            crazypod_music_play(CRAZYPOD_SCOPE_ALL, 0, 0);
        crazypod_state_forget_resume();
        crazypod_state_mark_dirty();
        return;
    }
    if(status & AUDIO_STATUS_PAUSE)
        audio_resume();
    else if(status & AUDIO_STATUS_PLAY)
        audio_pause();
    else
        playlist_start(
            crazypod_queue_index(),
            crazypod_state_take_resume_elapsed(), 0);
    crazypod_now_playing_overlay_refresh_after_playback();
}

void crazypod_playback_previous_or_restart(void)
{
    const struct mp3entry *id3;

    if(crazypod_queue_count() <= 0)
        return;
    id3 = audio_current_track();
    if(id3 != NULL &&
       id3->elapsed >= PREVIOUS_RESTART_THRESHOLD_MS) {
        audio_pre_ff_rewind();
        audio_ff_rewind(0);
    }
    else
        audio_prev();
}

void crazypod_playback_update_timer(lv_timer_t *timer)
{
    const struct crazypod_track *track;
    struct mp3entry *id3;

    (void)timer;
    if(crazypod_music_library_update())
        return;
    track = current_track();
    id3 = audio_current_track();
    crazypod_now_capsule_update(
        track,
        id3 != NULL ? (uint32_t)id3->elapsed : 0,
        id3 != NULL ? (uint32_t)id3->length : 0);
    if(crazypod_ui_routes_depth() <= 0 ||
       current_route()->route != MUSIC_ROUTE_NOW_PLAYING)
        return;
    if(track != NULL &&
       strcmp(
           crazypod_now_playing_feature_rendered_track_path(),
           track->path) != 0) {
        int slot =
            crazypod_now_playing_artwork_slot(track);
        enum crazypod_now_playing_overlay overlay =
            crazypod_now_playing_overlay_kind();

        (void)crazypod_artwork_load_priority(
            slot, track, NOW_ARTWORK_CACHE_SIZE, 0);
        if(crazypod_artwork_state(
               slot, track, NOW_ARTWORK_CACHE_SIZE) ==
           CRAZYPOD_ARTWORK_PENDING)
            return;
        playback.host.render(false);
        crazypod_now_playing_overlay_restore(overlay);
        return;
    }
    if(track == NULL &&
       crazypod_now_playing_feature_rendered_track_path()[0] != '\0') {
        enum crazypod_now_playing_overlay overlay =
            crazypod_now_playing_overlay_kind();

        playback.host.render(false);
        crazypod_now_playing_overlay_restore(overlay);
        return;
    }
    crazypod_now_playing_overlay_refresh_if_queue_changed();
    if(id3 != NULL)
        crazypod_now_playing_feature_update_playback(
            (uint32_t)id3->elapsed,
            (uint32_t)id3->length);
}

void crazypod_playback_process_artwork(void)
{
    unsigned generation;

    crazypod_now_capsule_poll_artwork(current_track());
    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0 ||
       crazypod_music_library_loading() ||
       crazypod_render_scheduler_blocked() ||
       crazypod_coverflow_active() ||
       current_route()->route >= DIY_ROUTE_MENU)
        return;
    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING &&
       crazypod_now_playing_overlay_visible())
        return;
    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING) {
        if(!crazypod_now_playing_artwork_changed())
            return;
    }
    else if(current_route()->route == MUSIC_ROUTE_MENU) {
        generation = menu_artwork_signature();
        if(generation == playback.menu_generation ||
           crazypod_preview_motion_active())
            return;
        playback.menu_generation = generation;
        crazypod_menu_preview_render(current_route(), false);
        return;
    }
    else {
        generation = crazypod_artwork_slot_generation(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        if(generation == playback.preview_generation)
            return;
        if(crazypod_menu_list_matches(
               current_route()->route) &&
           crazypod_menu_preview_is_music_route(
               current_route()->route) &&
           crazypod_preview_motion_active())
            return;
        playback.preview_generation = generation;
    }
    if(crazypod_menu_list_matches(current_route()->route) &&
       crazypod_menu_preview_is_music_route(
           current_route()->route))
        crazypod_menu_preview_render(current_route(), false);
    else
        playback.host.render(false);
}

void crazypod_playback_process_media(void)
{
    enum crazypod_feature_media_update update =
        CRAZYPOD_FEATURE_MEDIA_NONE;
    enum crazypod_route route;

    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0)
        return;
    route = current_route()->route;
    if(route >= PHOTOS_ROUTE_MENU &&
       route <= PHOTOS_ROUTE_DETAIL)
        update = crazypod_photos_feature_poll_media(
            route, crazypod_render_scheduler_blocked(),
            crazypod_preview_motion_active());
    else if(route == DIY_ROUTE_WALLPAPER_FILES ||
            route == DIY_ROUTE_WALLPAPER_CROP)
        update = crazypod_customize_feature_poll_media(
            route, crazypod_render_scheduler_blocked());
    if(update == CRAZYPOD_FEATURE_MEDIA_PREVIEW)
        crazypod_menu_preview_render(current_route(), false);
    else if(update == CRAZYPOD_FEATURE_MEDIA_ROUTE)
        playback.host.render(false);
}

void crazypod_playback_tick_wave(long now)
{
    bool active = crazypod_shell_product_active() &&
        crazypod_ui_routes_depth() > 0 &&
        current_route()->route == MUSIC_ROUTE_NOW_PLAYING;

    crazypod_now_playing_feature_tick_wave(now, active);
}

void crazypod_playback_sync_album_flow(void)
{
    int index;

    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0 ||
       !crazypod_coverflow_active() ||
       current_route()->route != MUSIC_ROUTE_ALBUM_FLOW)
        return;
    index = crazypod_music_feature_sync_album_flow();
    if(index >= 0)
        current_route()->selected = index;
}

void crazypod_playback_warm_album_flow(
    long now, bool locked)
{
    if(!crazypod_shell_product_active() || locked ||
       crazypod_ui_routes_depth() <= 0 ||
       crazypod_coverflow_active() ||
       crazypod_render_scheduler_preview_pending() ||
       crazypod_preview_motion_active() ||
       current_route()->route != MUSIC_ROUTE_MENU ||
       current_route()->selected != 1)
        return;
    if(playback.last_album_warm != 0 &&
       TIME_BEFORE(
           now, playback.last_album_warm + HZ / 10))
        return;
    playback.last_album_warm = now;
    (void)crazypod_coverflow_warm(
        crazypod_playback_initial_album_index());
}

#endif
