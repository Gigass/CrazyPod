#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"

#include "../../../crazypod_appearance.h"
#include "../../../crazypod_artwork.h"
#include "../../../crazypod_artwork_palette.h"
#include "../../../crazypod_playlist.h"
#include "../../../crazypod_runtime_font.h"
#include "../../../crazypod_soundwave.h"
#include "crazypod_now_playing_feature.h"
#include "crazypod_now_presentation.h"
#include "crazypod_now_screen.h"

#define NOW_SHADE_OPA 118
#define COLOR_WHITE 0xFFFFFF

static struct crazypod_artwork_palette wave_palette;
static char wave_palette_path[MAX_PATH];
static unsigned wave_palette_generation;
static bool wave_palette_from_artwork;

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

static void use_fallback_wave_palette(void)
{
    crazypod_artwork_palette_fallback(
        &wave_palette, primary_color(), secondary_color());
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

static bool copy_current_track(struct crazypod_track *track)
{
    char path[MAX_PATH];

    return crazypod_queue_copy_path(
            crazypod_queue_index(), path, sizeof(path)) &&
        crazypod_music_copy_track(crazypod_music_find_track(path), track);
}

static void refresh_wave_palette(
    const lv_image_dsc_t *artwork, const char *track_path,
    unsigned generation)
{
    if(artwork == NULL || track_path == NULL) {
        wave_palette_path[0] = '\0';
        wave_palette_generation = 0;
        wave_palette_from_artwork = false;
        use_fallback_wave_palette();
        return;
    }
    if(generation == wave_palette_generation &&
       strcmp(wave_palette_path, track_path) == 0) {
        if(!wave_palette_from_artwork)
            use_fallback_wave_palette();
        return;
    }

    wave_palette_from_artwork =
        crazypod_artwork_palette_extract(
            artwork, &wave_palette);
    if(!wave_palette_from_artwork)
        use_fallback_wave_palette();
    snprintf(
        wave_palette_path, sizeof(wave_palette_path),
        "%s", track_path);
    wave_palette_generation = generation;
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
        wave_palette.primary,
        wave_palette.secondary,
        wave_palette.highlight);
}

void crazypod_now_playing_feature_render(
    const struct crazypod_now_playing_render_context *context)
{
    struct crazypod_track track;
    bool have_track = copy_current_track(&track);
    const char *visual_track_path;
    unsigned visual_generation;
    const lv_image_dsc_t *visual_artwork;

    crazypod_now_playing_artwork_sync();
    visual_artwork = crazypod_now_playing_artwork_committed(
        &visual_track_path, &visual_generation);
    refresh_wave_palette(
        visual_artwork, visual_track_path, visual_generation);
    {
        const struct crazypod_now_screen_context screen = {
            .parent = context->parent,
            .track = have_track ? &track : NULL,
            .lyrics_mode = crazypod_now_playing_lyrics_mode(),
            .metadata_font = context->metadata_font,
            .lyrics_context_font = crazypod_runtime_font_resolve(
                CRAZYPOD_FONT_FAMILY_SYSTEM, 12, 400,
                CRAZYPOD_FONT_STYLE_NORMAL, 16),
            .lyrics_current_font = crazypod_runtime_font_resolve(
                CRAZYPOD_FONT_FAMILY_SYSTEM, 16, 700,
                CRAZYPOD_FONT_STYLE_NORMAL, 20),
            .primary_color = primary_color(),
            .visual_artwork = visual_artwork,
            .visual_track_path = visual_track_path,
            .visual_generation = visual_generation,
            .fallback_color = fallback_color,
            .render_artwork = context->render_artwork,
            .boost = context->boost,
            .draw_wave = draw_wave,
        };

        crazypod_now_screen_render(&screen);
    }
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
    wave_palette_path[0] = '\0';
    wave_palette_generation = 0;
    wave_palette_from_artwork = false;
    use_fallback_wave_palette();
    crazypod_now_presentation_discard();
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
