#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "kernel.h"
#include "lvgl.h"

#include "../../../crazypod_artwork.h"
#include "../../../crazypod_music.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_music_feature.h"

#define COLOR_DETAIL 0x08080D
#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x26CFF5

struct music_library_state {
    struct crazypod_music_library_host host;
    lv_obj_t *loading_detail;
    bool loaded;
    bool loading;
    bool scan_start_failed;
    bool artwork_cache_failed;
    bool scan_pending;
    bool artwork_preparing;
    unsigned scan_generation_seen;
    long scan_not_before;
};

static struct music_library_state library;

static void render_loading(void)
{
    lv_obj_t *symbol;
    lv_obj_t *title;
    char detail[64];

    if(library.host.parent == NULL ||
       library.host.prepare_loading_surface == NULL)
        return;
    library.host.prepare_loading_surface();
    symbol = crazypod_ui_widget_label(
        library.host.parent, LV_SYMBOL_REFRESH,
        &lv_font_montserrat_24, COLOR_CYAN, LV_OPA_COVER);
    lv_obj_set_pos(symbol, 148, 91);
    title = crazypod_ui_widget_label(
        library.host.parent,
        library.artwork_cache_failed
            ? "Artwork Cache Failed"
            : library.scan_start_failed
                ? "Library Scan Failed"
                : library.artwork_preparing
                    ? "Preparing Album Artwork"
                    : "Building Music Library",
        &lv_font_montserrat_12, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(title, 260);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 30, 132);
    if(library.artwork_preparing) {
        snprintf(detail, sizeof(detail), "%d / %d albums",
                 crazypod_artwork_library_prime_completed(),
                 crazypod_artwork_library_prime_total());
    }
    else {
        snprintf(
            detail, sizeof(detail), "%s",
            library.artwork_cache_failed
                ? "Could not write the CoverFlow cache"
                : library.scan_start_failed
                    ? "No background thread was available"
                    : "Reading local files and metadata");
    }
    library.loading_detail = crazypod_ui_widget_label(
        library.host.parent, detail, &lv_font_montserrat_8,
        COLOR_WHITE, 110);
    lv_obj_set_width(library.loading_detail, 260);
    lv_obj_set_style_text_align(
        library.loading_detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(library.loading_detail, 30, 155);
}

static void finish_loading(void)
{
    library.loading = false;
    library.loading_detail = NULL;
    if(library.host.route_visible != NULL &&
       library.host.route_visible() &&
       library.host.render_route != NULL)
        library.host.render_route(true);
}

void crazypod_music_library_configure(
    const struct crazypod_music_library_host *host)
{
    if(host != NULL)
        library.host = *host;
}

void crazypod_music_library_initialize(long now)
{
    library.loading_detail = NULL;
    library.scan_generation_seen = crazypod_music_scan_generation();
    library.loaded = false;
    library.loading = false;
    library.scan_start_failed = false;
    library.artwork_cache_failed = false;
    library.artwork_preparing = false;
    library.scan_pending = true;
    library.scan_not_before = now + HZ;
}

void crazypod_music_library_begin(long now)
{
    if(library.scan_pending ||
       crazypod_music_is_scanning() ||
       library.artwork_preparing ||
       crazypod_music_scan_generation() !=
           library.scan_generation_seen) {
        library.loaded = false;
        library.loading = true;
        library.scan_start_failed = false;
        render_loading();
        lv_refr_now(NULL);
        return;
    }
    if(crazypod_music_track_count() > 0) {
        library.loaded = false;
        library.loading = true;
        library.artwork_preparing = true;
        library.artwork_cache_failed = false;
        crazypod_artwork_prime_library();
        render_loading();
        lv_refr_now(NULL);
        return;
    }

    library.loaded = false;
    library.loading = true;
    library.scan_start_failed = false;
    library.artwork_cache_failed = false;
    library.scan_generation_seen = crazypod_music_scan_generation();
    library.scan_pending = true;
    library.scan_not_before = now;
    render_loading();
    lv_refr_now(NULL);
}

void crazypod_music_library_service(long now, bool storage_active)
{
    if(!library.scan_pending || storage_active ||
       crazypod_music_is_scanning() ||
       TIME_BEFORE(now, library.scan_not_before))
        return;

    library.scan_pending = false;
    library.scan_generation_seen = crazypod_music_scan_generation();
    library.artwork_preparing = false;
    library.artwork_cache_failed = false;
    crazypod_artwork_cancel_library_prime();
    if(!crazypod_music_scan_async()) {
        library.scan_start_failed = true;
        if(library.loading)
            render_loading();
    }
}

bool crazypod_music_library_update(void)
{
    if(!crazypod_music_is_scanning() &&
       crazypod_music_scan_generation() !=
           library.scan_generation_seen) {
        library.scan_generation_seen =
            crazypod_music_scan_generation();
        library.loaded = false;
        if(crazypod_music_track_count() > 0) {
            library.artwork_preparing = true;
            library.artwork_cache_failed = false;
            crazypod_artwork_prime_library();
            if(library.loading)
                render_loading();
        }
        else {
            library.artwork_preparing = false;
            if(library.loading)
                finish_loading();
        }
    }
    if(library.artwork_preparing) {
        if(library.loading_detail != NULL) {
            char progress[48];

            snprintf(progress, sizeof(progress), "%d / %d albums",
                     crazypod_artwork_library_prime_completed(),
                     crazypod_artwork_library_prime_total());
            lv_label_set_text(library.loading_detail, progress);
        }
        if(crazypod_artwork_library_prime_failed()) {
            library.artwork_preparing = false;
            library.artwork_cache_failed = true;
            if(library.loading)
                render_loading();
            return true;
        }
        if(!crazypod_artwork_library_priming()) {
            library.artwork_preparing = false;
            library.loaded = crazypod_music_track_count() > 0;
            if(library.loading)
                finish_loading();
        }
    }
    return library.loading;
}

void crazypod_music_library_schedule_rescan(long not_before)
{
    library.scan_pending = true;
    library.scan_not_before = not_before;
    library.loaded = false;
}

bool crazypod_music_library_loaded(void)
{
    return library.loaded;
}

bool crazypod_music_library_loading(void)
{
    return library.loading;
}

#endif
