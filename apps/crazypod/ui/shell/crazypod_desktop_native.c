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
#define NATIVE_MAX_ICON_SIZE LCD_WIDTH

static fb_data backdrop[LCD_WIDTH * NATIVE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static uint16_t sample_x_offset[NATIVE_MAX_ICON_SIZE];
static uint16_t sample_x_next_offset[NATIVE_MAX_ICON_SIZE];
static uint8_t sample_x_fraction[NATIVE_MAX_ICON_SIZE];
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

static FORCE_INLINE int interpolate_channel(
    int top_left, int top_right,
    int bottom_left, int bottom_right,
    int fraction_x, int fraction_y)
{
    int top_q8 =
        (top_left << 8) +
        (top_right - top_left) * fraction_x;
    int bottom_q8 =
        (bottom_left << 8) +
        (bottom_right - bottom_left) * fraction_x;

    return ((top_q8 << 8) +
            (bottom_q8 - top_q8) * fraction_y) >> 16;
}

static inline fb_data blend_icon_premultiplied(
    int red, int green, int blue, int alpha,
    fb_data background, int opacity)
{
    int scale = opacity + 1;
    int inverse;

    if(opacity == 255 && alpha == 255)
        return LCD_RGBPACK(red, green, blue);
    alpha = alpha * scale >> 8;
    red = red * scale >> 8;
    green = green * scale >> 8;
    blue = blue * scale >> 8;
    inverse = 256 - alpha;
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
    int left;
    int top;
    int first_x;
    int last_x;
    int first_y;
    int last_y;
    int source_y_q16;
    int source_y_step;
    int source_x_q16;
    int source_x_step;
    int y;

    if(image == NULL || source == NULL) {
        draw_desktop_placeholder(app_index, center_x, center_y,
                                 size, opacity);
        return;
    }
    source_width = image->width;
    source_height = image->height;
    source_stride = image->stride;
    if(size > NATIVE_MAX_ICON_SIZE)
        size = NATIVE_MAX_ICON_SIZE;
    left = center_x - size / 2;
    top = center_y - size / 2;
    first_x = left < 0 ? -left : 0;
    last_x =
        left + size > LCD_WIDTH
            ? LCD_WIDTH - left : size;
    first_y = top < NATIVE_TOP
        ? NATIVE_TOP - top : 0;
    last_y = top + size > NATIVE_BOTTOM
        ? NATIVE_BOTTOM - top : size;
    if(first_x >= last_x || first_y >= last_y)
        return;
    source_x_step = (source_width << 16) / size;
    source_x_q16 =
        ((source_width << 15) / size) - 32768;
    for(y = 0; y < size; ++y) {
        int sx = source_x_q16 >> 16;

        if(sx < 0)
            sx = 0;
        if(sx >= source_width)
            sx = source_width - 1;
        sample_x_offset[y] = sx * 4;
        sample_x_next_offset[y] =
            (sx + 1 < source_width ? sx + 1 : sx) * 4;
        sample_x_fraction[y] =
            (source_x_q16 >> 8) & 255;
        source_x_q16 += source_x_step;
    }
    source_y_step = (source_height << 16) / size;
    source_y_q16 =
        ((source_height << 15) / size) - 32768 +
        first_y * source_y_step;
    for(y = first_y; y < last_y; ++y) {
        int py = top + y;
        int sy = source_y_q16 >> 16;
        int sy1;
        int fraction_y;
        const uint8_t *source_top;
        const uint8_t *source_bottom;
        fb_data *destination =
            pixels + py * LCD_WIDTH + left + first_x;
        int x;

        if(sy < 0)
            sy = 0;
        if(sy >= source_height)
            sy = source_height - 1;
        sy1 = sy + 1 < source_height ? sy + 1 : sy;
        fraction_y = (source_y_q16 >> 8) & 255;
        source_top = source + sy * source_stride;
        source_bottom = source + sy1 * source_stride;
        for(x = first_x; x < last_x; ++x) {
            const uint8_t *top_left =
                source_top + sample_x_offset[x];
            const uint8_t *top_right =
                source_top + sample_x_next_offset[x];
            const uint8_t *bottom_left =
                source_bottom + sample_x_offset[x];
            const uint8_t *bottom_right =
                source_bottom + sample_x_next_offset[x];
            int fraction_x = sample_x_fraction[x];
            int alpha = interpolate_channel(
                top_left[3], top_right[3],
                bottom_left[3], bottom_right[3],
                fraction_x, fraction_y);

            if(alpha > 0) {
                int blue = interpolate_channel(
                    top_left[0], top_right[0],
                    bottom_left[0], bottom_right[0],
                    fraction_x, fraction_y);
                int green = interpolate_channel(
                    top_left[1], top_right[1],
                    bottom_left[1], bottom_right[1],
                    fraction_x, fraction_y);
                int red = interpolate_channel(
                    top_left[2], top_right[2],
                    bottom_left[2], bottom_right[2],
                    fraction_x, fraction_y);

                *destination =
                    blend_icon_premultiplied(
                        red, green, blue, alpha,
                        *destination, opacity);
            }
            ++destination;
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

    if(blocked || !dirty)
        return;
    if(!backdrop_ready) {
        memcpy(
            backdrop,
            framebuffer + NATIVE_TOP * LCD_WIDTH,
            sizeof(backdrop));
        backdrop_ready = true;
    }
    memcpy(
        framebuffer + NATIVE_TOP * LCD_WIDTH,
        backdrop, sizeof(backdrop));
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
