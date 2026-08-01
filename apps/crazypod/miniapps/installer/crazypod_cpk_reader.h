#ifndef CRAZYPOD_CPK_READER_H
#define CRAZYPOD_CPK_READER_H

#include <stdint.h>

#define MINIAPP_CPK_ENTRIES 5
#define MINIAPP_CPK_MAX_ENTRIES MINIAPP_CPK_ENTRIES

enum cpk_entry_id {
    CPK_MANIFEST = 0,
    CPK_APP,
    CPK_PROFILE,
    CPK_ASSETS,
    CPK_ICON,
};

struct cpk_entry {
    char name[24];
    uint32_t crc32;
    uint32_t size;
    uint32_t local_offset;
    uint32_t data_offset;
    uint32_t span_end;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t method;
    uint16_t dos_time;
    uint16_t dos_date;
};

struct cpk_reader {
    int fd;
    uint32_t file_size;
    uint32_t central_offset;
    uint8_t entry_count;
    struct cpk_entry entries[MINIAPP_CPK_MAX_ENTRIES];
};

int crazypod_cpk_open(
    const char *path, struct cpk_reader *reader);
int crazypod_cpk_validate_local_headers(
    struct cpk_reader *reader);
void crazypod_cpk_close(struct cpk_reader *reader);

#endif
