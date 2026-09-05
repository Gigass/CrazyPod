#include "config.h"
#include <errno.h>
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

static bool read_exact(int fd, void *buffer, size_t size)
{
    uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
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

static enum crazypod_gameboy_result load_save(void)
{
    uint8_t header[SAVE_HEADER_SIZE], digest[32];
    uint32_t clock[8], saved_at, now;
    bool valid;
    int fd, i;

    if(!cartridge.battery && !cartridge.clock)
        return CRAZYPOD_GAMEBOY_OK;
    fd = open(save_path, O_RDONLY);
    if(fd < 0)
        return errno == ENOENT ? CRAZYPOD_GAMEBOY_OK :
            CRAZYPOD_GAMEBOY_IO_ERROR;
    valid = filesize(fd) ==
        (off_t)(sizeof(header) + cartridge.ram_size) &&
        read_exact(fd, header, sizeof(header)) &&
        memcmp(header, "CPGBSV01", 8) == 0 &&
        read_u32(header + 8) == cartridge.ram_size &&
        read_exact(fd, save_ram, cartridge.ram_size);
    close(fd);
    if(!valid)
        return CRAZYPOD_GAMEBOY_BAD_SAVE;
    save_digest(header, digest);
    if(memcmp(header + 48, digest, sizeof(digest)) != 0)
        return CRAZYPOD_GAMEBOY_BAD_SAVE;
    for(i = 0; i < 8; ++i)
        clock[i] = read_u32(header + 16 + i * 4);
    if(!crazypod_gameboy_core_clock_import(clock))
        return CRAZYPOD_GAMEBOY_BAD_SAVE;
    saved_at = read_u32(header + 12);
    now = (uint32_t)mktime(get_time());
    if(cartridge.clock && saved_at > 0 && now > saved_at)
        crazypod_gameboy_core_clock_advance(now - saved_at);
    return CRAZYPOD_GAMEBOY_OK;
}

enum crazypod_gameboy_result crazypod_gameboy_open(
    int index, void (*audio)(const int16_t *, size_t))
{
    uint8_t header[0x150], digest[32];
    struct crazypod_sha256 hash;
    uint8_t *data;
    off_t size;
    int fd, i;
    enum crazypod_gameboy_result result;

    if(opened || index < 0 || index >= game_count)
        return CRAZYPOD_GAMEBOY_BAD_ROM;
    fd = open(games[index], O_RDONLY);
    if(fd < 0)
        return CRAZYPOD_GAMEBOY_IO_ERROR;
    size = filesize(fd);
    if(size < 0 || !read_exact(fd, header, sizeof(header)) ||
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
    memcpy(data, header, sizeof(header));
    if(!read_exact(fd, data + sizeof(header),
                   cartridge.rom_size - sizeof(header))) {
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
    result = load_save();
    if(result != CRAZYPOD_GAMEBOY_OK) {
        /* Never silently reset, or later overwrite, a damaged save. */
        crazypod_gameboy_close();
        return result;
    }
    opened = true;
    return CRAZYPOD_GAMEBOY_OK;
}

bool crazypod_gameboy_save(void)
{
    uint8_t header[SAVE_HEADER_SIZE] = { 0 };
    uint32_t clock[8];
    char temporary[MAX_PATH];
    bool success;
    int fd, i;

    if(!opened)
        return false;
    if(!cartridge.battery && !cartridge.clock)
        return true;
    if((!dir_exists("/.crazypod") && mkdir("/.crazypod") < 0) ||
       (!dir_exists(SAVE_DIRECTORY) && mkdir(SAVE_DIRECTORY) < 0))
        return false;
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
        return false;
    success = write_exact(fd, header, sizeof(header)) &&
        write_exact(fd, save_ram, cartridge.ram_size);
    if(fsync(fd) < 0)
        success = false;
    if(close(fd) < 0)
        success = false;
    if(success && rename(temporary, save_path) == 0)
        return true;
    remove(temporary);
    return false;
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
