#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "crazypod_state.h"
#include "crazypod_soundwave.h"

#define SOUND_WAVE_POINT_SLOTS 6
#define SOUND_WAVE_MAX_POINTS 148
#define SOUND_WAVE_BALL_RADIAL_RAYS 18
#define SOUND_WAVE_BALL_PARTICLES 20

static lv_point_precise_t
    sound_wave_points[SOUND_WAVE_POINT_SLOTS][SOUND_WAVE_MAX_POINTS];
static int32_t sound_wave_taper[LCD_WIDTH];
static int sound_wave_taper_width;

static const char *const sound_wave_style_names[
    CRAZYPOD_SOUND_WAVE_STYLE_COUNT] = {
    CP_TR("Torrent"),
    CP_TR("Radial Spectrum"),
    CP_TR("Liquid Ribbon"),
    CP_TR("Vinyl Groove"),
    CP_TR("Mini LED Meter"),
    CP_TR("Particle Pulse"),
};

static int clamp_int(int value, int minimum, int maximum)
{
    if(value < minimum)
        return minimum;
    if(value > maximum)
        return maximum;
    return value;
}

static int positive_mod(int value, int modulus)
{
    value %= modulus;
    return value < 0 ? value + modulus : value;
}

static int wave_sin(int degrees)
{
    return (int)lv_trigo_sin((int16_t)positive_mod(degrees, 360));
}

static int wave_cos(int degrees)
{
    return wave_sin(degrees + 90);
}

static int wave_abs(int value)
{
    return value < 0 ? -value : value;
}

static int wave_taper(int x, int width)
{
    int index;

    if(width <= 1)
        return 0;
    if(width > LCD_WIDTH)
        return wave_sin(x * 180 / (width - 1));
    if(sound_wave_taper_width != width) {
        for(index = 0; index < width; ++index)
            sound_wave_taper[index] =
                wave_sin(index * 180 / (width - 1));
        sound_wave_taper_width = width;
    }
    return sound_wave_taper[x];
}

static int area_width(const lv_area_t *area)
{
    return (int)lv_area_get_width(area);
}

static int area_height(const lv_area_t *area)
{
    return (int)lv_area_get_height(area);
}

static void draw_rect(lv_layer_t *layer, int x, int y,
                      int width, int height, int radius,
                      uint32_t color, lv_opa_t opacity)
{
    lv_draw_rect_dsc_t rectangle;
    lv_area_t rectangle_area;

    if(width <= 0 || height <= 0 || opacity <= LV_OPA_MIN)
        return;
    lv_draw_rect_dsc_init(&rectangle);
    rectangle.base.layer = layer;
    rectangle.bg_color = lv_color_hex(color);
    rectangle.bg_opa = opacity;
    rectangle.radius = radius;
    rectangle_area.x1 = x;
    rectangle_area.y1 = y;
    rectangle_area.x2 = x + width - 1;
    rectangle_area.y2 = y + height - 1;
    lv_draw_rect(layer, &rectangle, &rectangle_area);
}

static void draw_dot(lv_layer_t *layer, int x, int y, int radius,
                     uint32_t color, lv_opa_t opacity)
{
    radius = radius < 1 ? 1 : radius;
    draw_rect(layer, x - radius, y - radius,
              radius * 2 + 1, radius * 2 + 1,
              LV_RADIUS_CIRCLE, color, opacity);
}

static void draw_line_segment(lv_layer_t *layer,
                              int x1, int y1, int x2, int y2,
                              int width, uint32_t color,
                              lv_opa_t opacity, bool rounded)
{
    lv_draw_line_dsc_t line;

    if(opacity <= LV_OPA_MIN)
        return;
    lv_draw_line_dsc_init(&line);
    line.base.layer = layer;
    line.p1.x = x1;
    line.p1.y = y1;
    line.p2.x = x2;
    line.p2.y = y2;
    line.width = width;
    line.color = lv_color_hex(color);
    line.opa = opacity;
    line.round_start = rounded;
    line.round_end = rounded;
    lv_draw_line(layer, &line);
}

static void draw_polyline(lv_layer_t *layer, int slot, int point_count,
                          int width, uint32_t color, lv_opa_t opacity)
{
    lv_draw_line_dsc_t line;

    if(slot < 0 || slot >= SOUND_WAVE_POINT_SLOTS ||
       point_count < 2 || opacity <= LV_OPA_MIN)
        return;
    lv_draw_line_dsc_init(&line);
    line.base.layer = layer;
    line.points = sound_wave_points[slot];
    line.point_cnt = point_count;
    line.width = width;
    line.color = lv_color_hex(color);
    line.opa = opacity;
    line.round_start = 1;
    line.round_end = 1;
    lv_draw_line(layer, &line);
}

static void append_wave_point(
    int slot, int *point_count, int x, int y)
{
    if(slot < 0 || slot >= SOUND_WAVE_POINT_SLOTS ||
       point_count == NULL ||
       *point_count >= SOUND_WAVE_MAX_POINTS)
        return;
    sound_wave_points[slot][*point_count].x = x;
    sound_wave_points[slot][*point_count].y = y;
    ++(*point_count);
}

static int build_wave_points(const lv_area_t *area, int slot, int step,
                             int phase_degrees, int cycle_tenths,
                             int amplitude)
{
    int width = area_width(area);
    int center_y = area->y1 + area_height(area) / 2;
    int point_count = 0;
    int x;

    if(slot < 0 || slot >= SOUND_WAVE_POINT_SLOTS || width <= 1)
        return 0;
    for(x = 0;
        x < width && point_count < SOUND_WAVE_MAX_POINTS - 1;
        x += step) {
        int taper = wave_taper(x, width);
        int angle = phase_degrees +
                    x * cycle_tenths * 36 / width;
        int vertical = wave_sin(angle) * amplitude >> LV_TRIGO_SHIFT;

        vertical = vertical * taper >> LV_TRIGO_SHIFT;
        sound_wave_points[slot][point_count].x = area->x1 + x;
        sound_wave_points[slot][point_count].y = center_y + vertical;
        ++point_count;
    }
    if(point_count < SOUND_WAVE_MAX_POINTS &&
       sound_wave_points[slot][point_count - 1].x != area->x2) {
        sound_wave_points[slot][point_count].x = area->x2;
        sound_wave_points[slot][point_count].y = center_y;
        ++point_count;
    }
    return point_count;
}

static void draw_torrent(lv_layer_t *layer, const lv_area_t *area,
                         int phase, bool playing, bool ball,
                         uint32_t primary, uint32_t secondary,
                         uint32_t highlight)
{
    int height = area_height(area);
    int primary_amp = ball ? height * 22 / 100
                           : (height * 29 / 100 * 3 + 1) / 2;
    int secondary_amp = ball ? height * 17 / 100
                             : primary_amp * 72 / 100;
    int highlight_amp = ball ? height * 12 / 100
                             : primary_amp * 44 / 100;
    int point_step = 2;
    int primary_cycles = ball ? 10 : 37;
    int secondary_cycles = ball ? 7 : 28;
    int highlight_cycles = ball ? 12 : 46;
    int count;

    if(playing) {
        primary_amp = primary_amp *
            (84 + 16 * wave_sin(phase * 5) / (1 << LV_TRIGO_SHIFT)) /
            100;
        secondary_amp = secondary_amp *
            (80 + 20 * wave_cos(phase * 4) / (1 << LV_TRIGO_SHIFT)) /
            100;
        highlight_amp = highlight_amp *
            (90 + 10 * wave_sin(phase * 7) / (1 << LV_TRIGO_SHIFT)) /
            100;
    }
    else {
        primary_amp = ball ? 1 : 2;
        secondary_amp = 1;
        highlight_amp = 0;
    }

    count = build_wave_points(
        area, 0, point_step, phase * 13,
        primary_cycles, primary_amp);
    draw_polyline(layer, 0, count, 2, primary,
                  playing ? 209 : 92);
    count = build_wave_points(
        area, 1, point_step, phase * 10 + 120,
        secondary_cycles, secondary_amp);
    draw_polyline(layer, 1, count, 2, secondary,
                  playing ? 179 : 76);
    /*
     * The high-frequency highlight reads as a needle when its crest crosses
     * the two main curves on the 320x240 bar. Keep it for the compact ball,
     * where it supplies useful separation, but not for the full-width wave.
     */
    if(ball) {
        int width = area_width(area);
        int center_x = area->x1 + width / 2;
        int center_y = area->y1 + height / 2;
        int radius = (width < height ? width : height) * 34 / 100;
        int orbit_angle = phase * 17;

        count = build_wave_points(
            area, 2, point_step, phase * 17 + 240,
            highlight_cycles, highlight_amp);
        draw_polyline(layer, 2, count, 1, highlight,
                      playing ? 151 : 55);
        if(playing)
            draw_dot(
                layer,
                center_x +
                    (wave_cos(orbit_angle) * radius >> LV_TRIGO_SHIFT),
                center_y +
                    (wave_sin(orbit_angle) * radius >> LV_TRIGO_SHIFT),
                1, highlight, 190);
    }
}

static void draw_mirrored_spectrum(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary)
{
    int width = area_width(area);
    int height = area_height(area);
    int bar_count = clamp_int(width / 7, 22, 52);
    int center_y = area->y1 + height / 2;
    int step = width / bar_count;
    int bar_width = clamp_int(step - 2, 2, 7);
    int max_height = height * 42 / 100;
    int index;

    for(index = 0; index < bar_count; ++index) {
        int progress = index * 32767 / (bar_count - 1);
        int envelope =
            7864 + (wave_sin(progress * 180 / 32767) * 76 / 100);
        int movement = wave_abs(wave_sin(index * 42 + phase * 15));
        int secondary_movement =
            wave_abs(wave_cos(index * 21 + phase * 8));
        int blend = (movement * 72 + secondary_movement * 28) / 100;
        int level = playing ? 8519 + blend * 74 / 100 : 5243;
        int bar_height =
            max_height * level / 32767 * envelope / 32767;
        int x = area->x1 + index * width / bar_count;
        uint32_t color =
            index % 3 == 0 ? secondary : primary;
        lv_opa_t opacity = playing
            ? (lv_opa_t)clamp_int(92 + level * 132 / 32767, 0, 255)
            : 82;

        bar_height = clamp_int(bar_height, 1, max_height);
        draw_rect(layer, x, center_y - bar_height,
                  bar_width, bar_height * 2 + 1,
                  LV_RADIUS_CIRCLE, color, opacity);
    }
}

static void draw_radial_spectrum(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary,
    uint32_t highlight)
{
    static int32_t direction_x[SOUND_WAVE_BALL_RADIAL_RAYS];
    static int32_t direction_y[SOUND_WAVE_BALL_RADIAL_RAYS];
    static bool directions_ready;
    int width = area_width(area);
    int height = area_height(area);
    int diameter = width < height ? width : height;
    int center_x = area->x1 + width / 2;
    int center_y = area->y1 + height / 2;
    int inner_radius = diameter * 18 / 100;
    int maximum_length = diameter * 23 / 100;
    int index;

    if(!directions_ready) {
        for(index = 0;
            index < SOUND_WAVE_BALL_RADIAL_RAYS;
            ++index) {
            int angle =
                index * 360 / SOUND_WAVE_BALL_RADIAL_RAYS;

            direction_x[index] = wave_cos(angle);
            direction_y[index] = wave_sin(angle);
        }
        directions_ready = true;
    }
    for(index = 0;
        index < SOUND_WAVE_BALL_RADIAL_RAYS;
        ++index) {
        int movement = wave_abs(wave_sin(index * 36 + phase * 13));
        int level = playing ? 7864 + movement * 76 / 100 : 5898;
        int length = clamp_int(
            maximum_length * level / 32767, 1, maximum_length);
        int start_x = center_x +
            (direction_x[index] * inner_radius >> LV_TRIGO_SHIFT);
        int start_y = center_y +
            (direction_y[index] * inner_radius >> LV_TRIGO_SHIFT);
        int end_radius = inner_radius + length;
        int end_x = center_x +
            (direction_x[index] * end_radius >> LV_TRIGO_SHIFT);
        int end_y = center_y +
            (direction_y[index] * end_radius >> LV_TRIGO_SHIFT);

        draw_line_segment(
            layer, start_x, start_y, end_x, end_y,
            2, index % 3 == 0 ? secondary : primary,
            playing ? 184 : 92, false);
    }
    draw_dot(layer, center_x, center_y, 2,
             highlight, playing ? 140 : 61);
}

static void draw_liquid_ribbon(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, bool ball,
    uint32_t primary, uint32_t secondary, uint32_t highlight)
{
    int width = area_width(area);
    int height = area_height(area);
    int center_y = area->y1 + height / 2;
    int amplitude = playing
        ? height * (ball ? 19 : 18) / 100
        : 1;
    int thickness = playing
        ? height * (ball ? 16 : 24) / 100
        : (ball ? 2 : 3);
    int step = ball ? 2 : 4;
    int point_count = 0;
    int x;

    if(playing) {
        amplitude += height * wave_sin(phase * 7) /
                     (20 * (1 << LV_TRIGO_SHIFT));
        thickness += height * wave_cos(phase * 5) /
                     (25 * (1 << LV_TRIGO_SHIFT));
    }
    amplitude = clamp_int(amplitude, 1, height / 3);
    thickness = clamp_int(thickness, 1, height / 3);

    for(x = 0; x < width; x += step) {
        int taper = width > 1
            ? wave_sin(x * 180 / (width - 1)) : 0;
        int offset = wave_sin(
            x * (ball ? 7 : 3) + phase * 9);
        int local_thickness = thickness *
            (23600 + wave_cos(x * 2 + phase * 5) * 28 / 100) /
            32767;
        int y = center_y +
            ((offset * amplitude >> LV_TRIGO_SHIFT) *
             taper >> LV_TRIGO_SHIFT);
        uint32_t color;

        local_thickness = clamp_int(local_thickness, 1, height / 3);
        if(x * 3 < width)
            color = primary;
        else if(x * 3 < width * 2)
            color = secondary;
        else
            color = highlight;
        draw_rect(layer, area->x1 + x,
                  y - local_thickness,
                  step + 1, local_thickness * 2 + 1,
                  step, color, playing ? 174 : 70);
        if(point_count < SOUND_WAVE_MAX_POINTS) {
            sound_wave_points[0][point_count].x = area->x1 + x;
            sound_wave_points[0][point_count].y = y;
            ++point_count;
        }
    }
    if(point_count < SOUND_WAVE_MAX_POINTS) {
        sound_wave_points[0][point_count].x = area->x2;
        sound_wave_points[0][point_count].y = center_y;
        ++point_count;
    }
    draw_polyline(layer, 0, point_count, 1, highlight,
                  playing ? 122 : 55);
}

static void draw_liquid_ribbon_ball(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing,
    uint32_t primary, uint32_t secondary, uint32_t highlight)
{
    int counts[4] = { 0, 0, 0, 0 };
    int width = area_width(area);
    int height = area_height(area);
    int center_y = area->y1 + height / 2;
    int amplitude = playing ? height * 19 / 100 : 1;
    int thickness = playing ? height * 16 / 100 : 2;
    int previous_x = area->x1;
    int previous_y = center_y;
    int previous_segment = -1;
    int x;

    if(playing) {
        amplitude += height * wave_sin(phase * 7) /
                     (20 * (1 << LV_TRIGO_SHIFT));
        thickness += height * wave_cos(phase * 5) /
                     (25 * (1 << LV_TRIGO_SHIFT));
    }
    amplitude = clamp_int(amplitude, 1, height / 3);
    thickness = clamp_int(thickness, 2, height / 3);

    for(x = 0; x < width; x += 2) {
        int taper = wave_taper(x, width);
        int offset = wave_sin(x * 7 + phase * 9);
        int y = center_y +
            ((offset * amplitude >> LV_TRIGO_SHIFT) *
             taper >> LV_TRIGO_SHIFT);
        int segment = clamp_int(x * 3 / width, 0, 2);
        int absolute_x = area->x1 + x;

        if(previous_segment >= 0 && segment != previous_segment)
            append_wave_point(
                segment, &counts[segment],
                previous_x, previous_y);
        append_wave_point(
            segment, &counts[segment], absolute_x, y);
        append_wave_point(3, &counts[3], absolute_x, y);
        previous_segment = segment;
        previous_x = absolute_x;
        previous_y = y;
    }
    append_wave_point(
        previous_segment, &counts[previous_segment],
        area->x2, center_y);
    append_wave_point(3, &counts[3], area->x2, center_y);

    for(x = 0; x < 3; ++x) {
        uint32_t color = x == 0 ? primary :
            (x == 1 ? secondary : highlight);

        draw_polyline(
            layer, x, counts[x], thickness * 2 + 1,
            color, playing ? 174 : 70);
    }
    draw_polyline(
        layer, 3, counts[3], 1, highlight,
        playing ? 122 : 55);
}

static void draw_vinyl_groove_bar(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary,
    uint32_t highlight)
{
    int width = area_width(area);
    int height = area_height(area);
    int column_count = clamp_int(width / 10, 21, 29);
    int center_y = area->y1 + height / 2;
    int maximum_half_height = clamp_int(height * 36 / 100, 5, 12);
    int center_gap = height >= 24 ? 2 : 1;
    int focus;
    int index;

    if(crazypod_state_reduce_motion())
        phase = 0;
    focus = positive_mod(phase, column_count);

    draw_line_segment(
        layer, area->x1 + 2, center_y,
        area->x2 - 2, center_y, 1,
        primary, playing ? 31 : 22, false);

    for(index = 0; index < column_count; ++index) {
        int progress = index * 32767 / (column_count - 1);
        int envelope = wave_sin(progress * 180 / 32767);
        int upper_motion = wave_abs(
            wave_sin(index * 31 + phase * 16));
        int lower_motion = wave_abs(
            wave_sin(index * 47 - phase * 11 + 73));
        int pulse = wave_abs(
            wave_sin(index * 19 + phase * 7 + 41));
        int upper_level =
            (upper_motion * 72 + pulse * 28) / 100;
        int lower_level =
            (lower_motion * 68 + pulse * 32) / 100;
        int upper_height;
        int lower_height;
        int distance = wave_abs(index - focus);
        int x = area->x1 +
            index * (width - 1) / (column_count - 1);
        int column_width = width >= 240 ? 3 : 2;
        bool focused;
        uint32_t upper_color;
        uint32_t lower_color;
        lv_opa_t upper_opacity;
        lv_opa_t lower_opacity;

        if(distance > column_count / 2)
            distance = column_count - distance;
        focused = playing && distance <= 1;
        if(playing) {
            upper_height = 2 + maximum_half_height *
                (6553 + upper_level * 80 / 100) /
                32767 * envelope / 32767;
            lower_height = 2 + maximum_half_height *
                (6553 + lower_level * 80 / 100) /
                32767 * envelope / 32767;
        }
        else {
            upper_height = 1 + 2 * envelope / 32767;
            lower_height = upper_height;
        }
        upper_height = clamp_int(
            upper_height, 1, maximum_half_height);
        lower_height = clamp_int(
            lower_height, 1, maximum_half_height);
        upper_color = focused ? highlight :
            (index % 5 == 0 ? secondary : primary);
        lower_color = focused ? highlight :
            (index % 5 == 0 ? primary : secondary);
        upper_opacity = focused ? 235 :
            (playing ? (lv_opa_t)(112 + envelope * 75 / 32767) : 72);
        lower_opacity = focused ? 196 :
            (playing ? (lv_opa_t)(82 + envelope * 67 / 32767) : 54);

        draw_rect(
            layer, x - column_width / 2,
            center_y - center_gap - upper_height,
            column_width, upper_height,
            LV_RADIUS_CIRCLE, upper_color, upper_opacity);
        draw_rect(
            layer, x - column_width / 2,
            center_y + center_gap,
            column_width, lower_height,
            LV_RADIUS_CIRCLE, lower_color, lower_opacity);
    }
}

static void draw_vinyl_groove_ball(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary,
    uint32_t highlight)
{
    int width = area_width(area);
    int height = area_height(area);
    int diameter = width < height ? width : height;
    int center_x = area->x1 + width / 2;
    int center_y = area->y1 + height / 2;
    int base_radius = clamp_int(diameter * 14 / 100, 3, 8);
    int rotation = playing ? phase * 3 : 0;
    int ring;

    for(ring = 0; ring < 3; ++ring) {
        lv_draw_arc_dsc_t arc;
        int start = positive_mod(
            18 + ring * 29 + rotation * (ring + 1), 360);

        lv_draw_arc_dsc_init(&arc);
        arc.base.layer = layer;
        arc.center.x = center_x;
        arc.center.y = center_y;
        arc.radius = (uint16_t)(
            base_radius + ring * diameter * 13 / 100);
        arc.start_angle = start;
        arc.end_angle = start + 310 - ring * 22;
        arc.width = ring == 1 ? 2 : 1;
        arc.rounded = 0;
        arc.color = lv_color_hex(
            ring % 2 == 0 ? primary : secondary);
        arc.opa = playing ? 122 : 61;
        lv_draw_arc(layer, &arc);
    }
    {
        lv_draw_arc_dsc_t glint;
        int start = positive_mod(238 + rotation, 360);

        lv_draw_arc_dsc_init(&glint);
        glint.base.layer = layer;
        glint.center.x = center_x;
        glint.center.y = center_y;
        glint.radius = (uint16_t)(diameter * 39 / 100);
        glint.start_angle = start;
        glint.end_angle = start + 26;
        glint.width = 2;
        glint.rounded = 1;
        glint.color = lv_color_hex(highlight);
        glint.opa = playing ? 107 : 36;
        lv_draw_arc(layer, &glint);
    }
}

static void draw_mini_led_meter(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, bool ball,
    uint32_t primary, uint32_t secondary, uint32_t highlight)
{
    int width = area_width(area);
    int height = area_height(area);
    int column_count = ball ? 7 : clamp_int(width / 10, 18, 36);
    int row_count = ball ? 5 : 4;
    int gap_x = ball ? 2 : 3;
    int gap_y = ball ? 1 : 1;
    int segment_width = ball ? clamp_int(width * 75 / 1000, 2, 4)
                             : clamp_int(
                                   (width - (column_count - 1) * gap_x) /
                                   column_count, 3, 8);
    int segment_height = ball ? clamp_int(height * 55 / 1000, 2, 3)
                              : clamp_int(
                                    (height / 2 - 4) / row_count, 2, 3);
    int total_width = column_count * segment_width +
                      (column_count - 1) * gap_x;
    int start_x = area->x1 + (width - total_width) / 2;
    int center_y = area->y1 + height / 2;
    int column;

    if(ball) {
        int maximum_half_height =
            clamp_int(height * 27 / 100, 4, height / 3);

        for(column = 0; column < column_count; ++column) {
            int movement = wave_abs(
                wave_sin(column * 44 + phase * 13));
            int half_height = playing
                ? 3 + movement * (maximum_half_height - 3) / 32767
                : 3;
            int x = start_x +
                column * (segment_width + gap_x);
            uint32_t color = column == column_count / 2
                ? highlight
                : (column % 2 == 0 ? secondary : primary);

            draw_rect(
                layer, x, center_y - half_height,
                segment_width, half_height * 2 + 1,
                LV_RADIUS_CIRCLE, color,
                playing ? 205 : 82);
        }
        return;
    }

    for(column = 0; column < column_count; ++column) {
        int movement = wave_abs(
            wave_sin(column * (ball ? 44 : 33) + phase * 13));
        int level = playing
            ? clamp_int((movement * row_count + 32766) / 32767,
                        1, row_count)
            : 1;
        int x = start_x + column * (segment_width + gap_x);
        uint32_t color = ball && column == column_count / 2
            ? 0xFFFFFF
            : (column % (ball ? 2 : 4) == 0 ? secondary : primary);
        int row;

        for(row = 0; row < row_count; ++row) {
            bool lit = row < level;
            lv_opa_t opacity = lit
                ? (playing ? 219 : 117) : 30;
            int upper_y = center_y -
                (row + 1) * segment_height - row * gap_y - 1;
            int lower_y = center_y +
                row * segment_height + (row + 1) * gap_y;

            draw_rect(layer, x, upper_y,
                      segment_width, segment_height,
                      LV_RADIUS_CIRCLE, color, opacity);
            draw_rect(layer, x, lower_y,
                      segment_width, segment_height,
                      LV_RADIUS_CIRCLE, color,
                      (lv_opa_t)(opacity * 82 / 100));
        }
    }
}

static void draw_particle_pulse_bar(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary,
    uint32_t highlight)
{
    int width = area_width(area);
    int height = area_height(area);
    int particle_count = clamp_int(width / 5, 34, 82);
    int center_y = area->y1 + height / 2;
    int amplitude = playing ? height * 32 / 100
                            : height * 4 / 100;
    int index;

    for(index = 0; index < particle_count; ++index) {
        int progress = index * 32767 / (particle_count - 1);
        int x = area->x1 + index * (width - 1) /
                            (particle_count - 1);
        int lane = index % 3;
        int taper = wave_sin(progress * 180 / 32767);
        int movement = wave_sin(
            x * 3 + phase * (6 + lane * 2) + lane * 97);
        int y = center_y +
            ((movement * amplitude >> LV_TRIGO_SHIFT) *
             taper >> LV_TRIGO_SHIFT);
        int sparkle =
            16384 + wave_sin(phase * 10 + index * 29) / 2;
        int radius = playing && sparkle > 22000 ? 2 : 1;
        uint32_t color = index % 3 == 0 ? highlight :
            (index % 2 == 0 ? secondary : primary);
        lv_opa_t opacity = playing
            ? (lv_opa_t)clamp_int(89 + sparkle * 122 / 32767, 0, 255)
            : 61;

        draw_dot(layer, x, y, radius, color, opacity);
    }
}

static void draw_particle_pulse_ball(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary,
    uint32_t highlight)
{
    static int32_t base_x[SOUND_WAVE_BALL_PARTICLES];
    static int32_t base_y[SOUND_WAVE_BALL_PARTICLES];
    static bool directions_ready;
    int lane_cos[4];
    int lane_sin[4];
    int lane_pulse[4];
    int width = area_width(area);
    int height = area_height(area);
    int diameter = width < height ? width : height;
    int center_x = area->x1 + width / 2;
    int center_y = area->y1 + height / 2;
    int maximum_radius = diameter * 39 / 100;
    int index;

    if(!directions_ready) {
        for(index = 0;
            index < SOUND_WAVE_BALL_PARTICLES;
            ++index) {
            int angle = index * 137;

            base_x[index] = wave_cos(angle);
            base_y[index] = wave_sin(angle);
        }
        directions_ready = true;
    }
    for(index = 0; index < 4; ++index) {
        if(playing) {
            int rotation = phase * (2 + index);

            lane_cos[index] = wave_cos(rotation);
            lane_sin[index] = wave_sin(rotation);
            lane_pulse[index] =
                16384 + wave_sin(phase * 8 + index * 67) / 2;
        }
        else {
            lane_cos[index] = 32767;
            lane_sin[index] = 0;
            lane_pulse[index] = 16384;
        }
    }
    for(index = 0;
        index < SOUND_WAVE_BALL_PARTICLES;
        ++index) {
        int lane = index % 4;
        int direction_x = (int)(
            ((int64_t)base_x[index] * lane_cos[lane] -
             (int64_t)base_y[index] * lane_sin[lane]) >>
            LV_TRIGO_SHIFT);
        int direction_y = (int)(
            ((int64_t)base_x[index] * lane_sin[lane] +
             (int64_t)base_y[index] * lane_cos[lane]) >>
            LV_TRIGO_SHIFT);
        int pulse = lane_pulse[lane];
        int radius = maximum_radius *
            (18 + 78 * (index % 11) / 10) / 100;
        int orbit = radius + (playing ? pulse * 2 / 32767 : 0);
        int x = center_x +
            (direction_x * orbit >> LV_TRIGO_SHIFT);
        int y = center_y +
            (direction_y * orbit >> LV_TRIGO_SHIFT);
        int dot_radius = playing && pulse > 23000 ? 2 : 1;
        uint32_t color = index % 5 == 0 ? highlight :
            (index % 2 == 0 ? secondary : primary);
        lv_opa_t opacity = playing
            ? (lv_opa_t)clamp_int(82 + pulse * 117 / 32767, 0, 255)
            : 51;

        draw_dot(layer, x, y, dot_radius, color, opacity);
    }
}

const char *crazypod_sound_wave_style_name(int style)
{
    return style >= 0 && style < CRAZYPOD_SOUND_WAVE_STYLE_COUNT
        ? sound_wave_style_names[style] : "";
}

void crazypod_sound_wave_draw_bar(
    lv_layer_t *layer, const lv_area_t *area,
    enum crazypod_sound_wave_style style, int phase, bool playing,
    uint32_t primary_color, uint32_t secondary_color,
    uint32_t highlight_color)
{
    if(layer == NULL || area == NULL)
        return;
    switch(style) {
    case CRAZYPOD_SOUND_WAVE_TORRENT:
        draw_torrent(layer, area, phase, playing, false,
                     primary_color, secondary_color, highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_RADIAL_SPECTRUM:
        draw_mirrored_spectrum(
            layer, area, phase, playing,
            primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_LIQUID_RIBBON:
        draw_liquid_ribbon(layer, area, phase, playing, false,
                           primary_color, secondary_color,
                           highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_VINYL_GROOVE:
        draw_vinyl_groove_bar(
            layer, area, phase, playing,
            primary_color, secondary_color, highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_MINI_LED_METER:
        draw_mini_led_meter(layer, area, phase, playing, false,
                            primary_color, secondary_color,
                            highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_PARTICLE_PULSE:
        draw_particle_pulse_bar(
            layer, area, phase, playing,
            primary_color, secondary_color, highlight_color);
        break;
    }
}

void crazypod_sound_wave_draw_ball(
    lv_layer_t *layer, const lv_area_t *area,
    enum crazypod_sound_wave_style style, int phase, bool playing,
    uint32_t primary_color, uint32_t secondary_color,
    uint32_t highlight_color)
{
    if(layer == NULL || area == NULL)
        return;
    switch(style) {
    case CRAZYPOD_SOUND_WAVE_TORRENT:
        draw_torrent(layer, area, phase, playing, true,
                     primary_color, secondary_color, highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_RADIAL_SPECTRUM:
        draw_radial_spectrum(
            layer, area, phase, playing,
            primary_color, secondary_color, highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_LIQUID_RIBBON:
        draw_liquid_ribbon_ball(
            layer, area, phase, playing,
            primary_color, secondary_color, highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_VINYL_GROOVE:
        draw_vinyl_groove_ball(
            layer, area, phase, playing,
            primary_color, secondary_color, highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_MINI_LED_METER:
        draw_mini_led_meter(layer, area, phase, playing, true,
                            primary_color, secondary_color,
                            highlight_color);
        break;
    case CRAZYPOD_SOUND_WAVE_PARTICLE_PULSE:
        draw_particle_pulse_ball(
            layer, area, phase, playing,
            primary_color, secondary_color, highlight_color);
        break;
    }
}

#endif
