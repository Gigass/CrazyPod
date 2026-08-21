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

size_t crazypod_image_glass_sample_pixels(
    int destination_width, int destination_height)
{
    int sample_width;
    int sample_height;

    if(destination_width <= 0 || destination_height <= 0)
        return 0;
    sample_width =
        (destination_width + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) /
        CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
    sample_height =
        (destination_height + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) /
        CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
    return (size_t)sample_width * sample_height;
}

static int clamp_sample_coordinate(int value, int maximum)
{
    if(value < 0)
        return 0;
    if(value >= maximum)
        return maximum - 1;
    return value;
}

static fb_data mix_rgb565(fb_data first, fb_data second, unsigned fraction)
{
    unsigned inverse = 256 - fraction;

    if(fraction == 0)
        return first;
    return LCD_RGBPACK(
        (RGB_UNPACK_RED(first) * inverse +
         RGB_UNPACK_RED(second) * fraction) >> 8,
        (RGB_UNPACK_GREEN(first) * inverse +
         RGB_UNPACK_GREEN(second) * fraction) >> 8,
        (RGB_UNPACK_BLUE(first) * inverse +
         RGB_UNPACK_BLUE(second) * fraction) >> 8);
}

static bool scale_glass_rgb565(
    const fb_data *source, int source_width, int source_height,
    fb_data *destination, int destination_width, int destination_height)
{
    int source_x_step;
    int source_y_step;
    int y;

    if(source == NULL || destination == NULL ||
       source_width <= 0 || source_height <= 0 ||
       destination_width < source_width ||
       destination_height < source_height)
        return false;

    source_x_step = destination_width > 1
        ? (source_width - 1) * 256 / (destination_width - 1)
        : 0;
    for(y = 0; y < source_height; ++y) {
        int source_x_q8 = 0;
        int x;

        for(x = 0; x < destination_width; ++x) {
            int x0 = source_x_q8 >> 8;
            int x1 = x0 + 1 < source_width ? x0 + 1 : x0;

            destination[y * destination_width + x] = mix_rgb565(
                source[y * source_width + x0],
                source[y * source_width + x1],
                (unsigned)source_x_q8 & 255);
            source_x_q8 += source_x_step;
        }
    }

    source_y_step = destination_height > 1
        ? (source_height - 1) * 256 / (destination_height - 1)
        : 0;
    for(y = destination_height - 1; y >= 0; --y) {
        int source_y_q8 = y * source_y_step;
        int y0 = source_y_q8 >> 8;
        int y1 = y0 + 1 < source_height ? y0 + 1 : y0;
        unsigned fraction = (unsigned)source_y_q8 & 255;
        int x;

        for(x = 0; x < destination_width; ++x) {
            destination[y * destination_width + x] = mix_rgb565(
                destination[y0 * destination_width + x],
                destination[y1 * destination_width + x],
                fraction);
        }
    }
    return true;
}

bool crazypod_image_render_glass_rgb565(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x, int source_y,
    int source_region_width, int source_region_height,
    uint32_t tint, unsigned tint_opa,
    fb_data *sample_pixels, fb_data *scratch_pixels,
    size_t sample_capacity, fb_data *destination,
    int destination_width, int destination_height)
{
    int sample_width;
    int sample_height;
    unsigned tint_red = (tint >> 16) & 0xff;
    unsigned tint_green = (tint >> 8) & 0xff;
    unsigned tint_blue = tint & 0xff;
    int y;

    if(source == NULL || sample_pixels == NULL ||
       scratch_pixels == NULL || destination == NULL ||
       source_width <= 0 || source_height <= 0 ||
       source_stride < source_width ||
       source_region_width <= 0 || source_region_height <= 0 ||
       destination_width <= 0 || destination_height <= 0)
        return false;
    if(tint_opa > 255)
        tint_opa = 255;

    sample_width =
        (destination_width + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) /
        CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
    sample_height =
        (destination_height + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) /
        CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
    if((size_t)sample_width * sample_height > sample_capacity)
        return false;

    if(source_region_width == destination_width &&
       source_region_height == destination_height &&
       source_x >= 0 && source_y >= 0 &&
       source_x + source_region_width <= source_width &&
       source_y + source_region_height <= source_height) {
        for(y = 0; y < sample_height; ++y) {
            int sy0 = source_y +
                y * CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
            int sy1 = sy0 + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
            int x;

            if(sy1 > source_y + source_region_height)
                sy1 = source_y + source_region_height;
            for(x = 0; x < sample_width; ++x) {
                int sx0 = source_x +
                    x * CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
                int sx1 = sx0 + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
                unsigned red = 0;
                unsigned green = 0;
                unsigned blue = 0;
                unsigned samples = 0;
                int sy;

                if(sx1 > source_x + source_region_width)
                    sx1 = source_x + source_region_width;
                for(sy = sy0; sy < sy1; ++sy) {
                    const fb_data *row = source + sy * source_stride;
                    int sx;

                    for(sx = sx0; sx < sx1; ++sx) {
                        fb_data pixel = row[sx];

                        red += RGB_UNPACK_RED(pixel);
                        green += RGB_UNPACK_GREEN(pixel);
                        blue += RGB_UNPACK_BLUE(pixel);
                        ++samples;
                    }
                }
                sample_pixels[y * sample_width + x] = LCD_RGBPACK(
                    red / samples, green / samples, blue / samples);
            }
        }
    }
    else {
        for(y = 0; y < sample_height; ++y) {
            int destination_y0 =
                y * CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
            int destination_y1 = destination_y0 +
                CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
            int sy0;
            int sy1;
            int x;

            if(destination_y1 > destination_height)
                destination_y1 = destination_height;
            sy0 = source_y + destination_y0 * source_region_height /
                destination_height;
            sy1 = source_y + destination_y1 * source_region_height /
                destination_height;
            if(sy1 <= sy0)
                sy1 = sy0 + 1;
            for(x = 0; x < sample_width; ++x) {
                int destination_x0 =
                    x * CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
                int destination_x1 = destination_x0 +
                    CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
                int sx0;
                int sx1;
                unsigned red = 0;
                unsigned green = 0;
                unsigned blue = 0;
                unsigned samples = 0;
                int sy;

                if(destination_x1 > destination_width)
                    destination_x1 = destination_width;
                sx0 = source_x + destination_x0 * source_region_width /
                    destination_width;
                sx1 = source_x + destination_x1 * source_region_width /
                    destination_width;
                if(sx1 <= sx0)
                    sx1 = sx0 + 1;
                for(sy = sy0; sy < sy1; ++sy) {
                    int source_sample_y =
                        clamp_sample_coordinate(sy, source_height);
                    int sx;

                    for(sx = sx0; sx < sx1; ++sx) {
                        int source_sample_x =
                            clamp_sample_coordinate(sx, source_width);
                        fb_data pixel = source[
                            source_sample_y * source_stride +
                            source_sample_x];

                        red += RGB_UNPACK_RED(pixel);
                        green += RGB_UNPACK_GREEN(pixel);
                        blue += RGB_UNPACK_BLUE(pixel);
                        ++samples;
                    }
                }
                if(samples == 0)
                    samples = 1;
                sample_pixels[y * sample_width + x] = LCD_RGBPACK(
                    red / samples, green / samples, blue / samples);
            }
        }
    }

    for(y = 0; y < sample_height; ++y) {
        unsigned red = 0;
        unsigned green = 0;
        unsigned blue = 0;
        int offset;
        int x;

        for(offset = -2; offset <= 2; ++offset) {
            int sample_x = clamp_sample_coordinate(
                offset, sample_width);
            fb_data pixel =
                sample_pixels[y * sample_width + sample_x];

            red += RGB_UNPACK_RED(pixel);
            green += RGB_UNPACK_GREEN(pixel);
            blue += RGB_UNPACK_BLUE(pixel);
        }
        for(x = 0; x < sample_width; ++x) {
            scratch_pixels[y * sample_width + x] =
                LCD_RGBPACK(red / 5, green / 5, blue / 5);
            if(x + 1 < sample_width) {
                fb_data removed = sample_pixels[
                    y * sample_width + clamp_sample_coordinate(
                        x - 2, sample_width)];
                fb_data added = sample_pixels[
                    y * sample_width + clamp_sample_coordinate(
                        x + 3, sample_width)];

                red -= RGB_UNPACK_RED(removed);
                green -= RGB_UNPACK_GREEN(removed);
                blue -= RGB_UNPACK_BLUE(removed);
                red += RGB_UNPACK_RED(added);
                green += RGB_UNPACK_GREEN(added);
                blue += RGB_UNPACK_BLUE(added);
            }
        }
    }

    {
        int x;

        for(x = 0; x < sample_width; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            int offset;

            for(offset = -2; offset <= 2; ++offset) {
                int sample_y = clamp_sample_coordinate(
                    offset, sample_height);
                fb_data pixel =
                    scratch_pixels[sample_y * sample_width + x];

                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
            }
            for(y = 0; y < sample_height; ++y) {
                unsigned tinted_red = red / 5;
                unsigned tinted_green = green / 5;
                unsigned tinted_blue = blue / 5;

                tinted_red = (tinted_red * (255 - tint_opa) +
                    tint_red * tint_opa + 127) / 255;
                tinted_green = (tinted_green * (255 - tint_opa) +
                    tint_green * tint_opa + 127) / 255;
                tinted_blue = (tinted_blue * (255 - tint_opa) +
                    tint_blue * tint_opa + 127) / 255;
                sample_pixels[y * sample_width + x] = LCD_RGBPACK(
                    tinted_red, tinted_green, tinted_blue);
                if(y + 1 < sample_height) {
                    fb_data removed = scratch_pixels[
                        clamp_sample_coordinate(y - 2, sample_height) *
                            sample_width + x];
                    fb_data added = scratch_pixels[
                        clamp_sample_coordinate(y + 3, sample_height) *
                            sample_width + x];

                    red -= RGB_UNPACK_RED(removed);
                    green -= RGB_UNPACK_GREEN(removed);
                    blue -= RGB_UNPACK_BLUE(removed);
                    red += RGB_UNPACK_RED(added);
                    green += RGB_UNPACK_GREEN(added);
                    blue += RGB_UNPACK_BLUE(added);
                }
            }
        }
    }

    return scale_glass_rgb565(
        sample_pixels, sample_width, sample_height,
        destination, destination_width, destination_height);
}

#endif
