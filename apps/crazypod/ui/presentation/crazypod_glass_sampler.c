#include "config.h"

#ifdef IPOD_6G

#include "kernel.h"
#include "system.h"

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "../../crazypod_image.h"
#include "crazypod_glass_sampler.h"

#define GLASS_SAMPLE_WIDTH 80
#define GLASS_SAMPLE_HEIGHT 60
#define GLASS_TINT_COLOR 0x11131A
#define GLASS_BAKE_TINT_OPA 104
#define GLASS_ARTWORK_CAPTION_TINT_OPA 48
#define GLASS_PANEL_TINT_OPA 92
#define GLASS_HEADPHONE_TINT_OPA 48
#define GLASS_BORDER_OPA 38
#define GLASS_SHADOW_OPA 92

static fb_data sample_pixels[
    GLASS_SAMPLE_WIDTH * GLASS_SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data sample_scratch[
    GLASS_SAMPLE_WIDTH * GLASS_SAMPLE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);

uint32_t crazypod_glass_material_tint(
    enum crazypod_glass_material material)
{
    (void)material;
    return GLASS_TINT_COLOR;
}

lv_opa_t crazypod_glass_material_tint_opa(
    enum crazypod_glass_material material)
{
    if(material == CRAZYPOD_GLASS_HEADPHONE_POPUP)
        return GLASS_HEADPHONE_TINT_OPA;
    return GLASS_PANEL_TINT_OPA;
}

lv_opa_t crazypod_glass_material_border_opa(
    enum crazypod_glass_material material)
{
    if(material == CRAZYPOD_GLASS_MENU_PANEL ||
       material == CRAZYPOD_GLASS_MENU_TOPBAR)
        return LV_OPA_TRANSP;
    return GLASS_BORDER_OPA;
}

lv_opa_t crazypod_glass_material_shadow_opa(
    enum crazypod_glass_material material)
{
    if(material == CRAZYPOD_GLASS_MENU_PANEL ||
       material == CRAZYPOD_GLASS_MENU_TOPBAR)
        return LV_OPA_TRANSP;
    return GLASS_SHADOW_OPA;
}

bool crazypod_glass_render_descriptor(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x, int source_y,
    int width, int height, enum crazypod_glass_material material,
    fb_data *render_pixels, lv_image_dsc_t *descriptor,
    crazypod_glass_boost_callback boost)
{
    lv_opa_t tint_opacity =
        material == CRAZYPOD_GLASS_ARTWORK_CAPTION
            ? GLASS_ARTWORK_CAPTION_TINT_OPA
            : GLASS_BAKE_TINT_OPA;

    if(width <= 0 || height <= 0 ||
       render_pixels == NULL || descriptor == NULL)
        return false;
    if(boost != NULL)
        boost(HZ / 4);
    if(!crazypod_image_render_glass_rgb565(
           source, source_width, source_height, source_stride,
           source_x, source_y, width, height,
           crazypod_glass_material_tint(material),
           tint_opacity,
           sample_pixels, sample_scratch,
           sizeof(sample_pixels) / sizeof(sample_pixels[0]),
           render_pixels, width, height))
        return false;
    if(descriptor->header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(descriptor);
    return crazypod_image_configure_rgb565(
        descriptor, render_pixels, width, height);
}

#endif
