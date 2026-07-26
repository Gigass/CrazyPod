#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "kernel.h"
#include "lcd.h"
#include "string-extra.h"

#include "lvgl.h"

#include "crazypod_artwork.h"
#include "crazypod_coverflow.h"
#include "crazypod_frameclock.h"
#include "crazypod_music.h"

#define FLOW_CACHE_SLOTS 25
#define FLOW_PREFETCH_AHEAD 12
#define FLOW_PREFETCH_BEHIND 12
#define FLOW_COVER_SIZE CRAZYPOD_COVERFLOW_ARTWORK_SIZE
#define FLOW_TOP 40
#define FLOW_BOTTOM 194
#define FLOW_CENTER_Y 106
#define FLOW_POSITION_ONE (1L << 16)
#define FLOW_RELEASE_STIFFNESS 400
#define FLOW_RELEASE_DAMPING 40
#define FLOW_RELEASE_MOMENTUM_DIVISOR 3
#define FLOW_RELEASE_GRACE_TICKS \
    (((HZ * 6) / 100) > 0 ? ((HZ * 6) / 100) : 1)
#define FLOW_INPUT_BASE_SPEED 2
#define FLOW_INPUT_RESPONSE 24
#define FLOW_SNAP_POSITION (FLOW_POSITION_ONE / 512)
#define FLOW_SNAP_VELOCITY (FLOW_POSITION_ONE / 20)
#define FLOW_PREFETCH_TICKS ((HZ / 10) > 0 ? (HZ / 10) : 1)
#define FLOW_CAMERA_DISTANCE 400
#define FLOW_SIDE_ANGLE 60
#define FLOW_SIDE_OFFSET 108
#define FLOW_SIDE_SPACING 65
#define FLOW_VISIBLE_DISTANCE_Q16 (FLOW_POSITION_ONE * 7 / 2)

struct flow_cache_entry {
    int album_index;
    const lv_image_dsc_t *image;
};

static struct flow_cache_entry cache[FLOW_CACHE_SLOTS];
static bool flow_active;
static bool flow_dirty;
static bool prefetch_pending;
static int selected_album;
static int32_t position_q16;
static int32_t target_position_q16;
static int32_t velocity_q16;
static int32_t input_velocity_q16;
static int flow_direction;
static int gesture_min_album;
static int prefetched_visual_album;
static long last_physics;
static long last_prefetch;
static long last_input;
static struct crazypod_frameclock render_clock;
static unsigned artwork_generation_seen;
static bool cache_initialized;
static bool input_active;

extern struct frame_buffer_t lcd_framebuffer_default;

static fb_data *framebuffer(void)
{
    return (fb_data *)lcd_framebuffer_default.data;
}

static fb_data blend565(fb_data foreground, fb_data background, int alpha)
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

static uint32_t album_color(int index)
{
    static const uint32_t colors[] = {
        0x6D4AFF, 0x1976D2, 0xD13E68, 0xD87822,
        0x168E7A, 0x514BC3, 0x9B367F, 0x24677D
    };
    return colors[(unsigned)index %
                  (sizeof(colors) / sizeof(colors[0]))];
}

static void clear_flow_area(void)
{
    fb_data *pixels = framebuffer();
    fb_data color = LCD_RGBPACK(0, 0, 0);
    int y;

    for(y = FLOW_TOP; y < FLOW_BOTTOM; ++y)
        memset16(pixels + y * LCD_WIDTH, color, LCD_WIDTH);
}

static const lv_image_dsc_t *cached_image(int album_index)
{
    int i;
    for(i = 0; i < FLOW_CACHE_SLOTS; ++i) {
        if(cache[i].album_index == album_index)
            return cache[i].image;
    }
    return NULL;
}

static bool index_is_wanted(int index)
{
    int visual_album =
        (position_q16 + FLOW_POSITION_ONE / 2) >> 16;

    return index >= visual_album - FLOW_PREFETCH_BEHIND &&
           index <= visual_album + FLOW_PREFETCH_AHEAD;
}

static struct flow_cache_entry *cache_entry_for(int album_index)
{
    int i;
    for(i = 0; i < FLOW_CACHE_SLOTS; ++i) {
        if(cache[i].album_index == album_index)
            return &cache[i];
    }
    return NULL;
}

static struct flow_cache_entry *cache_slot_for(int album_index)
{
    struct flow_cache_entry *farthest = NULL;
    int farthest_distance = -1;
    int visual_album =
        (position_q16 + FLOW_POSITION_ONE / 2) >> 16;
    int i;

    for(i = 0; i < FLOW_CACHE_SLOTS; ++i) {
        int distance;
        if(cache[i].album_index < 0)
            return &cache[i];
        if(!index_is_wanted(cache[i].album_index))
            return &cache[i];
        {
            int visual_distance =
                cache[i].album_index - visual_album;

            if(visual_distance < 0)
                visual_distance = -visual_distance;
            distance = visual_distance;
        }
        if(distance > farthest_distance) {
            farthest = &cache[i];
            farthest_distance = distance;
        }
    }
    (void)album_index;
    return farthest;
}

static void append_prefetch_candidate(int *order, int *count,
                                      int candidate)
{
    int i;

    if(*count >= FLOW_CACHE_SLOTS)
        return;
    for(i = 0; i < *count; ++i) {
        if(order[i] == candidate)
            return;
    }
    order[(*count)++] = candidate;
}

static bool prefetch_covers(void)
{
    int count = crazypod_music_album_count();
    int direction = flow_direction == 0 ? 1 : flow_direction;
    int visual_album =
        (position_q16 + FLOW_POSITION_ONE / 2) >> 16;
    int order[FLOW_CACHE_SLOTS];
    int order_count = 0;
    int distance;
    int candidate_number;
    bool complete = true;

    prefetched_visual_album = visual_album;
    append_prefetch_candidate(order, &order_count, visual_album);
    for(distance = 1; distance <= FLOW_PREFETCH_AHEAD; ++distance) {
        int ahead = visual_album + direction * distance;

        append_prefetch_candidate(order, &order_count, ahead);
    }
    for(distance = 1; distance <= FLOW_PREFETCH_BEHIND; ++distance) {
        int behind = visual_album - direction * distance;

        append_prefetch_candidate(order, &order_count, behind);
    }
    for(candidate_number = 0;
        candidate_number < order_count;
        ++candidate_number) {
        int album_index = order[candidate_number];
        struct flow_cache_entry *entry;
        const struct crazypod_track *track;
        const lv_image_dsc_t *image;
        int slot;

        if(album_index < 0 || album_index >= count)
            continue;
        entry = cache_entry_for(album_index);
        if(entry == NULL) {
            entry = cache_slot_for(album_index);
            if(entry == NULL)
                continue;
            entry->album_index = album_index;
            entry->image = NULL;
        }
        slot = (int)(entry - cache);
        track = crazypod_music_album_track(album_index, 0);
        image = track != NULL
            ? crazypod_artwork_load_cached_priority(
                slot, track, FLOW_COVER_SIZE, candidate_number)
            : NULL;
        if(track != NULL && image == NULL &&
           crazypod_artwork_state(slot, track, FLOW_COVER_SIZE) ==
               CRAZYPOD_ARTWORK_PENDING)
            complete = false;
        if(image != NULL && entry->image != image) {
            entry->image = image;
            flow_dirty = true;
        }
    }
    return complete;
}

static void reset_cache(void)
{
    int i;

    for(i = 0; i < FLOW_CACHE_SLOTS; ++i) {
        cache[i].album_index = -1;
        cache[i].image = NULL;
    }
    cache_initialized = true;
}

static fb_data placeholder_sample(int album_index, int x, int y,
                                  int width, int height)
{
    uint32_t rgb = album_color(album_index);
    int red = (rgb >> 16) & 0xff;
    int green = (rgb >> 8) & 0xff;
    int blue = rgb & 0xff;
    int gradient;
    int dx;
    int dy;
    int radius_squared;
    int outer_radius = width * 27 / 100;
    int inner_radius = width * 8 / 100;
    fb_data color;

    if(x < 0)
        x = 0;
    if(x >= width)
        x = width - 1;
    if(y < 0)
        y = 0;
    if(y >= height)
        y = height - 1;
    dx = x - width / 2;
    dy = y - height / 2;
    radius_squared = dx * dx + dy * dy;
    gradient = 188 + y * 52 / (height > 1 ? height - 1 : 1);
    color = LCD_RGBPACK(red * gradient >> 8,
                        green * gradient >> 8,
                        blue * gradient >> 8);
    if(x < 3 || x >= width - 3 || y < 3 || y >= height - 3)
        return blend565(LCD_RGBPACK(245, 245, 248), color, 104);
    if(radius_squared <= outer_radius * outer_radius) {
        fb_data disc = LCD_RGBPACK(24, 24, 30);

        if(radius_squared <= inner_radius * inner_radius)
            disc = LCD_RGBPACK(222, 222, 228);
        return blend565(disc, color, 218);
    }
    return color;
}

static int smoothstep_q8(int value_q8)
{
    int squared_q8;

    if(value_q8 <= 0)
        return 0;
    if(value_q8 >= 256)
        return 256;
    squared_q8 = value_q8 * value_q8 >> 8;
    return squared_q8 * (768 - 2 * value_q8) >> 8;
}

static inline fb_data sample_rgb565_bilinear(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x_q16, int source_y_q16)
{
    int max_x_q16 = (source_width - 1) << 16;
    int max_y_q16 = (source_height - 1) << 16;
    int sx;
    int sy;
    int sx1;
    int sy1;
    int fx;
    int fy;
    fb_data c00;
    fb_data c10;
    fb_data c01;
    fb_data c11;
    int red0;
    int red1;
    int green0;
    int green1;
    int blue0;
    int blue1;

    if(source_x_q16 < 0)
        source_x_q16 = 0;
    if(source_y_q16 < 0)
        source_y_q16 = 0;
    if(source_x_q16 > max_x_q16)
        source_x_q16 = max_x_q16;
    if(source_y_q16 > max_y_q16)
        source_y_q16 = max_y_q16;

    sx = source_x_q16 >> 16;
    sy = source_y_q16 >> 16;
    sx1 = sx + 1 < source_width ? sx + 1 : sx;
    sy1 = sy + 1 < source_height ? sy + 1 : sy;
    fx = (source_x_q16 >> 8) & 255;
    fy = (source_y_q16 >> 8) & 255;
    c00 = source[sy * source_stride + sx];
    c10 = source[sy * source_stride + sx1];
    c01 = source[sy1 * source_stride + sx];
    c11 = source[sy1 * source_stride + sx1];

    red0 = RGB_UNPACK_RED(c00) +
        ((RGB_UNPACK_RED(c10) - RGB_UNPACK_RED(c00)) * fx >> 8);
    red1 = RGB_UNPACK_RED(c01) +
        ((RGB_UNPACK_RED(c11) - RGB_UNPACK_RED(c01)) * fx >> 8);
    green0 = RGB_UNPACK_GREEN(c00) +
        ((RGB_UNPACK_GREEN(c10) - RGB_UNPACK_GREEN(c00)) * fx >> 8);
    green1 = RGB_UNPACK_GREEN(c01) +
        ((RGB_UNPACK_GREEN(c11) - RGB_UNPACK_GREEN(c01)) * fx >> 8);
    blue0 = RGB_UNPACK_BLUE(c00) +
        ((RGB_UNPACK_BLUE(c10) - RGB_UNPACK_BLUE(c00)) * fx >> 8);
    blue1 = RGB_UNPACK_BLUE(c01) +
        ((RGB_UNPACK_BLUE(c11) - RGB_UNPACK_BLUE(c01)) * fx >> 8);

    return LCD_RGBPACK(
        red0 + ((red1 - red0) * fy >> 8),
        green0 + ((green1 - green0) * fy >> 8),
        blue0 + ((blue1 - blue0) * fy >> 8));
}

static fb_data shade565(fb_data color, int shade)
{
    return LCD_RGBPACK(
        RGB_UNPACK_RED(color) * shade >> 8,
        RGB_UNPACK_GREEN(color) * shade >> 8,
        RGB_UNPACK_BLUE(color) * shade >> 8);
}

static int32_t projected_x_q16(int center_x, int u,
                               int sin_q15, int cos_q15)
{
    int denominator =
        FLOW_CAMERA_DISTANCE * 32768 - u * sin_q15;

    if(denominator <= 0)
        return center_x << 16;
    return (center_x << 16) + (int32_t)(
        ((int64_t)u * cos_q15 * FLOW_CAMERA_DISTANCE *
         FLOW_POSITION_ONE) / denominator);
}

static int column_coverage_alpha_q16(int x, int32_t left_q16,
                                     int32_t right_q16)
{
    int32_t pixel_left_q16 = x << 16;
    int32_t pixel_right_q16 = pixel_left_q16 + FLOW_POSITION_ONE;
    int32_t cover_left_q16 =
        pixel_left_q16 > left_q16 ? pixel_left_q16 : left_q16;
    int32_t cover_right_q16 =
        pixel_right_q16 < right_q16 ? pixel_right_q16 : right_q16;
    int32_t coverage_q16 = cover_right_q16 - cover_left_q16;

    if(coverage_q16 <= 0)
        return 0;
    if(coverage_q16 >= FLOW_POSITION_ONE)
        return 255;
    return (int)((coverage_q16 * 255 + FLOW_POSITION_ONE / 2) >> 16);
}

static void draw_projected_image(const lv_image_dsc_t *image,
                                 int album_index,
                                 int center_x, int center_y,
                                 int size, int yaw, int alpha,
                                 int side, bool bilinear)
{
    const fb_data *source;
    fb_data *pixels = framebuffer();
    int source_width;
    int source_height;
    int source_stride;
    int half_size = size / 2;
    int sin_q15 = lv_trigo_sin(yaw);
    int cos_q15 = lv_trigo_cos(yaw);
    int32_t x0_q16 = projected_x_q16(
        center_x, -half_size, sin_q15, cos_q15);
    int32_t x1_q16 = projected_x_q16(
        center_x, half_size, sin_q15, cos_q15);
    int32_t left_q16 = x0_q16 < x1_q16 ? x0_q16 : x1_q16;
    int32_t right_q16 = x0_q16 > x1_q16 ? x0_q16 : x1_q16;
    int left = left_q16 >> 16;
    int right = (right_q16 + FLOW_POSITION_ONE - 1) >> 16;
    int abs_sin_q15 = sin_q15 < 0 ? -sin_q15 : sin_q15;
    int32_t near_depth_q16 =
        (int32_t)half_size * abs_sin_q15 * 2;
    int32_t height_normalizer_q16 =
        (FLOW_CAMERA_DISTANCE << 16) - near_depth_q16;
    bool placeholder =
        image == NULL || image->header.cf != LV_COLOR_FORMAT_RGB565;
    int px;

    if(placeholder) {
        source = NULL;
        source_width = FLOW_COVER_SIZE;
        source_height = FLOW_COVER_SIZE;
        source_stride = 0;
    }
    else {
        source = (const fb_data *)image->data;
        source_width = image->header.w;
        source_height = image->header.h;
        source_stride = image->header.stride / sizeof(fb_data);
    }

    if(left < 0)
        left = 0;
    if(right > LCD_WIDTH)
        right = LCD_WIDTH;
    for(px = left; px < right; ++px) {
        int edge_alpha =
            column_coverage_alpha_q16(px, left_q16, right_q16);
        int32_t screen_x_q16 =
            (px << 16) + FLOW_POSITION_ONE / 2 -
            (center_x << 16);
        int64_t inverse_denominator =
            (int64_t)FLOW_CAMERA_DISTANCE * cos_q15 +
            (((int64_t)screen_x_q16 * sin_q15) >> 16);
        int32_t u_q16;
        int32_t depth_q16;
        int32_t depth_denominator_q16;
        int source_x_q16;
        int column_height;
        int top;
        int source_y_q16;
        int source_y_step;
        int source_position;
        int shade;
        int draw_alpha;
        int y;

        if(edge_alpha <= 0 || inverse_denominator <= 0)
            continue;
        u_q16 = (int32_t)(
            ((int64_t)screen_x_q16 * FLOW_CAMERA_DISTANCE *
             32768) / inverse_denominator);
        if(u_q16 < -(half_size << 16) ||
           u_q16 > (half_size << 16))
            continue;
        source_x_q16 = (int)(
            ((int64_t)(u_q16 + (half_size << 16)) *
             source_width) / size);
        if(source_x_q16 < 0)
            source_x_q16 = 0;
        if(source_x_q16 >= (source_width << 16))
            source_x_q16 = (source_width << 16) - 1;

        depth_q16 =
            (int32_t)(((int64_t)u_q16 * sin_q15) >> 15);
        depth_denominator_q16 =
            (FLOW_CAMERA_DISTANCE << 16) - depth_q16;
        if(depth_denominator_q16 <= 0)
            continue;
        column_height = (int)(
            (int64_t)size * height_normalizer_q16 /
            depth_denominator_q16);
        if(column_height < 1)
            column_height = 1;
        top = center_y - column_height / 2;
        source_y_step = (source_height << 16) / column_height;
        source_y_q16 = source_y_step / 2 - 32768;
        source_position =
            (int)(((int64_t)source_x_q16 * 255) /
                  (source_width << 16));
        if(side < 0)
            source_position = 255 - source_position;
        shade = side == 0 ? 256 : 184 + source_position * 60 / 255;
        draw_alpha = alpha;
        if(edge_alpha < 255)
            draw_alpha = (draw_alpha * edge_alpha + 127) / 255;

        for(y = 0; y < column_height; ++y) {
            int py = top + y;
            fb_data *destination;
            fb_data color;

            if(py < FLOW_TOP || py >= FLOW_BOTTOM) {
                source_y_q16 += source_y_step;
                continue;
            }
            if(placeholder) {
                color = placeholder_sample(
                    album_index, source_x_q16 >> 16,
                    source_y_q16 >> 16,
                    source_width, source_height);
            }
            else if(bilinear) {
                color = sample_rgb565_bilinear(
                    source, source_width, source_height,
                    source_stride, source_x_q16, source_y_q16);
            }
            else {
                int sx = source_x_q16 >> 16;
                int sy = source_y_q16 >> 16;

                if(sy < 0)
                    sy = 0;
                if(sy >= source_height)
                    sy = source_height - 1;
                color = source[sy * source_stride + sx];
            }
            source_y_q16 += source_y_step;
            if(shade != 256)
                color = shade565(color, shade);
            destination = pixels + py * LCD_WIDTH + px;
            *destination = draw_alpha == 255
                ? color : blend565(color, *destination, draw_alpha);
        }

        source_y_q16 = (source_height - 1) << 16;
        source_y_step = (source_height << 16) / 18;
        for(y = 0; y < 9; ++y) {
            int py = top + column_height + 2 + y;
            int reflection_alpha = draw_alpha * (27 - y * 3) >> 8;
            fb_data *destination;
            fb_data color;

            source_y_q16 -= source_y_step;
            if(py < FLOW_TOP || py >= FLOW_BOTTOM ||
               reflection_alpha <= 0)
                continue;
            if(placeholder) {
                color = placeholder_sample(
                    album_index, source_x_q16 >> 16,
                    source_y_q16 >> 16,
                    source_width, source_height);
            }
            else if(bilinear) {
                color = sample_rgb565_bilinear(
                    source, source_width, source_height,
                    source_stride, source_x_q16, source_y_q16);
            }
            else {
                int sx = source_x_q16 >> 16;
                int sy = source_y_q16 >> 16;

                if(sy < 0)
                    sy = 0;
                if(sy >= source_height)
                    sy = source_height - 1;
                color = source[sy * source_stride + sx];
            }
            if(shade != 256)
                color = shade565(color, shade);
            destination = pixels + py * LCD_WIDTH + px;
            *destination = blend565(
                color, *destination, reflection_alpha);
        }
    }
}

static void draw_album(int album_index)
{
    int32_t relative_q16 =
        album_index * FLOW_POSITION_ONE - position_q16;
    int32_t absolute_q16 =
        relative_q16 < 0 ? -relative_q16 : relative_q16;
    int direction = relative_q16 < 0 ? -1 : 1;
    int center_x;
    int size;
    int yaw;
    int alpha;
    bool bilinear;
    const lv_image_dsc_t *image;

    if(absolute_q16 > FLOW_VISIBLE_DISTANCE_Q16)
        return;
    if(absolute_q16 <= FLOW_POSITION_ONE) {
        int phase_q8 = (int)(absolute_q16 >> 8);
        int pose_q8 = smoothstep_q8(phase_q8);

        center_x = 160 + direction *
            (FLOW_SIDE_OFFSET * pose_q8 / 256);
        size = FLOW_COVER_SIZE;
        yaw = direction * (FLOW_SIDE_ANGLE * pose_q8 / 256);
        alpha = 255;
        bilinear = true;
    }
    else {
        int32_t outer_q16 =
            absolute_q16 - FLOW_POSITION_ONE;
        int outer_fade =
            (int)((int64_t)outer_q16 * 32 / FLOW_POSITION_ONE);

        if(outer_fade > 80)
            outer_fade = 80;
        center_x = 160 + direction *
            (FLOW_SIDE_OFFSET +
             (int)((int64_t)outer_q16 * FLOW_SIDE_SPACING /
                   FLOW_POSITION_ONE));
        size = FLOW_COVER_SIZE;
        yaw = direction * FLOW_SIDE_ANGLE;
        alpha = 255 - outer_fade;
        bilinear = true;
    }

    image = cached_image(album_index);
    draw_projected_image(image, album_index, center_x, FLOW_CENTER_Y,
                         size, yaw, alpha, direction, bilinear);
}

static void render_flow(void)
{
    int count = crazypod_music_album_count();
    int base = position_q16 >> 16;
    int indices[10];
    int32_t distances[10];
    int item_count = 0;
    int album_index;
    int item;

    clear_flow_area();
    for(album_index = base - 4;
        album_index <= base + 5; ++album_index) {
        int32_t relative_q16;
        int32_t absolute_q16;
        int insert;

        if(album_index < 0 || album_index >= count)
            continue;
        relative_q16 =
            album_index * FLOW_POSITION_ONE - position_q16;
        absolute_q16 =
            relative_q16 < 0 ? -relative_q16 : relative_q16;
        if(absolute_q16 > FLOW_VISIBLE_DISTANCE_Q16)
            continue;
        insert = item_count;
        while(insert > 0 &&
              distances[insert - 1] < absolute_q16) {
            distances[insert] = distances[insert - 1];
            indices[insert] = indices[insert - 1];
            --insert;
        }
        distances[insert] = absolute_q16;
        indices[insert] = album_index;
        ++item_count;
    }
    for(item = 0; item < item_count; ++item)
        draw_album(indices[item]);

    /*
     * LVGL flushes are held while CoverFlow is active. Present the complete
     * screen once here so metadata and covers belong to the same LCD frame.
     */
    crazypod_present_queue_full();
}

void crazypod_coverflow_enter(int selected)
{
    int count = crazypod_music_album_count();

    if(count <= 0)
        return;
    if(selected < 0)
        selected = 0;
    if(selected >= count)
        selected = count - 1;
    if(!cache_initialized)
        reset_cache();
    selected_album = selected;
    position_q16 = selected * FLOW_POSITION_ONE;
    target_position_q16 = position_q16;
    velocity_q16 = 0;
    input_velocity_q16 = 0;
    flow_direction = 1;
    gesture_min_album = selected;
    prefetched_visual_album = -1;
    last_physics = current_tick;
    last_prefetch = current_tick;
    last_input = current_tick;
    crazypod_frameclock_reset(&render_clock, current_tick);
    artwork_generation_seen = crazypod_artwork_generation();
    input_active = false;
    flow_active = true;
    flow_dirty = true;
    prefetch_pending = !prefetch_covers();
}

bool crazypod_coverflow_warm(int selected)
{
    int count = crazypod_music_album_count();
    bool complete;

    if(count <= 0 || flow_active)
        return count <= 0;
    if(selected < 0)
        selected = 0;
    if(selected >= count)
        selected = count - 1;
    if(!cache_initialized)
        reset_cache();
    selected_album = selected;
    position_q16 = selected * FLOW_POSITION_ONE;
    target_position_q16 = position_q16;
    velocity_q16 = 0;
    input_velocity_q16 = 0;
    flow_direction = 1;
    gesture_min_album = selected;
    input_active = false;
    prefetched_visual_album = -1;
    complete = prefetch_covers();
    last_prefetch = current_tick;
    return complete;
}

void crazypod_coverflow_leave(void)
{
    flow_active = false;
    velocity_q16 = 0;
    input_velocity_q16 = 0;
    input_active = false;
    prefetch_pending = false;
}

bool crazypod_coverflow_active(void)
{
    return flow_active;
}

int crazypod_coverflow_step(int direction)
{
    int count = crazypod_music_album_count();
    int magnitude;
    int speed;
    int direction_sign;
    int center;
    bool new_gesture;

    if(count <= 0 || direction == 0)
        return selected_album;
    direction_sign = direction < 0 ? -1 : 1;
    magnitude = direction < 0 ? -direction : direction;
    center = crazypod_coverflow_center_album();
    new_gesture =
        !input_active ||
        !TIME_BEFORE(current_tick,
                     last_input + FLOW_RELEASE_GRACE_TICKS) ||
        direction_sign != flow_direction;

    if(new_gesture) {
        gesture_min_album = center + direction_sign;
        if(gesture_min_album < 0)
            gesture_min_album = 0;
        if(gesture_min_album >= count)
            gesture_min_album = count - 1;
        selected_album = gesture_min_album;
        if(direction_sign != flow_direction)
            velocity_q16 /= 4;
    }

    last_input = current_tick;
    input_active = true;
    flow_direction = direction_sign;
    speed = FLOW_INPUT_BASE_SPEED + magnitude;
    input_velocity_q16 =
        direction_sign * speed * FLOW_POSITION_ONE;
    flow_dirty = true;
    prefetch_pending = true;
    last_prefetch = 0;
    return selected_album;
}

int crazypod_coverflow_center_album(void)
{
    int count = crazypod_music_album_count();
    int album_index =
        (position_q16 + FLOW_POSITION_ONE / 2) >> 16;

    if(album_index < 0)
        album_index = 0;
    if(album_index >= count)
        album_index = count > 0 ? count - 1 : 0;
    return album_index;
}

static void advance_position(long now)
{
    long elapsed = now - last_physics;
    int count = crazypod_music_album_count();
    int32_t maximum_position_q16 =
        (count > 0 ? count - 1 : 0) * FLOW_POSITION_ONE;
    bool released =
        !TIME_BEFORE(now, last_input + FLOW_RELEASE_GRACE_TICKS);

    if(released && input_active) {
        int target_album;

        input_active = false;
        input_velocity_q16 = 0;
        if(flow_direction > 0) {
            target_album =
                (position_q16 + FLOW_POSITION_ONE - 1) >> 16;
            if(target_album < gesture_min_album)
                target_album = gesture_min_album;
        }
        else {
            target_album = position_q16 >> 16;
            if(target_album > gesture_min_album)
                target_album = gesture_min_album;
        }
        if(target_album < 0)
            target_album = 0;
        if(target_album >= count)
            target_album = count - 1;
        target_position_q16 = target_album * FLOW_POSITION_ONE;
        selected_album = target_album;
        velocity_q16 /= FLOW_RELEASE_MOMENTUM_DIVISOR;
    }

    if(elapsed < 1)
        elapsed = 1;
    if(elapsed > 8)
        elapsed = 8;
    while(elapsed-- > 0) {
        if(!released) {
            velocity_q16 +=
                (int32_t)(((int64_t)
                    (input_velocity_q16 - velocity_q16) *
                    FLOW_INPUT_RESPONSE) / HZ);
        }
        else {
            int32_t error_q16 =
                target_position_q16 - position_q16;
            int64_t acceleration_q16 =
                (int64_t)error_q16 *
                    FLOW_RELEASE_STIFFNESS -
                (int64_t)velocity_q16 *
                    FLOW_RELEASE_DAMPING;

            velocity_q16 +=
                (int32_t)(acceleration_q16 / HZ);
        }
        position_q16 += velocity_q16 / HZ;
        if(position_q16 < 0) {
            position_q16 = 0;
            if(velocity_q16 < 0)
                velocity_q16 = 0;
        }
        else if(position_q16 > maximum_position_q16) {
            position_q16 = maximum_position_q16;
            if(velocity_q16 > 0)
                velocity_q16 = 0;
        }
    }
    last_physics = now;
    selected_album = crazypod_coverflow_center_album();

    if(released) {
        int32_t error_q16 =
            target_position_q16 - position_q16;
        int32_t absolute_error_q16 =
            error_q16 < 0 ? -error_q16 : error_q16;
        int32_t absolute_velocity_q16 =
            velocity_q16 < 0 ? -velocity_q16 : velocity_q16;

        if(absolute_error_q16 <= FLOW_SNAP_POSITION &&
           absolute_velocity_q16 <= FLOW_SNAP_VELOCITY) {
            position_q16 = target_position_q16;
            velocity_q16 = 0;
        }
    }
}

void crazypod_coverflow_invalidate(void)
{
    if(flow_active)
        flow_dirty = true;
}

static void schedule_next_frame(long now)
{
    crazypod_frameclock_schedule_next(&render_clock, now);
}

void crazypod_coverflow_tick(void)
{
    bool animating;
    bool frame_due;
    int visual_album;
    unsigned artwork_generation;

    if(!flow_active)
        return;
    animating =
        input_active ||
        position_q16 != target_position_q16 ||
        velocity_q16 != 0;
    frame_due = crazypod_frameclock_due(&render_clock, current_tick);
    if(frame_due && animating)
        advance_position(current_tick);

    artwork_generation = crazypod_artwork_generation();
    if(artwork_generation != artwork_generation_seen) {
        artwork_generation_seen = artwork_generation;
        prefetch_pending = true;
        last_prefetch = 0;
    }
    visual_album =
        (position_q16 + FLOW_POSITION_ONE / 2) >> 16;
    if(visual_album != prefetched_visual_album) {
        prefetch_pending = true;
        last_prefetch = 0;
    }
    if(prefetch_pending &&
       (last_prefetch == 0 ||
        current_tick - last_prefetch >= FLOW_PREFETCH_TICKS)) {
        prefetch_pending = !prefetch_covers();
        last_prefetch = current_tick;
    }
    if(frame_due && (flow_dirty || animating)) {
        render_flow();
        schedule_next_frame(current_tick);
        flow_dirty = false;
    }
}

#endif
