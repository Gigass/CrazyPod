#include "config.h"

#include "../../crazypod_l10n.h"

#if defined(IPOD_6G) && defined(HAVE_HEADPHONE_DETECTION)

#include <string.h>

#include "button.h"

#include "lvgl.h"

#include "../../crazypod_state.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_desktop_native.h"
#include "crazypod_headphone_popup.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_CERAMIC_TOP 0xFAFAFB
#define COLOR_CERAMIC_BOTTOM 0xD9DBDF
#define COLOR_CERAMIC_EDGE 0xB7BBC2
#define COLOR_INNER 0x111217
#define COLOR_GRAPHITE 0x202229
#define COLOR_GRAPHITE_LIGHT 0x626773
#define COLOR_GRAPHITE_EDGE 0xAEB3BD
#define COLOR_CABLE 0xD5D8DF
#define COLOR_GOLD 0xD9B45B
#define COLOR_GREEN 0x5CCA6B

#define PANEL_WIDTH 210
#define PANEL_HEIGHT 190
#define PANEL_X ((LCD_WIDTH - PANEL_WIDTH) / 2)
#define PANEL_Y ((LCD_HEIGHT - PANEL_HEIGHT) / 2)
#define DEVICE_X 10
#define DEVICE_Y 31
#define DEVICE_WIDTH 190
#define DEVICE_HEIGHT 128
#define ENTRANCE_MS 450
#define DEVICE_ENTRANCE_MS 520
#define CONNECTION_MS 1200
#define CONNECTION_REVEAL_MS 480
#define HOVER_START_MS 1800
#define HOVER_PERIOD_MS 2400
#define HOLD_END_MS 5500
#define EXIT_MS 300
#define REDUCED_HOLD_MS 3500

struct headphone_popup_state {
    lv_obj_t *parent;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *surface;
    lv_obj_t *title;
    lv_obj_t *status;
    lv_timer_t *hide_timer;
    struct crazypod_headphone_popup_callbacks callbacks;
    enum crazypod_headphone_popup_style style;
    int timeline_ms;
    int exit_origin_y;
    int exit_origin_scale;
    int exit_origin_opacity;
    bool ui_ready;
    bool inserted;
    bool connected_copy;
    bool dismissing;
    bool teardown_pending;
};

static struct headphone_popup_state popup;

static int clamp_int(int value, int minimum, int maximum)
{
    if(value < minimum)
        return minimum;
    if(value > maximum)
        return maximum;
    return value;
}

static int progress_between(int value, int start, int duration)
{
    if(duration <= 0)
        return value >= start ? 1024 : 0;
    return clamp_int((value - start) * 1024 / duration, 0, 1024);
}

static int lerp(int from, int to, int progress)
{
    return from + (to - from) * progress / 1024;
}

static int ease_out(int progress)
{
    int inverse = 1024 - clamp_int(progress, 0, 1024);

    return 1024 - inverse * inverse / 1024;
}

static int ease_in(int progress)
{
    int value = clamp_int(progress, 0, 1024);

    return value * value / 1024;
}

static int smooth_step(int progress)
{
    int value = clamp_int(progress, 0, 1024);
    int squared = value * value / 1024;

    return squared * (3072 - 2 * value) / 1024;
}

static int back_out(int progress)
{
    int shifted = clamp_int(progress, 0, 1024) - 1024;
    int squared = shifted * shifted / 1024;
    int cubed = squared * shifted / 1024;

    return 1024 + (2766 * cubed + 1742 * squared) / 1024;
}

static int sine(int degrees)
{
    degrees %= 360;
    if(degrees < 0)
        degrees += 360;
    return (int)lv_trigo_sin((int16_t)degrees);
}

static lv_opa_t scaled_opacity(int opacity, int progress)
{
    return (lv_opa_t)clamp_int(
        opacity * clamp_int(progress, 0, 1024) / 1024,
        LV_OPA_TRANSP, LV_OPA_COVER);
}

static void draw_rect(
    lv_layer_t *layer, int x, int y, int width, int height,
    int radius, uint32_t top, uint32_t bottom,
    lv_opa_t opacity, int border_width, uint32_t border,
    lv_opa_t border_opacity)
{
    lv_draw_rect_dsc_t rectangle;
    lv_area_t area;

    if(width <= 0 || height <= 0 || opacity <= LV_OPA_MIN)
        return;
    lv_draw_rect_dsc_init(&rectangle);
    rectangle.base.layer = layer;
    rectangle.radius = radius;
    rectangle.bg_color = lv_color_hex(top);
    rectangle.bg_opa = opacity;
    if(top != bottom) {
        rectangle.bg_grad.dir = LV_GRAD_DIR_VER;
        rectangle.bg_grad.stops[0].color = lv_color_hex(top);
        rectangle.bg_grad.stops[0].opa = opacity;
        rectangle.bg_grad.stops[0].frac = 0;
        rectangle.bg_grad.stops[1].color = lv_color_hex(bottom);
        rectangle.bg_grad.stops[1].opa = opacity;
        rectangle.bg_grad.stops[1].frac = 255;
        rectangle.bg_grad.stops_count = 2;
    }
    rectangle.border_width = border_width;
    rectangle.border_color = lv_color_hex(border);
    rectangle.border_opa = border_opacity;
    area.x1 = x;
    area.y1 = y;
    area.x2 = x + width - 1;
    area.y2 = y + height - 1;
    lv_draw_rect(layer, &rectangle, &area);
}

static void draw_line(
    lv_layer_t *layer, int x1, int y1, int x2, int y2,
    int width, uint32_t color, lv_opa_t opacity)
{
    lv_draw_line_dsc_t line;

    if(width <= 0 || opacity <= LV_OPA_MIN)
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
    line.round_start = 1;
    line.round_end = 1;
    lv_draw_line(layer, &line);
}

static void draw_arc(
    lv_layer_t *layer, int center_x, int center_y,
    int radius, int width, int start, int end,
    uint32_t color, lv_opa_t opacity)
{
    lv_draw_arc_dsc_t arc;

    if(radius <= 0 || width <= 0 || opacity <= LV_OPA_MIN)
        return;
    lv_draw_arc_dsc_init(&arc);
    arc.base.layer = layer;
    arc.center.x = center_x;
    arc.center.y = center_y;
    arc.radius = radius;
    arc.width = width;
    arc.rounded = 1;
    arc.color = lv_color_hex(color);
    arc.opa = opacity;
    arc.start_angle = start;
    arc.end_angle = end;
    lv_draw_arc(layer, &arc);
}

static void draw_ripple(
    lv_layer_t *layer, int x, int y, int timeline_ms)
{
    int progress = progress_between(timeline_ms, CONNECTION_MS, 750);
    int radius;
    int opacity;

    if(progress <= 0 || progress >= 1024)
        return;
    radius = lerp(3, 15, ease_out(progress));
    opacity = 210 * (1024 - progress) / 1024;
    draw_arc(
        layer, x, y, radius, 2, 0, 359,
        COLOR_GREEN, (lv_opa_t)opacity);
}

static void draw_airpod_bud(
    lv_layer_t *layer, int head_x, int head_y,
    int direction, int scale, lv_opa_t opacity,
    int twist)
{
    int head_size = clamp_int(15 * scale / 1024, 1, 17);
    int stem_width = clamp_int(5 * scale / 1024, 1, 6);
    int stem_length = clamp_int(15 * scale / 1024, 1, 17);
    int stem_x = head_x + direction * (2 + twist);
    int stem_y = head_y + head_size / 3;

    draw_rect(
        layer, head_x - head_size / 2,
        head_y - head_size / 2,
        head_size, head_size, LV_RADIUS_CIRCLE,
        COLOR_CERAMIC_TOP, COLOR_CERAMIC_BOTTOM,
        opacity, 1, COLOR_CERAMIC_EDGE, opacity / 2);
    draw_line(
        layer, stem_x, stem_y,
        stem_x + direction, stem_y + stem_length,
        stem_width, COLOR_CERAMIC_BOTTOM, opacity);
    draw_line(
        layer, stem_x - direction * 3,
        head_y - 1, stem_x + direction * 2,
        head_y - 2, 3, COLOR_INNER, opacity);
    draw_line(
        layer, stem_x - 1, stem_y + stem_length,
        stem_x + direction * 2, stem_y + stem_length,
        2, COLOR_GRAPHITE_LIGHT, opacity);
}

static void draw_airpods(
    lv_layer_t *layer, const lv_area_t *area, int timeline_ms)
{
    int intro_raw = progress_between(
        timeline_ms, 0, DEVICE_ENTRANCE_MS);
    int intro = back_out(intro_raw);
    int reveal_raw = progress_between(
        timeline_ms, CONNECTION_MS, CONNECTION_REVEAL_MS);
    int reveal = back_out(reveal_raw);
    int reveal_clamped = clamp_int(reveal, 0, 1120);
    int settle = smooth_step(reveal_raw);
    int case_scale = lerp(1024, 840, settle);
    int combined_scale = intro * case_scale / 1024;
    int center_x = area->x1 + DEVICE_WIDTH / 2;
    int base_y = area->y1 + 73;
    int intro_y = lerp(40, 0, ease_out(intro_raw));
    int hover_y = 0;
    int sway_x = 0;
    int body_width;
    int body_height;
    int body_x;
    int body_y;
    int lid_width;
    int lid_height;
    int lid_x;
    int lid_y;
    int lid_lift;
    int led_x;
    int led_y;
    int led_opacity;

    if(timeline_ms < CONNECTION_MS)
        sway_x = sine(timeline_ms * 360 / 2200) * 2 >>
            LV_TRIGO_SHIFT;
    if(timeline_ms >= HOVER_START_MS)
        hover_y = sine(
            (timeline_ms - HOVER_START_MS) * 360 /
                HOVER_PERIOD_MS) * 2 >> LV_TRIGO_SHIFT;
    center_x += sway_x;
    base_y += intro_y + hover_y + lerp(0, 6, settle);
    body_width = clamp_int(52 * combined_scale / 1024, 2, 58);
    body_height = clamp_int(32 * combined_scale / 1024, 2, 36);
    body_x = center_x - body_width / 2;
    body_y = base_y - body_height / 2;
    lid_width = clamp_int(
        lerp(52, 46, settle) * combined_scale / 1024, 2, 58);
    lid_height = clamp_int(
        lerp(13, 15, settle) * combined_scale / 1024, 1, 17);
    lid_lift = lerp(10, 25, settle) * combined_scale / 1024;
    lid_x = center_x - lid_width / 2;
    lid_y = body_y - lid_lift;

    draw_rect(
        layer, body_x, body_y,
        body_width, body_height, 11,
        COLOR_CERAMIC_TOP, COLOR_CERAMIC_BOTTOM,
        LV_OPA_COVER, 1, COLOR_CERAMIC_EDGE, 120);
    draw_line(
        layer, body_x + 5, body_y + 8,
        body_x + body_width - 5, body_y + 8,
        1, COLOR_CERAMIC_EDGE, 120);
    draw_rect(
        layer, body_x + 3, body_y + 3,
        body_width - 6, 5, 3,
        COLOR_INNER, COLOR_INNER, 205,
        0, COLOR_INNER, 0);
    draw_rect(
        layer, body_x + body_width / 5, body_y + 4,
        clamp_int(body_width / 6, 2, 9), 3, 2,
        0x050506, 0x050506, 235,
        0, 0x000000, 0);
    draw_rect(
        layer, body_x + body_width * 19 / 30, body_y + 4,
        clamp_int(body_width / 6, 2, 9), 3, 2,
        0x050506, 0x050506, 235,
        0, 0x000000, 0);
    if(settle > 128) {
        draw_line(
            layer, body_x + 6, body_y + 4,
            lid_x + 6, lid_y + lid_height - 2,
            2, COLOR_CERAMIC_EDGE,
            scaled_opacity(180, settle));
        draw_line(
            layer, body_x + body_width - 7, body_y + 4,
            lid_x + lid_width - 7,
            lid_y + lid_height - 2,
            2, COLOR_CERAMIC_EDGE,
            scaled_opacity(180, settle));
    }
    draw_rect(
        layer, lid_x, lid_y,
        lid_width, lid_height, 8,
        COLOR_CERAMIC_TOP, COLOR_CERAMIC_BOTTOM,
        LV_OPA_COVER, 1, COLOR_CERAMIC_EDGE, 120);
    if(settle > 64)
        draw_rect(
            layer, lid_x + 4, lid_y + lid_height - 6,
            lid_width - 8, 5, 3,
            COLOR_GRAPHITE_LIGHT, COLOR_INNER,
            scaled_opacity(150, settle),
            0, COLOR_INNER, 0);
    draw_line(
        layer, lid_x + 5, lid_y + 2,
        lid_x + lid_width - 5, lid_y + 2,
        1, COLOR_WHITE, 130);

    led_x = center_x;
    led_y = body_y + body_height * 2 / 3;
    if(timeline_ms < CONNECTION_MS) {
        int breath = sine(timeline_ms * 360 / 700);

        led_opacity = 115 + ((breath + (1 << LV_TRIGO_SHIFT)) *
            70 >> (LV_TRIGO_SHIFT + 1));
        draw_rect(
            layer, led_x - 2, led_y - 2, 4, 4,
            LV_RADIUS_CIRCLE, COLOR_WHITE, COLOR_WHITE,
            (lv_opa_t)led_opacity, 0, COLOR_WHITE, 0);
    }
    else {
        draw_rect(
            layer, led_x - 2, led_y - 2, 4, 4,
            LV_RADIUS_CIRCLE, COLOR_GREEN, COLOR_GREEN,
            LV_OPA_COVER, 0, COLOR_GREEN, 0);
        draw_ripple(layer, led_x, led_y, timeline_ms);
    }

    if(reveal_raw > 0) {
        int split = 36 * reveal_clamped / 1024;
        int lift = 30 * reveal_clamped / 1024;
        int bud_scale = clamp_int(reveal, 0, 1024);
        int opacity = 255 * clamp_int(reveal_raw, 0, 1024) / 1024;
        int twist = sine(reveal_raw * 360 / 1024) * 2 >>
            LV_TRIGO_SHIFT;

        draw_airpod_bud(
            layer, center_x - split, body_y + 8 - lift,
            -1, bud_scale, (lv_opa_t)opacity, twist);
        draw_airpod_bud(
            layer, center_x + split, body_y + 8 - lift,
            1, bud_scale, (lv_opa_t)opacity, -twist);
    }
}

static void draw_wired_earbud(
    lv_layer_t *layer, int head_x, int head_y,
    int direction, int scale, lv_opa_t opacity)
{
    int head_width = clamp_int(18 * scale / 1024, 2, 20);
    int head_height = clamp_int(15 * scale / 1024, 2, 17);
    int nozzle_width = clamp_int(7 * scale / 1024, 1, 8);
    int nozzle_height = clamp_int(7 * scale / 1024, 1, 8);
    int nozzle_x = direction < 0
        ? head_x - head_width / 2 - nozzle_width + 2
        : head_x + head_width / 2 - 2;
    int nozzle_y = head_y - nozzle_height / 2;
    int stem_x = head_x - direction * 4;

    draw_rect(
        layer, stem_x - 3, head_y + 3,
        6, clamp_int(15 * scale / 1024, 2, 17), 3,
        COLOR_CERAMIC_TOP, COLOR_CERAMIC_BOTTOM,
        opacity, 1, COLOR_CERAMIC_EDGE, opacity / 2);
    draw_rect(
        layer, nozzle_x, nozzle_y,
        nozzle_width, nozzle_height, 3,
        COLOR_CERAMIC_BOTTOM, COLOR_CERAMIC_TOP,
        opacity, 1, COLOR_CERAMIC_EDGE, opacity / 2);
    draw_rect(
        layer,
        direction < 0 ? nozzle_x : nozzle_x + nozzle_width - 3,
        nozzle_y + 1, 3, clamp_int(nozzle_height - 2, 1, 5),
        2, COLOR_INNER, COLOR_INNER, opacity,
        0, COLOR_INNER, 0);
    draw_rect(
        layer, head_x - head_width / 2,
        head_y - head_height / 2,
        head_width, head_height, LV_RADIUS_CIRCLE,
        COLOR_CERAMIC_TOP, COLOR_CERAMIC_BOTTOM,
        opacity, 1, COLOR_CERAMIC_EDGE, opacity / 2);
    draw_rect(
        layer, head_x - 2, head_y - 4,
        4, 8, LV_RADIUS_CIRCLE,
        COLOR_INNER, COLOR_GRAPHITE,
        opacity, 0, COLOR_INNER, 0);
    draw_rect(
        layer, head_x + direction * 5 - 1, head_y + 2,
        2, 3, LV_RADIUS_CIRCLE,
        COLOR_GRAPHITE_LIGHT, COLOR_GRAPHITE_LIGHT,
        opacity * 3 / 4, 0, COLOR_GRAPHITE_LIGHT, 0);
    draw_line(
        layer, head_x - direction * 5,
        head_y - head_height / 2 + 2,
        head_x + direction * 2,
        head_y - head_height / 2 + 1,
        1, COLOR_WHITE, opacity / 2);
}

static void draw_wired_earbuds(
    lv_layer_t *layer, const lv_area_t *area, int timeline_ms)
{
    int intro_raw = progress_between(
        timeline_ms, 0, DEVICE_ENTRANCE_MS);
    int intro = clamp_int(back_out(intro_raw), 0, 1120);
    int reveal_raw = progress_between(
        timeline_ms, CONNECTION_MS, CONNECTION_REVEAL_MS);
    int pulse = reveal_raw * (1024 - reveal_raw) / 256;
    int earbud_scale = intro *
        (1024 + 70 * pulse / 1024) / 1024;
    int cable = progress_between(
        timeline_ms,
        CONNECTION_MS + CONNECTION_REVEAL_MS, 180);
    lv_opa_t cable_opacity = scaled_opacity(235, cable);
    int center_x = area->x1 + DEVICE_WIDTH / 2;
    int head_y = area->y1 + 35;
    int left_x = center_x - 39;
    int right_x = center_x + 39;
    int left_anchor_x = left_x + 4;
    int right_anchor_x = right_x - 4;
    int anchor_y = head_y + 18;
    int splitter_y = area->y1 + 87;
    int plug_x = center_x + 2;
    int plug_y = area->y1 + 108;

    if(cable > 0) {
        draw_line(
            layer, left_anchor_x, anchor_y,
            center_x - 31, area->y1 + 62,
            2, COLOR_CABLE, cable_opacity);
        draw_line(
            layer, center_x - 31, area->y1 + 62,
            center_x - 17, area->y1 + 76,
            2, COLOR_CABLE, cable_opacity);
        draw_line(
            layer, center_x - 17, area->y1 + 76,
            center_x - 3, splitter_y,
            2, COLOR_CABLE, cable_opacity);
        draw_line(
            layer, right_anchor_x, anchor_y,
            center_x + 31, area->y1 + 62,
            2, COLOR_CABLE, cable_opacity);
        draw_line(
            layer, center_x + 31, area->y1 + 62,
            center_x + 17, area->y1 + 76,
            2, COLOR_CABLE, cable_opacity);
        draw_line(
            layer, center_x + 17, area->y1 + 76,
            center_x + 3, splitter_y,
            2, COLOR_CABLE, cable_opacity);
        draw_rect(
            layer, center_x - 3, splitter_y - 3,
            6, 10, 3,
            COLOR_CERAMIC_TOP, COLOR_CERAMIC_BOTTOM,
            cable_opacity, 1, COLOR_CERAMIC_EDGE,
            cable_opacity / 2);
        draw_line(
            layer, center_x, splitter_y + 7,
            center_x + 6, area->y1 + 106,
            2, COLOR_CABLE, cable_opacity);
        draw_line(
            layer, center_x + 6, area->y1 + 106,
            center_x + 6, plug_y,
            2, COLOR_CABLE, cable_opacity);
        draw_rect(
            layer, plug_x, plug_y,
            8, 10, 3,
            COLOR_CERAMIC_TOP, COLOR_CERAMIC_BOTTOM,
            cable_opacity, 1, COLOR_CERAMIC_EDGE,
            cable_opacity / 2);
        draw_rect(
            layer, plug_x + 1, plug_y + 9,
            6, 8, 2, COLOR_GOLD, COLOR_GOLD,
            cable_opacity, 0, COLOR_GOLD, 0);
        draw_line(
            layer, plug_x + 1, plug_y + 12,
            plug_x + 6, plug_y + 12,
            1, COLOR_INNER, cable_opacity);
        draw_rect(
            layer, plug_x + 2, plug_y + 16,
            4, 4, LV_RADIUS_CIRCLE,
            COLOR_GREEN, COLOR_GREEN,
            cable_opacity, 0, COLOR_GREEN, 0);
        draw_ripple(
            layer, plug_x + 4, plug_y + 18,
            timeline_ms - CONNECTION_REVEAL_MS);
    }

    draw_wired_earbud(
        layer, left_x, head_y, -1,
        earbud_scale,
        (lv_opa_t)clamp_int(255 * intro / 1024, 0, 255));
    draw_wired_earbud(
        layer, right_x, head_y, 1,
        earbud_scale,
        (lv_opa_t)clamp_int(255 * intro / 1024, 0, 255));
}

static void draw_over_ear(
    lv_layer_t *layer, const lv_area_t *area, int timeline_ms)
{
    int intro_raw = progress_between(
        timeline_ms, 0, DEVICE_ENTRANCE_MS);
    int intro = clamp_int(back_out(intro_raw), 0, 1120);
    int reveal_raw = progress_between(
        timeline_ms, CONNECTION_MS, CONNECTION_REVEAL_MS);
    int reveal = back_out(reveal_raw);
    int settle = smooth_step(reveal_raw);
    int center_x = area->x1 + DEVICE_WIDTH / 2;
    int center_y = area->y1 + 70 +
        lerp(40, 0, ease_out(intro_raw));
    int spread = lerp(9, 48, clamp_int(reveal, 0, 1024));
    int radius = lerp(23, 52, settle) * intro / 1024;
    int cup_width = clamp_int(20 * intro / 1024, 2, 22);
    int cup_height = clamp_int(39 * intro / 1024, 3, 43);
    int left_x;
    int right_x;
    int cup_y;

    left_x = center_x - spread - cup_width / 2;
    right_x = center_x + spread - cup_width / 2;
    cup_y = center_y - 5;

    draw_arc(
        layer, center_x, center_y + 4,
        radius, clamp_int(8 * intro / 1024, 1, 9),
        198, 342, COLOR_GRAPHITE, 245);
    draw_arc(
        layer, center_x, center_y + 4,
        radius, clamp_int(2 * intro / 1024, 1, 3),
        202, 338, COLOR_GRAPHITE_EDGE, 205);
    draw_arc(
        layer, center_x, center_y + 4,
        clamp_int(radius - 5, 1, radius),
        clamp_int(2 * intro / 1024, 1, 3),
        205, 335, COLOR_GRAPHITE_LIGHT, 180);

    draw_rect(
        layer, left_x, cup_y,
        cup_width, cup_height, 7,
        COLOR_GRAPHITE_LIGHT, COLOR_GRAPHITE,
        LV_OPA_COVER, 1, COLOR_GRAPHITE_EDGE, 120);
    draw_rect(
        layer, left_x + cup_width / 2,
        cup_y + 4, clamp_int(cup_width / 2, 1, cup_width),
        cup_height - 8, 5, COLOR_INNER, COLOR_INNER,
        230, 0, COLOR_INNER, 0);
    draw_rect(
        layer, right_x, cup_y,
        cup_width, cup_height, 7,
        COLOR_GRAPHITE_LIGHT, COLOR_GRAPHITE,
        LV_OPA_COVER, 1, COLOR_GRAPHITE_EDGE, 120);
    draw_rect(
        layer, right_x, cup_y + 4,
        clamp_int(cup_width / 2, 1, cup_width),
        cup_height - 8, 5, COLOR_INNER, COLOR_INNER,
        230, 0, COLOR_INNER, 0);
    draw_line(
        layer, left_x + cup_width / 2, cup_y - 4,
        left_x + cup_width / 2, cup_y + 3,
        3, COLOR_GRAPHITE_EDGE, 210);
    draw_line(
        layer, right_x + cup_width / 2, cup_y - 4,
        right_x + cup_width / 2, cup_y + 3,
        3, COLOR_GRAPHITE_EDGE, 210);

}

static void draw_device(lv_event_t *event)
{
    lv_obj_t *surface;
    lv_layer_t *layer;
    lv_area_t area;

    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;
    surface = lv_event_get_target(event);
    layer = lv_event_get_layer(event);
    lv_obj_get_coords(surface, &area);
    if(popup.style == CRAZYPOD_HEADPHONE_POPUP_AIRPODS)
        draw_airpods(layer, &area, popup.timeline_ms);
    else if(popup.style == CRAZYPOD_HEADPHONE_POPUP_OVER_EAR)
        draw_over_ear(layer, &area, popup.timeline_ms);
    else
        draw_wired_earbuds(layer, &area, popup.timeline_ms);
}

static void update_copy(bool connected)
{
    if(popup.connected_copy == connected ||
       popup.status == NULL)
        return;
    popup.connected_copy = connected;
    crazypod_ui_widget_set_label_text(
        popup.status,
        connected ? CP_TR("Connected") : CP_TR("Connecting..."));
    lv_obj_set_style_text_color(
        popup.status,
        lv_color_hex(connected ? COLOR_GREEN : COLOR_WHITE), 0);
}

static void apply_timeline(int timeline_ms)
{
    int entrance = ease_out(progress_between(
        timeline_ms, 0, ENTRANCE_MS));
    int reveal = progress_between(
        timeline_ms, CONNECTION_MS, CONNECTION_REVEAL_MS);
    int bounce = reveal * (1024 - reveal) / 256;
    bool connected = timeline_ms >= CONNECTION_MS;

    popup.timeline_ms = timeline_ms;
    update_copy(connected);
    if(popup.panel != NULL) {
        lv_obj_set_style_transform_scale(
            popup.panel, lerp(218, 256, entrance), 0);
        lv_obj_set_style_opa(
            popup.panel,
            (lv_opa_t)(255 * entrance / 1024), 0);
    }
    if(popup.title != NULL)
        lv_obj_set_style_transform_scale(
            popup.title, 256 + 13 * bounce / 1024, 0);
    if(popup.status != NULL)
        lv_obj_set_style_text_opa(
            popup.status,
            connected
                ? (lv_opa_t)lerp(110, 255, smooth_step(reveal))
                : 110,
            0);
    if(popup.surface != NULL)
        lv_obj_invalidate(popup.surface);
}

static void timeline_anim(void *target, int32_t value)
{
    (void)target;
    if(!popup.dismissing)
        apply_timeline((int)value);
}

static void teardown_refresh_ready(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);

    lv_display_remove_event_cb_with_user_data(
        display, teardown_refresh_ready, NULL);
    if(!popup.teardown_pending)
        return;
    popup.teardown_pending = false;
    if(popup.callbacks.dismissed != NULL)
        popup.callbacks.dismissed();
}

static void destroy_popup(void)
{
    lv_obj_t *root = popup.root;
    bool had_popup = root != NULL;
    lv_display_t *display;

    if(!had_popup)
        return;

    if(popup.hide_timer != NULL) {
        lv_timer_delete(popup.hide_timer);
        popup.hide_timer = NULL;
    }
    if(popup.panel != NULL)
        lv_anim_delete(popup.panel, NULL);
    popup.teardown_pending = had_popup;
    popup.root = NULL;
    popup.panel = NULL;
    popup.surface = NULL;
    popup.title = NULL;
    popup.status = NULL;
    display = lv_obj_get_display(root);
    lv_display_remove_event_cb_with_user_data(
        display, teardown_refresh_ready, NULL);
    lv_display_add_event_cb(
        display, teardown_refresh_ready, LV_EVENT_REFR_READY, NULL);
    lv_obj_delete(root);
    popup.connected_copy = false;
    popup.dismissing = false;
    popup.timeline_ms = 0;
}

static void exit_anim(void *target, int32_t value)
{
    int progress = ease_in((int)value);

    (void)target;
    if(popup.panel == NULL)
        return;
    lv_obj_set_y(
        popup.panel,
        popup.exit_origin_y + 250 * progress / 1024);
    lv_obj_set_style_transform_scale(
        popup.panel,
        lerp(popup.exit_origin_scale, 230, progress), 0);
    lv_obj_set_style_opa(
        popup.panel,
        (lv_opa_t)(popup.exit_origin_opacity *
            (1024 - progress) / 1024), 0);
}

static void exit_completed(lv_anim_t *animation)
{
    if(animation->var != popup.panel)
        return;
    popup.panel = NULL;
    destroy_popup();
}

void crazypod_headphone_popup_dismiss(bool animated)
{
    lv_anim_t animation;

    if(popup.root == NULL)
        return;
    if(!animated) {
        destroy_popup();
        return;
    }
    if(popup.dismissing)
        return;
    popup.dismissing = true;
    if(popup.hide_timer != NULL) {
        lv_timer_delete(popup.hide_timer);
        popup.hide_timer = NULL;
    }
    lv_anim_delete(popup.panel, NULL);
    popup.exit_origin_y = lv_obj_get_y(popup.panel);
    popup.exit_origin_scale =
        lv_obj_get_style_transform_scale_x(popup.panel, 0);
    popup.exit_origin_opacity =
        lv_obj_get_style_opa(popup.panel, 0);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, popup.panel);
    lv_anim_set_exec_cb(&animation, exit_anim);
    lv_anim_set_values(&animation, 0, 1024);
    lv_anim_set_duration(&animation, EXIT_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_completed_cb(&animation, exit_completed);
    lv_anim_set_early_apply(&animation, true);
    lv_anim_start(&animation);
}

static void timeline_completed(lv_anim_t *animation)
{
    if(animation->var == popup.panel && !popup.dismissing)
        crazypod_headphone_popup_dismiss(true);
}

static void reduced_hide(lv_timer_t *timer)
{
    if(timer != popup.hide_timer)
        return;
    popup.hide_timer = NULL;
    lv_timer_delete(timer);
    crazypod_headphone_popup_dismiss(true);
}

static void start_motion(void)
{
    lv_anim_t animation;

    if(crazypod_state_reduce_motion()) {
        apply_timeline(CONNECTION_MS + CONNECTION_REVEAL_MS);
        popup.hide_timer = lv_timer_create(
            reduced_hide, REDUCED_HOLD_MS, NULL);
        lv_timer_set_repeat_count(popup.hide_timer, 1);
        return;
    }
    apply_timeline(0);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, popup.panel);
    lv_anim_set_exec_cb(&animation, timeline_anim);
    lv_anim_set_values(&animation, 0, HOLD_END_MS);
    lv_anim_set_duration(&animation, HOLD_END_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_completed_cb(&animation, timeline_completed);
    lv_anim_set_early_apply(&animation, true);
    lv_anim_start(&animation);
}

static void show_popup(void)
{
    const char *title_text;

    if(crazypod_headphone_popup_visible() || popup.parent == NULL ||
       popup.callbacks.create_panel == NULL ||
       (popup.callbacks.can_show != NULL &&
        !popup.callbacks.can_show()))
        return;
    if(popup.callbacks.before_show != NULL)
        popup.callbacks.before_show();
    popup.style = crazypod_state_headphone_popup_style();
    popup.root = crazypod_ui_widget_box(
        popup.parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(popup.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(popup.root);
    (void)crazypod_desktop_native_create_modal_underlay(
        popup.root);
    popup.panel = popup.callbacks.create_panel(
        popup.root, PANEL_X, PANEL_Y,
        PANEL_WIDTH, PANEL_HEIGHT);
    if(popup.panel == NULL) {
        destroy_popup();
        return;
    }
    lv_obj_remove_flag(popup.panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_transform_pivot_x(
        popup.panel, PANEL_WIDTH / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        popup.panel, PANEL_HEIGHT / 2, 0);

    title_text = popup.style == CRAZYPOD_HEADPHONE_POPUP_AIRPODS
        ? CP_TR("AirPods")
        : popup.style == CRAZYPOD_HEADPHONE_POPUP_OVER_EAR
            ? CP_TR("Over-Ear Headphones")
            : CP_TR("Wired Earphones");
    popup.title = crazypod_ui_widget_label(
        popup.panel, title_text, &lv_font_montserrat_12,
        COLOR_WHITE, 245);
    lv_obj_set_size(popup.title, PANEL_WIDTH - 24, 22);
    lv_obj_set_pos(popup.title, 12, 8);
    lv_obj_set_style_text_align(
        popup.title, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(
        popup.title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_transform_pivot_x(
        popup.title, (PANEL_WIDTH - 24) / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        popup.title, 11, 0);

    popup.surface = crazypod_ui_widget_box(
        popup.panel, DEVICE_X, DEVICE_Y,
        DEVICE_WIDTH, DEVICE_HEIGHT, 0,
        0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(popup.surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        popup.surface, draw_device, LV_EVENT_DRAW_MAIN, NULL);

    popup.status = crazypod_ui_widget_label(
        popup.panel, CP_TR("Connecting..."),
        &lv_font_montserrat_10, COLOR_WHITE, 110);
    lv_obj_set_size(popup.status, PANEL_WIDTH - 24, 20);
    lv_obj_set_pos(popup.status, 12, 164);
    lv_obj_set_style_text_align(
        popup.status, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(
        popup.status, LV_LABEL_LONG_MODE_DOTS);
    popup.connected_copy = false;
    popup.dismissing = false;
    start_motion();
}

void crazypod_headphone_popup_configure(
    lv_obj_t *parent,
    const struct crazypod_headphone_popup_callbacks *callbacks)
{
    popup.parent = parent;
    if(callbacks != NULL)
        popup.callbacks = *callbacks;
}

void crazypod_headphone_popup_set_ui_ready(bool inserted)
{
    popup.inserted = inserted;
    popup.ui_ready = true;
}

void crazypod_headphone_popup_connection_changed(bool inserted)
{
    if(!popup.ui_ready) {
        popup.inserted = inserted;
        return;
    }
    if(popup.inserted == inserted)
        return;
    popup.inserted = inserted;
    if(inserted)
        show_popup();
    else
        crazypod_headphone_popup_dismiss(true);
}

bool crazypod_headphone_popup_visible(void)
{
    return popup.root != NULL || popup.teardown_pending;
}

bool crazypod_headphone_popup_handle_button(
    long base, bool repeated, intptr_t data)
{
    (void)data;

    if(!crazypod_headphone_popup_visible())
        return false;
    if(!repeated &&
       (base == BUTTON_MENU || base == BUTTON_SELECT ||
        base == BUTTON_PLAY))
        crazypod_headphone_popup_dismiss(true);
    return true;
}

#ifdef SIMULATOR
void crazypod_headphone_popup_simulator_show(void)
{
    if(crazypod_headphone_popup_visible())
        crazypod_headphone_popup_dismiss(false);
    show_popup();
}
#endif

#endif
