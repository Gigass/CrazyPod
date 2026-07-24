#ifndef CRAZYPOD_APPEARANCE_H
#define CRAZYPOD_APPEARANCE_H

#include <stdbool.h>
#include <stdint.h>

#define CRAZYPOD_ICON_THEME_COUNT 16
#define CRAZYPOD_APPEARANCE_COLOR_COUNT 8

enum crazypod_appearance_field {
    CRAZYPOD_APPEARANCE_ICON_THEME,
    CRAZYPOD_APPEARANCE_ICON_SCALE,
    CRAZYPOD_APPEARANCE_PLAYER_STYLE,
    CRAZYPOD_APPEARANCE_GLOW,
    CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE,
    CRAZYPOD_APPEARANCE_PRIMARY,
    CRAZYPOD_APPEARANCE_SECONDARY,
    CRAZYPOD_APPEARANCE_HOME_BACKGROUND,
    CRAZYPOD_APPEARANCE_MENU_BACKGROUND,
};

struct crazypod_appearance {
    int icon_theme;
    int icon_scale;
    int player_style;
    int glow;
    int highlight_style;
    int primary_color;
    int secondary_color;
    int home_background;
    int menu_background;
};

void crazypod_appearance_load(void);
void crazypod_appearance_save(void);
const struct crazypod_appearance *crazypod_appearance_get(void);
bool crazypod_appearance_valid(const struct crazypod_appearance *value);
bool crazypod_appearance_set(const struct crazypod_appearance *value);
void crazypod_appearance_cycle(enum crazypod_appearance_field field);
void crazypod_appearance_set_icon_theme(int theme);

const char *crazypod_icon_theme_name(int theme);
const char *crazypod_appearance_color_name(int color);
uint32_t crazypod_appearance_color(int color);
uint32_t crazypod_appearance_home_color(void);
uint32_t crazypod_appearance_menu_color(void);

#endif
