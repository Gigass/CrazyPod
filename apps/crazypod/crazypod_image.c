#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "kernel.h"

#include "crazypod_image.h"

static struct mutex image_decode_mutex;

void crazypod_image_init(void)
{
    mutex_init(&image_decode_mutex);
}

void crazypod_image_decode_lock(void)
{
    mutex_lock(&image_decode_mutex);
}

void crazypod_image_decode_unlock(void)
{
    mutex_unlock(&image_decode_mutex);
}

bool crazypod_image_configure_rgb565(
    lv_image_dsc_t *descriptor, const fb_data *pixels,
    int width, int height)
{
    if(descriptor == NULL || pixels == NULL || width <= 0 || height <= 0)
        return false;

    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->header.magic = LV_IMAGE_HEADER_MAGIC;
    descriptor->header.cf = LV_COLOR_FORMAT_RGB565;
    descriptor->header.w = width;
    descriptor->header.h = height;
    descriptor->header.stride = width * sizeof(fb_data);
    descriptor->data_size =
        (size_t)width * height * sizeof(fb_data);
    descriptor->data = (const uint8_t *)pixels;
    return true;
}

bool crazypod_image_scale_rgb565(
    const fb_data *source, int source_width, int source_height,
    int source_stride, fb_data *destination,
    int destination_width, int destination_height)
{
    int source_x_step;
    int source_y_step;
    int y;

    if(source == NULL || destination == NULL ||
       source_width <= 0 || source_height <= 0 ||
       source_stride < source_width ||
       destination_width <= 0 || destination_height <= 0)
        return false;

    source_x_step = destination_width > 1
        ? (source_width - 1) * 256 / (destination_width - 1)
        : 0;
    source_y_step = destination_height > 1
        ? (source_height - 1) * 256 / (destination_height - 1)
        : 0;
    for(y = 0; y < destination_height; ++y) {
        int source_y_q8 = y * source_y_step;
        int y0 = source_y_q8 >> 8;
        int y1 = y0 + 1 < source_height ? y0 + 1 : y0;
        int fy = source_y_q8 & 255;
        int source_x_q8 = 0;
        int x;

        for(x = 0; x < destination_width; ++x) {
            int x0 = source_x_q8 >> 8;
            int x1 = x0 + 1 < source_width ? x0 + 1 : x0;
            int fx = source_x_q8 & 255;
            fb_data p00 = source[y0 * source_stride + x0];
            fb_data p10 = source[y0 * source_stride + x1];
            fb_data p01 = source[y1 * source_stride + x0];
            fb_data p11 = source[y1 * source_stride + x1];
            unsigned red0 =
                RGB_UNPACK_RED(p00) * (256 - fx) +
                RGB_UNPACK_RED(p10) * fx;
            unsigned red1 =
                RGB_UNPACK_RED(p01) * (256 - fx) +
                RGB_UNPACK_RED(p11) * fx;
            unsigned green0 =
                RGB_UNPACK_GREEN(p00) * (256 - fx) +
                RGB_UNPACK_GREEN(p10) * fx;
            unsigned green1 =
                RGB_UNPACK_GREEN(p01) * (256 - fx) +
                RGB_UNPACK_GREEN(p11) * fx;
            unsigned blue0 =
                RGB_UNPACK_BLUE(p00) * (256 - fx) +
                RGB_UNPACK_BLUE(p10) * fx;
            unsigned blue1 =
                RGB_UNPACK_BLUE(p01) * (256 - fx) +
                RGB_UNPACK_BLUE(p11) * fx;

            destination[y * destination_width + x] =
                LCD_RGBPACK(
                    (red0 * (256 - fy) + red1 * fy) >> 16,
                    (green0 * (256 - fy) + green1 * fy) >> 16,
                    (blue0 * (256 - fy) + blue1 * fy) >> 16);
            source_x_q8 += source_x_step;
        }
    }
    return true;
}

#endif
