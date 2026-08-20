#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "lcd.h"
#include "system.h"

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "../../crazypod_frameclock.h"
#include "../../crazypod_icons.h"
#include "../../crazypod_image.h"
#include "../../platform/crazypod_platform_display.h"
#include "crazypod_app_catalog.h"
#include "crazypod_desktop_native.h"
#include "crazypod_screenshot_feedback.h"

#define NATIVE_TOP CRAZYPOD_DESKTOP_NATIVE_TOP
#define NATIVE_BOTTOM CRAZYPOD_DESKTOP_NATIVE_BOTTOM
#define NATIVE_HEIGHT (NATIVE_BOTTOM - NATIVE_TOP)
#define NATIVE_ICON_CENTER_X 160
#define NATIVE_ICON_CENTER_Y 91
#define NATIVE_MAX_ICON_SIZE 120

static fb_data backdrop[LCD_WIDTH * NATIVE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data modal_underlay[LCD_WIDTH * NATIVE_HEIGHT]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t modal_underlay_descriptor;
static uint8_t scaled_icons
    [CRAZYPOD_ICON_COUNT]
    [NATIVE_MAX_ICON_SIZE * NATIVE_MAX_ICON_SIZE * 4]
    CACHEALIGN_AT_LEAST_ATTR(16);
static uint16_t sample_x_offset[NATIVE_MAX_ICON_SIZE];
static uint16_t sample_x_next_offset[NATIVE_MAX_ICON_SIZE];
static uint8_t sample_x_fraction[NATIVE_MAX_ICON_SIZE];
static bool scaled_valid[CRAZYPOD_ICON_COUNT];
static int scaled_size;
static bool preserve_modal_underlay;
static bool rendered_bounds_valid;
static int rendered_left;
static int rendered_top;
static int rendered_width;
static int rendered_height;
static bool dirty;
static bool backdrop_ready;
static bool overlay_active;
static int overlay_left;
static int overlay_top;
static int overlay_right;
static int overlay_bottom;

static bool pixel_occluded(int x, int y)
{
    return overlay_active &&
        x >= overlay_left && x < overlay_right &&
        y >= overlay_top && y < overlay_bottom;
}

static void invalidate_scaled_icons(void)
{
    memset(scaled_valid, 0, sizeof(scaled_valid));
}

static int clamp_icon_size(int size)
{
    if(size < 1)
        return 1;
    if(size > NATIVE_MAX_ICON_SIZE)
        return NATIVE_MAX_ICON_SIZE;
    return size;
}

static void icon_bounds(int center_x, int center_y, int size,
                        int *left, int *top,
                        int *width, int *height)
{
    int right;
    int bottom;

    size = clamp_icon_size(size);
    *left = center_x - size / 2;
    *top = center_y - size / 2;
    right = *left + size;
    bottom = *top + size;
    if(*left < 0)
        *left = 0;
    if(*top < NATIVE_TOP)
        *top = NATIVE_TOP;
    if(right > LCD_WIDTH)
        right = LCD_WIDTH;
    if(bottom > NATIVE_BOTTOM)
        bottom = NATIVE_BOTTOM;
    *width = right - *left;
    *height = bottom - *top;
}

static void restore_backdrop_rect(
    fb_data *framebuffer, int left, int top, int width, int height)
{
    int row;

    for(row = 0; row < height; ++row) {
        memcpy(
            framebuffer + (top + row) * LCD_WIDTH + left,
            backdrop + (top + row - NATIVE_TOP) * LCD_WIDTH + left,
            (size_t)width * sizeof(fb_data));
    }
}

void crazypod_desktop_native_reset(void)
{
    dirty = true;
    backdrop_ready = false;
    scaled_size = 0;
    rendered_bounds_valid = false;
    invalidate_scaled_icons();
}

void crazypod_desktop_native_invalidate(bool discard_backdrop)
{
    dirty = true;
    if(discard_backdrop) {
        backdrop_ready = false;
        rendered_bounds_valid = false;
    }
}

void crazypod_desktop_native_invalidate_icons(void)
{
    scaled_size = 0;
    invalidate_scaled_icons();
    dirty = true;
}

lv_obj_t *crazypod_desktop_native_create_modal_underlay(
    lv_obj_t *parent)
{
    const fb_data *framebuffer =
        (const fb_data *)crazypod_platform_display_framebuffer();
    lv_obj_t *image;

    if(parent == NULL || framebuffer == NULL)
        return NULL;
    if(!preserve_modal_underlay) {
        if(modal_underlay_descriptor.header.magic ==
           LV_IMAGE_HEADER_MAGIC)
            lv_image_cache_drop(&modal_underlay_descriptor);
        memcpy(
            modal_underlay,
            framebuffer + NATIVE_TOP * LCD_WIDTH,
            sizeof(modal_underlay));
        if(!crazypod_image_configure_rgb565(
               &modal_underlay_descriptor, modal_underlay,
               LCD_WIDTH, NATIVE_HEIGHT))
            return NULL;
    }
    preserve_modal_underlay = false;
    if(modal_underlay_descriptor.header.magic != LV_IMAGE_HEADER_MAGIC)
        return NULL;
    image = lv_image_create(parent);
    lv_image_set_src(image, &modal_underlay_descriptor);
    lv_obj_set_pos(image, 0, NATIVE_TOP);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_background(image);
    return image;
}

void crazypod_desktop_native_preserve_modal_underlay(void)
{
    preserve_modal_underlay =
        modal_underlay_descriptor.header.magic == LV_IMAGE_HEADER_MAGIC;
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
        rendered_bounds_valid = false;
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
            if(pixel_occluded(px, py))
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

static bool prepare_scaled_icon(int app_index, int size)
{
    const struct crazypod_icon *image = crazypod_icon_get(app_index);
    const uint8_t *source = image != NULL ? image->pixels : NULL;
    uint8_t *scaled;
    int source_width;
    int source_height;
    int source_stride;
    int source_y_q16;
    int source_y_step;
    int source_x_q16;
    int source_x_step;
    int x;
    int y;

    if(app_index < 0 || app_index >= CRAZYPOD_ICON_COUNT ||
       image == NULL || source == NULL)
        return false;
    if(scaled_valid[app_index])
        return true;
    source_width = image->width;
    source_height = image->height;
    source_stride = image->stride;
    scaled = scaled_icons[app_index];
    source_x_step = (source_width << 16) / size;
    source_x_q16 =
        ((source_width << 15) / size) - 32768;
    for(x = 0; x < size; ++x) {
        int sx = source_x_q16 >> 16;

        if(sx < 0)
            sx = 0;
        if(sx >= source_width)
            sx = source_width - 1;
        sample_x_offset[x] = sx * 4;
        sample_x_next_offset[x] =
            (sx + 1 < source_width ? sx + 1 : sx) * 4;
        sample_x_fraction[x] =
            (source_x_q16 >> 8) & 255;
        source_x_q16 += source_x_step;
    }
    source_y_step = (source_height << 16) / size;
    source_y_q16 =
        ((source_height << 15) / size) - 32768;
    for(y = 0; y < size; ++y) {
        int sy = source_y_q16 >> 16;
        int sy1;
        int fraction_y;
        const uint8_t *source_top;
        const uint8_t *source_bottom;
        uint8_t *destination =
            scaled + y * NATIVE_MAX_ICON_SIZE * 4;

        if(sy < 0)
            sy = 0;
        if(sy >= source_height)
            sy = source_height - 1;
        sy1 = sy + 1 < source_height ? sy + 1 : sy;
        fraction_y = (source_y_q16 >> 8) & 255;
        source_top = source + sy * source_stride;
        source_bottom = source + sy1 * source_stride;
        for(x = 0; x < size; ++x) {
            const uint8_t *top_left =
                source_top + sample_x_offset[x];
            const uint8_t *top_right =
                source_top + sample_x_next_offset[x];
            const uint8_t *bottom_left =
                source_bottom + sample_x_offset[x];
            const uint8_t *bottom_right =
                source_bottom + sample_x_next_offset[x];
            int fraction_x = sample_x_fraction[x];
            int channel;

            for(channel = 0; channel < 4; ++channel)
                destination[channel] = (uint8_t)interpolate_channel(
                    top_left[channel], top_right[channel],
                    bottom_left[channel], bottom_right[channel],
                    fraction_x, fraction_y);
            destination += 4;
        }
        source_y_q16 += source_y_step;
    }
    scaled_valid[app_index] = true;
    return true;
}

static void draw_desktop_icon(int app_index, int center_x, int center_y,
                              int size)
{
    fb_data *pixels =
        (fb_data *)crazypod_platform_display_framebuffer();
    const uint8_t *source;
    int left = center_x - size / 2;
    int top = center_y - size / 2;
    int first_x = left < 0 ? -left : 0;
    int last_x = left + size > LCD_WIDTH
        ? LCD_WIDTH - left : size;
    int first_y = top < NATIVE_TOP ? NATIVE_TOP - top : 0;
    int last_y = top + size > NATIVE_BOTTOM
        ? NATIVE_BOTTOM - top : size;
    int y;

    if(!prepare_scaled_icon(app_index, size)) {
        draw_desktop_placeholder(
            app_index, center_x, center_y, size, 255);
        return;
    }
    source = scaled_icons[app_index];
    for(y = first_y; y < last_y; ++y) {
        const uint8_t *source_pixel = source +
            (y * NATIVE_MAX_ICON_SIZE + first_x) * 4;
        fb_data *destination =
            pixels + (top + y) * LCD_WIDTH + left + first_x;
        int x;

        for(x = first_x; x < last_x; ++x) {
            int alpha = source_pixel[3];

            if(alpha > 0 && !pixel_occluded(left + x, top + y))
                *destination = blend_icon_premultiplied(
                    source_pixel[2], source_pixel[1], source_pixel[0],
                    alpha, *destination, 255);
            source_pixel += 4;
            ++destination;
        }
    }
}

bool crazypod_desktop_native_render(
    const int *app_indices, const int *centers_x, int icon_count,
    int icon_size, bool blocked)
{
    fb_data *framebuffer =
        (fb_data *)crazypod_platform_display_framebuffer();
    int left[CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE];
    int top[CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE];
    int width[CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE];
    int height[CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE];
    int dirty_left = LCD_WIDTH;
    int dirty_top = NATIVE_BOTTOM;
    int dirty_right = 0;
    int dirty_bottom = NATIVE_TOP;
    bool any_bounds = false;
    int slot;

    if(blocked || !dirty)
        return false;
    if(icon_count < 0)
        icon_count = 0;
    if(icon_count > CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE)
        icon_count = CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE;
    if(!backdrop_ready) {
        memcpy(
            backdrop,
            framebuffer + NATIVE_TOP * LCD_WIDTH,
            sizeof(backdrop));
        backdrop_ready = true;
        rendered_bounds_valid = false;
    }
    overlay_active = crazypod_screenshot_feedback_bounds(
        &overlay_left, &overlay_top,
        &overlay_right, &overlay_bottom);
    icon_size = clamp_icon_size(icon_size);
    if(scaled_size != icon_size) {
        scaled_size = icon_size;
        invalidate_scaled_icons();
    }
    for(slot = 0; slot < icon_count; ++slot) {
        icon_bounds(
            centers_x[slot], NATIVE_ICON_CENTER_Y, icon_size,
            &left[slot], &top[slot], &width[slot], &height[slot]);
        if(app_indices[slot] < 0 ||
           app_indices[slot] >= CRAZYPOD_ICON_COUNT)
            continue;
        if(left[slot] < dirty_left)
            dirty_left = left[slot];
        if(top[slot] < dirty_top)
            dirty_top = top[slot];
        if(left[slot] + width[slot] > dirty_right)
            dirty_right = left[slot] + width[slot];
        if(top[slot] + height[slot] > dirty_bottom)
            dirty_bottom = top[slot] + height[slot];
        any_bounds = true;
    }
    if(rendered_bounds_valid) {
        int rendered_right = rendered_left + rendered_width;
        int rendered_bottom = rendered_top + rendered_height;

        if(rendered_left < dirty_left)
            dirty_left = rendered_left;
        if(rendered_top < dirty_top)
            dirty_top = rendered_top;
        if(rendered_right > dirty_right)
            dirty_right = rendered_right;
        if(rendered_bottom > dirty_bottom)
            dirty_bottom = rendered_bottom;
        any_bounds = true;
    }
    if(!any_bounds) {
        dirty = false;
        return false;
    }
    restore_backdrop_rect(
        framebuffer, dirty_left, dirty_top,
        dirty_right - dirty_left, dirty_bottom - dirty_top);
    for(slot = 0; slot < icon_count; ++slot) {
        int app_index = app_indices[slot];

        if(app_index < 0 || app_index >= CRAZYPOD_ICON_COUNT)
            continue;
        draw_desktop_icon(
            app_index, centers_x[slot],
            NATIVE_ICON_CENTER_Y, icon_size);
    }
    rendered_bounds_valid = true;
    rendered_left = dirty_left;
    rendered_top = dirty_top;
    rendered_width = dirty_right - dirty_left;
    rendered_height = dirty_bottom - dirty_top;
    crazypod_present_queue_rect(
        dirty_left, dirty_top,
        dirty_right - dirty_left,
        dirty_bottom - dirty_top);
    dirty = false;
    return true;
}

#endif
