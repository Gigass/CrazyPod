#include "config.h"

#ifdef IPOD_6G

#include "audio.h"

#include "../../../crazypod_appearance.h"
#include "../../../crazypod_playlist.h"
#include "../../../crazypod_soundwave.h"
#include "crazypod_now_playing_feature.h"
#include "crazypod_now_screen.h"

#define NOW_SHADE_OPA 118
#define COLOR_WHITE 0xFFFFFF

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

static uint32_t artwork_color(
    const char *text, int variant)
{
    static const uint32_t palette[] = {
        0x8A2BE2, 0x1D78F2, 0xE5446D, 0xE4812C,
        0x13A48C, 0x5A55D6, 0xB0388E, 0x276A82
    };

    return palette[
        (text_hash(text) + (uint32_t)variant * 3u) %
        (sizeof(palette) / sizeof(palette[0]))];
}

static uint32_t primary_color(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->primary_color);
}

static uint32_t secondary_color(void)
{
    return crazypod_appearance_color(
        crazypod_appearance_get()->secondary_color);
}

static uint32_t contrast_color(unsigned luminance)
{
    return luminance >= 118 ? 0x09090D : COLOR_WHITE;
}

static uint32_t fallback_color(
    const struct crazypod_track *track)
{
    uint32_t first = artwork_color(
        track != NULL ? track->album : "", 0);
    uint32_t second = artwork_color(
        track != NULL ? track->artist : "", 1);
    unsigned red =
        (((first >> 16) & 0xff) +
         ((second >> 16) & 0xff)) / 2;
    unsigned green =
        (((first >> 8) & 0xff) +
         ((second >> 8) & 0xff)) / 2;
    unsigned blue =
        ((first & 0xff) + (second & 0xff)) / 2;

    red = (red * (255 - NOW_SHADE_OPA) +
           5 * NOW_SHADE_OPA + 127) / 255;
    green = (green * (255 - NOW_SHADE_OPA) +
             5 * NOW_SHADE_OPA + 127) / 255;
    blue = (blue * (255 - NOW_SHADE_OPA) +
            8 * NOW_SHADE_OPA + 127) / 255;
    return contrast_color(
        (54 * red + 183 * green + 19 * blue) >> 8);
}

static const struct crazypod_track *current_track(void)
{
    const char *path =
        crazypod_queue_path(crazypod_queue_index());

    return crazypod_music_track(
        crazypod_music_find_track(path));
}

static void draw_wave(lv_event_t *event)
{
    lv_obj_t *surface = lv_event_get_target(event);
    lv_layer_t *layer;
    lv_area_t area;
    bool playing;

    if(lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;
    layer = lv_event_get_layer(event);
    lv_obj_get_coords(surface, &area);
    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
        (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    crazypod_sound_wave_draw_bar(
        layer, &area,
        (enum crazypod_sound_wave_style)
            crazypod_appearance_get()->sound_wave_style,
        crazypod_now_screen_wave_phase(), playing,
        primary_color(), secondary_color());
}

void crazypod_now_playing_feature_render(
    const struct crazypod_now_playing_render_context *context)
{
    const struct crazypod_now_screen_context screen = {
        .parent = context->parent,
        .track = current_track(),
        .lyrics_mode = crazypod_now_playing_lyrics_mode(),
        .metadata_font = context->metadata_font,
        .primary_color = primary_color(),
        .artwork_slot = crazypod_now_playing_artwork_slot,
        .fallback_color = fallback_color,
        .render_artwork = context->render_artwork,
        .boost = context->boost,
        .draw_wave = draw_wave,
    };

    crazypod_now_screen_render(&screen);
}

void crazypod_now_playing_feature_tick_wave(
    long now, bool active)
{
    crazypod_now_screen_tick_wave(
        now, active,
        crazypod_now_playing_overlay_visible());
}

void crazypod_now_playing_feature_reset_screen(void)
{
    crazypod_now_screen_reset();
}

const char *crazypod_now_playing_feature_rendered_track_path(void)
{
    return crazypod_now_screen_rendered_track_path();
}

void crazypod_now_playing_feature_update_playback(
    uint32_t elapsed_ms, uint32_t length_ms)
{
    crazypod_now_screen_update_playback(
        elapsed_ms, length_ms);
}

#endif
