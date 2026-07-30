#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

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
    lv_obj_t *detail_label;
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
            ? CP_TR("Artwork Cache Failed")
            : library.scan_start_failed
                ? CP_TR("Library Scan Failed")
                : library.artwork_preparing
                    ? CP_TR("Preparing Album Artwork")
                    : crazypod_music_catalog_validation() ==
                        CRAZYPOD_MUSIC_VALIDATION_RUNNING
                        ? CP_TR("Checking Music Library")
                        : CP_TR("Building Music Library"),
        &lv_font_montserrat_12, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(title, 260);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 30, 132);
    if(library.artwork_preparing) {
        snprintf(detail, sizeof(detail), CP_FMT("%d / %d albums"),
                 crazypod_artwork_library_prime_completed(),
                 crazypod_artwork_library_prime_total());
    }
    else {
        snprintf(
            detail, sizeof(detail), "%s",
            library.artwork_cache_failed
                ? CP_FMT("Could not commit the album artwork cache")
                : library.scan_start_failed
                    ? CP_FMT("No background thread was available")
                    : crazypod_music_catalog_validation() ==
                        CRAZYPOD_MUSIC_VALIDATION_RUNNING
                        ? CP_FMT("Checking local file names, sizes and dates")
                        : CP_FMT("Reading local files and metadata"));
    }
    detail_label = crazypod_ui_widget_label(
        library.host.parent, detail, &lv_font_montserrat_8,
        COLOR_WHITE, 110);
    library.loading_detail = detail_label;
    lv_obj_set_width(detail_label, 260);
    lv_obj_set_style_text_align(
        detail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(detail_label, 30, 155);
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

static void begin_artwork_preparation(void)
{
    if(crazypod_music_album_count() == 0 ||
       crazypod_artwork_library_cache_ready()) {
        library.loaded = true;
        library.artwork_preparing = false;
        library.artwork_cache_failed = false;
        if(library.loading)
            finish_loading();
        else if(library.host.route_visible != NULL &&
                library.host.route_visible() &&
                library.host.render_route != NULL)
            library.host.render_route(true);
        return;
    }

    library.loaded = false;
    library.loading = true;
    library.artwork_preparing = true;
    library.artwork_cache_failed = false;
    crazypod_artwork_prime_library();
    render_loading();
    lv_refr_now(NULL);
}

void crazypod_music_library_configure(
    const struct crazypod_music_library_host *host)
{
    if(host != NULL)
        library.host = *host;
}

void crazypod_music_library_initialize(long now)
{
    bool catalog_ready = crazypod_music_catalog_ready();

    library.loading_detail = NULL;
    library.scan_generation_seen = crazypod_music_scan_generation();
    library.loaded = false;
    library.loading = false;
    library.scan_start_failed = false;
    library.artwork_cache_failed = false;
    library.artwork_preparing = false;
    library.scan_pending = !catalog_ready;
    library.scan_not_before = now + HZ;
}

void crazypod_music_library_begin(long now)
{
    if(crazypod_music_catalog_validation() ==
           CRAZYPOD_MUSIC_VALIDATION_RUNNING ||
       library.scan_pending ||
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
    if(crazypod_music_catalog_ready()) {
        begin_artwork_preparation();
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

void crazypod_music_library_leave(long now)
{
    bool pending = library.scan_pending;

    crazypod_music_cancel_scan();
    crazypod_artwork_cancel_library_prime();
    library.loading = false;
    library.loading_detail = NULL;
    library.scan_start_failed = false;
    library.artwork_cache_failed = false;
    library.artwork_preparing = false;
    library.scan_generation_seen =
        crazypod_music_scan_generation();
    library.scan_pending =
        pending || !crazypod_music_catalog_ready();
    library.scan_not_before = now;
    library.loaded = false;
}

void crazypod_music_library_service(long now, bool storage_active)
{
    if(!storage_active &&
       crazypod_music_take_catalog_stale()) {
        crazypod_artwork_invalidate_library_cache();
        crazypod_music_library_schedule_rescan(now);
    }
    if(!library.scan_pending || storage_active ||
       !library.loading ||
       crazypod_music_is_scanning() ||
       crazypod_music_catalog_validation() ==
           CRAZYPOD_MUSIC_VALIDATION_RUNNING ||
       TIME_BEFORE(now, library.scan_not_before))
        return;

    if(crazypod_music_catalog_ready() &&
       crazypod_music_catalog_validation() ==
           CRAZYPOD_MUSIC_VALIDATION_UNCHECKED) {
        library.scan_pending = false;
        if(!crazypod_music_validate_catalog_async()) {
            library.scan_start_failed = true;
            if(library.loading)
                render_loading();
        }
        return;
    }
    if(crazypod_music_catalog_ready() &&
       crazypod_music_catalog_validation() ==
           CRAZYPOD_MUSIC_VALIDATION_CURRENT) {
        library.scan_pending = false;
        begin_artwork_preparation();
        return;
    }

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
    enum crazypod_music_catalog_validation validation =
        crazypod_music_catalog_validation();

    if(!crazypod_music_is_scanning() &&
       crazypod_music_scan_generation() !=
           library.scan_generation_seen) {
        library.scan_generation_seen =
            crazypod_music_scan_generation();
        if(crazypod_music_catalog_ready())
            begin_artwork_preparation();
    }
    if(!library.artwork_preparing &&
       !library.artwork_cache_failed &&
       library.loading && !library.scan_pending &&
       !crazypod_music_is_scanning() &&
       crazypod_music_catalog_ready() &&
       (validation == CRAZYPOD_MUSIC_VALIDATION_CURRENT ||
        validation == CRAZYPOD_MUSIC_VALIDATION_FAILED)) {
        begin_artwork_preparation();
    }
    if(library.artwork_preparing) {
        if(library.loading_detail != NULL) {
            char progress[48];

            snprintf(progress, sizeof(progress), CP_FMT("%d / %d albums"),
                     crazypod_artwork_library_prime_completed(),
                     crazypod_artwork_library_prime_total());
            CP_LV_LABEL_SET_TEXT(library.loading_detail, progress);
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
            library.loaded =
                crazypod_artwork_library_cache_ready();
            if(library.loaded && library.loading)
                finish_loading();
            else if(!library.loaded) {
                library.artwork_cache_failed = true;
                if(library.loading)
                    render_loading();
            }
        }
    }
    return library.loading;
}

void crazypod_music_library_schedule_rescan(long not_before)
{
    library.scan_pending = true;
    library.scan_not_before = not_before;
    library.loaded = false;
    library.artwork_preparing = false;
    crazypod_artwork_cancel_library_prime();
    if(library.host.route_visible != NULL &&
       library.host.route_visible()) {
        library.loading = true;
        library.scan_start_failed = false;
        render_loading();
        lv_refr_now(NULL);
    }
}

bool crazypod_music_library_loaded(void)
{
    return library.loaded;
}

bool crazypod_music_library_loading(void)
{
    return library.loading;
}

bool crazypod_music_library_preparing_artwork(void)
{
    return library.artwork_preparing;
}

#endif
