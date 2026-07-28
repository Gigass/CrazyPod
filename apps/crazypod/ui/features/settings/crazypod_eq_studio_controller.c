#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "settings.h"

#include "../../../crazypod_audio_shims.h"
#include "../../../crazypod_state.h"
#include "crazypod_settings_model.h"
#include "crazypod_eq_studio_controller.h"

#define GAIN_MIN (-240)
#define GAIN_MAX 240
#define GAIN_STEP 10
#define Q_MIN 1
#define Q_MAX 64
#define Q_STEP 1
#define CUTOFF_MIN 20
#define CUTOFF_MAX 22040
#define CUTOFF_STEP 100
#define PRECUT_MIN 0
#define PRECUT_MAX 240
#define PRECUT_STEP 10

static int selected_band = 5;
static enum crazypod_eq_studio_mode selected_mode =
    CRAZYPOD_EQ_STUDIO_GAIN;
static bool editing;

static int clamp(int value, int minimum, int maximum)
{
    if(value < minimum)
        return minimum;
    if(value > maximum)
        return maximum;
    return value;
}

void crazypod_eq_studio_open(void)
{
    selected_band = clamp(selected_band, 0, EQ_NUM_BANDS - 1);
    selected_mode = CRAZYPOD_EQ_STUDIO_GAIN;
    editing = false;
}

void crazypod_eq_studio_close(void)
{
    editing = false;
    crazypod_state_save(false);
}

void crazypod_eq_studio_toggle_editing(void)
{
    editing = !editing;
}

void crazypod_eq_studio_cycle_mode(void)
{
    selected_mode = (enum crazypod_eq_studio_mode)(
        (selected_mode + 1) % CRAZYPOD_EQ_STUDIO_MODE_COUNT);
}

void crazypod_eq_studio_toggle_enabled(void)
{
    global_settings.eq_enabled = !global_settings.eq_enabled;
    crazypod_eq_settings_apply();
    crazypod_state_mark_dirty();
}

static void apply_band(void)
{
    dsp_set_eq_coefs(
        selected_band,
        &global_settings.eq_band_settings[selected_band]);
    crazypod_state_mark_dirty();
}

void crazypod_eq_studio_adjust(int direction)
{
    struct eq_band_setting *band;
    int next;

    if(direction == 0)
        return;
    if(!editing) {
        crazypod_eq_studio_select_band(direction);
        return;
    }
    band = &global_settings.eq_band_settings[selected_band];
    switch(selected_mode) {
    case CRAZYPOD_EQ_STUDIO_CUTOFF:
        next = band->cutoff + direction * CUTOFF_STEP;
        band->cutoff = clamp(next, CUTOFF_MIN, CUTOFF_MAX);
        apply_band();
        break;
    case CRAZYPOD_EQ_STUDIO_Q:
        next = band->q + direction * Q_STEP;
        band->q = clamp(next, Q_MIN, Q_MAX);
        apply_band();
        break;
    case CRAZYPOD_EQ_STUDIO_PRECUT:
        next = global_settings.eq_precut + direction * PRECUT_STEP;
        global_settings.eq_precut =
            clamp(next, PRECUT_MIN, PRECUT_MAX);
        dsp_set_eq_precut(global_settings.eq_precut);
        crazypod_state_mark_dirty();
        break;
    case CRAZYPOD_EQ_STUDIO_GAIN:
    default:
        next = band->gain + direction * GAIN_STEP;
        band->gain = clamp(next, GAIN_MIN, GAIN_MAX);
        apply_band();
        break;
    }
}

void crazypod_eq_studio_select_band(int direction)
{
    if(direction != 0)
        selected_band = clamp(
            selected_band + direction, 0, EQ_NUM_BANDS - 1);
}

struct crazypod_eq_studio_model crazypod_eq_studio_model(void)
{
    struct crazypod_eq_studio_model model = {
        .band = selected_band,
        .mode = selected_mode,
        .editing = editing,
        .enabled = global_settings.eq_enabled,
        .precut = global_settings.eq_precut,
    };

    memcpy(model.bands, global_settings.eq_band_settings,
           sizeof(model.bands));
    return model;
}

const char *crazypod_eq_studio_mode_title(
    enum crazypod_eq_studio_mode mode)
{
    switch(mode) {
    case CRAZYPOD_EQ_STUDIO_CUTOFF: return "Freq";
    case CRAZYPOD_EQ_STUDIO_Q: return "Q";
    case CRAZYPOD_EQ_STUDIO_PRECUT: return "Precut";
    default: return "Gain";
    }
}

const char *crazypod_eq_studio_band_role(int band)
{
    if(band <= 1)
        return "Sub Bass";
    if(band <= 3)
        return "Low Mid";
    if(band <= 5)
        return "Presence";
    if(band <= 7)
        return "Air Detail";
    return "Top End";
}

void crazypod_eq_studio_format_db(
    char *buffer, size_t size, int value)
{
    int absolute = value < 0 ? -value : value;

    snprintf(buffer, size, "%c%d.%d dB",
             value < 0 ? '-' : '+',
             absolute / 10, absolute % 10);
}

void crazypod_eq_studio_format_precut(
    char *buffer, size_t size, int value)
{
    snprintf(buffer, size, value == 0 ? "0.0 dB" : "-%d.%d dB",
             value / 10, value % 10);
}

void crazypod_eq_studio_format_frequency(
    char *buffer, size_t size, int value)
{
    if(value >= 1000 && value % 1000 == 0)
        snprintf(buffer, size, "%dkHz", value / 1000);
    else if(value >= 1000)
        snprintf(buffer, size, "%d.%dkHz", value / 1000,
                 (value % 1000) / 100);
    else
        snprintf(buffer, size, "%dHz", value);
}

void crazypod_eq_studio_format_q(
    char *buffer, size_t size, int value)
{
    snprintf(buffer, size, "%d.%d Q", value / 10, value % 10);
}

#endif
