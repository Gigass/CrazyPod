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
#include "crazypod_music.h"

#define FLOW_CACHE_SLOTS 15
#define FLOW_PREFETCH_AHEAD 9
#define FLOW_PREFETCH_BEHIND 5
#define FLOW_COVER_SIZE 120
#define FLOW_TOP 40
#define FLOW_BOTTOM 194
#define FLOW_FRAME_TICKS ((HZ / 50) > 0 ? (HZ / 50) : 1)
#define FLOW_SPRING_SCALE 4096
#define FLOW_SPRING_ERROR 3844
#define FLOW_SPRING_VELOCITY 2746
#define FLOW_VELOCITY_ERROR -439
#define FLOW_VELOCITY_DECAY 1647
#define FLOW_MAX_VELOCITY_Q8 48
#define FLOW_MAX_STEP_Q8 40
#define FLOW_MAX_BURST_LAG_Q8 128
#define FLOW_INPUT_BURST_TICKS \
    ((HZ * 120 / 1000) > 0 ? (HZ * 120 / 1000) : 1)
#define FLOW_SNAP_Q8 8

struct flow_cache_entry {
    int album_index;
    const lv_image_dsc_t *image;
};

static struct flow_cache_entry cache[FLOW_CACHE_SLOTS];
static bool flow_active;
static bool flow_dirty;
static int selected_album;
static int position_q8;
static int target_position_q8;
static int velocity_q8;
static int flow_direction;
static long last_input;
static long last_render;
static long last_prefetch;
static bool cache_initialized;

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
    fb_data color = LCD_RGBPACK(5, 5, 9);
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
    if(flow_direction >= 0) {
        return index >= selected_album - FLOW_PREFETCH_BEHIND &&
               index <= selected_album + FLOW_PREFETCH_AHEAD;
    }
    return index >= selected_album - FLOW_PREFETCH_AHEAD &&
           index <= selected_album + FLOW_PREFETCH_BEHIND;
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
    int i;

    for(i = 0; i < FLOW_CACHE_SLOTS; ++i) {
        int distance;
        if(cache[i].album_index < 0)
            return &cache[i];
        if(!index_is_wanted(cache[i].album_index))
            return &cache[i];
        distance = cache[i].album_index - selected_album;
        if(distance < 0)
            distance = -distance;
        if(distance > farthest_distance) {
            farthest = &cache[i];
            farthest_distance = distance;
        }
    }
    (void)album_index;
    return farthest;
}

static void prefetch_covers(void)
{
    int count = crazypod_music_album_count();
    int direction = flow_direction == 0 ? 1 : flow_direction;
    int order[FLOW_CACHE_SLOTS];
    int order_count = 0;
    int distance;
    int candidate_number;

    order[order_count++] = selected_album;
    for(distance = 1;
        distance <= FLOW_PREFETCH_AHEAD &&
        order_count < FLOW_CACHE_SLOTS;
        ++distance) {
        order[order_count++] = selected_album + direction * distance;
        if(distance <= FLOW_PREFETCH_BEHIND &&
           order_count < FLOW_CACHE_SLOTS)
            order[order_count++] =
                selected_album - direction * distance;
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
            ? crazypod_artwork_load_priority(
                slot, track, FLOW_COVER_SIZE, candidate_number)
            : NULL;
        if(image != NULL && entry->image != image) {
            entry->image = image;
            flow_dirty = true;
        }
    }
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

static void draw_placeholder(int center_x, int center_y,
                             int width, int height, int alpha,
                             int album_index)
{
    fb_data *pixels = framebuffer();
    uint32_t rgb = album_color(album_index);
    fb_data color = LCD_RGBPACK((rgb >> 16) & 0xff,
                                (rgb >> 8) & 0xff,
                                rgb & 0xff);
    int left = center_x - width / 2;
    int top = center_y - height / 2;
    int y;

    for(y = 0; y < height; ++y) {
        int py = top + y;
        int x;
        if(py < FLOW_TOP || py >= FLOW_BOTTOM)
            continue;
        for(x = 0; x < width; ++x) {
            int px = left + x;
            fb_data *destination;
            if(px < 0 || px >= LCD_WIDTH)
                continue;
            destination = pixels + py * LCD_WIDTH + px;
            *destination = blend565(color, *destination, alpha);
        }
    }
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

static int motion_curve_q8(int value_q8)
{
    return (value_q8 + smoothstep_q8(value_q8) + 1) / 2;
}

static inline fb_data sample_rgb565_bilinear(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x_q16, int source_y_q16)
{
    int sx = source_x_q16 >> 16;
    int sy = source_y_q16 >> 16;
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

static void draw_image(const lv_image_dsc_t *image,
                       int center_x, int center_y,
                       int width, int height, int alpha, int side,
                       int perspective_q8)
{
    const fb_data *source;
    fb_data *pixels = framebuffer();
    int source_width;
    int source_height;
    int source_stride;
    int left = center_x - width / 2;
    int x;
    int source_x_q16;
    int source_x_step;

    if(image == NULL || image->header.cf != LV_COLOR_FORMAT_RGB565)
        return;
    source = (const fb_data *)image->data;
    source_width = image->header.w;
    source_height = image->header.h;
    source_stride = image->header.stride / sizeof(fb_data);
    source_x_step = (source_width << 16) / width;
    source_x_q16 =
        ((source_width << 15) / width) - 32768;

    for(x = 0; x < width; ++x) {
        int px = left + x;
        int sample_x_q16 = source_x_q16;
        int perspective = side == 0 ? 0 :
            (side > 0 ? width - 1 - x : x);
        int max_perspective_drop =
            height * perspective_q8 / (7 * 256);
        int column_height = height -
            perspective * max_perspective_drop /
                (width > 1 ? width - 1 : 1);
        int top = center_y - column_height / 2;
        int source_y_q16 =
            ((source_height << 15) / column_height) - 32768;
        int source_y_step = (source_height << 16) / column_height;
        int y;

        source_x_q16 += source_x_step;
        if(px < 0 || px >= LCD_WIDTH)
            continue;
        for(y = 0; y < column_height; ++y) {
            int py = top + y;
            fb_data *destination;
            fb_data color;
            if(py < FLOW_TOP || py >= FLOW_BOTTOM) {
                source_y_q16 += source_y_step;
                continue;
            }
            color = sample_rgb565_bilinear(
                source, source_width, source_height,
                source_stride, sample_x_q16, source_y_q16);
            source_y_q16 += source_y_step;
            destination = pixels + py * LCD_WIDTH + px;
            *destination = alpha == 255
                ? color : blend565(color, *destination, alpha);
        }

        source_y_q16 = (source_height - 1) << 16;
        source_y_step = (source_height << 16) / 24;
        for(y = 0; y < 12; ++y) {
            int py = top + column_height + 2 + y;
            int reflection_alpha = alpha * (36 - y * 3) >> 8;
            fb_data *destination;
            fb_data color;
            source_y_q16 -= source_y_step;
            if(py < FLOW_TOP || py >= FLOW_BOTTOM ||
               reflection_alpha <= 0)
                continue;
            color = sample_rgb565_bilinear(
                source, source_width, source_height,
                source_stride, sample_x_q16, source_y_q16);
            destination = pixels + py * LCD_WIDTH + px;
            *destination = blend565(
                color, *destination, reflection_alpha);
        }
    }
}

static void draw_album(int album_index)
{
    int relative_q8 = album_index * 256 - position_q8;
    int absolute_q8 = relative_q8 < 0 ? -relative_q8 : relative_q8;
    int direction = relative_q8 < 0 ? -1 : 1;
    int center_x;
    int center_y;
    int width;
    int height;
    int alpha;
    int perspective_q8;
    const lv_image_dsc_t *image;

    if(absolute_q8 > 560)
        return;
    if(absolute_q8 <= 256) {
        int pose_q8 = motion_curve_q8(absolute_q8);

        center_x = 160 + direction * (115 * pose_q8 / 256);
        center_y = 108 + 4 * pose_q8 / 256;
        width = 120 - 58 * pose_q8 / 256;
        height = 120 - 34 * pose_q8 / 256;
        alpha = 255 - 105 * pose_q8 / 256;
        perspective_q8 = smoothstep_q8(absolute_q8);
    }
    else {
        int outer = absolute_q8 - 256;
        int pose_q8 = motion_curve_q8(outer);

        if(outer > 256)
            pose_q8 += outer - 256;
        center_x = 160 + direction * (115 + 68 * pose_q8 / 256);
        center_y = 112 + 2 * pose_q8 / 256;
        width = 62 - 22 * pose_q8 / 256;
        height = 86 - 20 * pose_q8 / 256;
        alpha = 150 - 110 * pose_q8 / 256;
        perspective_q8 = 256;
    }

    image = cached_image(album_index);
    if(image != NULL) {
        draw_image(image, center_x, center_y, width, height,
                   alpha, direction, perspective_q8);
    }
    else {
        draw_placeholder(center_x, center_y, width, height,
                         alpha, album_index);
    }
}

static void render_flow(void)
{
    int count = crazypod_music_album_count();
    int center = (position_q8 + 128) / 256;
    int distance;

    clear_flow_area();
    for(distance = 3; distance >= 1; --distance) {
        int left = center - distance;
        int right = center + distance;
        if(left >= 0 && left < count)
            draw_album(left);
        if(right >= 0 && right < count)
            draw_album(right);
    }
    if(center >= 0 && center < count)
        draw_album(center);
    lcd_update_rect(0, FLOW_TOP, LCD_WIDTH, FLOW_BOTTOM - FLOW_TOP);
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
    position_q8 = selected * 256;
    target_position_q8 = position_q8;
    velocity_q8 = 0;
    flow_direction = 1;
    last_input = current_tick - FLOW_INPUT_BURST_TICKS - 1;
    last_render = 0;
    last_prefetch = current_tick;
    flow_active = true;
    flow_dirty = true;
    prefetch_covers();
}

void crazypod_coverflow_warm(int selected)
{
    int count = crazypod_music_album_count();

    if(count <= 0 || flow_active)
        return;
    if(selected < 0)
        selected = 0;
    if(selected >= count)
        selected = count - 1;
    if(!cache_initialized)
        reset_cache();
    selected_album = selected;
    flow_direction = 1;
    prefetch_covers();
    last_prefetch = current_tick;
}

void crazypod_coverflow_leave(void)
{
    flow_active = false;
    velocity_q8 = 0;
}

bool crazypod_coverflow_active(void)
{
    return flow_active;
}

int crazypod_coverflow_step(int direction)
{
    int count = crazypod_music_album_count();
    int next = selected_album + direction;
    bool burst_input;
    bool retargeting = position_q8 != target_position_q8;

    if(next < 0)
        next = 0;
    if(next >= count)
        next = count - 1;
    if(next == selected_album)
        return selected_album;

    flow_direction = direction < 0 ? -1 : 1;
    target_position_q8 = next * 256;
    selected_album = next;
    burst_input =
        retargeting || direction < -1 || direction > 1 ||
        current_tick - last_input <= FLOW_INPUT_BURST_TICKS;
    if(burst_input) {
        int lag_q8 = target_position_q8 - position_q8;
        if(lag_q8 > FLOW_MAX_BURST_LAG_Q8)
            position_q8 = target_position_q8 - FLOW_MAX_BURST_LAG_Q8;
        else if(lag_q8 < -FLOW_MAX_BURST_LAG_Q8)
            position_q8 = target_position_q8 + FLOW_MAX_BURST_LAG_Q8;
    }
    if((direction > 0 && velocity_q8 < 0) ||
       (direction < 0 && velocity_q8 > 0))
        velocity_q8 = 0;
    last_input = current_tick;
    flow_dirty = true;
    /*
     * A fast wheel gesture may update the target several times in one UI
     * pass. Defer requests to tick() so only the final neighbourhood reaches
     * the decoder instead of wasting time on covers already passed.
     */
    last_prefetch = 0;
    return selected_album;
}

static void advance_position(void)
{
    int error_q8 = position_q8 - target_position_q8;
    int movement_q8;
    int next_velocity_q8;
    int64_t spring_value;

    if(error_q8 == 0) {
        velocity_q8 = 0;
        return;
    }

    spring_value =
        (int64_t)FLOW_VELOCITY_ERROR * error_q8 +
        (int64_t)FLOW_VELOCITY_DECAY * velocity_q8;
    next_velocity_q8 = (int)(spring_value / FLOW_SPRING_SCALE);
    if(next_velocity_q8 > FLOW_MAX_VELOCITY_Q8)
        next_velocity_q8 = FLOW_MAX_VELOCITY_Q8;
    else if(next_velocity_q8 < -FLOW_MAX_VELOCITY_Q8)
        next_velocity_q8 = -FLOW_MAX_VELOCITY_Q8;

    spring_value =
        (int64_t)(FLOW_SPRING_ERROR - FLOW_SPRING_SCALE) * error_q8 +
        (int64_t)FLOW_SPRING_VELOCITY * velocity_q8;
    movement_q8 = (int)(spring_value / FLOW_SPRING_SCALE);
    if(movement_q8 > FLOW_MAX_STEP_Q8)
        movement_q8 = FLOW_MAX_STEP_Q8;
    else if(movement_q8 < -FLOW_MAX_STEP_Q8)
        movement_q8 = -FLOW_MAX_STEP_Q8;
    else if(movement_q8 == 0)
        movement_q8 = error_q8 < 0 ? 1 : -1;

    if((error_q8 < 0 && movement_q8 >= -error_q8) ||
       (error_q8 > 0 && movement_q8 <= -error_q8)) {
        position_q8 = target_position_q8;
        velocity_q8 = 0;
        return;
    }

    position_q8 += movement_q8;
    velocity_q8 = next_velocity_q8;
    error_q8 = position_q8 - target_position_q8;
    if((error_q8 < 0 ? -error_q8 : error_q8) <= FLOW_SNAP_Q8 &&
       (velocity_q8 < 0 ? -velocity_q8 : velocity_q8) <= FLOW_SNAP_Q8) {
        position_q8 = target_position_q8;
        velocity_q8 = 0;
    }
}

void crazypod_coverflow_tick(void)
{
    bool animating;

    if(!flow_active)
        return;
    if(last_prefetch == 0 ||
       current_tick - last_prefetch >= FLOW_FRAME_TICKS) {
        prefetch_covers();
        last_prefetch = current_tick;
    }
    animating = position_q8 != target_position_q8;
    if((flow_dirty || animating) &&
       (last_render == 0 ||
        current_tick - last_render >= FLOW_FRAME_TICKS)) {
        if(animating)
            advance_position();
        render_flow();
        last_render = current_tick;
        flow_dirty = false;
    }
}

#endif
