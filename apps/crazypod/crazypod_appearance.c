#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_appearance.h"

#define APPEARANCE_DIRECTORY "/.crazypod"
#define APPEARANCE_PATH APPEARANCE_DIRECTORY "/appearance.bin"
#define APPEARANCE_TEMP_PATH APPEARANCE_DIRECTORY "/appearance.tmp"
#define APPEARANCE_MAGIC 0x43504150u
#define APPEARANCE_VERSION 1u

struct appearance_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    struct crazypod_appearance value;
    uint32_t checksum;
};

static struct crazypod_appearance appearance;

static const char *const theme_names[CRAZYPOD_ICON_THEME_COUNT] = {
    "Basic", "Cel Frame", "Anime Pop", "Mecha Spec",
    "Toy", "Y2K", "Flat", "Skeuo",
    "Lucid Pop", "Noize Bloom", "Soft Skeuo", "Acrylic",
    "Ink", "Sticker", "Sticker 2", "Voxel"
};

static const char *const color_names[CRAZYPOD_APPEARANCE_COLOR_COUNT] = {
    "Charcoal", "Rose", "Violet", "Cyan",
    "Amber", "Emerald", "Blue", "White"
};

static const uint32_t colors[CRAZYPOD_APPEARANCE_COLOR_COUNT] = {
    0x262626, 0xFF2E54, 0x8A45AB, 0x00D1FF,
    0xFFA838, 0x299E66, 0x2B66E6, 0xFFFFFF
};

static uint32_t hash_bytes(const void *data, size_t size)
{
    const unsigned char *bytes = data;
    uint32_t hash = 2166136261u;
    size_t i;

    for(i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t disk_checksum(const struct appearance_disk *disk)
{
    struct appearance_disk copy = *disk;
    copy.checksum = 0;
    return hash_bytes(&copy, sizeof(copy));
}

bool crazypod_appearance_valid(const struct crazypod_appearance *value)
{
    if(value == NULL)
        return false;
    return value->icon_theme >= 0 &&
           value->icon_theme < CRAZYPOD_ICON_THEME_COUNT &&
           value->icon_scale >= 0 && value->icon_scale < 5 &&
           value->player_style >= 0 && value->player_style < 2 &&
           value->glow >= 0 && value->glow < 4 &&
           value->highlight_style >= 0 && value->highlight_style < 2 &&
           value->primary_color >= 0 &&
           value->primary_color < CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->secondary_color >= 0 &&
           value->secondary_color < CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->home_background >= 0 &&
           value->home_background <= CRAZYPOD_APPEARANCE_COLOR_COUNT &&
           value->menu_background >= 0 &&
           value->menu_background <= CRAZYPOD_APPEARANCE_COLOR_COUNT;
}

void crazypod_appearance_load(void)
{
    struct appearance_disk disk;
    int fd;

    memset(&appearance, 0, sizeof(appearance));
    appearance.icon_scale = 2;
    appearance.glow = 1;
    appearance.highlight_style = 1;
    appearance.primary_color = 1;
    appearance.secondary_color = 2;

    fd = open(APPEARANCE_PATH, O_RDONLY);
    if(fd < 0)
        return;
    if(read(fd, &disk, sizeof(disk)) == (ssize_t)sizeof(disk) &&
       disk.magic == APPEARANCE_MAGIC &&
       disk.version == APPEARANCE_VERSION &&
       disk.size == sizeof(disk) &&
       disk.checksum == disk_checksum(&disk) &&
       crazypod_appearance_valid(&disk.value))
        appearance = disk.value;
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

void crazypod_appearance_cycle(enum crazypod_appearance_field field)
{
    switch(field) {
    case CRAZYPOD_APPEARANCE_ICON_THEME:
        appearance.icon_theme =
            (appearance.icon_theme + 1) % CRAZYPOD_ICON_THEME_COUNT;
        break;
    case CRAZYPOD_APPEARANCE_ICON_SCALE:
        appearance.icon_scale = (appearance.icon_scale + 1) % 5;
        break;
    case CRAZYPOD_APPEARANCE_PLAYER_STYLE:
        appearance.player_style ^= 1;
        break;
    case CRAZYPOD_APPEARANCE_GLOW:
        appearance.glow = (appearance.glow + 1) % 4;
        break;
    case CRAZYPOD_APPEARANCE_HIGHLIGHT_STYLE:
        appearance.highlight_style ^= 1;
        break;
    case CRAZYPOD_APPEARANCE_PRIMARY:
        appearance.primary_color =
            (appearance.primary_color + 1) %
            CRAZYPOD_APPEARANCE_COLOR_COUNT;
        break;
    case CRAZYPOD_APPEARANCE_SECONDARY:
        appearance.secondary_color =
            (appearance.secondary_color + 1) %
            CRAZYPOD_APPEARANCE_COLOR_COUNT;
        break;
    case CRAZYPOD_APPEARANCE_HOME_BACKGROUND:
        appearance.home_background =
            (appearance.home_background + 1) %
            (CRAZYPOD_APPEARANCE_COLOR_COUNT + 1);
        break;
    case CRAZYPOD_APPEARANCE_MENU_BACKGROUND:
        appearance.menu_background =
            (appearance.menu_background + 1) %
            (CRAZYPOD_APPEARANCE_COLOR_COUNT + 1);
        break;
    }
    crazypod_appearance_save();
}

void crazypod_appearance_set_icon_theme(int theme)
{
    if(theme < 0 || theme >= CRAZYPOD_ICON_THEME_COUNT)
        return;
    appearance.icon_theme = theme;
    crazypod_appearance_save();
}

const char *crazypod_icon_theme_name(int theme)
{
    return theme >= 0 && theme < CRAZYPOD_ICON_THEME_COUNT
        ? theme_names[theme] : "";
}

const char *crazypod_appearance_color_name(int color)
{
    return color >= 0 && color < CRAZYPOD_APPEARANCE_COLOR_COUNT
        ? color_names[color] : "Default";
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

#endif
