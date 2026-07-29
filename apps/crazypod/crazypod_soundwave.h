#ifndef CRAZYPOD_SOUNDWAVE_H
#define CRAZYPOD_SOUNDWAVE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#define CRAZYPOD_SOUND_WAVE_STYLE_COUNT 6

enum crazypod_sound_wave_style {
    CRAZYPOD_SOUND_WAVE_TORRENT = 0,
    CRAZYPOD_SOUND_WAVE_RADIAL_SPECTRUM,
    CRAZYPOD_SOUND_WAVE_LIQUID_RIBBON,
    CRAZYPOD_SOUND_WAVE_VINYL_GROOVE,
    CRAZYPOD_SOUND_WAVE_MINI_LED_METER,
    CRAZYPOD_SOUND_WAVE_PARTICLE_PULSE,
};

const char *crazypod_sound_wave_style_name(int style);

void crazypod_sound_wave_draw_bar(
    lv_layer_t *layer, const lv_area_t *area,
    enum crazypod_sound_wave_style style, int phase, bool playing,
    uint32_t primary_color, uint32_t secondary_color,
    uint32_t highlight_color);

void crazypod_sound_wave_draw_ball(
    lv_layer_t *layer, const lv_area_t *area,
    enum crazypod_sound_wave_style style, int phase, bool playing,
    uint32_t primary_color, uint32_t secondary_color,
    uint32_t highlight_color);

#endif
