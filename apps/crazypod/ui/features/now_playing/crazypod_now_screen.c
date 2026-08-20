#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "kernel.h"
#include "settings.h"
#include "lvgl.h"

#include "../../../crazypod_lyrics.h"
#include "../../../crazypod_playlist.h"
#include "../../presentation/crazypod_marquee.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_now_presentation.h"
#include "crazypod_now_screen.h"

#define COLOR_CYAN 0x55D6E7
#define COLOR_FAVORITE 0xFF375F
#define COLOR_WHITE 0xFFFFFF
#define CRAZYPOD_NOW_LYRICS_COVER_SIZE 108
#define CRAZYPOD_NOW_SHADE_COLOR 0x05070A
#define CRAZYPOD_NOW_SHADE_OPA 96
#define CRAZYPOD_NOW_LYRICS_X 144
#define CRAZYPOD_NOW_LYRICS_Y 61
#define CRAZYPOD_NOW_LYRICS_WIDTH 158
#define CRAZYPOD_NOW_LYRICS_HEIGHT 96
#define CRAZYPOD_NOW_LYRICS_CONTEXT_HEIGHT 16
#define CRAZYPOD_NOW_LYRICS_CURRENT_LINE_SPACE 4
#define CRAZYPOD_NOW_LYRICS_CURRENT_ONE_HEIGHT 20
#define CRAZYPOD_NOW_LYRICS_CURRENT_TWO_HEIGHT 44
#define CRAZYPOD_NOW_LYRICS_CURRENT_THREE_HEIGHT 68
#define CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_HEIGHT 92
#define CRAZYPOD_NOW_LYRICS_CONTEXT_TOP_Y 2
#define CRAZYPOD_NOW_LYRICS_CURRENT_ONE_Y 38
#define CRAZYPOD_NOW_LYRICS_CURRENT_TWO_Y 26
#define CRAZYPOD_NOW_LYRICS_CURRENT_THREE_Y 2
#define CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_Y 2
#define CRAZYPOD_NOW_LYRICS_CONTEXT_BOTTOM_Y 78
#define CRAZYPOD_NOW_LYRICS_SCROLL_TICKS \
    ((HZ / 12) > 0 ? (HZ / 12) : 1)
#define CRAZYPOD_NOW_LYRICS_SCROLL_HOLD \
    ((HZ * 2) > 0 ? (HZ * 2) : 1)
#define CRAZYPOD_NOW_WAVE_FRAME_TICKS \
    ((HZ / 10) > 0 ? (HZ / 10) : 1)

static struct crazypod_now_screen_view now_view;
static char rendered_track_path[MAX_PATH];
static int wave_phase;

static lv_obj_t *make_box(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_box(
        parent, x, y, width, height, radius, color, opacity);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

static void hide_label(lv_obj_t *label)
{
    if(label != NULL)
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
}

static void set_context_label_text(lv_obj_t *label, const char *text)
{
    if(label == NULL)
        return;
    CP_LV_LABEL_SET_TEXT(label, text != NULL ? text : "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_line_space(label, 0, 0);
}

static int set_current_label_text(lv_obj_t *label, const char *text)
{
    const lv_font_t *font;
    lv_point_t size = { 0, 0 };

    if(label == NULL)
        return 0;
    CP_LV_LABEL_SET_TEXT(label, text != NULL ? text : "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(
        label, CRAZYPOD_NOW_LYRICS_CURRENT_LINE_SPACE, 0);
    font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_text_get_size(
        &size, lv_label_get_text(label), font, 0,
        CRAZYPOD_NOW_LYRICS_CURRENT_LINE_SPACE,
        CRAZYPOD_NOW_LYRICS_WIDTH, LV_TEXT_FLAG_NONE);
    if(size.y < lv_font_get_line_height(font))
        size.y = lv_font_get_line_height(font);
    return size.y;
}

static void show_wrapped_label(
    lv_obj_t *label, int y, int height)
{
    if(label == NULL)
        return;
    lv_obj_set_pos(label, 0, y);
    lv_obj_set_size(label, CRAZYPOD_NOW_LYRICS_WIDTH, height);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
}

static void layout_lyrics(
    const char *previous, const char *current, const char *next,
    bool reset_scroll)
{
    struct crazypod_now_screen_view *view = &now_view;
    int current_height;

    if(view->lyrics_current == NULL)
        return;
    set_context_label_text(view->lyrics_previous, previous);
    current_height = set_current_label_text(
        view->lyrics_current, current);
    set_context_label_text(view->lyrics_next, next);
    if(reset_scroll) {
        view->lyrics_scroll = 0;
        view->lyrics_scroll_tick = current_tick;
        view->lyrics_scroll_hold_until =
            current_tick + CRAZYPOD_NOW_LYRICS_SCROLL_HOLD;
    }
    view->lyrics_scroll_max =
        current_height > CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_HEIGHT
            ? current_height - CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_HEIGHT
            : 0;
    if(view->lyrics_scroll > view->lyrics_scroll_max)
        view->lyrics_scroll = view->lyrics_scroll_max;

    hide_label(view->lyrics_previous);
    hide_label(view->lyrics_next);

    if(current_height > CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_HEIGHT) {
        show_wrapped_label(
            view->lyrics_current,
            CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_Y -
                view->lyrics_scroll,
            current_height);
        return;
    }

    if(current_height <= CRAZYPOD_NOW_LYRICS_CURRENT_ONE_HEIGHT) {
        show_wrapped_label(
            view->lyrics_current,
            CRAZYPOD_NOW_LYRICS_CURRENT_ONE_Y,
            CRAZYPOD_NOW_LYRICS_CURRENT_ONE_HEIGHT);
        if(previous != NULL && previous[0] != '\0')
            show_wrapped_label(
                view->lyrics_previous,
                CRAZYPOD_NOW_LYRICS_CONTEXT_TOP_Y,
                CRAZYPOD_NOW_LYRICS_CONTEXT_HEIGHT);
        if(next != NULL && next[0] != '\0')
            show_wrapped_label(
                view->lyrics_next,
                CRAZYPOD_NOW_LYRICS_CONTEXT_BOTTOM_Y,
                CRAZYPOD_NOW_LYRICS_CONTEXT_HEIGHT);
    }
    else if(current_height <=
            CRAZYPOD_NOW_LYRICS_CURRENT_TWO_HEIGHT) {
        show_wrapped_label(
            view->lyrics_current,
            CRAZYPOD_NOW_LYRICS_CURRENT_TWO_Y,
            CRAZYPOD_NOW_LYRICS_CURRENT_TWO_HEIGHT);
        if(previous != NULL && previous[0] != '\0')
            show_wrapped_label(
                view->lyrics_previous,
                CRAZYPOD_NOW_LYRICS_CONTEXT_TOP_Y,
                CRAZYPOD_NOW_LYRICS_CONTEXT_HEIGHT);
        if(next != NULL && next[0] != '\0')
            show_wrapped_label(
                view->lyrics_next,
                CRAZYPOD_NOW_LYRICS_CONTEXT_BOTTOM_Y,
                CRAZYPOD_NOW_LYRICS_CONTEXT_HEIGHT);
    }
    else if(current_height <=
            CRAZYPOD_NOW_LYRICS_CURRENT_THREE_HEIGHT) {
        show_wrapped_label(
            view->lyrics_current,
            CRAZYPOD_NOW_LYRICS_CURRENT_THREE_Y,
            CRAZYPOD_NOW_LYRICS_CURRENT_THREE_HEIGHT);
        if(next != NULL && next[0] != '\0')
            show_wrapped_label(
                view->lyrics_next,
                CRAZYPOD_NOW_LYRICS_CONTEXT_BOTTOM_Y,
                CRAZYPOD_NOW_LYRICS_CONTEXT_HEIGHT);
    }
    else {
        show_wrapped_label(
            view->lyrics_current,
            CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_Y,
            CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_HEIGHT);
    }
}

static void tick_lyrics_scroll(long now)
{
    struct crazypod_now_screen_view *view = &now_view;

    if(view->lyrics_current == NULL || view->lyrics_scroll_max <= 0 ||
       TIME_BEFORE(now, view->lyrics_scroll_hold_until) ||
       TIME_BEFORE(now, view->lyrics_scroll_tick +
                   CRAZYPOD_NOW_LYRICS_SCROLL_TICKS))
        return;
    view->lyrics_scroll_tick = now;
    if(view->lyrics_scroll >= view->lyrics_scroll_max) {
        view->lyrics_scroll = 0;
        view->lyrics_scroll_hold_until =
            now + CRAZYPOD_NOW_LYRICS_SCROLL_HOLD;
    }
    else {
        ++view->lyrics_scroll;
        if(view->lyrics_scroll >= view->lyrics_scroll_max)
            view->lyrics_scroll_hold_until =
                now + CRAZYPOD_NOW_LYRICS_SCROLL_HOLD;
    }
    lv_obj_set_y(
        view->lyrics_current,
        CRAZYPOD_NOW_LYRICS_CURRENT_FOUR_Y -
            view->lyrics_scroll);
}

static uint32_t text_hash(const char *text)
{
    uint32_t hash = 2166136261u;

    if(text == NULL)
        return hash;
    while(*text != '\0') {
        hash ^= (unsigned char)*text++;
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t artwork_color(const char *text, int variant)
{
    static const uint32_t palette[] = {
        0x8A2BE2, 0x1D78F2, 0xE5446D, 0xE4812C,
        0x13A48C, 0x5A55D6, 0xB0388E, 0x276A82
    };

    return palette[(text_hash(text) + (uint32_t)variant * 3u) %
                   (sizeof(palette) / sizeof(palette[0]))];
}

void crazypod_now_screen_render(
    const struct crazypod_now_screen_context *context)
{
    struct crazypod_now_screen_view *view = &now_view;
    const struct crazypod_track *track = context->track;
    const lv_image_dsc_t *lyrics_artwork = NULL;
    const lv_image_dsc_t *presentation_backdrop = NULL;
    uint32_t content_color = COLOR_WHITE;
    lv_obj_t *backdrop;
    lv_obj_t *shade;
    lv_obj_t *title;
    lv_obj_t *artist;
    enum crazypod_ui_icon mode_icon;

    memset(view, 0, sizeof(*view));
    rendered_track_path[0] = '\0';
    if(track != NULL) {
        snprintf(rendered_track_path,
                 sizeof(rendered_track_path),
                 "%s", track->path);
    }
    if(context->visual_artwork != NULL &&
       context->visual_track_path != NULL &&
       !crazypod_now_presentation_matches(
           context->visual_track_path,
           context->visual_generation)) {
        context->boost(HZ / 2);
        (void)crazypod_now_presentation_prepare(
            context->visual_artwork,
            context->visual_track_path,
            context->visual_generation);
    }
    if(context->visual_track_path != NULL)
        (void)crazypod_now_presentation_get(
            context->visual_track_path,
            context->visual_generation,
            &lyrics_artwork, &presentation_backdrop,
            &content_color);
    if(presentation_backdrop != NULL) {
        backdrop = lv_image_create(context->parent);
        lv_image_set_src(backdrop, presentation_backdrop);
        lv_obj_center(backdrop);
    }
    else {
        content_color = context->fallback_color(track);
        backdrop = make_box(
            context->parent, 0, 0, 320, 240, 0,
            artwork_color(track != NULL ? track->album : "", 0),
            LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(
            backdrop,
            lv_color_hex(
                artwork_color(track != NULL ? track->artist : "", 1)),
            0);
        lv_obj_set_style_bg_grad_dir(
            backdrop, LV_GRAD_DIR_VER, 0);
    }
    shade = make_box(
        context->parent, 0, 0, 320, 240, 0,
        CRAZYPOD_NOW_SHADE_COLOR, CRAZYPOD_NOW_SHADE_OPA);
    (void)shade;

    {
        const char *previous = "";
        const char *current = "";
        const char *next = "";
        bool lyrics_available = false;
        lv_obj_t *metadata_shade;

        if(context->lyrics_mode && track != NULL)
            lyrics_available = crazypod_lyrics_load(track->path);
        if(lyrics_available)
            crazypod_lyrics_window(0, &previous, &current, &next);
        context->render_artwork(
            context->parent, track, 18, 55,
            CRAZYPOD_NOW_LYRICS_COVER_SIZE,
            lyrics_artwork, false);
        make_box(context->parent, 134, 55, 1, 106, 0,
                 content_color, 38);
        if(lyrics_available) {
            metadata_shade = make_box(
                context->parent, 18, 116, 108, 47, 0,
                0x000000, 112);
            (void)metadata_shade;
            title = make_label(
                context->parent,
                track != NULL ? track->title : CP_TR("No Track"),
                context->metadata_font,
                COLOR_WHITE, LV_OPA_COVER);
            lv_obj_set_size(title, 96, 23);
            lv_obj_set_style_text_align(
                title, LV_TEXT_ALIGN_CENTER, 0);
            crazypod_marquee_configure(title, true);
            lv_obj_set_pos(title, 24, 117);
            artist = make_label(
                context->parent,
                track != NULL ? track->artist : CP_TR("Local Music"),
                context->metadata_font,
                COLOR_WHITE, 180);
            lv_obj_set_size(artist, 96, 23);
            lv_obj_set_style_text_align(
                artist, LV_TEXT_ALIGN_CENTER, 0);
            crazypod_marquee_configure(artist, true);
            lv_obj_set_pos(artist, 24, 140);

            view->lyrics_viewport = make_box(
                context->parent,
                CRAZYPOD_NOW_LYRICS_X,
                CRAZYPOD_NOW_LYRICS_Y,
                CRAZYPOD_NOW_LYRICS_WIDTH,
                CRAZYPOD_NOW_LYRICS_HEIGHT,
                0, 0, LV_OPA_TRANSP);
            lv_obj_remove_flag(
                view->lyrics_viewport, LV_OBJ_FLAG_SCROLLABLE);
            view->lyrics_previous = make_label(
                view->lyrics_viewport, "",
                context->lyrics_context_font,
                content_color, 96);
            view->lyrics_current = make_label(
                view->lyrics_viewport, "",
                context->lyrics_current_font,
                content_color, LV_OPA_COVER);
            view->lyrics_next = make_label(
                view->lyrics_viewport, "",
                context->lyrics_context_font,
                content_color, 150);
            hide_label(view->lyrics_previous);
            hide_label(view->lyrics_current);
            hide_label(view->lyrics_next);
            layout_lyrics(previous, current, next, true);
        }
        else {
            lv_obj_t *album;
            bool favorite =
                track != NULL &&
                crazypod_music_track_is_favorite(track->path);

            title = make_label(
                context->parent,
                track != NULL ? track->title : CP_TR("No Track"),
                context->metadata_font,
                content_color, LV_OPA_COVER);
            lv_obj_set_size(title, 158, 23);
            lv_obj_set_style_text_align(
                title, LV_TEXT_ALIGN_CENTER, 0);
            crazypod_marquee_configure(title, true);
            lv_obj_set_pos(title, 144, 71);

            artist = make_label(
                context->parent,
                track != NULL ? track->artist : CP_TR("Local Music"),
                context->metadata_font,
                content_color, 220);
            lv_obj_set_size(artist, 158, 23);
            lv_obj_set_style_text_align(
                artist, LV_TEXT_ALIGN_CENTER, 0);
            crazypod_marquee_configure(artist, true);
            lv_obj_set_pos(artist, 144, 95);

            album = make_label(
                context->parent,
                track != NULL && track->album[0] != '\0'
                    ? track->album : "",
                context->metadata_font,
                content_color, 190);
            lv_obj_set_size(album, 158, 23);
            lv_obj_set_style_text_align(
                album, LV_TEXT_ALIGN_CENTER, 0);
            crazypod_marquee_configure(album, true);
            lv_obj_set_pos(album, 144, 119);

            crazypod_ui_widget_icon(
                context->parent, 190, 143,
                CRAZYPOD_UI_ICON_HEART,
                favorite ? COLOR_FAVORITE : content_color,
                favorite ? LV_OPA_COVER : 120);
            if(crazypod_queue_repeat() == REPEAT_ONE)
                mode_icon = CRAZYPOD_UI_ICON_REPEAT_ONE;
            else if(crazypod_queue_repeat() == REPEAT_ALL)
                mode_icon = CRAZYPOD_UI_ICON_REPEAT;
            else if(crazypod_queue_shuffle())
                mode_icon = CRAZYPOD_UI_ICON_SHUFFLE;
            else
                mode_icon = CRAZYPOD_UI_ICON_PLAY;
            crazypod_ui_widget_icon(
                context->parent, 215, 143, mode_icon,
                crazypod_queue_repeat() != REPEAT_OFF ||
                crazypod_queue_shuffle() ? COLOR_CYAN : content_color,
                220);
        }
    }

    view->wave_surface = lv_obj_create(context->parent);
    crazypod_ui_widget_make_plain(view->wave_surface);
    lv_obj_set_pos(view->wave_surface, 16, 181);
    lv_obj_set_size(view->wave_surface, 288, 34);
    lv_obj_set_style_bg_opa(view->wave_surface, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(view->wave_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(view->wave_surface, context->draw_wave,
                        LV_EVENT_DRAW_MAIN, NULL);
    view->wave_playing =
        (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
        (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    view->wave_tick = current_tick;
    view->progress_marker = make_box(
        view->wave_surface, 0, 14, 7, 7,
        LV_RADIUS_CIRCLE, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_style_shadow_width(view->progress_marker, 6, 0);
    lv_obj_set_style_shadow_color(
        view->progress_marker, lv_color_hex(context->primary_color), 0);
    lv_obj_set_style_shadow_opa(view->progress_marker, 190, 0);
    lv_obj_remove_flag(view->progress_marker, LV_OBJ_FLAG_CLICKABLE);
    view->elapsed = make_label(context->parent, "0:00",
                             &lv_font_montserrat_8,
                             COLOR_WHITE, 180);
    lv_obj_set_pos(view->elapsed, 30, 218);
    view->remaining = make_label(context->parent, "-0:00",
                               &lv_font_montserrat_8,
                               COLOR_WHITE, 180);
    lv_obj_set_width(view->remaining, 60);
    lv_obj_set_style_text_align(view->remaining, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(view->remaining, 230, 218);
}

void crazypod_now_screen_reset(void)
{
    memset(&now_view, 0, sizeof(now_view));
}

const char *crazypod_now_screen_rendered_track_path(void)
{
    return rendered_track_path;
}

int crazypod_now_screen_wave_phase(void)
{
    return wave_phase;
}

void crazypod_now_screen_tick_wave(long now, bool active, bool blocked)
{
    bool playing;

    if(!active || blocked || now_view.wave_surface == NULL)
        return;
    tick_lyrics_scroll(now);
    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
              (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    if(!playing) {
        if(now_view.wave_playing) {
            now_view.wave_playing = false;
            lv_obj_invalidate(now_view.wave_surface);
        }
        return;
    }
    if(TIME_BEFORE(now, now_view.wave_tick +
                   CRAZYPOD_NOW_WAVE_FRAME_TICKS))
        return;
    now_view.wave_tick = now;
    now_view.wave_playing = true;
    wave_phase = (wave_phase + 1) & 0x7fff;
    lv_obj_invalidate(now_view.wave_surface);
}

static bool label_text_changed(
    lv_obj_t *label, const char *text)
{
    return label != NULL && text != NULL &&
           strcmp(lv_label_get_text(label), text) != 0;
}

static void format_time_ms(
    uint32_t milliseconds, char *buffer, size_t capacity)
{
    unsigned seconds = milliseconds / 1000;

    snprintf(buffer, capacity, "%u:%02u", seconds / 60, seconds % 60);
}

void crazypod_now_screen_update_playback(
    uint32_t elapsed_ms, uint32_t length_ms)
{
    if(now_view.lyrics_current != NULL &&
       crazypod_lyrics_available()) {
        const char *previous;
        const char *current;
        const char *next;
        bool current_changed;

        crazypod_lyrics_window(
            elapsed_ms, &previous, &current, &next);
        current_changed = label_text_changed(
            now_view.lyrics_current, current);
        if(current_changed ||
           label_text_changed(now_view.lyrics_previous, previous) ||
           label_text_changed(now_view.lyrics_next, next))
            layout_lyrics(
                previous, current, next, current_changed);
    }
    if(length_ms > 0 && now_view.progress_marker != NULL) {
        int x = 281 * elapsed_ms / length_ms;
        char elapsed[16];
        char remaining[16];
        uint32_t left =
            length_ms > elapsed_ms ? length_ms - elapsed_ms : 0;

        if(x < 0)
            x = 0;
        if(x > 281)
            x = 281;
        lv_obj_set_x(now_view.progress_marker, x);
        format_time_ms(elapsed_ms, elapsed, sizeof(elapsed));
        format_time_ms(left, remaining + 1, sizeof(remaining) - 1);
        remaining[0] = '-';
        CP_LV_LABEL_SET_TEXT(now_view.elapsed, elapsed);
        CP_LV_LABEL_SET_TEXT(now_view.remaining, remaining);
    }
}



#endif
