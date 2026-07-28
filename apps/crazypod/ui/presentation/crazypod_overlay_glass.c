#include "config.h"

#ifdef IPOD_6G

#include "kernel.h"
#include "lcd.h"
#include "system.h"

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "../../crazypod_image.h"
#include "../../platform/crazypod_platform_display.h"
#include "crazypod_glass_panel.h"
#include "crazypod_overlay_glass.h"

#define POPUP_X 35
#define POPUP_Y 32
#define POPUP_WIDTH 250
#define POPUP_HEIGHT 176
#define POPUP_RADIUS 18
#define TINT_COLOR 0x11131A
#define TINT_OPA 104
#define SAMPLE_WIDTH \
    ((POPUP_WIDTH + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) / \
     CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE)
#define SAMPLE_HEIGHT \
    ((POPUP_HEIGHT + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) / \
     CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE)

static void (*boost_cpu)(int ticks);
static fb_data sample_pixels[SAMPLE_WIDTH * SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data sample_scratch[SAMPLE_WIDTH * SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data render_pixels[POPUP_WIDTH * POPUP_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t descriptor;
static bool valid;

void crazypod_overlay_glass_configure(
    void (*boost)(int ticks))
{
    boost_cpu = boost;
}

void crazypod_overlay_glass_prepare(bool refresh)
{
    const fb_data *framebuffer =
        (const fb_data *)crazypod_platform_display_framebuffer();

    if(boost_cpu != NULL)
        boost_cpu(HZ / 2);
    if(refresh)
        lv_refr_now(NULL);
    if(!crazypod_image_render_glass_rgb565(
           framebuffer, LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH,
           POPUP_X, POPUP_Y, POPUP_WIDTH, POPUP_HEIGHT,
           TINT_COLOR, TINT_OPA,
           sample_pixels, sample_scratch,
           sizeof(sample_pixels) / sizeof(sample_pixels[0]),
           render_pixels, POPUP_WIDTH, POPUP_HEIGHT)) {
        valid = false;
        return;
    }
    if(descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&descriptor);
    valid = crazypod_image_configure_rgb565(
        &descriptor, render_pixels, POPUP_WIDTH, POPUP_HEIGHT);
}

lv_obj_t *crazypod_overlay_glass_panel(
    lv_obj_t *parent, int x, int y, int width, int height)
{
    return crazypod_glass_panel_create(
        parent, x, y, width, height,
        POPUP_RADIUS, CRAZYPOD_GLASS_POPUP,
        valid ? &descriptor : NULL);
}

#endif
