#ifndef CRAZYPOD_APPEARANCE_H
#define CRAZYPOD_APPEARANCE_H

#include <stdbool.h>
#include <stdint.h>

#define CRAZYPOD_ICON_THEME_COUNT 16
#define CRAZYPOD_APPEARANCE_COLOR_COUNT 8
#define CRAZYPOD_WALLPAPER_PATH_SIZE 260

enum crazypod_appearance_field {
    CRAZYPOD_APPEARANCE_ICON_THEME,
    CRAZYPOD_APPEARANCE_ICON_SCALE,
    CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE,
    CRAZYPOD_APPEARANCE_GLOW,
    CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE,
    CRAZYPOD_APPEARANCE_PRIMARY,
    CRAZYPOD_APPEARANCE_SECONDARY,
    CRAZYPOD_APPEARANCE_HOME_BACKGROUND,
    CRAZYPOD_APPEARANCE_MENU_BACKGROUND,
    CRAZYPOD_APPEARANCE_LOCK_BACKGROUND,
    CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS,
    CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS,
};

struct crazypod_appearance {
    int icon_theme;
    int icon_scale;
    /*
     * Reuses the retired player-style word so existing appearance and preset
     * files keep exactly the same binary layout.
     */
    int sound_wave_style;
    int glow;
    int highlight_style;
    int primary_color;
    int secondary_color;
    int home_background;
    int menu_background;
    int lock_background;
    int screen_top_radius;
    int screen_bottom_radius;
    char home_wallpaper[CRAZYPOD_WALLPAPER_PATH_SIZE];
    char menu_wallpaper[CRAZYPOD_WALLPAPER_PATH_SIZE];
    char lock_wallpaper[CRAZYPOD_WALLPAPER_PATH_SIZE];
};

void crazypod_appearance_load(void);
void crazypod_appearance_save(void);
const struct crazypod_appearance *crazypod_appearance_get(void);
bool crazypod_appearance_valid(const struct crazypod_appearance *value);
bool crazypod_appearance_set(const struct crazypod_appearance *value);
bool crazypod_appearance_set_value(enum crazypod_appearance_field field,
                                   int value);
void crazypod_appearance_set_icon_theme(int theme);
bool crazypod_appearance_set_wallpaper(
    enum crazypod_appearance_field field, const char *path);

const char *crazypod_icon_theme_name(int theme);
const char *crazypod_appearance_color_name(int color);
uint32_t crazypod_appearance_color(int color);
uint32_t crazypod_appearance_home_color(void);
uint32_t crazypod_appearance_menu_color(void);
uint32_t crazypod_appearance_lock_color(void);

#endif
