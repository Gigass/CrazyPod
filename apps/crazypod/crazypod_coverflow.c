#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "button.h"
#include "kernel.h"
#include "lcd.h"
#include "string-extra.h"
#include "system.h"

#include "lvgl.h"

#include "crazypod_artwork.h"
#include "crazypod_coverflow.h"
#include "crazypod_frameclock.h"
#include "crazypod_music.h"

#define FLOW_CACHE_SLOTS CRAZYPOD_COVERFLOW_ARTWORK_SLOTS
#define FLOW_PREFETCH_AHEAD 25
#define FLOW_PREFETCH_BEHIND 25
#define FLOW_PREFETCH_VISIBLE 12
#define FLOW_COVER_SIZE CRAZYPOD_COVERFLOW_ARTWORK_SIZE
#define FLOW_TOP 40
#define FLOW_BOTTOM 180
#define FLOW_CENTER_Y 106
#define FLOW_POSITION_ONE (1L << 16)
#define FLOW_RELEASE_STIFFNESS 400
#define FLOW_RELEASE_DAMPING 30
#define FLOW_PHYSICS_MAX_USEC 50000u
#define FLOW_PHYSICS_STEP_USEC 8333u
#define FLOW_RELEASE_PROJECTION_TICKS \
    (((HZ * 32) / 100) > 0 ? ((HZ * 32) / 100) : 1)
#define FLOW_RELEASE_GRACE_TICKS \
    (((HZ * 6) / 100) > 0 ? ((HZ * 6) / 100) : 1)
#define FLOW_INPUT_BASE_SPEED 2
#define FLOW_INPUT_RESPONSE 24
#define FLOW_INPUT_IMPULSE_Q16 (FLOW_POSITION_ONE / 2)
#define FLOW_SNAP_POSITION (FLOW_POSITION_ONE / 512)
#define FLOW_SNAP_VELOCITY (FLOW_POSITION_ONE / 20)
#define FLOW_PREFETCH_TICKS ((HZ / 10) > 0 ? (HZ / 10) : 1)
#define FLOW_CAMERA_DISTANCE 400
#define FLOW_SIDE_ANGLE 60
#define FLOW_SIDE_OFFSET 108
#define FLOW_SIDE_SPACING 65
#define FLOW_VISIBLE_DISTANCE_Q16 (FLOW_POSITION_ONE * 7 / 2)
#define FLOW_WHEEL_POSITIONS 96
#define FLOW_WHEEL_CLICKS_PER_ALBUM 12
#define FLOW_WHEEL_SNAP_CLICKS (FLOW_WHEEL_CLICKS_PER_ALBUM / 2)
#define FLOW_WHEEL_RELEASE_TICKS \
    (((HZ * 6) / 100) > 0 ? ((HZ * 6) / 100) : 1)

struct flow_cache_entry {
    int album_index;
    const lv_image_dsc_t *image;
};

struct flow_dirty_rect {
    bool valid;
    int x1;
    int y1;
    int x2;
    int y2;
};

static struct flow_cache_entry cache[FLOW_CACHE_SLOTS];
static bool flow_active;
static bool flow_dirty;
static bool prefetch_pending;
static bool prefetch_deep_pending;
static int selected_album;
static int metadata_album;
static int32_t position_q16;
static int32_t target_position_q16;
static int32_t velocity_q16;
static int32_t input_velocity_q16;
static int flow_direction;
static int gesture_min_album;
static int prefetched_visual_album;
static uint32_t last_physics_usec;
static long last_prefetch;
static long last_input;
static struct crazypod_frameclock render_clock;
static unsigned artwork_generation_seen;
static bool cache_initialized;
static bool input_active;
static bool wheel_input_suspended;
static bool compositing_suspended;
static bool wheel_tracking;
static int wheel_position;
static int wheel_anchor_album;
static int wheel_step_accumulator;
static long wheel_last_seen;
static int wheel_feedback_direction;
static bool first_present;
static struct flow_dirty_rect deferred_top;
static struct flow_dirty_rect deferred_bottom;
static void (*boost_cpu)(int ticks);

extern struct frame_buffer_t lcd_framebuffer_default;

static void reset_deferred_presents(void)
{
    memset(&deferred_top, 0, sizeof(deferred_top));
    memset(&deferred_bottom, 0, sizeof(deferred_bottom));
}

static void accumulate_dirty_rect(
    struct flow_dirty_rect *dirty,
    int x1, int y1, int x2, int y2)
{
    if(x1 > x2 || y1 > y2)
        return;
    if(!dirty->valid) {
        dirty->valid = true;
        dirty->x1 = x1;
        dirty->y1 = y1;
        dirty->x2 = x2;
        dirty->y2 = y2;
        return;
    }
    if(x1 < dirty->x1)
        dirty->x1 = x1;
    if(y1 < dirty->y1)
        dirty->y1 = y1;
    if(x2 > dirty->x2)
        dirty->x2 = x2;
    if(y2 > dirty->y2)
        dirty->y2 = y2;
}

static bool queue_dirty_present(struct flow_dirty_rect *dirty)
{
    if(!dirty->valid)
        return false;
    crazypod_present_queue_music_rect(
        dirty->x1, dirty->y1,
        dirty->x2 - dirty->x1 + 1,
        dirty->y2 - dirty->y1 + 1);
    dirty->valid = false;
    return true;
}

static bool queue_deferred_present(void)
{
    if(queue_dirty_present(&deferred_top))
        return true;
    return queue_dirty_present(&deferred_bottom);
}

void crazypod_coverflow_configure(void (*boost)(int ticks))
{
    boost_cpu = boost;
}

static fb_data *framebuffer(void)
{
    return (fb_data *)lcd_framebuffer_default.data;
}

static inline unsigned alpha_weight6(int alpha)
{
    if(alpha <= 0)
        return 0;
    if(alpha >= 255)
        return 64;
    return ((unsigned)alpha + 2) >> 2;
}

static inline __attribute__((always_inline)) fb_data blend565_weight(
    fb_data foreground, fb_data background, unsigned foreground_weight)
{
    unsigned inverse;
    unsigned result;

    if(foreground_weight == 0)
        return background;
    if(foreground_weight >= 64)
        return foreground;
    /*
     * Use 6-bit weights. With a maximum weight sum of 64, the blue product
     * cannot carry into the red lane of the packed 0xf81f pair. The old
     * 8-bit multiply allowed exactly that and produced saturated blue noise.
     */
    inverse = 64 - foreground_weight;
    result =
        (((foreground & 0xf81f) * foreground_weight +
          (background & 0xf81f) * inverse) >> 6) & 0xf81f;
    result |=
        (((foreground & 0x07e0) * foreground_weight +
          (background & 0x07e0) * inverse) >> 6) & 0x07e0;
    return (fb_data)result;
}

static inline fb_data blend565(fb_data foreground, fb_data background,
                               int alpha)
{
    return blend565_weight(
        foreground, background, alpha_weight6(alpha));
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

static bool prefetch_covers(bool include_distant)
{
    int count = crazypod_music_album_count();
    int direction = flow_direction == 0 ? 1 : flow_direction;
    int visual_album =
        (position_q16 + FLOW_POSITION_ONE / 2) >> 16;
    int order[FLOW_CACHE_SLOTS];
    int order_count = 0;
    int distance;
    int candidate_number;
    int maximum_distance = include_distant
        ? FLOW_PREFETCH_AHEAD : FLOW_PREFETCH_VISIBLE;
    bool complete = true;

    prefetched_visual_album = visual_album;

    append_prefetch_candidate(order, &order_count, visual_album);
    /*
     * Both visible sides must win over invisible directional look-ahead.
     * Direction only decides which side of an equal-distance pair goes first.
     */
    for(distance = 1; distance <= maximum_distance; ++distance) {
        int ahead = visual_album + direction * distance;
        int behind = visual_album - direction * distance;

        append_prefetch_candidate(order, &order_count, ahead);
        append_prefetch_candidate(order, &order_count, behind);
    }
    if(include_distant) {
        for(distance = maximum_distance + 1;
            distance <= FLOW_PREFETCH_BEHIND;
            ++distance) {
            int ahead = visual_album + direction * distance;
            int behind = visual_album - direction * distance;

            append_prefetch_candidate(order, &order_count, ahead);
            append_prefetch_candidate(order, &order_count, behind);
        }
    }
    for(candidate_number = 0;
        candidate_number < order_count;
        ++candidate_number) {
        int album_index = order[candidate_number];
        struct flow_cache_entry *entry;
        const struct crazypod_track *track;
        const lv_image_dsc_t *image;
        bool decode_allowed;
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
        /*
         * Visible covers may read/decode on demand. Distant look-ahead is
         * cache-only so an idle CoverFlow never walks and decodes the whole
         * surrounding window just to prepare off-screen albums.
         */
        decode_allowed = !include_distant ||
            candidate_number <= FLOW_PREFETCH_VISIBLE * 2;
        image = track == NULL
            ? NULL
            : decode_allowed
                ? crazypod_artwork_load_priority(
                      slot, track, FLOW_COVER_SIZE,
                      candidate_number)
                : crazypod_artwork_load_cached_priority(
                      slot, track, FLOW_COVER_SIZE,
                      candidate_number);
        if(image == NULL)
            entry->image = NULL;
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
    int base = 28 + ((unsigned)album_index & 3) * 4;
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
    gradient = 188 + (y * 52 >> 7);
    color = LCD_RGBPACK(base * gradient >> 8,
                        base * gradient >> 8,
                        (base + 2) * gradient >> 8);
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

static int32_t multiply_q16(int32_t left_q16, int32_t right_q16)
{
    return (int32_t)(((int64_t)left_q16 * right_q16) >> 16);
}

/*
 * These quintic Hermite curves match position, velocity and acceleration at
 * both ends. The center slope keeps the focused cover responsive, while the
 * side slope joins FLOW_SIDE_SPACING without a velocity discontinuity.
 */
static int32_t flow_translation_q16(int32_t phase_q16)
{
    int32_t phase_squared_q16;
    int32_t cubic_terms_q16;

    if(phase_q16 <= 0)
        return 0;
    if(phase_q16 >= FLOW_POSITION_ONE)
        return FLOW_SIDE_OFFSET * FLOW_POSITION_ONE;
    phase_squared_q16 = multiply_q16(phase_q16, phase_q16);
    cubic_terms_q16 =
        172 * FLOW_POSITION_ONE +
        multiply_q16(
            phase_q16,
            -301 * FLOW_POSITION_ONE +
            multiply_q16(
                phase_q16, 129 * FLOW_POSITION_ONE));
    return multiply_q16(
        phase_q16,
        108 * FLOW_POSITION_ONE +
        multiply_q16(phase_squared_q16, cubic_terms_q16));
}

static int32_t flow_rotation_q16(int32_t phase_q16)
{
    int32_t phase_squared_q16;
    int32_t cubic_terms_q16;

    if(phase_q16 <= 0)
        return 0;
    if(phase_q16 >= FLOW_POSITION_ONE)
        return FLOW_SIDE_ANGLE * FLOW_POSITION_ONE;
    phase_squared_q16 = multiply_q16(phase_q16, phase_q16);
    cubic_terms_q16 =
        240 * FLOW_POSITION_ONE +
        multiply_q16(
            phase_q16,
            -420 * FLOW_POSITION_ONE +
            multiply_q16(
                phase_q16, 180 * FLOW_POSITION_ONE));
    return multiply_q16(
        phase_q16,
        60 * FLOW_POSITION_ONE +
        multiply_q16(phase_squared_q16, cubic_terms_q16));
}

static int32_t flow_trig_q15(int32_t angle_q16, bool cosine)
{
    int whole_angle = angle_q16 / FLOW_POSITION_ONE;
    int32_t remainder_q16 =
        angle_q16 - whole_angle * FLOW_POSITION_ONE;
    int next_angle = whole_angle +
        (remainder_q16 < 0 ? -1 : 1);
    int32_t amount_q16 =
        remainder_q16 < 0 ? -remainder_q16 : remainder_q16;
    int32_t start_q15 = cosine
        ? lv_trigo_cos(whole_angle) : lv_trigo_sin(whole_angle);
    int32_t end_q15 = cosine
        ? lv_trigo_cos(next_angle) : lv_trigo_sin(next_angle);

    return start_q15 +
        (int32_t)(((int64_t)(end_q15 - start_q15) *
                   amount_q16) >> 16);
}

static inline __attribute__((always_inline)) fb_data shade565_weight(
    fb_data color, unsigned weight)
{
    unsigned result;

    if(weight >= 64)
        return color;
    if(weight == 0)
        return LCD_BLACK;
    result =
        (((color & 0xf81f) * weight) >> 6) & 0xf81f;
    result |=
        (((color & 0x07e0) * weight) >> 6) & 0x07e0;
    return (fb_data)result;
}

static int32_t divide_q15(int32_t numerator, int32_t denominator)
{
    uint32_t magnitude;
    int shift = 0;

    if(denominator == 0)
        return 0;
    magnitude = numerator < 0
        ? (uint32_t)(-(int64_t)numerator)
        : (uint32_t)numerator;
    while(shift < 15 && magnitude <= (uint32_t)INT32_MAX / 2) {
        magnitude <<= 1;
        ++shift;
    }
    numerator = (int32_t)((int64_t)numerator * (1L << shift));
    denominator >>= 15 - shift;
    if(denominator == 0)
        denominator = 1;
    return numerator / denominator;
}

static int32_t projected_x_q16(int32_t center_x_q16, int u,
                               int sin_q15, int cos_q15)
{
    int denominator =
        FLOW_CAMERA_DISTANCE * 32768 - u * sin_q15;

    if(denominator <= 0)
        return center_x_q16;
    return center_x_q16 + (int32_t)(
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
                                 int32_t center_x_q16, int center_y,
                                 int size, int32_t yaw_q16, int alpha,
                                 int side)
{
    const fb_data *source;
    fb_data *pixels = framebuffer();
    int source_width;
    int source_height;
    int half_size = size / 2;
    int sin_q15 = flow_trig_q15(yaw_q16, false);
    int cos_q15 = flow_trig_q15(yaw_q16, true);
    int32_t absolute_yaw_q16 =
        yaw_q16 < 0 ? -yaw_q16 : yaw_q16;
    int yaw_amount_q8 =
        absolute_yaw_q16 /
        (FLOW_SIDE_ANGLE * FLOW_POSITION_ONE / 256);
    int32_t x0_q16 = projected_x_q16(
        center_x_q16, -half_size, sin_q15, cos_q15);
    int32_t x1_q16 = projected_x_q16(
        center_x_q16, half_size, sin_q15, cos_q15);
    int32_t left_q16 = x0_q16 < x1_q16 ? x0_q16 : x1_q16;
    int32_t right_q16 = x0_q16 > x1_q16 ? x0_q16 : x1_q16;
    int left = left_q16 >> 16;
    int right = (right_q16 + FLOW_POSITION_ONE - 1) >> 16;
    int abs_sin_q15 = sin_q15 < 0 ? -sin_q15 : sin_q15;
    int near_depth_q8 =
        ((int32_t)half_size * abs_sin_q15 * 2) >> 8;
    int height_normalizer_q8 =
        (FLOW_CAMERA_DISTANCE << 8) - near_depth_q8;
    bool placeholder =
        image == NULL || image->header.cf != LV_COLOR_FORMAT_RGB565;
    int vertical_scale_q23;
    int32_t screen_x_q8;
    int32_t inverse_numerator;
    int32_t inverse_denominator;
    int px;

    if(yaw_amount_q8 > 256)
        yaw_amount_q8 = 256;
    if(placeholder) {
        source = NULL;
        source_width = FLOW_COVER_SIZE;
        source_height = FLOW_COVER_SIZE;
    }
    else {
        source = (const fb_data *)image->data;
        source_width = image->header.w;
        source_height = image->header.h;
    }
    vertical_scale_q23 =
        (source_height << 23) /
        (size * height_normalizer_q8);

    if(left < 0)
        left = 0;
    if(right > LCD_WIDTH)
        right = LCD_WIDTH;
    screen_x_q8 =
        (left << 8) + 128 - (center_x_q16 >> 8);
    inverse_numerator =
        screen_x_q8 * FLOW_CAMERA_DISTANCE;
    inverse_denominator =
        FLOW_CAMERA_DISTANCE * cos_q15 +
        (screen_x_q8 * sin_q15 >> 8);
    for(px = left; px < right; ++px) {
        int edge_alpha =
            column_coverage_alpha_q16(px, left_q16, right_q16);
        int32_t u_q8;
        int32_t depth_q8;
        int32_t depth_denominator_q8;
        int normalized_q8;
        int source_x;
        int source_step_q16;
        int source_position;
        int shade;
        unsigned shade_weight;
        int draw_alpha;
        unsigned draw_weight;
        int reflection_y;
        int y;

        if(edge_alpha <= 0 || inverse_denominator <= 0)
            goto next_column;
        u_q8 = divide_q15(
            inverse_numerator, inverse_denominator);
        if(u_q8 < -(half_size << 8) ||
           u_q8 > (half_size << 8))
            goto next_column;
        normalized_q8 =
            (u_q8 + (half_size << 8)) / size;
        if(normalized_q8 < 0)
            normalized_q8 = 0;
        if(normalized_q8 > 255)
            normalized_q8 = 255;
        source_x = normalized_q8 * source_width >> 8;
        if(source_x >= source_width)
            source_x = source_width - 1;

        depth_q8 =
            u_q8 * sin_q15 >> 15;
        depth_denominator_q8 =
            (FLOW_CAMERA_DISTANCE << 8) - depth_q8;
        if(depth_denominator_q8 <= 0)
            goto next_column;
        source_step_q16 =
            depth_denominator_q8 * vertical_scale_q23 >> 7;
        if(source_step_q16 < 1)
            source_step_q16 = 1;
        source_position = normalized_q8;
        if(side < 0)
            source_position = 255 - source_position;
        shade = 184 + ((source_position * 60 + 128) >> 8);
        shade =
            256 - (((256 - shade) * yaw_amount_q8 + 128) >> 8);
        shade_weight = alpha_weight6(shade);
        draw_alpha = alpha;
        if(edge_alpha < 255)
            draw_alpha =
                (draw_alpha * edge_alpha + 128) >> 8;
        draw_weight = alpha_weight6(draw_alpha);

        {
            int source_y_q16 =
                (source_height << 15) - 32768;
            int py = center_y - 1;

            while(source_y_q16 >= 0 && py >= FLOW_TOP) {
                fb_data *destination;
                fb_data color;
                int sy = source_y_q16 >> 16;

                if(placeholder) {
                    color = placeholder_sample(
                        album_index, source_x, sy,
                        source_width, source_height);
                }
                else
                    color = source[
                        sy * source_width + source_x];
                if(shade_weight < 64)
                    color = shade565_weight(color, shade_weight);
                destination = pixels + py * LCD_WIDTH + px;
                *destination = draw_weight >= 64
                    ? color : blend565_weight(
                        color, *destination, draw_weight);
                source_y_q16 -= source_step_q16;
                --py;
            }
        }
        {
            int source_y_q16 = source_height << 15;
            int py = center_y;

            while(source_y_q16 < (source_height << 16) &&
                  py < FLOW_BOTTOM) {
                fb_data *destination;
                fb_data color;
                int sy = source_y_q16 >> 16;

                if(placeholder) {
                    color = placeholder_sample(
                        album_index, source_x, sy,
                        source_width, source_height);
                }
                else
                    color = source[
                        sy * source_width + source_x];
                if(shade_weight < 64)
                    color = shade565_weight(color, shade_weight);
                destination = pixels + py * LCD_WIDTH + px;
                *destination = draw_weight >= 64
                    ? color : blend565_weight(
                        color, *destination, draw_weight);
                source_y_q16 += source_step_q16;
                ++py;
            }
            reflection_y = py + 2;
        }

        for(y = 0; y < 9; ++y) {
            int py = reflection_y + y;
            int reflection_alpha = draw_alpha * (27 - y * 3) >> 8;
            fb_data *destination;
            fb_data color;
            int sy =
                source_height - 1 -
                (y + 1) * source_height / 18;

            if(py < FLOW_TOP || py >= FLOW_BOTTOM ||
               reflection_alpha <= 0)
                continue;
            if(placeholder) {
                color = placeholder_sample(
                    album_index, source_x,
                    sy,
                    source_width, source_height);
            }
            else
                color = source[
                    sy * source_width + source_x];
            if(shade_weight < 64)
                color = shade565_weight(color, shade_weight);
            destination = pixels + py * LCD_WIDTH + px;
            *destination = blend565(
                color, *destination, reflection_alpha);
        }
next_column:
        inverse_numerator += FLOW_CAMERA_DISTANCE << 8;
        inverse_denominator += sin_q15;
    }
}

static void draw_album(int album_index)
{
    int32_t relative_q16 =
        album_index * FLOW_POSITION_ONE - position_q16;
    int32_t absolute_q16 =
        relative_q16 < 0 ? -relative_q16 : relative_q16;
    int direction = relative_q16 < 0 ? -1 : 1;
    int32_t center_x_q16;
    int size;
    int32_t yaw_q16;
    int alpha;
    const lv_image_dsc_t *image;

    if(absolute_q16 > FLOW_VISIBLE_DISTANCE_Q16)
        return;
    if(absolute_q16 <= FLOW_POSITION_ONE) {
        int32_t translation_q16 =
            flow_translation_q16(absolute_q16);
        int32_t rotation_q16 =
            flow_rotation_q16(absolute_q16);

        center_x_q16 =
            160 * FLOW_POSITION_ONE +
            direction * translation_q16;
        size = FLOW_COVER_SIZE;
        yaw_q16 = direction * rotation_q16;
        alpha = 255;
    }
    else {
        int32_t outer_q16 =
            absolute_q16 - FLOW_POSITION_ONE;
        int outer_fade =
            (int)((int64_t)outer_q16 * 32 / FLOW_POSITION_ONE);

        if(outer_fade > 80)
            outer_fade = 80;
        center_x_q16 =
            160 * FLOW_POSITION_ONE +
            direction *
            (FLOW_SIDE_OFFSET * FLOW_POSITION_ONE +
             outer_q16 * FLOW_SIDE_SPACING);
        size = FLOW_COVER_SIZE;
        yaw_q16 =
            direction * FLOW_SIDE_ANGLE * FLOW_POSITION_ONE;
        alpha = 255 - outer_fade;
    }

    image = cached_image(album_index);
    draw_projected_image(
        image, album_index, center_x_q16, FLOW_CENTER_Y,
        size, yaw_q16, alpha, direction);
}

static void render_flow(void)
{
    uint32_t render_started_us = crazypod_monotonic_usec();
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

    /* Initialize the route once, then keep motion DMA inside the native
       CoverFlow band. LVGL dirt outside this band is drained separately. */
    if(first_present) {
        crazypod_present_queue_full();
        first_present = false;
        reset_deferred_presents();
    }
    else
        crazypod_present_queue_music_rect(
            0, FLOW_TOP, LCD_WIDTH, FLOW_BOTTOM - FLOW_TOP);
    crazypod_present_note_render(
        CRAZYPOD_RENDER_MUSIC,
        crazypod_monotonic_usec() - render_started_us);
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
    metadata_album = selected;
    position_q16 = selected * FLOW_POSITION_ONE;
    target_position_q16 = position_q16;
    velocity_q16 = 0;
    input_velocity_q16 = 0;
    flow_direction = 1;
    gesture_min_album = selected;
    prefetched_visual_album = -1;
    last_physics_usec = crazypod_monotonic_usec();
    last_prefetch = current_tick;
    last_input = current_tick;
    crazypod_frameclock_reset(&render_clock, current_tick);
    artwork_generation_seen = crazypod_artwork_generation();
    input_active = false;
    wheel_input_suspended = false;
    compositing_suspended = false;
    wheel_tracking = false;
    wheel_position = -1;
    wheel_anchor_album = selected;
    wheel_step_accumulator = 0;
    wheel_last_seen = 0;
    wheel_feedback_direction = 0;
    first_present = true;
    reset_deferred_presents();
    flow_active = true;
    flow_dirty = true;
    prefetch_pending = !prefetch_covers(false);
    prefetch_deep_pending = true;
#ifdef HAVE_WHEEL_POSITION
    wheel_send_events(false);
#endif
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
    metadata_album = selected;
    position_q16 = selected * FLOW_POSITION_ONE;
    target_position_q16 = position_q16;
    velocity_q16 = 0;
    input_velocity_q16 = 0;
    flow_direction = 1;
    gesture_min_album = selected;
    input_active = false;
    wheel_anchor_album = selected;
    prefetched_visual_album = -1;
    complete = prefetch_covers(false);
    prefetch_deep_pending = true;
    last_prefetch = current_tick;
    return complete;
}

void crazypod_coverflow_leave(void)
{
    flow_active = false;
    metadata_album = -1;
    velocity_q16 = 0;
    input_velocity_q16 = 0;
    input_active = false;
    wheel_input_suspended = false;
    compositing_suspended = false;
    wheel_tracking = false;
    wheel_anchor_album = 0;
    wheel_step_accumulator = 0;
    wheel_feedback_direction = 0;
    first_present = false;
    reset_deferred_presents();
    prefetch_pending = false;
    prefetch_deep_pending = false;
#ifdef HAVE_WHEEL_POSITION
    wheel_send_events(true);
#endif
}

bool crazypod_coverflow_active(void)
{
    return flow_active;
}

bool crazypod_coverflow_compositing_active(void)
{
    return flow_active && !compositing_suspended;
}

bool crazypod_coverflow_motion_active(void)
{
    return crazypod_coverflow_compositing_active() &&
        !wheel_input_suspended &&
        (flow_dirty || wheel_tracking || input_active ||
         position_q16 != target_position_q16 ||
         velocity_q16 != 0);
}

void crazypod_coverflow_set_compositing_suspended(bool suspended)
{
    if(!flow_active || compositing_suspended == suspended)
        return;
    compositing_suspended = suspended;
    reset_deferred_presents();
    if(suspended)
        return;

    /* LVGL owned the framebuffer while a modal surface was visible. Rebuild
       the complete route before returning to native band-only presents. */
    first_present = true;
    flow_dirty = true;
    crazypod_frameclock_reset(&render_clock, current_tick);
}

void crazypod_coverflow_set_input_suspended(bool suspended)
{
    int album_index;

    if(!flow_active || wheel_input_suspended == suspended)
        return;
    wheel_input_suspended = suspended;
    wheel_tracking = false;
    wheel_position = -1;
    wheel_anchor_album = crazypod_coverflow_center_album();
    wheel_step_accumulator = 0;
    wheel_last_seen = 0;
    wheel_feedback_direction = 0;
    velocity_q16 = 0;
    input_velocity_q16 = 0;
    input_active = false;
    last_input = current_tick;
    last_physics_usec = crazypod_monotonic_usec();
    if(suspended) {
        album_index = crazypod_coverflow_center_album();
        selected_album = album_index;
        metadata_album = album_index;
        wheel_anchor_album = album_index;
        position_q16 = album_index * FLOW_POSITION_ONE;
        target_position_q16 = position_q16;
        flow_dirty = true;
    }
    else {
        /* The lock screen replaces every panel row. Rebuild the complete
           album-flow route once before returning to partial TE updates. */
        first_present = true;
        reset_deferred_presents();
        flow_dirty = true;
    }
#ifdef HAVE_WHEEL_POSITION
    /*
     * CoverFlow normally consumes absolute wheel position directly. The lock
     * screen consumes Rockbox queue events, so hand wheel ownership back while
     * locked and reclaim it only after the unlock transition completes.
     */
    wheel_send_events(suspended);
#endif
}

int crazypod_coverflow_step(int direction)
{
    int count = crazypod_music_album_count();
    int magnitude;
    int speed;
    int direction_sign;
    int center;
    int64_t boosted_velocity_q16;
    bool new_gesture;
    bool direction_changed;

    if(count <= 0 || direction == 0)
        return selected_album;
    direction_sign = direction < 0 ? -1 : 1;
    magnitude = direction < 0 ? -direction : direction;
    center = crazypod_coverflow_center_album();
    direction_changed = direction_sign != flow_direction;
    new_gesture =
        !input_active ||
        !TIME_BEFORE(current_tick,
                     last_input + FLOW_RELEASE_GRACE_TICKS) ||
        direction_changed;

    if(new_gesture) {
        last_physics_usec = crazypod_monotonic_usec();
        gesture_min_album = center + direction_sign;
        if(gesture_min_album < 0)
            gesture_min_album = 0;
        if(gesture_min_album >= count)
            gesture_min_album = count - 1;
        selected_album = gesture_min_album;
        metadata_album = gesture_min_album;
    }

    if(direction_changed)
        velocity_q16 = 0;
    boosted_velocity_q16 =
        (int64_t)velocity_q16 +
        (int64_t)direction_sign * magnitude *
            FLOW_INPUT_IMPULSE_Q16;
    if(boosted_velocity_q16 > INT32_MAX)
        velocity_q16 = INT32_MAX;
    else if(boosted_velocity_q16 < INT32_MIN)
        velocity_q16 = INT32_MIN;
    else
        velocity_q16 = (int32_t)boosted_velocity_q16;

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

int crazypod_coverflow_selected_album(void)
{
    return metadata_album;
}

int crazypod_coverflow_take_wheel_feedback(void)
{
    int direction = wheel_feedback_direction;

    wheel_feedback_direction = 0;
    return direction;
}

#ifdef HAVE_WHEEL_POSITION
static bool step_wheel_album_anchor(int direction)
{
    int count = crazypod_music_album_count();
    int target_album;

    if(count <= 0 || direction == 0)
        return false;
    target_album = wheel_anchor_album + direction;
    if(target_album < 0)
        target_album = 0;
    if(target_album >= count)
        target_album = count - 1;
    if(target_album == wheel_anchor_album)
        return false;

    wheel_anchor_album = target_album;
    gesture_min_album = target_album;
    selected_album = target_album;
    metadata_album = target_album;
    input_velocity_q16 = 0;
    wheel_feedback_direction = direction;
    flow_dirty = true;
    prefetch_pending = true;
    prefetch_deep_pending = true;
    last_prefetch = 0;
    return true;
}

static void update_wheel_fractional_target(void)
{
    int count = crazypod_music_album_count();
    int64_t target_q16;
    int64_t maximum_q16;

    if(count <= 0)
        return;
    target_q16 =
        (int64_t)wheel_anchor_album * FLOW_POSITION_ONE +
        (int64_t)wheel_step_accumulator * FLOW_POSITION_ONE /
            FLOW_WHEEL_CLICKS_PER_ALBUM;
    maximum_q16 = (int64_t)(count - 1) * FLOW_POSITION_ONE;
    if(target_q16 < 0)
        target_q16 = 0;
    if(target_q16 > maximum_q16)
        target_q16 = maximum_q16;
    if(target_position_q16 == (int32_t)target_q16)
        return;

    target_position_q16 = (int32_t)target_q16;
    flow_dirty = true;
}

static int snapped_wheel_album(void)
{
    int count = crazypod_music_album_count();
    int album = wheel_anchor_album;

    if(wheel_step_accumulator >= FLOW_WHEEL_SNAP_CLICKS)
        ++album;
    else if(wheel_step_accumulator <= -FLOW_WHEEL_SNAP_CLICKS)
        --album;
    if(album < 0)
        album = 0;
    if(album >= count)
        album = count > 0 ? count - 1 : 0;
    return album;
}
#endif

static void sample_wheel_position(long now, uint32_t now_usec)
{
#ifdef HAVE_WHEEL_POSITION
    int current = wheel_status();

    if(current >= 0) {
        current %= FLOW_WHEEL_POSITIONS;
        if(!wheel_tracking) {
            int target_album = metadata_album;

            if(target_album < 0 ||
               target_album >= crazypod_music_album_count())
                target_album = crazypod_coverflow_center_album();

            wheel_tracking = true;
            wheel_position = current;
            last_physics_usec = now_usec;
            selected_album = target_album;
            metadata_album = target_album;
            wheel_anchor_album = target_album;
            target_position_q16 = target_album * FLOW_POSITION_ONE;
            gesture_min_album = target_album;
            velocity_q16 = 0;
            input_velocity_q16 = 0;
            wheel_step_accumulator = 0;
        }
        else {
            int delta = current - wheel_position;

            if(delta < -FLOW_WHEEL_POSITIONS / 2)
                delta += FLOW_WHEEL_POSITIONS;
            else if(delta > FLOW_WHEEL_POSITIONS / 2)
                delta -= FLOW_WHEEL_POSITIONS;
            if(delta != 0) {
                int delta_direction = delta < 0 ? -1 : 1;
                int step_direction;

                if(delta_direction != flow_direction)
                    velocity_q16 = 0;
                flow_direction = delta_direction;
                wheel_position = current;
                wheel_step_accumulator += delta;
                while(wheel_step_accumulator >=
                          FLOW_WHEEL_CLICKS_PER_ALBUM ||
                      wheel_step_accumulator <=
                          -FLOW_WHEEL_CLICKS_PER_ALBUM) {
                    step_direction =
                        wheel_step_accumulator < 0 ? -1 : 1;
                    if(!step_wheel_album_anchor(step_direction)) {
                        wheel_step_accumulator = 0;
                        break;
                    }
                    wheel_step_accumulator -=
                        step_direction * FLOW_WHEEL_CLICKS_PER_ALBUM;
                }
                update_wheel_fractional_target();
            }
        }
        wheel_last_seen = now;
        last_input = now;
        input_active = true;
        return;
    }
    if(wheel_tracking &&
       !TIME_BEFORE(now, wheel_last_seen + FLOW_WHEEL_RELEASE_TICKS)) {
        wheel_tracking = false;
        wheel_position = -1;
    }
#else
    (void)now;
    (void)now_usec;
#endif
}

static void advance_position(long now, uint32_t now_usec)
{
    uint32_t elapsed_usec = now_usec - last_physics_usec;
    int count = crazypod_music_album_count();
    int32_t maximum_position_q16 =
        (count > 0 ? count - 1 : 0) * FLOW_POSITION_ONE;
    bool released = !wheel_tracking &&
        !TIME_BEFORE(now, last_input + FLOW_RELEASE_GRACE_TICKS);

    if(released && input_active) {
#ifndef HAVE_WHEEL_POSITION
        int32_t projected_position_q16;
#endif
        int target_album;

        input_active = false;
#ifdef HAVE_WHEEL_POSITION
        /* The wheel moves the cover continuously inside each 12-click
           detent. On release, six clicks is the midpoint: below it returns
           to the anchor, at or beyond it advances in that direction. */
        target_album = snapped_wheel_album();
        wheel_anchor_album = target_album;
        wheel_step_accumulator = 0;
#else
        projected_position_q16 =
            position_q16 +
            (int32_t)(((int64_t)velocity_q16 *
                       FLOW_RELEASE_PROJECTION_TICKS) / HZ);
        target_album =
            (projected_position_q16 + FLOW_POSITION_ONE / 2) >> 16;
        if(flow_direction > 0) {
            if(target_album < gesture_min_album)
                target_album = gesture_min_album;
        }
        else {
            if(target_album > gesture_min_album)
                target_album = gesture_min_album;
        }
#endif
        if(target_album < 0)
            target_album = 0;
        if(target_album >= count)
            target_album = count - 1;
        target_position_q16 = target_album * FLOW_POSITION_ONE;
        selected_album = target_album;
        metadata_album = target_album;
        input_velocity_q16 = 0;
    }

    last_physics_usec = now_usec;
    if(elapsed_usec > FLOW_PHYSICS_MAX_USEC)
        elapsed_usec = FLOW_PHYSICS_MAX_USEC;
    while(elapsed_usec > 0) {
        uint32_t step_usec = elapsed_usec;

        if(step_usec > FLOW_PHYSICS_STEP_USEC)
            step_usec = FLOW_PHYSICS_STEP_USEC;
        if(!released && !wheel_tracking) {
            velocity_q16 +=
                (int32_t)(((int64_t)
                    (input_velocity_q16 - velocity_q16) *
                    FLOW_INPUT_RESPONSE * step_usec) /
                    1000000);
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
                (int32_t)((acceleration_q16 * step_usec) /
                          1000000);
        }
        position_q16 +=
            (int32_t)(((int64_t)velocity_q16 * step_usec) /
                      1000000);
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
        elapsed_usec -= step_usec;
    }
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

void crazypod_coverflow_capture_flush(
    int x, int y, int width, int height)
{
    int x2;
    int y2;

    if(!flow_active || width <= 0 || height <= 0)
        return;
    x2 = x + width - 1;
    y2 = y + height - 1;
    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;
    if(x2 >= LCD_WIDTH)
        x2 = LCD_WIDTH - 1;
    if(y2 >= LCD_HEIGHT)
        y2 = LCD_HEIGHT - 1;
    if(x > x2 || y > y2)
        return;

    if(y < FLOW_TOP)
        accumulate_dirty_rect(
            &deferred_top, x, y, x2,
            y2 < FLOW_TOP ? y2 : FLOW_TOP - 1);
    if(y2 >= FLOW_BOTTOM)
        accumulate_dirty_rect(
            &deferred_bottom, x,
            y > FLOW_BOTTOM ? y : FLOW_BOTTOM,
            x2, y2);
    if(y < FLOW_BOTTOM && y2 >= FLOW_TOP)
        flow_dirty = true;
}

static void schedule_next_frame(long now)
{
    crazypod_frameclock_schedule_next(&render_clock, now);
}

static bool position_animating(void)
{
#ifdef HAVE_WHEEL_POSITION
    return position_q16 != target_position_q16 ||
        velocity_q16 != 0;
#else
    return input_active ||
        position_q16 != target_position_q16 ||
        velocity_q16 != 0;
#endif
}

void crazypod_coverflow_tick(void)
{
    bool animating;
    bool frame_due;
    int visual_album;
    unsigned artwork_generation;
    uint32_t now_usec;

    if(!crazypod_coverflow_compositing_active())
        return;
    now_usec = crazypod_monotonic_usec();
    sample_wheel_position(current_tick, now_usec);
    animating = position_animating();
    if(crazypod_coverflow_motion_active() && boost_cpu != NULL)
        boost_cpu(HZ / 10 > 0 ? HZ / 10 : 1);
    frame_due = crazypod_frameclock_due(&render_clock, current_tick);
    if(frame_due &&
       (wheel_tracking || input_active || animating)) {
        int32_t previous_position_q16 = position_q16;

        advance_position(current_tick, now_usec);
        if(position_q16 != previous_position_q16)
            flow_dirty = true;
        animating = position_animating();
    }

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
        prefetch_pending = !prefetch_covers(false);
        last_prefetch = current_tick;
    }
    if(!prefetch_pending && prefetch_deep_pending &&
       !wheel_tracking && !animating &&
       current_tick - last_input >= FLOW_PREFETCH_TICKS) {
        prefetch_deep_pending = !prefetch_covers(true);
        last_prefetch = current_tick;
    }
    if(frame_due && (flow_dirty || animating)) {
        render_flow();
        schedule_next_frame(current_tick);
        flow_dirty = false;
    }
    else if(frame_due && !animating && queue_deferred_present())
        schedule_next_frame(current_tick);
}

#endif
