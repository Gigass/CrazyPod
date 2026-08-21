#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <string.h>

#include "kernel.h"
#include "lcd.h"
#include "system.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "../crazypod_image.h"
#include "../crazypod_photos.h"
#include "crazypod_photo_viewport.h"

static fb_data pixels[
    CRAZYPOD_PHOTO_VIEWPORT_WIDTH * CRAZYPOD_PHOTO_VIEWPORT_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t descriptor;
static const uint8_t *viewport_source;
static const uint8_t *crop_source;
static int crop_image_x;
static int crop_image_y;
static int viewport_index = -1;
static int viewport_zoom;
static int viewport_pan_x;
static int viewport_pan_y;

static int begin_background_render(void)
{
#ifdef HAVE_PRIORITY_SCHEDULING
    return thread_set_priority(
        thread_self(), PRIORITY_BACKGROUND);
#else
    return -1;
#endif
}

static void finish_background_render(int old_priority)
{
#ifdef HAVE_PRIORITY_SCHEDULING
    if(old_priority >= HIGHEST_PRIORITY &&
       old_priority <= LOWEST_PRIORITY)
        thread_set_priority(thread_self(), old_priority);
#else
    (void)old_priority;
#endif
}

static fb_data sample_bilinear(
    const fb_data *source, int width, int height,
    int x_q8, int y_q8)
{
    int x0 = x_q8 >> 8;
    int y0 = y_q8 >> 8;
    int x1;
    int y1;
    int fx;
    int fy;
    fb_data p00;
    fb_data p10;
    fb_data p01;
    fb_data p11;
    unsigned red0;
    unsigned red1;
    unsigned green0;
    unsigned green1;
    unsigned blue0;
    unsigned blue1;

    if(x0 < 0)
        x0 = 0;
    if(y0 < 0)
        y0 = 0;
    if(x0 >= width)
        x0 = width - 1;
    if(y0 >= height)
        y0 = height - 1;
    x1 = x0 + 1 < width ? x0 + 1 : x0;
    y1 = y0 + 1 < height ? y0 + 1 : y0;
    fx = x_q8 & 255;
    fy = y_q8 & 255;
    p00 = source[y0 * width + x0];
    p10 = source[y0 * width + x1];
    p01 = source[y1 * width + x0];
    p11 = source[y1 * width + x1];
    red0 = RGB_UNPACK_RED(p00) * (256 - fx) +
        RGB_UNPACK_RED(p10) * fx;
    red1 = RGB_UNPACK_RED(p01) * (256 - fx) +
        RGB_UNPACK_RED(p11) * fx;
    green0 = RGB_UNPACK_GREEN(p00) * (256 - fx) +
        RGB_UNPACK_GREEN(p10) * fx;
    green1 = RGB_UNPACK_GREEN(p01) * (256 - fx) +
        RGB_UNPACK_GREEN(p11) * fx;
    blue0 = RGB_UNPACK_BLUE(p00) * (256 - fx) +
        RGB_UNPACK_BLUE(p10) * fx;
    blue1 = RGB_UNPACK_BLUE(p01) * (256 - fx) +
        RGB_UNPACK_BLUE(p11) * fx;
    return LCD_RGBPACK(
        (red0 * (256 - fy) + red1 * fy) >> 16,
        (green0 * (256 - fy) + green1 * fy) >> 16,
        (blue0 * (256 - fy) + blue1 * fy) >> 16);
}

void crazypod_photo_viewport_reset(void)
{
    viewport_source = NULL;
    crop_source = NULL;
    viewport_index = -1;
}

const lv_image_dsc_t *crazypod_photo_viewport_render(
    int index, const lv_image_dsc_t *source_descriptor,
    int zoom_percent, int *pan_x, int *pan_y)
{
    const fb_data *source;
    uint32_t scale_x;
    uint32_t scale_y;
    uint32_t scale;
    int source_width;
    int source_height;
    int display_width;
    int display_height;
    int image_x;
    int image_y;
    int overflow_x;
    int overflow_y;
    int shift_x;
    int shift_y;
    int retained_x;
    int retained_y;
    int retained_width;
    int retained_height;
    int old_priority;
    bool incremental_pan;
    int y;

    if(source_descriptor == NULL || pan_x == NULL || pan_y == NULL)
        return NULL;
    source = (const fb_data *)source_descriptor->data;
    source_width = source_descriptor->header.w;
    source_height = source_descriptor->header.h;
    if(source == NULL || source_width <= 0 || source_height <= 0)
        return NULL;
    scale_x = (uint32_t)CRAZYPOD_PHOTO_VIEWPORT_WIDTH *
        LV_SCALE_NONE / source_width;
    scale_y = (uint32_t)CRAZYPOD_PHOTO_VIEWPORT_HEIGHT *
        LV_SCALE_NONE / source_height;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if(zoom_percent < 100)
        zoom_percent = 100;
    if(zoom_percent > 500)
        zoom_percent = 500;
    scale = scale * (uint32_t)zoom_percent / 100;
    if(scale == 0)
        scale = 1;
    display_width = source_width * scale / LV_SCALE_NONE;
    display_height = source_height * scale / LV_SCALE_NONE;
    overflow_x = display_width > CRAZYPOD_PHOTO_VIEWPORT_WIDTH
        ? (display_width - CRAZYPOD_PHOTO_VIEWPORT_WIDTH) / 2 : 0;
    overflow_y = display_height > CRAZYPOD_PHOTO_VIEWPORT_HEIGHT
        ? (display_height - CRAZYPOD_PHOTO_VIEWPORT_HEIGHT) / 2 : 0;
    if(zoom_percent == 100) {
        *pan_x = 0;
        *pan_y = 0;
    }
    if(*pan_x < -overflow_x)
        *pan_x = -overflow_x;
    if(*pan_x > overflow_x)
        *pan_x = overflow_x;
    if(*pan_y < -overflow_y)
        *pan_y = -overflow_y;
    if(*pan_y > overflow_y)
        *pan_y = overflow_y;
    if(descriptor.header.magic == LV_IMAGE_HEADER_MAGIC &&
       viewport_source == source_descriptor->data &&
       viewport_index == index && viewport_zoom == zoom_percent &&
       viewport_pan_x == *pan_x && viewport_pan_y == *pan_y)
        return &descriptor;
    old_priority = begin_background_render();
    image_x = (CRAZYPOD_PHOTO_VIEWPORT_WIDTH - display_width) / 2 +
        *pan_x;
    image_y = (CRAZYPOD_PHOTO_VIEWPORT_HEIGHT - display_height) / 2 +
        *pan_y;
    shift_x = *pan_x - viewport_pan_x;
    shift_y = *pan_y - viewport_pan_y;
    incremental_pan =
        descriptor.header.magic == LV_IMAGE_HEADER_MAGIC &&
        viewport_source == source_descriptor->data &&
        viewport_index == index && viewport_zoom == zoom_percent &&
        shift_x > -CRAZYPOD_PHOTO_VIEWPORT_WIDTH &&
        shift_x < CRAZYPOD_PHOTO_VIEWPORT_WIDTH &&
        shift_y > -CRAZYPOD_PHOTO_VIEWPORT_HEIGHT &&
        shift_y < CRAZYPOD_PHOTO_VIEWPORT_HEIGHT;
    if(descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&descriptor);
    retained_x = shift_x > 0 ? shift_x : 0;
    retained_y = shift_y > 0 ? shift_y : 0;
    retained_width = CRAZYPOD_PHOTO_VIEWPORT_WIDTH -
        (shift_x < 0 ? -shift_x : shift_x);
    retained_height = CRAZYPOD_PHOTO_VIEWPORT_HEIGHT -
        (shift_y < 0 ? -shift_y : shift_y);
    if(incremental_pan) {
        int y_start;
        int y_end;
        int y_step;

        if(shift_y > 0) {
            y_start = CRAZYPOD_PHOTO_VIEWPORT_HEIGHT - 1;
            y_end = retained_y - 1;
            y_step = -1;
        }
        else {
            y_start = retained_y;
            y_end = retained_y + retained_height;
            y_step = 1;
        }
        for(y = y_start; y != y_end; y += y_step) {
            int source_y = y - shift_y;
            int source_x = shift_x < 0 ? -shift_x : 0;

            memmove(
                &pixels[y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH +
                        retained_x],
                &pixels[source_y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH +
                        source_x],
                retained_width * sizeof(fb_data));
        }
    }
    else {
        memset(pixels, 0, sizeof(pixels));
        retained_width = 0;
        retained_height = 0;
    }
    for(y = 0; y < CRAZYPOD_PHOTO_VIEWPORT_HEIGHT; ++y) {
        int display_y = y - image_y;
        int source_y_q8 = 0;
        int x;

        if(display_y >= 0 && display_y < display_height)
            source_y_q8 =
                display_y * source_height * 256 / display_height;
        for(x = 0; x < CRAZYPOD_PHOTO_VIEWPORT_WIDTH; ++x) {
            int display_x = x - image_x;
            int source_x_q8;

            if(incremental_pan &&
               x >= retained_x && x < retained_x + retained_width &&
               y >= retained_y && y < retained_y + retained_height)
                continue;
            pixels[y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH + x] = 0;
            if(display_y < 0 || display_y >= display_height ||
               display_x < 0 || display_x >= display_width)
                continue;
            source_x_q8 =
                display_x * source_width * 256 / display_width;
            pixels[y * CRAZYPOD_PHOTO_VIEWPORT_WIDTH + x] =
                sample_bilinear(source, source_width, source_height,
                                source_x_q8, source_y_q8);
        }
    }
    crazypod_image_configure_rgb565(
        &descriptor, pixels, CRAZYPOD_PHOTO_VIEWPORT_WIDTH,
        CRAZYPOD_PHOTO_VIEWPORT_HEIGHT);
    viewport_source = source_descriptor->data;
    viewport_index = index;
    viewport_zoom = zoom_percent;
    viewport_pan_x = *pan_x;
    viewport_pan_y = *pan_y;
    finish_background_render(old_priority);
    return &descriptor;
}

const lv_image_dsc_t *crazypod_photo_viewport_render_crop(
    const lv_image_dsc_t *source_descriptor,
    int center_x, int center_y)
{
    const int preview_width = CRAZYPOD_PHOTO_VIEWPORT_WIDTH;
    const int preview_height = CRAZYPOD_PHOTO_VIEWPORT_HEIGHT;
    const fb_data *source;
    int source_width;
    int source_height;
    int display_width;
    int display_height;
    int image_x;
    int image_y;
    int old_priority;
    int y;

    if(source_descriptor == NULL)
        return NULL;
    source = (const fb_data *)source_descriptor->data;
    source_width = source_descriptor->header.w;
    source_height = source_descriptor->header.h;
    if(source == NULL || source_width <= 0 || source_height <= 0)
        return NULL;
    if(source_width * preview_height >
       source_height * preview_width) {
        display_height = preview_height;
        display_width = source_width * preview_height / source_height;
    }
    else {
        display_width = preview_width;
        display_height = source_height * preview_width / source_width;
    }
    if(center_x < 0)
        center_x = source_width / 2;
    if(center_y < 0)
        center_y = source_height / 2;
    image_x = preview_width / 2 -
        center_x * display_width / source_width;
    image_y = preview_height / 2 -
        center_y * display_height / source_height;
    if(image_x > 0)
        image_x = 0;
    if(image_x < preview_width - display_width)
        image_x = preview_width - display_width;
    if(image_y > 0)
        image_y = 0;
    if(image_y < preview_height - display_height)
        image_y = preview_height - display_height;
    if(descriptor.header.magic == LV_IMAGE_HEADER_MAGIC &&
       descriptor.header.w == preview_width &&
       descriptor.header.h == preview_height &&
       crop_source == source_descriptor->data &&
       crop_image_x == image_x && crop_image_y == image_y)
        return &descriptor;
    old_priority = begin_background_render();
    if(descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&descriptor);
    memset(pixels, 0, preview_width * preview_height * sizeof(fb_data));
    for(y = 0; y < preview_height; ++y) {
        int display_y = y - image_y;
        int source_y_q8 = 0;
        int x;

        if(display_y >= 0 && display_y < display_height)
            source_y_q8 =
                display_y * source_height * 256 / display_height;
        for(x = 0; x < preview_width; ++x) {
            int display_x = x - image_x;
            int source_x_q8;

            if(display_y < 0 || display_y >= display_height ||
               display_x < 0 || display_x >= display_width)
                continue;
            source_x_q8 =
                display_x * source_width * 256 / display_width;
            pixels[y * preview_width + x] =
                sample_bilinear(source, source_width, source_height,
                                source_x_q8, source_y_q8);
        }
    }
    crazypod_image_configure_rgb565(
        &descriptor, pixels, preview_width, preview_height);
    crop_source = source_descriptor->data;
    crop_image_x = image_x;
    crop_image_y = image_y;
    viewport_source = NULL;
    viewport_index = -1;
    finish_background_render(old_priority);
    return &descriptor;
}

#endif
