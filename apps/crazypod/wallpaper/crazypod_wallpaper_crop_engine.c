#include "config.h"

#ifdef IPOD_6G

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bmp.h"
#include "buflib.h"
#include "core_alloc.h"
#include "jpeg_load.h"
#include "resize.h"
#include "string-extra.h"

#include "../crazypod_image.h"
#include "crazypod_wallpaper_crop_engine.h"

#define TARGET_WIDTH LCD_WIDTH
#define TARGET_HEIGHT LCD_HEIGHT
#define MAX_DECODE_PIXELS (4u * 1024u * 1024u)
#define CROP_OVERSAMPLE 2

static bool path_has_extension(const char *path, const char *extension)
{
    const char *suffix;

    if(path == NULL || extension == NULL)
        return false;
    suffix = strrchr(path, '.');
    return suffix != NULL && strcasecmp(suffix, extension) == 0;
}

static fb_data sample_pixel(
    const fb_data *source, int width, int height, int stride,
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
    p00 = source[y0 * stride + x0];
    p10 = source[y0 * stride + x1];
    p01 = source[y1 * stride + x0];
    p11 = source[y1 * stride + x1];
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

static int max_decode_scale(int width, int height)
{
    uint64_t source_pixels;
    int scale = 1;

    if(width <= 0 || height <= 0)
        return 0;
    source_pixels = (uint64_t)width * height;
    if(source_pixels > MAX_DECODE_PIXELS)
        return 0;
    while((uint64_t)(scale + 1) * (scale + 1) *
              source_pixels <= MAX_DECODE_PIXELS)
        ++scale;
    return scale;
}

int crazypod_wallpaper_crop_engine_max_zoom(
    const lv_image_dsc_t *source)
{
    int source_width;
    int source_height;
    int maximum_width;
    int maximum_height;
    int decode_scale;
    int width_zoom;
    int height_zoom;
    int maximum_zoom;

    if(source == NULL ||
       source->header.magic != LV_IMAGE_HEADER_MAGIC ||
       source->header.w <= 0 || source->header.h <= 0)
        return 100;
    source_width = source->header.w;
    source_height = source->header.h;
    if(source_width * 3 > source_height * 4) {
        maximum_height = source_height;
        maximum_width = maximum_height * 4 / 3;
    }
    else {
        maximum_width = source_width;
        maximum_height = maximum_width * 3 / 4;
    }
    decode_scale = max_decode_scale(source_width, source_height);
    if(decode_scale < 1)
        return 100;
    width_zoom = maximum_width * decode_scale * 100 / TARGET_WIDTH;
    height_zoom = maximum_height * decode_scale * 100 / TARGET_HEIGHT;
    maximum_zoom = width_zoom < height_zoom ? width_zoom : height_zoom;
    if(maximum_zoom < 100)
        maximum_zoom = 100;
    if(maximum_zoom > 500)
        maximum_zoom = 500;
    return maximum_zoom;
}

static enum crazypod_wallpaper_apply_result decode_source(
    const char *path, int width, int height,
    struct bitmap *bitmap, int *decode_handle)
{
    size_t decode_bytes;
    fb_data *decode_buffer;
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int required_bytes;
    int probe_handle = -1;
    void *probe_buffer = NULL;
    int result;
    bool jpeg;

    if(path == NULL || bitmap == NULL || decode_handle == NULL ||
       width <= 0 || height <= 0 ||
       (uint64_t)width * height > MAX_DECODE_PIXELS)
        return CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
    jpeg = path_has_extension(path, ".jpg") ||
        path_has_extension(path, ".jpeg");
    if(jpeg) {
        probe_handle = core_alloc_ex(
            JPEG_DECODE_OVERHEAD, &buflib_ops_locked);
        if(probe_handle < 0)
            return CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED;
        probe_buffer = core_get_data(probe_handle);
    }
    memset(bitmap, 0, sizeof(*bitmap));
    bitmap->width = width;
    bitmap->height = height;
    bitmap->data = probe_buffer;
    crazypod_image_decode_lock();
    if(jpeg)
        required_bytes = read_jpeg_file(
            path, bitmap, (int)JPEG_DECODE_OVERHEAD,
            format | FORMAT_RETURN_SIZE, &format_native);
    else if(path_has_extension(path, ".bmp"))
        required_bytes = read_bmp_file(
            path, bitmap, 0, format | FORMAT_RETURN_SIZE,
            &format_native);
    else
        required_bytes = -1;
    crazypod_image_decode_unlock();
    if(probe_handle >= 0)
        core_free(probe_handle);
    if(required_bytes <= 0)
        return CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED;
    decode_bytes = (size_t)required_bytes;
    if(decode_bytes > INT_MAX)
        return CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED;
    *decode_handle = core_alloc_ex(
        decode_bytes, &buflib_ops_locked);
    if(*decode_handle < 0)
        return CRAZYPOD_WALLPAPER_APPLY_WORKSPACE_FAILED;
    decode_buffer = core_get_data(*decode_handle);
    memset(bitmap, 0, sizeof(*bitmap));
    bitmap->width = width;
    bitmap->height = height;
    bitmap->data = (unsigned char *)decode_buffer;
    crazypod_image_decode_lock();
    if(jpeg)
        result = read_jpeg_file(
            path, bitmap, (int)decode_bytes, format, &format_native);
    else if(path_has_extension(path, ".bmp"))
        result = read_bmp_file(
            path, bitmap, (int)decode_bytes, format, &format_native);
    else
        result = -1;
    crazypod_image_decode_unlock();
    if(result < 0 || bitmap->width <= 0 || bitmap->height <= 0 ||
       bitmap->width > width || bitmap->height > height ||
       bitmap->data == NULL) {
        *decode_handle = core_free(*decode_handle);
        return CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED;
    }
    return CRAZYPOD_WALLPAPER_APPLY_OK;
}

enum crazypod_wallpaper_apply_result
crazypod_wallpaper_crop_engine_render(
    const char *path, const lv_image_dsc_t *preview,
    int crop_x, int crop_y, int crop_width, int crop_height,
    fb_data *destination,
    crazypod_wallpaper_progress_cb progress_cb,
    void *progress_user_data)
{
    struct bitmap bitmap;
    const fb_data *source;
    int preview_width;
    int preview_height;
    int maximum_scale;
    int width_scale;
    int height_scale;
    int decode_scale;
    int decode_width;
    int decode_height;
    int decode_handle = -1;
    int high_x;
    int high_y;
    int high_right;
    int high_bottom;
    int high_width;
    int high_height;
    int y;
    enum crazypod_wallpaper_apply_result result;

    if(preview == NULL || destination == NULL)
        return CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
    preview_width = preview->header.w;
    preview_height = preview->header.h;
    maximum_scale = max_decode_scale(preview_width, preview_height);
    if(maximum_scale < 1)
        return CRAZYPOD_WALLPAPER_APPLY_INVALID_SOURCE;
    width_scale =
        (TARGET_WIDTH * CROP_OVERSAMPLE + crop_width - 1) / crop_width;
    height_scale =
        (TARGET_HEIGHT * CROP_OVERSAMPLE + crop_height - 1) /
        crop_height;
    decode_scale = width_scale > height_scale ?
        width_scale : height_scale;
    if(decode_scale < 1)
        decode_scale = 1;
    if(decode_scale > maximum_scale)
        decode_scale = maximum_scale;
    decode_width = preview_width * decode_scale;
    decode_height = preview_height * decode_scale;
    if(progress_cb != NULL)
        progress_cb(12, progress_user_data);
    result = decode_source(
        path, decode_width, decode_height, &bitmap, &decode_handle);
    if(result != CRAZYPOD_WALLPAPER_APPLY_OK)
        return result;
    if(progress_cb != NULL)
        progress_cb(55, progress_user_data);
    high_x = (int)(((int64_t)crop_x * bitmap.width +
                    preview_width / 2) / preview_width);
    high_y = (int)(((int64_t)crop_y * bitmap.height +
                    preview_height / 2) / preview_height);
    high_right = (int)(((int64_t)(crop_x + crop_width) *
                        bitmap.width + preview_width / 2) /
                       preview_width);
    high_bottom = (int)(((int64_t)(crop_y + crop_height) *
                         bitmap.height + preview_height / 2) /
                        preview_height);
    if(high_x < 0)
        high_x = 0;
    if(high_y < 0)
        high_y = 0;
    if(high_right > bitmap.width)
        high_right = bitmap.width;
    if(high_bottom > bitmap.height)
        high_bottom = bitmap.height;
    high_width = high_right - high_x;
    high_height = high_bottom - high_y;
    if(high_width < TARGET_WIDTH || high_height < TARGET_HEIGHT) {
        core_free(decode_handle);
        return CRAZYPOD_WALLPAPER_APPLY_DECODE_FAILED;
    }
    source = (const fb_data *)bitmap.data;
    for(y = 0; y < TARGET_HEIGHT; ++y) {
        int source_y_q8 = high_y * 256 +
            y * (high_height - 1) * 256 / (TARGET_HEIGHT - 1);
        int x;

        for(x = 0; x < TARGET_WIDTH; ++x) {
            int source_x_q8 = high_x * 256 +
                x * (high_width - 1) * 256 / (TARGET_WIDTH - 1);

            destination[y * TARGET_WIDTH + x] =
                sample_pixel(source, bitmap.width, bitmap.height,
                             bitmap.width, source_x_q8, source_y_q8);
        }
        if(progress_cb != NULL && (y % 48) == 47)
            progress_cb(
                55 + (y + 1) * 35 / TARGET_HEIGHT,
                progress_user_data);
    }
    core_free(decode_handle);
    if(progress_cb != NULL)
        progress_cb(90, progress_user_data);
    return CRAZYPOD_WALLPAPER_APPLY_OK;
}

#endif
