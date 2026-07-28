#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "file.h"
#include "lcd.h"
#include "system.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "../../../crazypod_image.h"
#include "crazypod_now_presentation.h"

#define PRESENTATION_BANKS 2
#define COVER_SIZE 108
#define BACKDROP_WIDTH 40
#define BACKDROP_HEIGHT 30
#define SHADE_OPA 118
#define COLOR_WHITE 0xFFFFFF

static char track_paths[PRESENTATION_BANKS][MAX_PATH];
static fb_data backdrop_pixels[BACKDROP_WIDTH * BACKDROP_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data backdrop_scratch[BACKDROP_WIDTH * BACKDROP_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data backdrop_render_pixels[
    PRESENTATION_BANKS][LCD_WIDTH * LCD_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t backdrop_descriptors[PRESENTATION_BANKS];
static fb_data cover_pixels[PRESENTATION_BANKS][COVER_SIZE * COVER_SIZE]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t cover_descriptors[PRESENTATION_BANKS];
static uint32_t text_colors[PRESENTATION_BANKS];
static bool valid[PRESENTATION_BANKS];
static int active_bank = -1;

static bool prepare_cover(const lv_image_dsc_t *source, int bank)
{
    const fb_data *source_pixels;
    int width;
    int height;

    if(source == NULL || source->data == NULL ||
       bank < 0 || bank >= PRESENTATION_BANKS ||
       source->header.cf != LV_COLOR_FORMAT_RGB565)
        return false;
    width = source->header.w;
    height = source->header.h;
    if(width <= 1 || height <= 1 ||
       source->header.stride != width * sizeof(fb_data))
        return false;
    if(cover_descriptors[bank].header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&cover_descriptors[bank]);
    source_pixels = (const fb_data *)source->data;
    crazypod_image_scale_rgb565(
        source_pixels, width, height, width,
        cover_pixels[bank], COVER_SIZE, COVER_SIZE);
    crazypod_image_configure_rgb565(
        &cover_descriptors[bank], cover_pixels[bank],
        COVER_SIZE, COVER_SIZE);
    return true;
}

static bool prepare_backdrop(const lv_image_dsc_t *artwork, int bank)
{
    const fb_data *source;
    int source_stride;
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    int y;

    if(artwork == NULL || bank < 0 || bank >= PRESENTATION_BANKS ||
       artwork->header.cf != LV_COLOR_FORMAT_RGB565 ||
       artwork->header.w <= 0 || artwork->header.h <= 0)
        return false;
    source = (const fb_data *)artwork->data;
    source_stride = artwork->header.stride / sizeof(fb_data);
    crop_x = 0;
    crop_y = 0;
    crop_width = artwork->header.w;
    crop_height = artwork->header.h;
    if(crop_width * 3 > crop_height * 4) {
        int target_width = crop_height * 4 / 3;
        crop_x = (crop_width - target_width) / 2;
        crop_width = target_width;
    }
    else {
        int target_height = crop_width * 3 / 4;
        crop_y = (crop_height - target_height) / 2;
        crop_height = target_height;
    }

    for(y = 0; y < BACKDROP_HEIGHT; ++y) {
        int sy0 = crop_y + y * crop_height / BACKDROP_HEIGHT;
        int sy1 = crop_y + (y + 1) * crop_height / BACKDROP_HEIGHT;
        int x;
        if(sy1 <= sy0)
            sy1 = sy0 + 1;
        for(x = 0; x < BACKDROP_WIDTH; ++x) {
            int sx0 = crop_x + x * crop_width / BACKDROP_WIDTH;
            int sx1 = crop_x + (x + 1) * crop_width / BACKDROP_WIDTH;
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            unsigned samples = 0;
            int sy;
            if(sx1 <= sx0)
                sx1 = sx0 + 1;
            for(sy = sy0; sy < sy1; ++sy) {
                int sx;
                for(sx = sx0; sx < sx1; ++sx) {
                    fb_data pixel = source[sy * source_stride + sx];
                    red += RGB_UNPACK_RED(pixel);
                    green += RGB_UNPACK_GREEN(pixel);
                    blue += RGB_UNPACK_BLUE(pixel);
                    ++samples;
                }
            }
            if(samples == 0)
                samples = 1;
            backdrop_pixels[y * BACKDROP_WIDTH + x] =
                LCD_RGBPACK(
                    red / samples, green / samples, blue / samples);
        }
    }

    for(y = 0; y < BACKDROP_HEIGHT; ++y) {
        int x;
        for(x = 0; x < BACKDROP_WIDTH; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            int offset;
            for(offset = -2; offset <= 2; ++offset) {
                int sample_x = x + offset;
                fb_data pixel;
                if(sample_x < 0)
                    sample_x = 0;
                if(sample_x >= BACKDROP_WIDTH)
                    sample_x = BACKDROP_WIDTH - 1;
                pixel = backdrop_pixels[y * BACKDROP_WIDTH + sample_x];
                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
            }
            backdrop_scratch[y * BACKDROP_WIDTH + x] =
                LCD_RGBPACK(red / 5, green / 5, blue / 5);
        }
    }
    for(y = 0; y < BACKDROP_HEIGHT; ++y) {
        int x;
        for(x = 0; x < BACKDROP_WIDTH; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            int offset;
            for(offset = -2; offset <= 2; ++offset) {
                int sample_y = y + offset;
                fb_data pixel;
                if(sample_y < 0)
                    sample_y = 0;
                if(sample_y >= BACKDROP_HEIGHT)
                    sample_y = BACKDROP_HEIGHT - 1;
                pixel = backdrop_scratch[
                    sample_y * BACKDROP_WIDTH + x];
                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
            }
            backdrop_pixels[y * BACKDROP_WIDTH + x] =
                LCD_RGBPACK(red / 5, green / 5, blue / 5);
        }
    }

    crazypod_image_scale_rgb565(
        backdrop_pixels, BACKDROP_WIDTH, BACKDROP_HEIGHT,
        BACKDROP_WIDTH, backdrop_render_pixels[bank],
        LCD_WIDTH, LCD_HEIGHT);
    if(backdrop_descriptors[bank].header.magic ==
       LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&backdrop_descriptors[bank]);
    crazypod_image_configure_rgb565(
        &backdrop_descriptors[bank], backdrop_render_pixels[bank],
        LCD_WIDTH, LCD_HEIGHT);
    return true;
}

static unsigned shaded_luminance(
    unsigned red, unsigned green, unsigned blue)
{
    red = (red * (255 - SHADE_OPA) + 5 * SHADE_OPA + 127) / 255;
    green = (green * (255 - SHADE_OPA) + 5 * SHADE_OPA + 127) / 255;
    blue = (blue * (255 - SHADE_OPA) + 8 * SHADE_OPA + 127) / 255;
    return (54 * red + 183 * green + 19 * blue) >> 8;
}

static uint32_t contrast_color(int bank)
{
    const fb_data *pixels = backdrop_render_pixels[bank];
    unsigned long luminance = 0;
    unsigned samples = 0;
    int y;

    for(y = 68; y <= 148; y += 8) {
        int x;
        for(x = 144; x <= 296; x += 8) {
            fb_data pixel = pixels[y * LCD_WIDTH + x];
            luminance += shaded_luminance(
                RGB_UNPACK_RED(pixel),
                RGB_UNPACK_GREEN(pixel),
                RGB_UNPACK_BLUE(pixel));
            ++samples;
        }
    }
    if(samples == 0)
        return COLOR_WHITE;
    return luminance / samples >= 118 ? 0x09090D : COLOR_WHITE;
}

bool crazypod_now_presentation_matches(const char *track_path)
{
    return track_path != NULL && active_bank >= 0 &&
           valid[active_bank] &&
           strcmp(track_paths[active_bank], track_path) == 0;
}

bool crazypod_now_presentation_prepare(
    const lv_image_dsc_t *artwork, const char *track_path)
{
    int bank;

    if(artwork == NULL || track_path == NULL)
        return false;
    bank = active_bank == 0 ? 1 : 0;
    valid[bank] = false;
    if(!prepare_cover(artwork, bank) ||
       !prepare_backdrop(artwork, bank))
        return false;
    text_colors[bank] = contrast_color(bank);
    snprintf(track_paths[bank], sizeof(track_paths[bank]),
             "%s", track_path);
    valid[bank] = true;
    active_bank = bank;
    return true;
}

bool crazypod_now_presentation_get(
    const char *track_path,
    const lv_image_dsc_t **cover,
    const lv_image_dsc_t **backdrop,
    uint32_t *text_color)
{
    if(!crazypod_now_presentation_matches(track_path))
        return false;
    if(cover != NULL)
        *cover = &cover_descriptors[active_bank];
    if(backdrop != NULL)
        *backdrop = &backdrop_descriptors[active_bank];
    if(text_color != NULL)
        *text_color = text_colors[active_bank];
    return true;
}

void crazypod_now_presentation_discard(void)
{
    active_bank = -1;
}

#endif
