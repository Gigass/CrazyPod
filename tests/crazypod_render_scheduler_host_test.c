#include <assert.h>
#include <stddef.h>
#include "../apps/crazypod/ui/navigation/crazypod_render_scheduler.h"

/* Exercise the real scheduler without LVGL or media workers. */
#define CRAZYPOD_MENU_PREVIEW_H
#define CRAZYPOD_MENU_LIST_H
#define CRAZYPOD_PREVIEW_MOTION_H
static bool available = true;
static bool matching = true;
static int refreshes;
static int selection;
static int previews;
static struct route_state route;
static bool crazypod_menu_list_matches(enum crazypod_route id)
{ (void)id; return matching; }
static bool crazypod_menu_preview_motion_ready(void) { return false; }
static bool crazypod_menu_preview_is_skeuomorphic_route(enum crazypod_route id)
{ (void)id; return false; }
static bool crazypod_preview_motion_has_content(void) { return false; }
static bool crazypod_preview_motion_active(void) { return false; }
static bool crazypod_preview_motion_media_refresh_pending(void) { return false; }
static long crazypod_preview_motion_media_due(void) { return 0; }
static void crazypod_preview_motion_clear_media_refresh(void) {}
static void crazypod_preview_motion_start_exit(void) {}
static void crazypod_menu_preview_render(const struct route_state *s, bool animate)
{ (void)s; (void)animate; ++previews; }
#include "../apps/crazypod/ui/navigation/crazypod_render_scheduler.c"

static bool route_available(void) { return available; }
static struct route_state *current_route(void) { return &route; }
static void render_route(bool transition) { (void)transition; }
static void refresh_rows(const struct route_state *s)
{ ++refreshes; selection = s->selected; }

int main(void)
{
    const struct crazypod_render_scheduler_host host = {
        .route_available = route_available,
        .current_route = current_route,
        .render_route = render_route,
        .refresh_menu_rows = refresh_rows,
    };
    crazypod_render_scheduler_configure(&host);
    for(int i = 0; i < 16; ++i) {
        route.selected = i;
        crazypod_render_scheduler_schedule_rows();
        crazypod_render_scheduler_schedule_preview(100 + i);
    }
    crazypod_render_scheduler_service(99);
    assert(refreshes == 0 && previews == 0);
    crazypod_render_scheduler_refresh_rows();
    assert(refreshes == 1 && selection == 15);
    crazypod_render_scheduler_refresh_rows();
    assert(refreshes == 1);
    crazypod_render_scheduler_service(115);
    assert(previews == 1);
    crazypod_render_scheduler_schedule_rows();
    crazypod_render_scheduler_reset();
    crazypod_render_scheduler_refresh_rows();
    assert(refreshes == 1);
    crazypod_render_scheduler_schedule_rows();
    available = false;
    crazypod_render_scheduler_refresh_rows();
    available = true;
    crazypod_render_scheduler_refresh_rows();
    assert(refreshes == 1);
    crazypod_render_scheduler_schedule_rows();
    matching = false;
    crazypod_render_scheduler_refresh_rows();
    assert(refreshes == 1);
    return 0;
}
