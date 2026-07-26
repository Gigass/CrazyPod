#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "crazypod_soundwave.h"

#define SOUND_WAVE_POINT_SLOTS 6
#define SOUND_WAVE_MAX_POINTS 148

static lv_point_precise_t
    sound_wave_points[SOUND_WAVE_POINT_SLOTS][SOUND_WAVE_MAX_POINTS];

static const char *const sound_wave_style_names[
    CRAZYPOD_SOUND_WAVE_STYLE_COUNT] = {
    "Torrent",
    "Radial Spectrum",
    "Liquid Ribbon",
    "Vinyl Groove",
    "Mini LED Meter",
    "Particle Pulse",
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
        int taper = wave_sin(x * 180 / (width - 1));
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
                         uint32_t primary, uint32_t secondary)
{
    int height = area_height(area);
    int primary_amp = ball ? height * 22 / 100
                           : height * 29 / 100;
    int secondary_amp = ball ? height * 17 / 100
                             : primary_amp * 72 / 100;
    int highlight_amp = ball ? height * 12 / 100
                             : primary_amp * 44 / 100;
    int point_step = ball ? 1 : 2;
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
    count = build_wave_points(
        area, 2, point_step, phase * 17 + 240,
        highlight_cycles, highlight_amp);
    draw_polyline(layer, 2, count, 1, 0xFFFFFF,
                  playing ? 151 : 55);
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
    draw_rect(layer, area->x1, center_y,
              width, 1, LV_RADIUS_CIRCLE, 0xFFFFFF, 61);
}

static void draw_radial_spectrum(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary)
{
    int width = area_width(area);
    int height = area_height(area);
    int diameter = width < height ? width : height;
    int center_x = area->x1 + width / 2;
    int center_y = area->y1 + height / 2;
    int inner_radius = diameter * 18 / 100;
    int maximum_length = diameter * 23 / 100;
    int bar_count = diameter < 48 ? 36 : 44;
    int index;

    for(index = 0; index < bar_count; ++index) {
        int angle = index * 360 / bar_count;
        int movement = wave_abs(wave_sin(index * 36 + phase * 13));
        int level = playing ? 7864 + movement * 76 / 100 : 5898;
        int length = clamp_int(
            maximum_length * level / 32767, 1, maximum_length);
        int start_x = center_x +
            (wave_cos(angle) * inner_radius >> LV_TRIGO_SHIFT);
        int start_y = center_y +
            (wave_sin(angle) * inner_radius >> LV_TRIGO_SHIFT);
        int end_radius = inner_radius + length;
        int end_x = center_x +
            (wave_cos(angle) * end_radius >> LV_TRIGO_SHIFT);
        int end_y = center_y +
            (wave_sin(angle) * end_radius >> LV_TRIGO_SHIFT);

        draw_line_segment(
            layer, start_x, start_y, end_x, end_y,
            2, index % 3 == 0 ? secondary : primary,
            playing ? 184 : 92, true);
    }
    draw_dot(layer, center_x, center_y, 2,
             0xFFFFFF, playing ? 140 : 61);
}

static void draw_liquid_ribbon(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, bool ball,
    uint32_t primary, uint32_t secondary)
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
            color = 0xFFFFFF;
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
    draw_polyline(layer, 0, point_count, 1, 0xFFFFFF,
                  playing ? 122 : 55);
}

static void draw_vinyl_groove_bar(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary)
{
    int height = area_height(area);
    int center_y = area->y1 + height / 2;
    int spacing = clamp_int(height / 9, 2, 5);
    int amplitude = playing ? clamp_int(height * 6 / 100, 1, 2) : 0;
    int line;

    for(line = 0; line < 5; ++line) {
        lv_area_t line_area = *area;
        int offset = (line - 2) * spacing;
        int count;
        uint32_t color = line == 2 ? primary :
            (line % 2 == 0 ? secondary : 0xFFFFFF);

        line_area.y1 = center_y + offset - amplitude - 1;
        line_area.y2 = center_y + offset + amplitude + 1;
        count = build_wave_points(
            &line_area, line, 3,
            phase * 6 + line * 57, 25, amplitude);
        draw_polyline(layer, line, count,
                      line == 2 ? 2 : 1, color,
                      playing ? (line == 2 ? 184 : 87) : 55);
    }
}

static void draw_vinyl_groove_ball(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary)
{
    int width = area_width(area);
    int height = area_height(area);
    int diameter = width < height ? width : height;
    int center_x = area->x1 + width / 2;
    int center_y = area->y1 + height / 2;
    int base_radius = clamp_int(diameter * 14 / 100, 3, 8);
    int rotation = playing ? phase * 5 : 0;
    int ring;

    for(ring = 0; ring < 6; ++ring) {
        lv_draw_arc_dsc_t arc;
        int start = positive_mod(18 + ring * 13 + rotation, 360);

        lv_draw_arc_dsc_init(&arc);
        arc.base.layer = layer;
        arc.center.x = center_x;
        arc.center.y = center_y;
        arc.radius = (uint16_t)(
            base_radius + ring * diameter * 55 / 1000);
        arc.start_angle = start;
        arc.end_angle = start + 280 - ring * 4;
        arc.width = ring == 2 ? 2 : 1;
        arc.rounded = 1;
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
        glint.color = lv_color_white();
        glint.opa = playing ? 107 : 36;
        lv_draw_arc(layer, &glint);
    }
}

static void draw_mini_led_meter(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, bool ball,
    uint32_t primary, uint32_t secondary)
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
    int phase, bool playing, uint32_t primary, uint32_t secondary)
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
        uint32_t color = index % 3 == 0 ? 0xFFFFFF :
            (index % 2 == 0 ? secondary : primary);
        lv_opa_t opacity = playing
            ? (lv_opa_t)clamp_int(89 + sparkle * 122 / 32767, 0, 255)
            : 61;

        draw_dot(layer, x, y, radius, color, opacity);
    }
}

static void draw_particle_pulse_ball(
    lv_layer_t *layer, const lv_area_t *area,
    int phase, bool playing, uint32_t primary, uint32_t secondary)
{
    int width = area_width(area);
    int height = area_height(area);
    int diameter = width < height ? width : height;
    int center_x = area->x1 + width / 2;
    int center_y = area->y1 + height / 2;
    int maximum_radius = diameter * 39 / 100;
    int particle_count = diameter < 48 ? 46 : 62;
    int index;

    for(index = 0; index < particle_count; ++index) {
        int lane = index % 4;
        int angle = index * 137 +
            (playing ? phase * (2 + lane) : 0);
        int pulse = 16384 +
            wave_sin(phase * 8 + index * 24) / 2;
        int radius = maximum_radius *
            (18 + 78 * (index % 17) / 16) / 100;
        int orbit = radius + (playing ? pulse * 2 / 32767 : 0);
        int x = center_x +
            (wave_cos(angle) * orbit >> LV_TRIGO_SHIFT);
        int y = center_y +
            (wave_sin(angle) * orbit >> LV_TRIGO_SHIFT);
        int dot_radius = playing && pulse > 23000 ? 2 : 1;
        uint32_t color = index % 5 == 0 ? 0xFFFFFF :
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
    uint32_t primary_color, uint32_t secondary_color)
{
    if(layer == NULL || area == NULL)
        return;
    switch(style) {
    case CRAZYPOD_SOUND_WAVE_TORRENT:
        draw_torrent(layer, area, phase, playing, false,
                     primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_RADIAL_SPECTRUM:
        draw_mirrored_spectrum(
            layer, area, phase, playing,
            primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_LIQUID_RIBBON:
        draw_liquid_ribbon(layer, area, phase, playing, false,
                           primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_VINYL_GROOVE:
        draw_vinyl_groove_bar(
            layer, area, phase, playing,
            primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_MINI_LED_METER:
        draw_mini_led_meter(layer, area, phase, playing, false,
                            primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_PARTICLE_PULSE:
        draw_particle_pulse_bar(
            layer, area, phase, playing,
            primary_color, secondary_color);
        break;
    }
}

void crazypod_sound_wave_draw_ball(
    lv_layer_t *layer, const lv_area_t *area,
    enum crazypod_sound_wave_style style, int phase, bool playing,
    uint32_t primary_color, uint32_t secondary_color)
{
    if(layer == NULL || area == NULL)
        return;
    switch(style) {
    case CRAZYPOD_SOUND_WAVE_TORRENT:
        draw_torrent(layer, area, phase, playing, true,
                     primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_RADIAL_SPECTRUM:
        draw_radial_spectrum(
            layer, area, phase, playing,
            primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_LIQUID_RIBBON:
        draw_liquid_ribbon(layer, area, phase, playing, true,
                           primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_VINYL_GROOVE:
        draw_vinyl_groove_ball(
            layer, area, phase, playing,
            primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_MINI_LED_METER:
        draw_mini_led_meter(layer, area, phase, playing, true,
                            primary_color, secondary_color);
        break;
    case CRAZYPOD_SOUND_WAVE_PARTICLE_PULSE:
        draw_particle_pulse_ball(
            layer, area, phase, playing,
            primary_color, secondary_color);
        break;
    }
}

#endif
