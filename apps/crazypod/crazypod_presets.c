#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_presets.h"
#include "crazypod_checksum.h"
#include "crazypod_soundwave.h"

#define PRESET_DIRECTORY "/.crazypod"
#define PRESET_PATH PRESET_DIRECTORY "/presets.bin"
#define PRESET_TEMP_PATH PRESET_DIRECTORY "/presets.tmp"
#define PRESET_MAGIC 0x43505052u
#define PRESET_VERSION 3u
#define THEME_IMPORT_PATH PRESET_DIRECTORY "/import.upodtheme"
#define THEME_EXPORT_DIRECTORY PRESET_DIRECTORY "/export"
#define THEME_MAGIC 0x43505448u
#define THEME_VERSION 3u

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

struct preset_disk_entry {
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance appearance;
};

struct preset_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t user_count;
    struct preset_disk_entry users[
        CRAZYPOD_PRESET_COUNT_MAX - CRAZYPOD_BUILTIN_PRESET_COUNT];
    uint32_t checksum;
};

struct preset_disk_entry_v1 {
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance_v1 appearance;
};

struct preset_disk_v1 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t user_count;
    struct preset_disk_entry_v1 users[
        CRAZYPOD_PRESET_COUNT_MAX - CRAZYPOD_BUILTIN_PRESET_COUNT];
    uint32_t checksum;
};

struct preset_disk_entry_v2 {
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance_v2 appearance;
};

struct preset_disk_v2 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t user_count;
    struct preset_disk_entry_v2 users[
        CRAZYPOD_PRESET_COUNT_MAX - CRAZYPOD_BUILTIN_PRESET_COUNT];
    uint32_t checksum;
};

struct portable_theme {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance appearance;
    uint32_t checksum;
};

struct portable_theme_v1 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance_v1 appearance;
    uint32_t checksum;
};

struct portable_theme_v2 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance_v2 appearance;
    uint32_t checksum;
};

static struct crazypod_preset presets[CRAZYPOD_PRESET_COUNT_MAX];
static int preset_count;

static uint32_t disk_checksum(const struct preset_disk *disk)
{
    return crazypod_checksum_with_zeroed_u32(
        disk, sizeof(*disk), offsetof(struct preset_disk, checksum));
}

static uint32_t theme_checksum(const struct portable_theme *theme)
{
    return crazypod_checksum_with_zeroed_u32(
        theme, sizeof(*theme),
        offsetof(struct portable_theme, checksum));
}

static uint32_t disk_v1_checksum(const struct preset_disk_v1 *disk)
{
    return crazypod_checksum_with_zeroed_u32(
        disk, sizeof(*disk),
        offsetof(struct preset_disk_v1, checksum));
}

static uint32_t theme_v1_checksum(const struct portable_theme_v1 *theme)
{
    return crazypod_checksum_with_zeroed_u32(
        theme, sizeof(*theme),
        offsetof(struct portable_theme_v1, checksum));
}

static uint32_t disk_v2_checksum(const struct preset_disk_v2 *disk)
{
    return crazypod_checksum_with_zeroed_u32(
        disk, sizeof(*disk),
        offsetof(struct preset_disk_v2, checksum));
}

static uint32_t theme_v2_checksum(const struct portable_theme_v2 *theme)
{
    return crazypod_checksum_with_zeroed_u32(
        theme, sizeof(*theme),
        offsetof(struct portable_theme_v2, checksum));
}

static struct crazypod_appearance migrate_appearance_v1(
    const struct crazypod_appearance_v1 *source)
{
    struct crazypod_appearance result;

    memset(&result, 0, sizeof(result));
    result.icon_theme = source->icon_theme;
    result.icon_scale = source->icon_scale;
    result.sound_wave_style =
        source->player_style >= 0 &&
        source->player_style < CRAZYPOD_SOUND_WAVE_STYLE_COUNT
            ? source->player_style
            : CRAZYPOD_SOUND_WAVE_TORRENT;
    result.glow = source->glow;
    result.highlight_style = source->highlight_style;
    result.primary_color = source->primary_color;
    result.secondary_color = source->secondary_color;
    result.home_background = source->home_background;
    result.menu_background = source->menu_background;
    return result;
}

static struct crazypod_appearance migrate_appearance_v2(
    const struct crazypod_appearance_v2 *source)
{
    struct crazypod_appearance result;

    memset(&result, 0, sizeof(result));
    result.icon_theme = source->icon_theme;
    result.icon_scale = source->icon_scale;
    result.sound_wave_style = source->sound_wave_style;
    result.glow = source->glow;
    result.highlight_style = source->highlight_style;
    result.primary_color = source->primary_color;
    result.secondary_color = source->secondary_color;
    result.home_background = source->home_background;
    result.menu_background = source->menu_background;
    result.screen_top_radius = source->screen_top_radius;
    result.screen_bottom_radius = source->screen_bottom_radius;
    snprintf(result.home_wallpaper, sizeof(result.home_wallpaper),
             "%s", source->home_wallpaper);
    snprintf(result.menu_wallpaper, sizeof(result.menu_wallpaper),
             "%s", source->menu_wallpaper);
    return result;
}

static bool valid_name(const char *name)
{
    size_t length = 0;

    if(name == NULL)
        return false;
    while(length < CRAZYPOD_PRESET_NAME_SIZE &&
          name[length] != '\0')
        ++length;
    return length > 0 && length < CRAZYPOD_PRESET_NAME_SIZE;
}

static void seed_builtins(void)
{
    static const struct crazypod_appearance classic = {
        .icon_theme = 0,
        .icon_scale = 4,
        .sound_wave_style = CRAZYPOD_SOUND_WAVE_TORRENT,
        .glow = 1,
        .highlight_style = 1,
        .primary_color = 1,
        .secondary_color = 2,
        .home_background = 0,
        .menu_background = 0,
    };
    static const struct crazypod_appearance black_red = {
        .icon_theme = 12,
        .icon_scale = 4,
        .sound_wave_style = CRAZYPOD_SOUND_WAVE_RADIAL_SPECTRUM,
        .glow = 2,
        .highlight_style = 1,
        .primary_color = 1,
        .secondary_color = 0,
        .home_background = 1,
        .menu_background = 1,
    };

    memset(presets, 0, sizeof(presets));
    snprintf(presets[0].name, sizeof(presets[0].name), CP_FMT("Classic"));
    presets[0].appearance = classic;
    presets[0].builtin = true;
    snprintf(presets[1].name, sizeof(presets[1].name), CP_FMT("Black Red"));
    presets[1].appearance = black_red;
    presets[1].builtin = true;
    preset_count = CRAZYPOD_BUILTIN_PRESET_COUNT;
}

static void save_presets(void)
{
    static struct preset_disk disk;
    int user_count = preset_count - CRAZYPOD_BUILTIN_PRESET_COUNT;
    int fd;
    int i;

    if(user_count < 0)
        user_count = 0;
    memset(&disk, 0, sizeof(disk));
    disk.magic = PRESET_MAGIC;
    disk.version = PRESET_VERSION;
    disk.size = sizeof(disk);
    disk.user_count = (uint32_t)user_count;
    for(i = 0; i < user_count; ++i) {
        const struct crazypod_preset *source =
            &presets[CRAZYPOD_BUILTIN_PRESET_COUNT + i];
        snprintf(disk.users[i].name, sizeof(disk.users[i].name),
                 "%s", source->name);
        disk.users[i].appearance = source->appearance;
    }
    disk.checksum = disk_checksum(&disk);

    mkdir(PRESET_DIRECTORY);
    fd = open(PRESET_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return;
    if(write(fd, &disk, sizeof(disk)) == (ssize_t)sizeof(disk) &&
       fsync(fd) == 0) {
        close(fd);
        rename(PRESET_TEMP_PATH, PRESET_PATH);
        return;
    }
    close(fd);
}

void crazypod_presets_load(void)
{
    static struct preset_disk disk;
    static struct preset_disk_v2 disk_v2;
    static struct preset_disk_v1 disk_v1;
    uint32_t header[3];
    int user_count = 0;
    int source_version = PRESET_VERSION;
    int fd;
    int i;

    seed_builtins();
    fd = open(PRESET_PATH, O_RDONLY);
    if(fd < 0)
        return;
    if(read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
       lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return;
    }
    if(header[0] == PRESET_MAGIC &&
       header[1] == PRESET_VERSION &&
       header[2] == sizeof(disk) &&
       read(fd, &disk, sizeof(disk)) == (ssize_t)sizeof(disk) &&
       disk.user_count <=
           CRAZYPOD_PRESET_COUNT_MAX - CRAZYPOD_BUILTIN_PRESET_COUNT &&
       disk.checksum == disk_checksum(&disk))
        user_count = (int)disk.user_count;
    else if(header[0] == PRESET_MAGIC &&
            header[1] == 2u &&
            header[2] == sizeof(disk_v2) &&
            lseek(fd, 0, SEEK_SET) >= 0 &&
            read(fd, &disk_v2, sizeof(disk_v2)) ==
                (ssize_t)sizeof(disk_v2) &&
            disk_v2.user_count <=
                CRAZYPOD_PRESET_COUNT_MAX -
                    CRAZYPOD_BUILTIN_PRESET_COUNT &&
            disk_v2.checksum == disk_v2_checksum(&disk_v2)) {
        user_count = (int)disk_v2.user_count;
        source_version = 2;
    }
    else if(header[0] == PRESET_MAGIC &&
            header[1] == 1u &&
            header[2] == sizeof(disk_v1) &&
            lseek(fd, 0, SEEK_SET) >= 0 &&
            read(fd, &disk_v1, sizeof(disk_v1)) ==
                (ssize_t)sizeof(disk_v1) &&
            disk_v1.user_count <=
                CRAZYPOD_PRESET_COUNT_MAX -
                    CRAZYPOD_BUILTIN_PRESET_COUNT &&
            disk_v1.checksum == disk_v1_checksum(&disk_v1)) {
        user_count = (int)disk_v1.user_count;
        source_version = 1;
    }
    else {
        close(fd);
        return;
    }
    close(fd);

    for(i = 0; i < user_count; ++i) {
        struct crazypod_preset *target = &presets[preset_count];
        const char *name = source_version == 1
            ? disk_v1.users[i].name
            : source_version == 2
                ? disk_v2.users[i].name
                : disk.users[i].name;
        if(!valid_name(name))
            continue;
        if(source_version == 1)
            target->appearance =
                migrate_appearance_v1(&disk_v1.users[i].appearance);
        else if(source_version == 2)
            target->appearance =
                migrate_appearance_v2(&disk_v2.users[i].appearance);
        else
            target->appearance = disk.users[i].appearance;
        if(!crazypod_appearance_valid(&target->appearance))
            continue;
        snprintf(target->name, sizeof(target->name), "%s",
                 name);
        target->builtin = false;
        ++preset_count;
    }
    if(source_version != PRESET_VERSION)
        save_presets();
}

int crazypod_preset_count(void)
{
    return preset_count;
}

const struct crazypod_preset *crazypod_preset_get(int index)
{
    return index >= 0 && index < preset_count ? &presets[index] : NULL;
}

bool crazypod_preset_apply(int index)
{
    const struct crazypod_preset *preset = crazypod_preset_get(index);
    return preset != NULL &&
           crazypod_appearance_set(&preset->appearance);
}

int crazypod_preset_save_current(void)
{
    struct crazypod_preset *preset;
    int number;

    if(preset_count >= CRAZYPOD_PRESET_COUNT_MAX)
        return -1;
    preset = &presets[preset_count];
    memset(preset, 0, sizeof(*preset));
    number = preset_count - CRAZYPOD_BUILTIN_PRESET_COUNT + 1;
    snprintf(preset->name, sizeof(preset->name), CP_FMT("Appearance %d"), number);
    preset->appearance = *crazypod_appearance_get();
    preset->builtin = false;
    ++preset_count;
    save_presets();
    return preset_count - 1;
}

int crazypod_preset_duplicate(int index)
{
    const struct crazypod_preset *source = crazypod_preset_get(index);
    struct crazypod_preset *target;
    int number;

    if(source == NULL || preset_count >= CRAZYPOD_PRESET_COUNT_MAX)
        return -1;
    target = &presets[preset_count];
    memset(target, 0, sizeof(*target));
    number = preset_count - CRAZYPOD_BUILTIN_PRESET_COUNT + 1;
    snprintf(target->name, sizeof(target->name), CP_FMT("Appearance %d"), number);
    target->appearance = source->appearance;
    target->builtin = false;
    ++preset_count;
    save_presets();
    return preset_count - 1;
}

bool crazypod_preset_update(int index)
{
    struct crazypod_preset *preset;

    if(index < CRAZYPOD_BUILTIN_PRESET_COUNT || index >= preset_count)
        return false;
    preset = &presets[index];
    preset->appearance = *crazypod_appearance_get();
    save_presets();
    return true;
}

bool crazypod_preset_rename(int index, const char *name)
{
    if(index < CRAZYPOD_BUILTIN_PRESET_COUNT || index >= preset_count ||
       !valid_name(name))
        return false;
    snprintf(presets[index].name, sizeof(presets[index].name), "%s", name);
    save_presets();
    return true;
}

bool crazypod_preset_delete(int index)
{
    if(index < CRAZYPOD_BUILTIN_PRESET_COUNT || index >= preset_count)
        return false;
    if(index + 1 < preset_count) {
        memmove(&presets[index], &presets[index + 1],
                (size_t)(preset_count - index - 1) * sizeof(presets[0]));
    }
    --preset_count;
    memset(&presets[preset_count], 0, sizeof(presets[0]));
    save_presets();
    return true;
}

bool crazypod_preset_export(int index)
{
    const struct crazypod_preset *preset = crazypod_preset_get(index);
    struct portable_theme theme;
    char filename[CRAZYPOD_PRESET_NAME_SIZE];
    char path[MAX_PATH];
    int fd;
    int i;

    if(preset == NULL)
        return false;
    memset(&theme, 0, sizeof(theme));
    theme.magic = THEME_MAGIC;
    theme.version = THEME_VERSION;
    theme.size = sizeof(theme);
    snprintf(theme.name, sizeof(theme.name), "%s", preset->name);
    theme.appearance = preset->appearance;
    theme.checksum = theme_checksum(&theme);

    for(i = 0; i < CRAZYPOD_PRESET_NAME_SIZE - 1 &&
               preset->name[i] != '\0'; ++i) {
        char value = preset->name[i];
        filename[i] = (value >= 'A' && value <= 'Z') ||
                      (value >= 'a' && value <= 'z') ||
                      (value >= '0' && value <= '9') ||
                      value == '-' || value == '_'
            ? value : '_';
    }
    filename[i] = '\0';
    if(filename[0] == '\0')
        snprintf(filename, sizeof(filename), "appearance");

    mkdir(PRESET_DIRECTORY);
    mkdir(THEME_EXPORT_DIRECTORY);
    snprintf(path, sizeof(path), CP_FMT("%s/%s.upodtheme"),
             THEME_EXPORT_DIRECTORY, filename);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    if(write(fd, &theme, sizeof(theme)) != (ssize_t)sizeof(theme) ||
       fsync(fd) != 0) {
        close(fd);
        return false;
    }
    close(fd);
    return true;
}

int crazypod_preset_import(void)
{
    struct portable_theme theme;
    struct portable_theme_v2 theme_v2;
    struct portable_theme_v1 theme_v1;
    struct crazypod_preset *target;
    uint32_t header[3];
    int fd;

    if(preset_count >= CRAZYPOD_PRESET_COUNT_MAX)
        return -1;
    fd = open(THEME_IMPORT_PATH, O_RDONLY);
    if(fd < 0)
        return -1;
    if(read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
       lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }
    if(header[0] == THEME_MAGIC &&
       header[1] == THEME_VERSION &&
       header[2] == sizeof(theme) &&
       read(fd, &theme, sizeof(theme)) == (ssize_t)sizeof(theme) &&
       theme.checksum == theme_checksum(&theme)) {
    }
    else if(header[0] == THEME_MAGIC &&
            header[1] == 2u &&
            header[2] == sizeof(theme_v2) &&
            lseek(fd, 0, SEEK_SET) >= 0 &&
            read(fd, &theme_v2, sizeof(theme_v2)) ==
                (ssize_t)sizeof(theme_v2) &&
            theme_v2.checksum == theme_v2_checksum(&theme_v2)) {
        memset(&theme, 0, sizeof(theme));
        snprintf(theme.name, sizeof(theme.name), "%s", theme_v2.name);
        theme.appearance =
            migrate_appearance_v2(&theme_v2.appearance);
    }
    else if(header[0] == THEME_MAGIC &&
            header[1] == 1u &&
            header[2] == sizeof(theme_v1) &&
            lseek(fd, 0, SEEK_SET) >= 0 &&
            read(fd, &theme_v1, sizeof(theme_v1)) ==
                (ssize_t)sizeof(theme_v1) &&
            theme_v1.checksum == theme_v1_checksum(&theme_v1)) {
        memset(&theme, 0, sizeof(theme));
        snprintf(theme.name, sizeof(theme.name), "%s", theme_v1.name);
        theme.appearance =
            migrate_appearance_v1(&theme_v1.appearance);
    }
    else {
        close(fd);
        return -1;
    }
    close(fd);
    if(!valid_name(theme.name) ||
       !crazypod_appearance_valid(&theme.appearance))
        return -1;

    target = &presets[preset_count];
    memset(target, 0, sizeof(*target));
    snprintf(target->name, sizeof(target->name), "%s", theme.name);
    target->appearance = theme.appearance;
    target->builtin = false;
    ++preset_count;
    save_presets();
    return preset_count - 1;
}

#endif
