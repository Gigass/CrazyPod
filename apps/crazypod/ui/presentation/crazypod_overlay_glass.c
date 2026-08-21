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

#define POPUP_MAX_WIDTH (LCD_WIDTH - 24)
#define POPUP_MAX_HEIGHT (LCD_HEIGHT - 16)
#define POPUP_RADIUS 18
#define TINT_COLOR 0x11131A
#define TINT_OPA 104
#define HEADPHONE_TINT_OPA 72
#define SAMPLE_WIDTH \
    ((POPUP_MAX_WIDTH + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) / \
     CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE)
#define SAMPLE_HEIGHT \
    ((POPUP_MAX_HEIGHT + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) / \
     CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE)

static void (*boost_cpu)(int ticks);
static fb_data sample_pixels[SAMPLE_WIDTH * SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data sample_scratch[SAMPLE_WIDTH * SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data render_pixels[POPUP_MAX_WIDTH * POPUP_MAX_HEIGHT]
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
    if(boost_cpu != NULL)
        boost_cpu(HZ / 2);
    if(refresh)
        lv_refr_now(NULL);
}

static void prepare_panel_descriptor(
    int x, int y, int width, int height, lv_opa_t tint_opacity)
{
    const fb_data *framebuffer =
        (const fb_data *)crazypod_platform_display_framebuffer();

    if(framebuffer == NULL || width <= 0 || height <= 0 ||
       width > POPUP_MAX_WIDTH || height > POPUP_MAX_HEIGHT ||
       x < 0 || y < 0 || x + width > LCD_WIDTH ||
       y + height > LCD_HEIGHT ||
       !crazypod_image_render_glass_rgb565(
           framebuffer, LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH,
           x, y, width, height, TINT_COLOR, tint_opacity,
           sample_pixels, sample_scratch,
           sizeof(sample_pixels) / sizeof(sample_pixels[0]),
           render_pixels, width, height)) {
        valid = false;
        return;
    }
    if(descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&descriptor);
    valid = crazypod_image_configure_rgb565(
        &descriptor, render_pixels, width, height);
}

lv_obj_t *crazypod_overlay_glass_panel(
    lv_obj_t *parent, int x, int y, int width, int height)
{
    prepare_panel_descriptor(x, y, width, height, TINT_OPA);
    return crazypod_glass_panel_create(
        parent, x, y, width, height,
        POPUP_RADIUS, CRAZYPOD_GLASS_POPUP,
        valid ? &descriptor : NULL);
}

lv_obj_t *crazypod_overlay_glass_headphone_panel(
    lv_obj_t *parent, int x, int y, int width, int height)
{
    prepare_panel_descriptor(
        x, y, width, height, HEADPHONE_TINT_OPA);
    return crazypod_glass_panel_create(
        parent, x, y, width, height,
        POPUP_RADIUS, CRAZYPOD_GLASS_HEADPHONE_POPUP,
        valid ? &descriptor : NULL);
}

#endif
