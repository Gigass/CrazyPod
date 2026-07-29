#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "kernel.h"
#include "settings.h"
#include "lvgl.h"

#include "../../../crazypod_artwork.h"
#include "../../../crazypod_lyrics.h"
#include "../../../crazypod_playlist.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_now_presentation.h"
#include "crazypod_now_screen.h"

#define COLOR_CYAN 0x55D6E7
#define COLOR_FAVORITE 0xFF375F
#define COLOR_WHITE 0xFFFFFF
#define CRAZYPOD_NOW_ARTWORK_CACHE_SIZE CRAZYPOD_COVERFLOW_ARTWORK_SIZE
#define CRAZYPOD_NOW_LYRICS_COVER_SIZE 108
#define CRAZYPOD_NOW_SHADE_COLOR 0x05070A
#define CRAZYPOD_NOW_SHADE_OPA 96
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
    const lv_image_dsc_t *artwork = NULL;
    const lv_image_dsc_t *lyrics_artwork = NULL;
    const lv_image_dsc_t *presentation_backdrop = NULL;
    enum crazypod_artwork_state artwork_state =
        CRAZYPOD_ARTWORK_EMPTY;
    int artwork_slot = CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT;
    uint32_t content_color = COLOR_WHITE;
    lv_obj_t *backdrop;
    lv_obj_t *shade;
    lv_obj_t *title;
    lv_obj_t *artist;
    lv_obj_t *mode;

    memset(view, 0, sizeof(*view));
    rendered_track_path[0] = '\0';
    if(track != NULL) {
        snprintf(rendered_track_path,
                 sizeof(rendered_track_path),
                 "%s", track->path);
        artwork_slot = context->artwork_slot(track);
        artwork = crazypod_artwork_load_priority(
            artwork_slot, track,
            CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 0);
        artwork_state = crazypod_artwork_state(
            artwork_slot, track,
            CRAZYPOD_NOW_ARTWORK_CACHE_SIZE);
        if(artwork_slot == CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT &&
           artwork != NULL) {
            (void)crazypod_artwork_load_priority(
                CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT,
                track, CRAZYPOD_NOW_ARTWORK_CACHE_SIZE, 10);
        }
        if(artwork != NULL &&
           !crazypod_now_presentation_matches(track->path)) {
            context->boost(HZ / 2);
            (void)crazypod_now_presentation_prepare(
                artwork, track->path);
        }
        else if(artwork_state == CRAZYPOD_ARTWORK_EMPTY &&
                !crazypod_now_presentation_matches(track->path))
            crazypod_now_presentation_discard();
    }

    if(track != NULL)
        (void)crazypod_now_presentation_get(
            track->path, &lyrics_artwork,
            &presentation_backdrop, &content_color);
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
                track != NULL ? track->title : "No Track",
                context->metadata_font,
                COLOR_WHITE, LV_OPA_COVER);
            lv_obj_set_size(title, 96, 17);
            lv_obj_set_style_text_align(
                title, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                title, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(title, 24, 120);
            artist = make_label(
                context->parent,
                track != NULL ? track->artist : "Local Music",
                context->metadata_font,
                COLOR_WHITE, 180);
            lv_obj_set_size(artist, 96, 17);
            lv_obj_set_style_text_align(
                artist, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                artist, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(artist, 24, 140);

            view->lyrics_previous = make_label(
                context->parent, previous,
                context->metadata_font,
                content_color, 120);
            lv_obj_set_pos(view->lyrics_previous, 144, 71);
            lv_obj_set_size(view->lyrics_previous, 158, 18);
            lv_label_set_long_mode(
                view->lyrics_previous, LV_LABEL_LONG_MODE_DOTS);
            view->lyrics_current = make_label(
                context->parent, current,
                context->metadata_font,
                content_color, LV_OPA_COVER);
            lv_obj_set_pos(view->lyrics_current, 144, 100);
            lv_obj_set_size(view->lyrics_current, 158, 18);
            lv_label_set_long_mode(
                view->lyrics_current, LV_LABEL_LONG_MODE_DOTS);
            view->lyrics_next = make_label(
                context->parent, next,
                context->metadata_font,
                content_color, 150);
            lv_obj_set_pos(view->lyrics_next, 144, 129);
            lv_obj_set_size(view->lyrics_next, 158, 18);
            lv_label_set_long_mode(
                view->lyrics_next, LV_LABEL_LONG_MODE_DOTS);
        }
        else {
            lv_obj_t *album;
            bool favorite =
                track != NULL &&
                crazypod_music_track_is_favorite(track->path);

            title = make_label(
                context->parent,
                track != NULL ? track->title : "No Track",
                context->metadata_font,
                content_color, LV_OPA_COVER);
            lv_obj_set_size(title, 158, 18);
            lv_obj_set_style_text_align(
                title, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                title, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(title, 144, 71);

            artist = make_label(
                context->parent,
                track != NULL ? track->artist : "Local Music",
                context->metadata_font,
                content_color, 220);
            lv_obj_set_size(artist, 158, 18);
            lv_obj_set_style_text_align(
                artist, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                artist, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(artist, 144, 95);

            album = make_label(
                context->parent,
                track != NULL && track->album[0] != '\0'
                    ? track->album : "",
                &lv_font_montserrat_10,
                content_color, 190);
            lv_obj_set_size(album, 158, 16);
            lv_obj_set_style_text_align(
                album, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(
                album, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_pos(album, 144, 119);

            crazypod_ui_widget_pixel_heart(
                context->parent, 190, 145, 2,
                favorite ? COLOR_FAVORITE : content_color,
                favorite ? LV_OPA_COVER : 120);
            mode = make_label(
                context->parent,
                crazypod_queue_repeat() == REPEAT_ONE ? "1" :
                crazypod_queue_repeat() == REPEAT_ALL ? LV_SYMBOL_LOOP :
                crazypod_queue_shuffle() ? LV_SYMBOL_SHUFFLE :
                LV_SYMBOL_PLAY,
                &lv_font_montserrat_12,
                crazypod_queue_repeat() != REPEAT_OFF ||
                crazypod_queue_shuffle() ? COLOR_CYAN : content_color,
                220);
            lv_obj_set_width(mode, 24);
            lv_obj_set_style_text_align(
                mode, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(mode, 211, 143);
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

static void set_label_text_if_changed(
    lv_obj_t *label, const char *text)
{
    if(label != NULL && text != NULL &&
       strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
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

        crazypod_lyrics_window(
            elapsed_ms, &previous, &current, &next);
        set_label_text_if_changed(now_view.lyrics_previous, previous);
        set_label_text_if_changed(now_view.lyrics_current, current);
        set_label_text_if_changed(now_view.lyrics_next, next);
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
        lv_label_set_text(now_view.elapsed, elapsed);
        lv_label_set_text(now_view.remaining, remaining);
    }
}



#endif
