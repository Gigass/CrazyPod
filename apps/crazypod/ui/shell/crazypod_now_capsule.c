#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "kernel.h"

#include "../../crazypod_appearance.h"
#include "../../crazypod_artwork.h"
#include "../../crazypod_soundwave.h"
#include "../../crazypod_wallpaper.h"
#include "../presentation/crazypod_glass_sampler.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_now_capsule.h"

#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x26CFF5
#define CAPSULE_TINT_COLOR 0x11131A
#define CAPSULE_TINT_OPA 48
#define CAPSULE_FALLBACK_OPA 34
#define SPECTRUM_FRAME_TICKS ((HZ / 10) > 0 ? (HZ / 10) : 1)

struct capsule_view {
    lv_obj_t *root;
    lv_obj_t *glass;
    lv_obj_t *track;
    lv_obj_t *artist;
    lv_obj_t *progress;
    lv_obj_t *spectrum;
    lv_obj_t *wave_ball;
    lv_obj_t *wave_glow;
    lv_obj_t *artwork;
    lv_obj_t *artwork_image;
    lv_obj_t *artwork_symbol;
    char artwork_path[MAX_PATH];
    unsigned artwork_generation_seen;
    int spectrum_phase;
    long spectrum_tick;
    bool spectrum_playing;
};

static struct capsule_view capsule;

void crazypod_now_capsule_initialize_artwork(void)
{
    capsule.artwork_generation_seen =
        crazypod_artwork_slot_generation(
            CRAZYPOD_CAPSULE_ARTWORK_SLOT);
}

void crazypod_now_capsule_poll_artwork(
    const struct crazypod_track *track)
{
    unsigned generation = crazypod_artwork_slot_generation(
        CRAZYPOD_CAPSULE_ARTWORK_SLOT);

    if(generation == capsule.artwork_generation_seen)
        return;
    capsule.artwork_generation_seen = generation;
    crazypod_now_capsule_update_artwork(track);
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

static void set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if(label != NULL && text != NULL &&
       strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

static void set_hidden_if_changed(lv_obj_t *object, bool hidden)
{
    if(object == NULL ||
       lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) == hidden)
        return;
    if(hidden)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
}

void crazypod_now_capsule_refresh_material(void)
{
    const lv_image_dsc_t *glass = NULL;

    if(capsule.root == NULL)
        return;
    if(crazypod_wallpaper_prepare_frosted_capsule(
           CAPSULE_TINT_COLOR, CAPSULE_TINT_OPA))
        glass = crazypod_frosted_wallpaper_capsule();
    if(glass != NULL) {
        if(capsule.glass == NULL) {
            capsule.glass = lv_image_create(capsule.root);
            lv_obj_set_pos(capsule.glass, 0, 0);
            lv_obj_remove_flag(capsule.glass, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_move_to_index(capsule.glass, 0);
        }
        lv_image_set_src(capsule.glass, glass);
        lv_obj_set_style_image_opa(capsule.glass, LV_OPA_COVER, 0);
        lv_obj_remove_flag(capsule.glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(capsule.root, LV_OPA_TRANSP, 0);
    }
    else {
        if(capsule.glass != NULL)
            lv_obj_add_flag(capsule.glass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            capsule.root, lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            capsule.root, CAPSULE_FALLBACK_OPA, 0);
    }
}

void crazypod_now_capsule_refresh_appearance(void)
{
    bool playing =
        (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
        (audio_status() & AUDIO_STATUS_PAUSE) == 0;

    if(capsule.wave_ball != NULL) {
        lv_obj_set_style_shadow_width(
            capsule.wave_ball, playing ? 10 : 4, 0);
        lv_obj_set_style_shadow_color(
            capsule.wave_ball, lv_color_hex(primary_color()), 0);
        lv_obj_set_style_shadow_opa(
            capsule.wave_ball, playing ? 112 : 34, 0);
    }
    if(capsule.wave_glow != NULL) {
        lv_obj_set_style_bg_color(
            capsule.wave_glow, lv_color_hex(primary_color()), 0);
        lv_obj_set_style_bg_grad_color(
            capsule.wave_glow, lv_color_hex(secondary_color()), 0);
        lv_obj_set_style_bg_grad_dir(
            capsule.wave_glow, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_bg_opa(
            capsule.wave_glow, playing ? 82 : 18, 0);
    }
    if(capsule.spectrum != NULL)
        lv_obj_invalidate(capsule.spectrum);
}

static void draw_spectrum(lv_event_t *event)
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
    crazypod_sound_wave_draw_ball(
        layer, &area,
        (enum crazypod_sound_wave_style)
            crazypod_appearance_get()->sound_wave_style,
        capsule.spectrum_phase, playing,
        primary_color(), secondary_color());
}

void crazypod_now_capsule_create(
    lv_obj_t *parent, const lv_font_t *metadata_font)
{
    lv_obj_t *progress_track;
    lv_obj_t *glass_border;

    memset(&capsule, 0, sizeof(capsule));
    capsule.root = crazypod_ui_widget_box(
        parent, 8, 174, 304, 58, 29,
        COLOR_WHITE, CAPSULE_FALLBACK_OPA);
    capsule.artwork = crazypod_ui_widget_box(
        capsule.root, 17, 8, 42, 42, 9, 0x941FFC, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        capsule.artwork, lv_color_hex(0x2E5CFA), 0);
    lv_obj_set_style_bg_grad_dir(
        capsule.artwork, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_clip_corner(capsule.artwork, true, 0);
    capsule.artwork_image = lv_image_create(capsule.artwork);
    lv_obj_center(capsule.artwork_image);
    lv_obj_remove_flag(capsule.artwork_image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(capsule.artwork_image, LV_OBJ_FLAG_HIDDEN);
    capsule.artwork_symbol = crazypod_ui_widget_label(
        capsule.artwork, LV_SYMBOL_AUDIO,
        &lv_font_montserrat_16, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_center(capsule.artwork_symbol);

    capsule.track = crazypod_ui_widget_label(
        capsule.root, "No Track", metadata_font,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(capsule.track, 60, 7);
    lv_obj_set_size(capsule.track, 171, 17);
    lv_obj_set_style_text_align(
        capsule.track, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(capsule.track, LV_LABEL_LONG_MODE_DOTS);
    capsule.artist = crazypod_ui_widget_label(
        capsule.root, "Local Music", metadata_font,
        COLOR_WHITE, 190);
    lv_obj_set_pos(capsule.artist, 60, 25);
    lv_obj_set_size(capsule.artist, 171, 17);
    lv_obj_set_style_text_align(
        capsule.artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(capsule.artist, LV_LABEL_LONG_MODE_DOTS);

    progress_track = crazypod_ui_widget_box(
        capsule.root, 60, 45, 171, 3,
        LV_RADIUS_CIRCLE, COLOR_WHITE, 31);
    capsule.progress = crazypod_ui_widget_box(
        progress_track, 0, 0, 6, 3, LV_RADIUS_CIRCLE,
        0x2ECC71, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        capsule.progress, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_bg_grad_dir(
        capsule.progress, LV_GRAD_DIR_HOR, 0);

    capsule.wave_ball = crazypod_ui_widget_box(
        capsule.root, 253, 8, 42, 42,
        LV_RADIUS_CIRCLE, 0x080A14, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        capsule.wave_ball, lv_color_hex(0x1A1F38), 0);
    lv_obj_set_style_bg_grad_dir(
        capsule.wave_ball, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_clip_corner(capsule.wave_ball, true, 0);
    lv_obj_set_style_border_width(capsule.wave_ball, 1, 0);
    lv_obj_set_style_border_color(
        capsule.wave_ball, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(capsule.wave_ball, 66, 0);
    capsule.wave_glow = crazypod_ui_widget_box(
        capsule.wave_ball, 0, 0, 32, 32,
        LV_RADIUS_CIRCLE, primary_color(), 82);
    lv_obj_center(capsule.wave_glow);
    capsule.spectrum = lv_obj_create(capsule.wave_ball);
    crazypod_ui_widget_make_plain(capsule.spectrum);
    lv_obj_set_size(capsule.spectrum, 42, 42);
    lv_obj_center(capsule.spectrum);
    lv_obj_remove_flag(capsule.spectrum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        capsule.spectrum, draw_spectrum, LV_EVENT_DRAW_MAIN, NULL);

    glass_border = crazypod_ui_widget_box(
        capsule.root, 0, 0, 304, 58, 29,
        COLOR_WHITE, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(glass_border, 1, 0);
    lv_obj_set_style_border_color(
        glass_border, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        glass_border,
        crazypod_glass_material_border_opa(
            CRAZYPOD_GLASS_HOME_CAPSULE), 0);
    lv_obj_remove_flag(glass_border, LV_OBJ_FLAG_CLICKABLE);
    crazypod_now_capsule_refresh_appearance();
}

void crazypod_now_capsule_update_artwork(
    const struct crazypod_track *track)
{
    const lv_image_dsc_t *descriptor = NULL;
    enum crazypod_artwork_state state = CRAZYPOD_ARTWORK_EMPTY;

    if(capsule.artwork == NULL)
        return;
    if(track != NULL) {
        descriptor = crazypod_artwork_load(
            CRAZYPOD_CAPSULE_ARTWORK_SLOT, track,
            CRAZYPOD_CAPSULE_ARTWORK_SIZE);
        state = crazypod_artwork_state(
            CRAZYPOD_CAPSULE_ARTWORK_SLOT, track,
            CRAZYPOD_CAPSULE_ARTWORK_SIZE);
        (void)crazypod_artwork_load_priority(
            CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT, track,
            CRAZYPOD_COVERFLOW_ARTWORK_SIZE, 8);
        if(state == CRAZYPOD_ARTWORK_PENDING)
            return;
        if(strcmp(capsule.artwork_path, track->path) != 0) {
            snprintf(capsule.artwork_path,
                     sizeof(capsule.artwork_path), "%s", track->path);
            lv_obj_set_style_bg_color(
                capsule.artwork,
                lv_color_hex(artwork_color(track->album, 0)), 0);
            lv_obj_set_style_bg_grad_color(
                capsule.artwork,
                lv_color_hex(artwork_color(track->artist, 1)), 0);
        }
    }
    else {
        if(capsule.artwork_path[0] == '\0')
            return;
        capsule.artwork_path[0] = '\0';
        lv_obj_set_style_bg_color(
            capsule.artwork, lv_color_hex(0x941FFC), 0);
        lv_obj_set_style_bg_grad_color(
            capsule.artwork, lv_color_hex(0x2E5CFA), 0);
    }
    if(descriptor != NULL) {
        if(lv_image_get_src(capsule.artwork_image) != descriptor)
            lv_image_set_src(capsule.artwork_image, descriptor);
        set_hidden_if_changed(capsule.artwork_image, false);
        set_hidden_if_changed(capsule.artwork_symbol, true);
    }
    else {
        set_hidden_if_changed(capsule.artwork_image, true);
        set_hidden_if_changed(capsule.artwork_symbol, false);
    }
}

void crazypod_now_capsule_update(
    const struct crazypod_track *track,
    uint32_t elapsed_ms, uint32_t length_ms)
{
    int width = 6;
    const char *track_text =
        track != NULL ? track->title : "No Track";
    const char *artist_text =
        track != NULL ? track->artist : "Local Music";

    set_label_text_if_changed(capsule.track, track_text);
    set_label_text_if_changed(capsule.artist, artist_text);
    crazypod_now_capsule_update_artwork(track);
    if(length_ms > 0) {
        width = 171 * elapsed_ms / length_ms;
        if(width < 6)
            width = 6;
        if(width > 171)
            width = 171;
    }
    if(lv_obj_get_width(capsule.progress) != width)
        lv_obj_set_width(capsule.progress, width);
}

void crazypod_now_capsule_reset_motion(long now)
{
    capsule.spectrum_tick = now;
    capsule.spectrum_phase = 0;
    capsule.spectrum_playing = false;
}

void crazypod_now_capsule_tick(long now, bool home_active)
{
    bool playing;

    if(!home_active || capsule.spectrum == NULL)
        return;
    playing = (audio_status() & AUDIO_STATUS_PLAY) != 0 &&
              (audio_status() & AUDIO_STATUS_PAUSE) == 0;
    if(!playing) {
        if(capsule.spectrum_playing) {
            capsule.spectrum_playing = false;
            crazypod_now_capsule_refresh_appearance();
            lv_obj_invalidate(capsule.spectrum);
        }
        return;
    }
    if(TIME_BEFORE(
           now, capsule.spectrum_tick + SPECTRUM_FRAME_TICKS))
        return;
    capsule.spectrum_tick = now;
    if(!capsule.spectrum_playing) {
        capsule.spectrum_playing = true;
        crazypod_now_capsule_refresh_appearance();
    }
    capsule.spectrum_phase =
        (capsule.spectrum_phase + 1) & 0x7fff;
    lv_obj_invalidate(capsule.spectrum);
}

#endif
