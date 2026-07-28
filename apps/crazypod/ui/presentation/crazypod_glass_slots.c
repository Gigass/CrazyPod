#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>

#include "system.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_wallpaper.h"
#include "../../platform/crazypod_platform_display.h"
#include "crazypod_glass_panel.h"
#include "crazypod_glass_slots.h"

#define MENU_TOPBAR_PIXELS (LCD_WIDTH * 32)
#define MENU_PANEL_PIXELS (160 * (LCD_HEIGHT - 32))
#define SEARCH_QUERY_PIXELS (136 * 38)
#define SEARCH_RESULTS_PIXELS (136 * 104)
#define INFO_TOAST_PIXELS (230 * 50)
#define INFO_BAR_PIXELS (LCD_WIDTH * 34)

static fb_data menu_topbar_pixels[MENU_TOPBAR_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data menu_panel_pixels[MENU_PANEL_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data search_query_pixels[SEARCH_QUERY_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data search_results_pixels[SEARCH_RESULTS_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data info_toast_pixels[INFO_TOAST_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data info_bar_pixels[INFO_BAR_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data info_bar_alt_pixels[INFO_BAR_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t descriptors[7];
static crazypod_glass_boost_callback boost_cpu;

fb_data *crazypod_glass_slot_pixels(enum crazypod_glass_slot slot)
{
    switch(slot) {
    case CRAZYPOD_GLASS_SLOT_MENU_TOPBAR:
        return menu_topbar_pixels;
    case CRAZYPOD_GLASS_SLOT_MENU_PANEL:
        return menu_panel_pixels;
    case CRAZYPOD_GLASS_SLOT_SEARCH_QUERY:
        return search_query_pixels;
    case CRAZYPOD_GLASS_SLOT_SEARCH_RESULTS:
        return search_results_pixels;
    case CRAZYPOD_GLASS_SLOT_INFO_TOAST:
        return info_toast_pixels;
    case CRAZYPOD_GLASS_SLOT_INFO_BAR:
        return info_bar_pixels;
    case CRAZYPOD_GLASS_SLOT_INFO_BAR_ALT:
        return info_bar_alt_pixels;
    }
    return NULL;
}

lv_image_dsc_t *crazypod_glass_slot_descriptor(
    enum crazypod_glass_slot slot)
{
    if((unsigned)slot >= 7)
        return NULL;
    return &descriptors[slot];
}

void crazypod_glass_slots_configure(
    crazypod_glass_boost_callback boost)
{
    boost_cpu = boost;
}

static bool render_slot(
    enum crazypod_glass_slot slot,
    const fb_data *source, int source_width, int source_height,
    int source_stride, int x, int y, int width, int height,
    enum crazypod_glass_material material)
{
    return crazypod_glass_render_descriptor(
        source, source_width, source_height, source_stride,
        x, y, width, height, material,
        crazypod_glass_slot_pixels(slot),
        crazypod_glass_slot_descriptor(slot), boost_cpu);
}

bool crazypod_glass_slot_prepare_frame(
    enum crazypod_glass_slot slot,
    int x, int y, int width, int height,
    enum crazypod_glass_material material)
{
    const fb_data *framebuffer =
        (const fb_data *)crazypod_platform_display_framebuffer();

    lv_refr_now(NULL);
    return render_slot(
        slot, framebuffer, LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH,
        x, y, width, height, material);
}

bool crazypod_glass_slot_prepare_menu(
    enum crazypod_glass_slot slot,
    int x, int y, int width, int height,
    enum crazypod_glass_material material)
{
    const lv_image_dsc_t *wallpaper =
        crazypod_custom_menu_wallpaper();
    fb_data solid_pixel;
    const fb_data *source;
    int source_width;
    int source_height;
    int source_stride;

    if(wallpaper != NULL &&
       wallpaper->header.cf == LV_COLOR_FORMAT_RGB565 &&
       wallpaper->data != NULL &&
       wallpaper->header.stride % sizeof(fb_data) == 0) {
        source = (const fb_data *)wallpaper->data;
        source_width = wallpaper->header.w;
        source_height = wallpaper->header.h;
        source_stride =
            wallpaper->header.stride / sizeof(fb_data);
    }
    else {
        uint32_t color = crazypod_appearance_menu_color();

        solid_pixel = LCD_RGBPACK(
            (color >> 16) & 0xff,
            (color >> 8) & 0xff,
            color & 0xff);
        source = &solid_pixel;
        source_width = 1;
        source_height = 1;
        source_stride = 1;
        x = 0;
        y = 0;
    }
    return render_slot(
        slot, source, source_width, source_height, source_stride,
        x, y, width, height, material);
}

lv_obj_t *crazypod_glass_slot_panel(
    enum crazypod_glass_slot slot, bool prepared,
    lv_obj_t *parent, int x, int y,
    int width, int height, int radius,
    enum crazypod_glass_material material)
{
    return crazypod_glass_panel_create(
        parent, x, y, width, height, radius, material,
        prepared ? crazypod_glass_slot_descriptor(slot) : NULL);
}

#endif
