#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_presets.h"

#define PRESET_DIRECTORY "/.crazypod"
#define PRESET_PATH PRESET_DIRECTORY "/presets.bin"
#define PRESET_TEMP_PATH PRESET_DIRECTORY "/presets.tmp"
#define PRESET_MAGIC 0x43505052u
#define PRESET_VERSION 1u
#define THEME_IMPORT_PATH PRESET_DIRECTORY "/import.upodtheme"
#define THEME_EXPORT_DIRECTORY PRESET_DIRECTORY "/export"
#define THEME_MAGIC 0x43505448u
#define THEME_VERSION 1u

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

struct portable_theme {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    char name[CRAZYPOD_PRESET_NAME_SIZE];
    struct crazypod_appearance appearance;
    uint32_t checksum;
};

static struct crazypod_preset presets[CRAZYPOD_PRESET_COUNT_MAX];
static int preset_count;

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

static uint32_t disk_checksum(const struct preset_disk *disk)
{
    struct preset_disk copy = *disk;
    copy.checksum = 0;
    return hash_bytes(&copy, sizeof(copy));
}

static uint32_t theme_checksum(const struct portable_theme *theme)
{
    struct portable_theme copy = *theme;
    copy.checksum = 0;
    return hash_bytes(&copy, sizeof(copy));
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
        .icon_scale = 2,
        .player_style = 0,
        .glow = 1,
        .highlight_style = 1,
        .primary_color = 1,
        .secondary_color = 2,
        .home_background = 0,
        .menu_background = 0,
    };
    static const struct crazypod_appearance black_red = {
        .icon_theme = 12,
        .icon_scale = 2,
        .player_style = 1,
        .glow = 2,
        .highlight_style = 1,
        .primary_color = 1,
        .secondary_color = 0,
        .home_background = 1,
        .menu_background = 1,
    };

    memset(presets, 0, sizeof(presets));
    snprintf(presets[0].name, sizeof(presets[0].name), "Classic");
    presets[0].appearance = classic;
    presets[0].builtin = true;
    snprintf(presets[1].name, sizeof(presets[1].name), "Black Red");
    presets[1].appearance = black_red;
    presets[1].builtin = true;
    preset_count = CRAZYPOD_BUILTIN_PRESET_COUNT;
}

static void save_presets(void)
{
    struct preset_disk disk;
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
    struct preset_disk disk;
    int fd;
    int i;

    seed_builtins();
    fd = open(PRESET_PATH, O_RDONLY);
    if(fd < 0)
        return;
    if(read(fd, &disk, sizeof(disk)) != (ssize_t)sizeof(disk) ||
       disk.magic != PRESET_MAGIC ||
       disk.version != PRESET_VERSION ||
       disk.size != sizeof(disk) ||
       disk.user_count >
           CRAZYPOD_PRESET_COUNT_MAX - CRAZYPOD_BUILTIN_PRESET_COUNT ||
       disk.checksum != disk_checksum(&disk)) {
        close(fd);
        return;
    }
    close(fd);

    for(i = 0; i < (int)disk.user_count; ++i) {
        struct crazypod_preset *target = &presets[preset_count];
        if(!valid_name(disk.users[i].name))
            continue;
        target->appearance = disk.users[i].appearance;
        if(!crazypod_appearance_valid(&target->appearance))
            continue;
        snprintf(target->name, sizeof(target->name), "%s",
                 disk.users[i].name);
        target->builtin = false;
        ++preset_count;
    }

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
    snprintf(preset->name, sizeof(preset->name), "Appearance %d", number);
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
    snprintf(target->name, sizeof(target->name), "Appearance %d", number);
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
    snprintf(path, sizeof(path), "%s/%s.upodtheme",
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
    struct crazypod_preset *target;
    int fd;

    if(preset_count >= CRAZYPOD_PRESET_COUNT_MAX)
        return -1;
    fd = open(THEME_IMPORT_PATH, O_RDONLY);
    if(fd < 0)
        return -1;
    if(read(fd, &theme, sizeof(theme)) != (ssize_t)sizeof(theme) ||
       theme.magic != THEME_MAGIC ||
       theme.version != THEME_VERSION ||
       theme.size != sizeof(theme) ||
       theme.checksum != theme_checksum(&theme) ||
       !valid_name(theme.name) ||
       !crazypod_appearance_valid(&theme.appearance)) {
        close(fd);
        return -1;
    }
    close(fd);

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
