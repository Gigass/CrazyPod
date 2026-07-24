#ifndef CRAZYPOD_PRESETS_H
#define CRAZYPOD_PRESETS_H

#include <stdbool.h>

#include "crazypod_appearance.h"

#define CRAZYPOD_PRESET_COUNT_MAX 12
#define CRAZYPOD_PRESET_NAME_SIZE 32
#define CRAZYPOD_BUILTIN_PRESET_COUNT 2

struct crazypod_preset {
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance appearance;
    bool builtin;
};

void crazypod_presets_load(void);
int crazypod_preset_count(void);
const struct crazypod_preset *crazypod_preset_get(int index);
bool crazypod_preset_apply(int index);
int crazypod_preset_save_current(void);
int crazypod_preset_duplicate(int index);
bool crazypod_preset_update(int index);
bool crazypod_preset_rename(int index, const char *name);
bool crazypod_preset_delete(int index);
bool crazypod_preset_export(int index);
int crazypod_preset_import(void);

#endif
