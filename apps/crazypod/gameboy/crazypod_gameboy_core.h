#ifndef CRAZYPOD_GAMEBOY_CORE_H
#define CRAZYPOD_GAMEBOY_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRAZYPOD_GAMEBOY_ROM_MAX (8u * 1024u * 1024u)
#define CRAZYPOD_GAMEBOY_RAM_MAX (128u * 1024u)
#define CRAZYPOD_GAMEBOY_WIDTH 160
#define CRAZYPOD_GAMEBOY_HEIGHT 144

enum crazypod_gameboy_button {
    CRAZYPOD_GB_RIGHT = 1, CRAZYPOD_GB_LEFT = 2,
    CRAZYPOD_GB_UP = 4, CRAZYPOD_GB_DOWN = 8,
    CRAZYPOD_GB_A = 16, CRAZYPOD_GB_B = 32,
    CRAZYPOD_GB_SELECT = 64, CRAZYPOD_GB_START = 128
};

struct crazypod_gameboy_cartridge {
    size_t rom_size;
    size_t ram_size;
    unsigned mapper;
    bool battery;
    bool clock;
    bool color;
};

/* Buffers remain owned by the caller until close. SRAM is preloaded and
 * must have CRAZYPOD_GAMEBOY_RAM_MAX bytes, including for RAM-less carts. */
bool crazypod_gameboy_cartridge_probe(
    const uint8_t *header, size_t header_size, size_t file_size,
    struct crazypod_gameboy_cartridge *cartridge);
bool crazypod_gameboy_core_open(
    uint8_t *rom_data, size_t size, uint8_t *save_ram,
    void (*audio)(const int16_t *samples, size_t count));
bool crazypod_gameboy_core_frame(uint8_t buttons, bool render);
const uint16_t *crazypod_gameboy_core_pixels(void);
void crazypod_gameboy_core_close(void);
void crazypod_gameboy_core_clock_export(uint32_t clock[8]);
bool crazypod_gameboy_core_clock_import(const uint32_t clock[8]);
void crazypod_gameboy_core_clock_advance(uint32_t seconds);
bool crazypod_gameboy_path_supported(const char *path);

#endif
