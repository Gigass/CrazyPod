#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_appearance.h"
#include "crazypod_checksum.h"
#include "crazypod_soundwave.h"

#define APPEARANCE_DIRECTORY "/.crazypod"
#define APPEARANCE_PATH APPEARANCE_DIRECTORY "/appearance.bin"
#define APPEARANCE_TEMP_PATH APPEARANCE_DIRECTORY "/appearance.tmp"
#define APPEARANCE_MAGIC 0x43504150u
#define APPEARANCE_VERSION 3u

struct crazypod_appearance_v1 {
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

struct crazypod_appearance_v2 {
    int icon_theme;
    int icon_scale;
    int sound_wave_style;
    int glow;
    int highlight_style;
    int primary_color;
    int secondary_color;
    int home_background;
    int menu_background;
    int screen_top_radius;
    int screen_bottom_radius;
    char home_wallpaper[CRAZYPOD_WALLPAPER_PATH_SIZE];
    char menu_wallpaper[CRAZYPOD_WALLPAPER_PATH_SIZE];
};

struct appearance_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    struct crazypod_appearance value;
    uint32_t checksum;
};

struct appearance_disk_v2 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    struct crazypod_appearance_v2 value;
    uint32_t checksum;
};

struct appearance_disk_v1 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    struct crazypod_appearance_v1 value;
    uint32_t checksum;
};

static struct crazypod_appearance appearance;

static const char *const theme_names[CRAZYPOD_ICON_THEME_COUNT] = {
    CP_TR("Basic"), CP_TR("Cel Frame"), CP_TR("Anime Pop"), CP_TR("Mecha Spec"),
    CP_TR("Toy"), CP_TR("Y2K"), CP_TR("Flat"), CP_TR("Skeuo"),
    CP_TR("Lucid Pop"), CP_TR("Noize Bloom"), CP_TR("Soft Skeuo"), CP_TR("Acrylic"),
    CP_TR("Ink"), CP_TR("Sticker"), CP_TR("Sticker 2"), CP_TR("Voxel")
};

static const char *const color_names[CRAZYPOD_APPEARANCE_COLOR_COUNT] = {
    CP_TR("Charcoal"), CP_TR("Rose"), CP_TR("Violet"), CP_TR("Cyan"),
    CP_TR("Amber"), CP_TR("Emerald"), CP_TR("Blue"), CP_TR("White")
};

static const uint32_t colors[CRAZYPOD_APPEARANCE_COLOR_COUNT] = {
    0x262626, 0xFF2E54, 0x8A45AB, 0x00D1FF,
    0xFFA838, 0x299E66, 0x2B66E6, 0xFFFFFF
};

static uint32_t disk_checksum(const struct appearance_disk *disk)
{
    return crazypod_checksum_with_zeroed_u32(
        disk, sizeof(*disk),
        offsetof(struct appearance_disk, checksum));
}

static uint32_t disk_v1_checksum(const struct appearance_disk_v1 *disk)
{
    return crazypod_checksum_with_zeroed_u32(
        disk, sizeof(*disk),
        offsetof(struct appearance_disk_v1, checksum));
}

static uint32_t disk_v2_checksum(const struct appearance_disk_v2 *disk)
{
    return crazypod_checksum_with_zeroed_u32(
        disk, sizeof(*disk),
        offsetof(struct appearance_disk_v2, checksum));
}

static bool valid_wallpaper_path(const char *path)
{
    size_t length = 0;

    if(path == NULL)
        return false;
    while(length < CRAZYPOD_WALLPAPER_PATH_SIZE &&
          path[length] != '\0')
        ++length;
    if(length >= CRAZYPOD_WALLPAPER_PATH_SIZE)
        return false;
    return length == 0 || path[0] == '/';
}

bool crazypod_appearance_valid(const struct crazypod_appearance *value)
{
    if(value == NULL)
        return false;
    return value->icon_theme >= 0 &&
           value->icon_theme < CRAZYPOD_ICON_THEME_COUNT &&
           value->icon_scale >= 0 && value->icon_scale < 5 &&
           value->sound_wave_style >= 0 &&
           value->sound_wave_style < CRAZYPOD_SOUND_WAVE_STYLE_COUNT &&
           value->glow >= 0 && value->glow < 4 &&
           value->highlight_style >= 0 && value->highlight_style < 2 &&
           value->primary_color >= 0 &&
           value->primary_color < CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->secondary_color >= 0 &&
           value->secondary_color < CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->home_background >= 0 &&
           value->home_background <= CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->menu_background >= 0 &&
           value->menu_background <= CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->lock_background >= 0 &&
           value->lock_background <= CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->screen_top_radius >= 0 &&
           value->screen_top_radius <= 32 &&
           value->screen_bottom_radius >= 0 &&
           value->screen_bottom_radius <= 32 &&
           valid_wallpaper_path(value->home_wallpaper) &&
           valid_wallpaper_path(value->menu_wallpaper) &&
           valid_wallpaper_path(value->lock_wallpaper);
}

void crazypod_appearance_load(void)
{
    struct appearance_disk disk;
    struct appearance_disk_v2 disk_v2;
    struct appearance_disk_v1 disk_v1;
    uint32_t header[3];
    int fd;

    memset(&appearance, 0, sizeof(appearance));
    appearance.icon_scale = 4;
    appearance.sound_wave_style = CRAZYPOD_SOUND_WAVE_MINI_LED_METER;
    appearance.glow = 1;
    appearance.highlight_style = 1;
    appearance.primary_color = 1;
    appearance.secondary_color = 2;

    fd = open(APPEARANCE_PATH, O_RDONLY);
    if(fd < 0)
        return;
    if(read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
       lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return;
    }
    if(header[0] == APPEARANCE_MAGIC &&
       header[1] == APPEARANCE_VERSION &&
       header[2] == sizeof(disk) &&
       read(fd, &disk, sizeof(disk)) == (ssize_t)sizeof(disk) &&
       disk.checksum == disk_checksum(&disk) &&
       crazypod_appearance_valid(&disk.value)) {
        appearance = disk.value;
    }
    else if(header[0] == APPEARANCE_MAGIC &&
            header[1] == 2u &&
            header[2] == sizeof(disk_v2) &&
            lseek(fd, 0, SEEK_SET) >= 0 &&
            read(fd, &disk_v2, sizeof(disk_v2)) ==
                (ssize_t)sizeof(disk_v2) &&
            disk_v2.checksum == disk_v2_checksum(&disk_v2)) {
        appearance.icon_theme = disk_v2.value.icon_theme;
        appearance.icon_scale = disk_v2.value.icon_scale;
        appearance.sound_wave_style = disk_v2.value.sound_wave_style;
        appearance.glow = disk_v2.value.glow;
        appearance.highlight_style = disk_v2.value.highlight_style;
        appearance.primary_color = disk_v2.value.primary_color;
        appearance.secondary_color = disk_v2.value.secondary_color;
        appearance.home_background = disk_v2.value.home_background;
        appearance.menu_background = disk_v2.value.menu_background;
        appearance.screen_top_radius =
            disk_v2.value.screen_top_radius;
        appearance.screen_bottom_radius =
            disk_v2.value.screen_bottom_radius;
        snprintf(appearance.home_wallpaper,
                 sizeof(appearance.home_wallpaper), "%s",
                 disk_v2.value.home_wallpaper);
        snprintf(appearance.menu_wallpaper,
                 sizeof(appearance.menu_wallpaper), "%s",
                 disk_v2.value.menu_wallpaper);
        if(crazypod_appearance_valid(&appearance))
            crazypod_appearance_save();
    }
    else if(header[0] == APPEARANCE_MAGIC &&
            header[1] == 1u &&
            header[2] == sizeof(disk_v1) &&
            lseek(fd, 0, SEEK_SET) >= 0 &&
            read(fd, &disk_v1, sizeof(disk_v1)) ==
                (ssize_t)sizeof(disk_v1) &&
            disk_v1.checksum == disk_v1_checksum(&disk_v1)) {
        appearance.icon_theme = disk_v1.value.icon_theme;
        appearance.icon_scale = disk_v1.value.icon_scale;
        appearance.sound_wave_style =
            disk_v1.value.player_style >= 0 &&
            disk_v1.value.player_style < CRAZYPOD_SOUND_WAVE_STYLE_COUNT
                ? disk_v1.value.player_style
                : CRAZYPOD_SOUND_WAVE_TORRENT;
        appearance.glow = disk_v1.value.glow;
        appearance.highlight_style = disk_v1.value.highlight_style;
        appearance.primary_color = disk_v1.value.primary_color;
        appearance.secondary_color = disk_v1.value.secondary_color;
        appearance.home_background = disk_v1.value.home_background;
        appearance.menu_background = disk_v1.value.menu_background;
        if(crazypod_appearance_valid(&appearance))
            crazypod_appearance_save();
    }
    close(fd);
}

void crazypod_appearance_save(void)
{
    struct appearance_disk disk;
    int fd;

    mkdir(APPEARANCE_DIRECTORY);
    memset(&disk, 0, sizeof(disk));
    disk.magic = APPEARANCE_MAGIC;
    disk.version = APPEARANCE_VERSION;
    disk.size = sizeof(disk);
    disk.value = appearance;
    disk.checksum = disk_checksum(&disk);

    fd = open(APPEARANCE_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return;
    if(write(fd, &disk, sizeof(disk)) == (ssize_t)sizeof(disk) &&
       fsync(fd) == 0) {
        close(fd);
        rename(APPEARANCE_TEMP_PATH, APPEARANCE_PATH);
        return;
    }
    close(fd);
}

const struct crazypod_appearance *crazypod_appearance_get(void)
{
    return &appearance;
}

bool crazypod_appearance_set(const struct crazypod_appearance *value)
{
    if(!crazypod_appearance_valid(value))
        return false;
    appearance = *value;
    crazypod_appearance_save();
    return true;
}

bool crazypod_appearance_set_value(enum crazypod_appearance_field field,
                                   int value)
{
    struct crazypod_appearance next = appearance;

    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_THEME:
        next.icon_theme = value;
        break;
    case CRAZYPOD_APPEARANCE_ICON_SCALE:
        next.icon_scale = value;
        break;
    case CRAZYPOD_APPEARANCE_SOUND_WAVE_STYLE:
        next.sound_wave_style = value;
        break;
    case CRAZYPOD_APPEARANCE_GLOW:
        next.glow = value;
        break;
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE:
        next.highlight_style = value;
        break;
    case CRAZYPOD_APPEARANCE_PRIMARY:
        next.primary_color = value;
        break;
    case CRAZYPOD_APPEARANCE_SECONDARY:
        next.secondary_color = value;
        break;
    case CRAZYPOD_APPEARANCE_HOME_BACKGROUND:
        next.home_background = value;
        break;
    case CRAZYPOD_APPEARANCE_MENU_BACKGROUND:
        next.menu_background = value;
        break;
    case CRAZYPOD_APPEARANCE_LOCK_BACKGROUND:
        next.lock_background = value;
        break;
    case CRAZYPOD_APPEARANCE_SCREEN_TOP_RADIUS:
        next.screen_top_radius = value;
        break;
    case CRAZYPOD_APPEARANCE_SCREEN_BOTTOM_RADIUS:
        next.screen_bottom_radius = value;
        break;
    }
    if(!crazypod_appearance_valid(&next))
        return false;
    appearance = next;
    crazypod_appearance_save();
    return true;
}

void crazypod_appearance_set_icon_theme(int theme)
{
    if(theme < 0 || theme >= CRAZYPOD_ICON_THEME_COUNT)
        return;
    appearance.icon_theme = theme;
    crazypod_appearance_save();
}

bool crazypod_appearance_set_wallpaper(
    enum crazypod_appearance_field field, const char *path)
{
    struct crazypod_appearance next = appearance;
    char *target;

    if(!valid_wallpaper_path(path))
        return false;
    if(field == CRAZYPOD_APPEARANCE_HOME_BACKGROUND)
        target = next.home_wallpaper;
    else if(field == CRAZYPOD_APPEARANCE_MENU_BACKGROUND)
        target = next.menu_wallpaper;
    else if(field == CRAZYPOD_APPEARANCE_LOCK_BACKGROUND)
        target = next.lock_wallpaper;
    else
        return false;
    snprintf(target, CRAZYPOD_WALLPAPER_PATH_SIZE, "%s", path);
    if(!crazypod_appearance_valid(&next))
        return false;
    appearance = next;
    crazypod_appearance_save();
    return true;
}

const char *crazypod_icon_theme_name(int theme)
{
    return theme >= 0 && theme < CRAZYPOD_ICON_THEME_COUNT
        ? theme_names[theme] : "";
}

const char *crazypod_appearance_color_name(int color)
{
    return color >= 0 && color < CRAZYPOD_APPEARANCE_COLOR_COUNT
        ? color_names[color] : CP_TR("Default");
}

uint32_t crazypod_appearance_color(int color)
{
    return color >= 0 && color < CRAZYPOD_APPEARANCE_COLOR_COUNT
        ? colors[color] : 0x141419;
}

uint32_t crazypod_appearance_home_color(void)
{
    return appearance.home_background == 0
        ? 0x141419
        : crazypod_appearance_color(appearance.home_background - 1);
}

uint32_t crazypod_appearance_menu_color(void)
{
    return appearance.menu_background == 0
        ? 0x08080D
        : crazypod_appearance_color(appearance.menu_background - 1);
}

uint32_t crazypod_appearance_lock_color(void)
{
    return appearance.lock_background == 0
        ? 0x07090D
        : crazypod_appearance_color(appearance.lock_background - 1);
}

#endif
