#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "lcd.h"
#include "system.h"

#include "../../crazypod_apps.h"
#include "../../crazypod_frameclock.h"
#include "../../crazypod_icons.h"
#include "../../platform/crazypod_platform_display.h"
#include "crazypod_app_catalog.h"
#include "crazypod_desktop_native.h"

#define NATIVE_TOP 40
#define NATIVE_BOTTOM 143
#define NATIVE_HEIGHT (NATIVE_BOTTOM - NATIVE_TOP)

static fb_data backdrop[LCD_WIDTH * NATIVE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static bool dirty;
static bool backdrop_ready;

static int interpolate_pose(const int *values, int absolute_q8)
{
    int segment = absolute_q8 / 256;
    int fraction = absolute_q8 & 255;

    if(segment >= 3)
        return values[3];
    return values[segment] +
           (values[segment + 1] - values[segment]) * fraction / 256;
}

void crazypod_desktop_native_reset(void)
{
    dirty = true;
    backdrop_ready = false;
}

void crazypod_desktop_native_invalidate(bool discard_backdrop)
{
    dirty = true;
    if(discard_backdrop)
        backdrop_ready = false;
}

void crazypod_desktop_native_capture_flush(const lv_area_t *area)
{
    fb_data *framebuffer;
    int left;
    int right;
    int top;
    int bottom;
    int width;
    int y;

    if(area == NULL || area->y1 >= NATIVE_BOTTOM ||
       area->y2 < NATIVE_TOP) {
        return;
    }
    if(backdrop_ready) {
        framebuffer = (fb_data *)crazypod_platform_display_framebuffer();
        left = area->x1 < 0 ? 0 : area->x1;
        right = area->x2 >= LCD_WIDTH ? LCD_WIDTH - 1 : area->x2;
        top = area->y1 < NATIVE_TOP ? NATIVE_TOP : area->y1;
        bottom = area->y2 >= NATIVE_BOTTOM
            ? NATIVE_BOTTOM - 1 : area->y2;
        width = right - left + 1;
        for(y = top; y <= bottom; ++y) {
            memcpy(
                backdrop + (y - NATIVE_TOP) * LCD_WIDTH + left,
                framebuffer + y * LCD_WIDTH + left,
                (size_t)width * sizeof(fb_data));
        }
    }
    dirty = true;
}

static fb_data desktop_blend565(fb_data foreground, fb_data background,
                                int alpha)
{
    int fr = RGB_UNPACK_RED(foreground);
    int fg = RGB_UNPACK_GREEN(foreground);
    int fb = RGB_UNPACK_BLUE(foreground);
    int br = RGB_UNPACK_RED(background);
    int bg = RGB_UNPACK_GREEN(background);
    int bb = RGB_UNPACK_BLUE(background);

    return LCD_RGBPACK(
        (fr * alpha + br * (256 - alpha)) >> 8,
        (fg * alpha + bg * (256 - alpha)) >> 8,
        (fb * alpha + bb * (256 - alpha)) >> 8);
}

static void draw_desktop_placeholder(int app_index, int center_x,
                                     int center_y, int size, int opacity)
{
    fb_data *pixels =
        (fb_data *)crazypod_platform_display_framebuffer();
    const struct crazypod_app_descriptor *app =
        crazypod_app_catalog_at(app_index);
    uint32_t rgb = app != NULL ? app->color : 0x59606B;
    fb_data color = LCD_RGBPACK((rgb >> 16) & 0xff,
                                (rgb >> 8) & 0xff,
                                rgb & 0xff);
    int left = center_x - size / 2;
    int top = center_y - size / 2;
    int y;

    for(y = 0; y < size; ++y) {
        int py = top + y;
        int x;
        if(py < NATIVE_TOP ||
           py >= NATIVE_BOTTOM)
            continue;
        for(x = 0; x < size; ++x) {
            int px = left + x;
            fb_data *destination;
            if(px < 0 || px >= LCD_WIDTH)
                continue;
            destination = pixels + py * LCD_WIDTH + px;
            *destination = desktop_blend565(
                color, *destination, opacity);
        }
    }
}

static inline void sample_icon_bilinear(
    const uint8_t *source, int source_width, int source_height,
    int source_stride, int source_x_q16, int source_y_q16,
    uint8_t *filtered)
{
    const uint8_t *samples[4];
    uint32_t weights[4];
    int sx = source_x_q16 >> 16;
    int sy = source_y_q16 >> 16;
    int sx1;
    int sy1;
    int fx;
    int fy;
    int channel;

    if(sx < 0)
        sx = 0;
    if(sy < 0)
        sy = 0;
    if(sx >= source_width)
        sx = source_width - 1;
    if(sy >= source_height)
        sy = source_height - 1;
    sx1 = sx + 1 < source_width ? sx + 1 : sx;
    sy1 = sy + 1 < source_height ? sy + 1 : sy;
    fx = (source_x_q16 >> 8) & 255;
    fy = (source_y_q16 >> 8) & 255;
    weights[0] = (uint32_t)(256 - fx) * (256 - fy);
    weights[1] = (uint32_t)fx * (256 - fy);
    weights[2] = (uint32_t)(256 - fx) * fy;
    weights[3] = (uint32_t)fx * fy;
    samples[0] = source + sy * source_stride + sx * 4;
    samples[1] = source + sy * source_stride + sx1 * 4;
    samples[2] = source + sy1 * source_stride + sx * 4;
    samples[3] = source + sy1 * source_stride + sx1 * 4;
    for(channel = 0; channel < 4; ++channel) {
        uint32_t sum =
            samples[0][channel] * weights[0] +
            samples[1][channel] * weights[1] +
            samples[2][channel] * weights[2] +
            samples[3][channel] * weights[3];
        filtered[channel] = sum >> 16;
    }
}

static inline fb_data blend_icon_premultiplied(
    const uint8_t *color, fb_data background, int opacity)
{
    int scale = opacity + 1;
    int alpha = color[3] * scale >> 8;
    int red = color[2] * scale >> 8;
    int green = color[1] * scale >> 8;
    int blue = color[0] * scale >> 8;
    int inverse = 256 - alpha;

    red += RGB_UNPACK_RED(background) * inverse >> 8;
    green += RGB_UNPACK_GREEN(background) * inverse >> 8;
    blue += RGB_UNPACK_BLUE(background) * inverse >> 8;
    return LCD_RGBPACK(red, green, blue);
}

static void draw_desktop_icon(int app_index, int center_x, int center_y,
                              int size, int opacity)
{
    const struct crazypod_icon *image = crazypod_icon_get(app_index);
    const uint8_t *source = image != NULL ? image->pixels : NULL;
    fb_data *pixels =
        (fb_data *)crazypod_platform_display_framebuffer();
    int source_width;
    int source_height;
    int source_stride;
    int left = center_x - size / 2;
    int top = center_y - size / 2;
    int source_y_q16;
    int source_y_step;
    int y;

    if(image == NULL || source == NULL) {
        draw_desktop_placeholder(app_index, center_x, center_y,
                                 size, opacity);
        return;
    }
    source_width = image->width;
    source_height = image->height;
    source_stride = image->stride;
    source_y_step = (source_height << 16) / size;
    source_y_q16 =
        ((source_height << 15) / size) - 32768;
    for(y = 0; y < size; ++y) {
        int py = top + y;
        int source_x_q16 =
            ((source_width << 15) / size) - 32768;
        int source_x_step = (source_width << 16) / size;
        int x;
        if(py < NATIVE_TOP ||
           py >= NATIVE_BOTTOM) {
            source_y_q16 += source_y_step;
            continue;
        }
        for(x = 0; x < size; ++x) {
            int px = left + x;
            uint8_t filtered[4];
            fb_data *destination;
            if(px >= 0 && px < LCD_WIDTH) {
                sample_icon_bilinear(
                    source, source_width, source_height,
                    source_stride, source_x_q16,
                    source_y_q16, filtered);
                if(filtered[3] > 0) {
                    destination =
                        pixels + py * LCD_WIDTH + px;
                    *destination =
                        blend_icon_premultiplied(
                            filtered, *destination, opacity);
                }
            }
            source_x_q16 += source_x_step;
        }
        source_y_q16 += source_y_step;
    }

}

void crazypod_desktop_native_render(
    int position_q8, int base_size, bool blocked)
{
    static const int size_percent[] = { 100, 72, 56, 44 };
    fb_data *framebuffer =
        (fb_data *)crazypod_platform_display_framebuffer();
    int distance_layer;
    int row;

    if(blocked || !dirty)
        return;
    if(!backdrop_ready) {
        for(row = 0; row < NATIVE_HEIGHT; ++row) {
            memcpy(
                backdrop + row * LCD_WIDTH,
                framebuffer + (NATIVE_TOP + row) * LCD_WIDTH,
                LCD_WIDTH * sizeof(fb_data));
        }
        backdrop_ready = true;
    }
    for(row = 0; row < NATIVE_HEIGHT; ++row) {
        memcpy(
            framebuffer + (NATIVE_TOP + row) * LCD_WIDTH,
            backdrop + row * LCD_WIDTH,
            LCD_WIDTH * sizeof(fb_data));
    }
    for(distance_layer = 2; distance_layer >= 0; --distance_layer) {
        int i;

        for(i = 0; i < crazypod_apps_visible_count(); ++i) {
            int catalog_index = crazypod_app_catalog_index(
                crazypod_apps_visible_id(i));
            int distance_q8 = i * 256 - position_q8;
            int absolute_q8 =
                distance_q8 < 0 ? -distance_q8 : distance_q8;
            int rounded_distance = (absolute_q8 + 128) / 256;
            int direction = distance_q8 < 0 ? -1 : 1;
            int center_x;
            int center_y;
            int icon_size;

            if(absolute_q8 > 640 ||
               rounded_distance != distance_layer)
                continue;
            center_x = 160 + direction *
                (absolute_q8 <= 256
                    ? 92 * absolute_q8 / 256
                    : 92 + 46 * (absolute_q8 - 256) / 256);
            center_y = 91 + 4 * absolute_q8 / 256;
            icon_size = base_size *
                interpolate_pose(size_percent, absolute_q8) / 100;
            if(catalog_index >= 0)
                draw_desktop_icon(
                    catalog_index, center_x, center_y, icon_size, 255);
        }
    }
    crazypod_present_queue_rect(
        0, NATIVE_TOP, LCD_WIDTH, NATIVE_HEIGHT);
    dirty = false;
}

#endif
