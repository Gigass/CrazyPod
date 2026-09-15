#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "timefuncs.h"
#include "crazypod_gameboy.h"
#include "../miniapps/installer/crazypod_sha256.h"

#define GAME_LIMIT 128
#define SAVE_DIRECTORY "/.crazypod/gameboy"
#define SAVE_HEADER_SIZE 80

static char games[GAME_LIMIT][MAX_PATH];
static int game_count;
static int memory_handle;
static uint8_t *save_ram;
static struct crazypod_gameboy_cartridge cartridge;
static char save_path[MAX_PATH];
static bool opened;
static enum crazypod_gameboy_save_state save_state;
static char save_detail[64];

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
        (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write_u32(uint8_t *p, uint32_t value)
{
    p[0] = value; p[1] = value >> 8;
    p[2] = value >> 16; p[3] = value >> 24;
}

static int compare_games(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

void crazypod_gameboy_scan(void)
{
    static const char *const directories[] = {
        "/MiniApps/Games", "/MiniApps/Games/GB", "/MiniApps/Games/GBC"
    };
    unsigned i;

    game_count = 0;
    for(i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i) {
        DIR *directory = opendir(directories[i]);
        struct dirent *entry;

        if(directory == NULL)
            continue;
        while(game_count < GAME_LIMIT &&
              (entry = readdir(directory)) != NULL) {
            struct dirinfo info = dir_get_info(directory, entry);
            int length;

            if((info.attribute & ATTR_DIRECTORY) ||
               !crazypod_gameboy_path_supported(entry->d_name))
                continue;
            length = snprintf(games[game_count], MAX_PATH, "%s/%s",
                              directories[i], entry->d_name);
            if(length > 0 && length < MAX_PATH)
                ++game_count;
        }
        closedir(directory);
    }
    qsort(games, game_count, sizeof(games[0]), compare_games);
}

int crazypod_gameboy_count(void)
{
    return game_count;
}

const char *crazypod_gameboy_title(int index)
{
    const char *name;

    if(index < 0 || index >= game_count)
        return "";
    name = strrchr(games[index], '/');
    return name != NULL ? name + 1 : games[index];
}

static void save_digest(const uint8_t header[SAVE_HEADER_SIZE],
                        uint8_t digest[32])
{
    struct crazypod_sha256 hash;

    crazypod_sha256_init(&hash);
    crazypod_sha256_update(&hash, header, 48);
    crazypod_sha256_update(&hash, save_ram, cartridge.ram_size);
    crazypod_sha256_final(&hash, digest);
}

/*
 * Whether this cartridge has anything to persist.
 *
 * The header's battery byte is the intent, but plenty of dumps carry a
 * mapper and RAM with a type byte this table does not list, and then both
 * the save and the load silently did nothing: the game ran, "Save and
 * exit" reported success, and the next launch started over with no error
 * anywhere. Save whenever there is cartridge RAM to save.
 */
static bool cartridge_saves(void)
{
    return cartridge.battery || cartridge.clock ||
           cartridge.ram_size > 0;
}

/*
 * Load this game's save, and say exactly what happened.
 *
 * A save that cannot be used never stops the game from running: the file
 * is left on disk untouched, the cartridge RAM is returned to its blank
 * state, and the menu reports which check gave up. Refusing to launch
 * turned every unreadable save into "Game file or save could not be
 * written/read", which says nothing about which of six steps failed.
 */
static void load_save(void)
{
    uint8_t header[SAVE_HEADER_SIZE], digest[32];
    uint32_t clock[8], saved_at, now;
    const char *step = NULL;
    off_t size = 0;
    int fd, i;

    save_state = CRAZYPOD_GAMEBOY_SAVE_UNSUPPORTED;
    snprintf(save_detail, sizeof(save_detail),
             "type %02x ram %lu: cartridge cannot save",
             cartridge.type, (unsigned long)cartridge.ram_size);
    if(!cartridge_saves())
        return;

    save_state = CRAZYPOD_GAMEBOY_SAVE_ABSENT;
    snprintf(save_detail, sizeof(save_detail),
             "type %02x ram %lu: no file yet",
             cartridge.type, (unsigned long)cartridge.ram_size);
    /*
     * Having no save yet is the normal first run, not a failure -- but do
     * not ask errno which it was. Rockbox's open() speculatively allocates
     * a descriptor before resolving the path and calls close() on it when
     * resolution fails; close() finds a stream it never opened and sets
     * errno to EBADF, burying the ENOENT underneath.
     */
    if(!file_exists(save_path))
        return;

    fd = open(save_path, O_RDONLY);
    if(fd < 0)
        step = "open failed";
    else {
        size = filesize(fd);
        if(size != (off_t)(sizeof(header) + cartridge.ram_size))
            step = "wrong size";
        else if(read(fd, header, sizeof(header)) !=
                (ssize_t)sizeof(header))
            step = "short header";
        else if(memcmp(header, "CPGBSV01", 8) != 0)
            step = "bad magic";
        else if(read_u32(header + 8) != cartridge.ram_size)
            step = "ram size moved";
        else if(read(fd, save_ram, cartridge.ram_size) !=
                (ssize_t)cartridge.ram_size)
            step = "short ram";
        close(fd);
    }
    if(step == NULL) {
        save_digest(header, digest);
        if(memcmp(header + 48, digest, sizeof(digest)) != 0)
            step = "checksum";
    }
    if(step == NULL) {
        for(i = 0; i < 8; ++i)
            clock[i] = read_u32(header + 16 + i * 4);
        if(!crazypod_gameboy_core_clock_import(clock))
            step = "clock";
    }
    if(step != NULL) {
        /* Never start from half a save read out of a broken file. */
        memset(save_ram, 0xff, CRAZYPOD_GAMEBOY_RAM_MAX);
        save_state = CRAZYPOD_GAMEBOY_SAVE_REJECTED;
        snprintf(save_detail, sizeof(save_detail),
                 "ram %lu file %ld: %s",
                 (unsigned long)cartridge.ram_size, (long)size, step);
        return;
    }

    save_state = CRAZYPOD_GAMEBOY_SAVE_LOADED;
    snprintf(save_detail, sizeof(save_detail),
             "ram %lu file %ld: loaded",
             (unsigned long)cartridge.ram_size, (long)size);
    saved_at = read_u32(header + 12);
    now = (uint32_t)mktime(get_time());
    if(cartridge.clock && saved_at > 0 && now > saved_at)
        crazypod_gameboy_core_clock_advance(now - saved_at);
}

enum crazypod_gameboy_result crazypod_gameboy_open(
    int index, void (*audio)(const int16_t *, size_t))
{
    uint8_t header[0x150], digest[32];
    struct crazypod_sha256 hash;
    uint8_t *data;
    off_t size;
    int fd, i;

    if(opened || index < 0 || index >= game_count)
        return CRAZYPOD_GAMEBOY_BAD_ROM;
    fd = open(games[index], O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_GAMEBOY_IO_ERROR;
    size = filesize(fd);
    if(size < 0 || read(fd, header, sizeof(header)) !=
       (ssize_t)sizeof(header) ||
       !crazypod_gameboy_cartridge_probe(
           header, sizeof(header), (size_t)size, &cartridge)) {
        close(fd);
        return CRAZYPOD_GAMEBOY_BAD_ROM;
    }
    memory_handle = core_alloc_ex(
        cartridge.rom_size + CRAZYPOD_GAMEBOY_RAM_MAX,
        &buflib_ops_locked);
    if(memory_handle <= 0) {
        memory_handle = 0;
        close(fd);
        return CRAZYPOD_GAMEBOY_NO_MEMORY;
    }
    data = core_get_data(memory_handle);
    save_ram = data + cartridge.rom_size;
    memset(save_ram, 0xff, CRAZYPOD_GAMEBOY_RAM_MAX);
    if(lseek(fd, 0, SEEK_SET) != 0 ||
       read(fd, data, cartridge.rom_size) != (ssize_t)cartridge.rom_size) {
        close(fd);
        crazypod_gameboy_close();
        return CRAZYPOD_GAMEBOY_IO_ERROR;
    }
    close(fd);
    crazypod_sha256_init(&hash);
    crazypod_sha256_update(&hash, data, cartridge.rom_size);
    crazypod_sha256_final(&hash, digest);
    strcpy(save_path, SAVE_DIRECTORY "/");
    for(i = 0; i < 32; ++i)
        snprintf(save_path + sizeof(SAVE_DIRECTORY) + i * 2, 3,
                 "%02x", digest[i]);
    strcat(save_path, ".sav");
    if(!crazypod_gameboy_core_open(data, cartridge.rom_size,
                                   save_ram, audio)) {
        crazypod_gameboy_close();
        return CRAZYPOD_GAMEBOY_BAD_ROM;
    }
    load_save();
    opened = true;
    return CRAZYPOD_GAMEBOY_OK;
}

/* Report where a save gave up, and leave save_state saying it failed. */
static bool save_gave_up(const char *step)
{
    save_state = CRAZYPOD_GAMEBOY_SAVE_FAILED;
    snprintf(save_detail, sizeof(save_detail),
             "ram %lu: write %s",
             (unsigned long)cartridge.ram_size, step);
    return false;
}

bool crazypod_gameboy_save(void)
{
    uint8_t header[SAVE_HEADER_SIZE] = { 0 };
    uint32_t clock[8];
    char temporary[MAX_PATH + 8];
    const char *step = NULL;
    int fd, i;

    if(!opened)
        return false;
    if(!cartridge_saves())
        return true;
    if((!dir_exists("/.crazypod") && mkdir("/.crazypod") < 0) ||
       (!dir_exists(SAVE_DIRECTORY) && mkdir(SAVE_DIRECTORY) < 0))
        return save_gave_up("mkdir failed");
    memcpy(header, "CPGBSV01", 8);
    write_u32(header + 8, cartridge.ram_size);
    write_u32(header + 12, (uint32_t)mktime(get_time()));
    crazypod_gameboy_core_clock_export(clock);
    for(i = 0; i < 8; ++i)
        write_u32(header + 16 + i * 4, clock[i]);
    save_digest(header, header + 48);
    snprintf(temporary, sizeof(temporary), "%s.tmp", save_path);
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return save_gave_up("open failed");
    if(write(fd, header, sizeof(header)) != (ssize_t)sizeof(header))
        step = "short header";
    else if(write(fd, save_ram, cartridge.ram_size) !=
            (ssize_t)cartridge.ram_size)
        step = "short ram";
    if(fsync(fd) < 0 && step == NULL)
        step = "fsync failed";
    if(close(fd) < 0 && step == NULL)
        step = "close failed";
    if(step == NULL) {
        /*
         * A save we could not read is still the player's only copy of
         * whatever is in it, so move it aside rather than let the rename
         * drop it.
         */
        if(save_state == CRAZYPOD_GAMEBOY_SAVE_REJECTED) {
            /* Room for the suffix: a truncated name here would be a
             * prefix of the real one and the rename would eat it. */
            char rescue[MAX_PATH + 8];

            snprintf(rescue, sizeof(rescue), "%s.bad", save_path);
            remove(rescue);
            rename(save_path, rescue);
        }
        if(rename(temporary, save_path) != 0)
            step = "rename failed";
    }
    if(step != NULL) {
        remove(temporary);
        return save_gave_up(step);
    }
    save_state = CRAZYPOD_GAMEBOY_SAVE_WRITTEN;
    snprintf(save_detail, sizeof(save_detail),
             "ram %lu: written",
             (unsigned long)cartridge.ram_size);
    return true;
}

enum crazypod_gameboy_save_state crazypod_gameboy_save_state(void)
{
    return save_state;
}

const char *crazypod_gameboy_save_detail(void)
{
    return save_detail;
}

bool crazypod_gameboy_saves_progress(void)
{
    return opened && cartridge_saves();
}

void crazypod_gameboy_close(void)
{
    crazypod_gameboy_core_close();
    if(memory_handle > 0)
        core_free(memory_handle);
    memory_handle = 0;
    save_ram = NULL;
    opened = false;
}
