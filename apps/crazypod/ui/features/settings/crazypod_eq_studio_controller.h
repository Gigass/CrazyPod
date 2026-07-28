#ifndef CRAZYPOD_EQ_STUDIO_CONTROLLER_H
#define CRAZYPOD_EQ_STUDIO_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>

#include "settings.h"

enum crazypod_eq_studio_mode {
    CRAZYPOD_EQ_STUDIO_GAIN = 0,
    CRAZYPOD_EQ_STUDIO_CUTOFF,
    CRAZYPOD_EQ_STUDIO_Q,
    CRAZYPOD_EQ_STUDIO_PRECUT,
    CRAZYPOD_EQ_STUDIO_MODE_COUNT,
};

struct crazypod_eq_studio_model {
    int band;
    enum crazypod_eq_studio_mode mode;
    bool editing;
    bool enabled;
    int precut;
    struct eq_band_setting bands[EQ_NUM_BANDS];
};

void crazypod_eq_studio_open(void);
void crazypod_eq_studio_close(void);
void crazypod_eq_studio_toggle_editing(void);
void crazypod_eq_studio_cycle_mode(void);
void crazypod_eq_studio_toggle_enabled(void);
void crazypod_eq_studio_adjust(int direction);
void crazypod_eq_studio_select_band(int direction);
struct crazypod_eq_studio_model crazypod_eq_studio_model(void);
const char *crazypod_eq_studio_mode_title(
    enum crazypod_eq_studio_mode mode);
const char *crazypod_eq_studio_band_role(int band);
void crazypod_eq_studio_format_db(
    char *buffer, size_t size, int value);
void crazypod_eq_studio_format_precut(
    char *buffer, size_t size, int value);
void crazypod_eq_studio_format_frequency(
    char *buffer, size_t size, int value);
void crazypod_eq_studio_format_q(
    char *buffer, size_t size, int value);

#endif
