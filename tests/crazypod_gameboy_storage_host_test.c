#include "config.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "timefuncs.h"
#include "gameboy/crazypod_gameboy.h"

/* Virtual disk with real ROM/core/save code and injectable storage faults. */
static struct test_file {
    char path[260];
    uint8_t data[CRAZYPOD_GAMEBOY_RAM_MAX + 80];
    size_t size, position;
} files[8];
static uint8_t *allocation;
static bool fail_allocate, fail_write, fail_sync, fail_rename;
static int file_count, handles;
static time_t now = 1700000000;
struct buflib_callbacks buflib_ops_locked;

static int find_file(const char *path)
{
    int i;
    for(i = 0; i < file_count; ++i)
        if(strcmp(files[i].path, path) == 0)
            return i;
    return -1;
}

int gb_test_open(const char *path, int flags, ...)
{
    int i = find_file(path);
    if(i < 0 && !(flags & O_CREAT)) {
        errno = ENOENT;
        return -1;
    }
    if(i < 0) {
        assert(file_count < 8);
        i = file_count++;
        strcpy(files[i].path, path);
    }
    files[i].position = 0;
    if(flags & O_TRUNC)
        files[i].size = 0;
    ++handles;
    return i;
}

ssize_t gb_test_read(int fd, void *data, size_t size)
{
    struct test_file *file = &files[fd];
    if(size > file->size - file->position)
        size = file->size - file->position;
    memcpy(data, file->data + file->position, size);
    file->position += size;
    return size;
}

ssize_t gb_test_write(int fd, const void *data, size_t size)
{
    struct test_file *file = &files[fd];
    if(fail_write)
        return -1;
    assert(file->position + size <= sizeof(file->data));
    memcpy(file->data + file->position, data, size);
    file->position += size;
    file->size = file->position;
    return size;
}

off_t gb_test_lseek(int fd, off_t offset, int origin)
{
    assert(origin == SEEK_SET && offset >= 0);
    files[fd].position = offset;
    return offset;
}

off_t filesize(int fd) { return files[fd].size; }
int gb_test_close(int fd) { (void)fd; --handles; return 0; }
int gb_test_fsync(int fd) { (void)fd; return fail_sync ? -1 : 0; }
int gb_test_remove(const char *path)
{
    int i = find_file(path);
    if(i >= 0)
        files[i].path[0] = '\0';
    return i >= 0 ? 0 : -1;
}

int gb_test_rename(const char *from, const char *to)
{
    int source = find_file(from), target = find_file(to);
    if(fail_rename)
        return -1;
    assert(source >= 0);
    if(target >= 0) {
        files[target] = files[source];
        files[source].path[0] = '\0';
        strcpy(files[target].path, to);
    }
    else
        strcpy(files[source].path, to);
    return 0;
}

DIR *opendir(const char *path)
{
    static DIR directory;
    directory.path = path;
    directory.next = 0;
    return &directory;
}

struct dirent *readdir(DIR *directory)
{
    static struct dirent entry;
    size_t prefix = strlen(directory->path);
    while(directory->next < file_count) {
        const char *path = files[directory->next++].path;
        if(strncmp(path, directory->path, prefix) == 0 &&
           path[prefix] == '/' && !strchr(path + prefix + 1, '/')) {
            strcpy(entry.d_name, path + prefix + 1);
            return &entry;
        }
    }
    return NULL;
}

struct dirinfo dir_get_info(DIR *directory, struct dirent *entry)
{
    struct dirinfo info = { 0 };
    (void)directory; (void)entry;
    return info;
}
int closedir(DIR *directory) { (void)directory; return 0; }
bool dir_exists(const char *path) { (void)path; return true; }
int mkdir(const char *path) { (void)path; return 0; }
time_t gb_test_time(time_t *value) { (void)value; return now; }
struct tm *get_time(void)
{
    static struct tm calendar;
    struct tm *value = gmtime(&now);

    if(value != NULL)
        calendar = *value;
    return &calendar;
}

int core_alloc_ex(size_t size, struct buflib_callbacks *ops)
{
    assert(ops == &buflib_ops_locked && allocation == NULL);
    if(fail_allocate)
        return -1;
    allocation = malloc(size);
    assert(allocation != NULL);
    return 1;
}
void *core_get_data(int handle) { assert(handle == 1); return allocation; }
int core_free(int handle)
{
    assert(handle == 1);
    free(allocation);
    allocation = NULL;
    return 0;
}

int main(void)
{
    int save_index, i;
    uint32_t clock[8] = { 4, 0, 0, 3, 0, 0, 0, 0 };
    uint8_t saved[80 + 8192];

    strcpy(files[0].path, "/MiniApps/Games/clock.gbc");
    files[0].size = 32768;
    files[0].data[0x143] = 0x80;
    files[0].data[0x147] = 0x10; /* MBC3 + battery + RTC */
    files[0].data[0x149] = 2;
    strcpy(files[1].path, "/MiniApps/Games/skip.gba");
    file_count = 2;
    crazypod_gameboy_scan();
    assert(crazypod_gameboy_count() == 1);
    fail_allocate = true;
    assert(crazypod_gameboy_open(0, NULL) == CRAZYPOD_GAMEBOY_NO_MEMORY);
    assert(handles == 0 && allocation == NULL);
    fail_allocate = false;
    assert(crazypod_gameboy_open(0, NULL) == CRAZYPOD_GAMEBOY_OK);
    allocation[32768] = 0x42;
    assert(crazypod_gameboy_core_clock_import(clock));
    assert(crazypod_gameboy_save());
    save_index = 2;
    assert(files[save_index].size == sizeof(saved));
    memcpy(saved, files[save_index].data, sizeof(saved));
    allocation[32768] = 0x99;
    for(i = 0; i < 3; ++i) {
        fail_write = i == 0; fail_sync = i == 1; fail_rename = i == 2;
        assert(!crazypod_gameboy_save());
        assert(memcmp(saved, files[save_index].data, sizeof(saved)) == 0);
        assert(handles == 0);
    }
    fail_write = fail_sync = fail_rename = false;
    crazypod_gameboy_close();
    assert(allocation == NULL);
    /* Renaming a ROM must keep its battery save and elapsed RTC time. */
    strcpy(files[0].path, "/MiniApps/Games/GBC/renamed.GBC");
    crazypod_gameboy_scan();
    now += 120;
    assert(crazypod_gameboy_open(0, NULL) == CRAZYPOD_GAMEBOY_OK);
    assert(allocation[32768] == 0x42);
    crazypod_gameboy_core_clock_export(clock);
    assert(clock[0] == 4 && clock[2] == 2 && clock[3] == 3);
    assert(crazypod_gameboy_save());
    crazypod_gameboy_close();
    /* Corrupt or truncated saves are refused without overwriting them. */
    files[save_index].data[80] ^= 1;
    assert(crazypod_gameboy_open(0, NULL) == CRAZYPOD_GAMEBOY_BAD_SAVE);
    assert(!crazypod_gameboy_save());
    assert(handles == 0 && allocation == NULL);
    files[save_index].size = 12;
    assert(crazypod_gameboy_open(0, NULL) == CRAZYPOD_GAMEBOY_BAD_SAVE);
    assert(handles == 0 && allocation == NULL);
    puts("Game Boy storage: save/RTC reload, rename, failure preservation pass");
    return 0;
}
