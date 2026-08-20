#ifndef CRAZYPOD_CUSTOMIZE_CATALOG_H
#define CRAZYPOD_CUSTOMIZE_CATALOG_H

#include <stdint.h>

#include "../../../crazypod_appearance.h"
#include "../../../crazypod_wallpaper.h"

#define CRAZYPOD_CUSTOMIZE_MENU_COUNT 7
#define CRAZYPOD_CUSTOMIZE_PRESET_ACTION_COUNT 3
#define CRAZYPOD_CUSTOMIZE_PRESET_EDIT_COUNT 3
#define CRAZYPOD_CUSTOMIZE_DETAIL_COUNT 6
#define CRAZYPOD_CUSTOMIZE_LAYOUT_COUNT 2
#define CRAZYPOD_CUSTOMIZE_RADIUS_COUNT 8
#define CRAZYPOD_CUSTOMIZE_BACKGROUND_COUNT 3

extern const char *const crazypod_customize_menu_titles[
    CRAZYPOD_CUSTOMIZE_MENU_COUNT];
extern const char *const crazypod_customize_menu_symbols[
    CRAZYPOD_CUSTOMIZE_MENU_COUNT];
extern const char *const crazypod_customize_preset_actions[
    CRAZYPOD_CUSTOMIZE_PRESET_ACTION_COUNT];
extern const char *const crazypod_customize_preset_edit_actions[
    CRAZYPOD_CUSTOMIZE_PRESET_EDIT_COUNT];
extern const char *const crazypod_customize_detail_titles[
    CRAZYPOD_CUSTOMIZE_DETAIL_COUNT];
extern const enum crazypod_appearance_field
    crazypod_customize_detail_fields[CRAZYPOD_CUSTOMIZE_DETAIL_COUNT];
extern const char *const crazypod_customize_layout_titles[
    CRAZYPOD_CUSTOMIZE_LAYOUT_COUNT];
extern const enum crazypod_appearance_field
    crazypod_customize_layout_fields[CRAZYPOD_CUSTOMIZE_LAYOUT_COUNT];
extern const int crazypod_customize_radius_values[
    CRAZYPOD_CUSTOMIZE_RADIUS_COUNT];
extern const char *const crazypod_customize_background_titles[
    CRAZYPOD_CUSTOMIZE_BACKGROUND_COUNT];

enum crazypod_appearance_field
crazypod_customize_background_field(int index);
const char *crazypod_customize_background_title(
    enum crazypod_appearance_field field);
const char *crazypod_customize_background_wallpaper(
    const struct crazypod_appearance *appearance,
    enum crazypod_appearance_field field);
enum crazypod_wallpaper_target
crazypod_customize_background_target(
    enum crazypod_appearance_field field);
uint32_t crazypod_customize_background_default_color(
    enum crazypod_appearance_field field);
int crazypod_customize_field_value(
    enum crazypod_appearance_field field);
int crazypod_customize_choice_count(
    enum crazypod_appearance_field field);
int crazypod_customize_choice_value(
    enum crazypod_appearance_field field, int index);
int crazypod_customize_choice_index(
    enum crazypod_appearance_field field);
const char *crazypod_customize_choice_title(
    enum crazypod_appearance_field field, int index);
const char *crazypod_customize_field_title(
    enum crazypod_appearance_field field);

#endif
