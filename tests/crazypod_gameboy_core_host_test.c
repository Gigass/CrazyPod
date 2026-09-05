#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gameboy/crazypod_gameboy_core.h"

static uint8_t rom_data[32768];
static uint8_t save_ram[CRAZYPOD_GAMEBOY_RAM_MAX];
static unsigned audio_samples;

static void audio(const int16_t *samples, size_t count)
{
    assert(samples != NULL && count > 0 && count <= 2048);
    audio_samples += count;
}

static void run_cartridge(bool color)
{
    /* Authored test ROM: boot, write SRAM and poll the joypad. */
    static const uint8_t program[] = {
        0x31, 0xfe, 0xff,       /* LD SP,$fffe */
        0xea, 0x02, 0xc0,       /* LD ($c002),A (boot model) */
        0x3e, 0x0a, 0xea, 0x00, 0x00, /* enable MBC1 RAM */
        0xfa, 0x02, 0xc0, 0xea, 0x02, 0xa0,
        0x3e, 0x5a, 0xea, 0x00, 0xa0,
        0x3e, 0xe4, 0xe0, 0x47, /* DMG palette */
        0x3e, 0xff, 0xea, 0x00, 0x80, /* tile pattern */
        0x3e, 0x80, 0xe0, 0x68, /* CGB palette: white and red */
        0x3e, 0xff, 0xe0, 0x69, 0x3e, 0x7f, 0xe0, 0x69,
        0x3e, 0x1f, 0xe0, 0x69, 0x3e, 0x00, 0xe0, 0x69,
        0x3e, 0x00, 0xe0, 0x00, /* select both joypad rows */
        0xf0, 0x00, 0xea, 0x01, 0xa0, 0x18, 0xf9
    };
    struct crazypod_gameboy_cartridge cart;
    uint32_t clock[8] = { 511, 23, 59, 59, 0, 0, 0, 0 };
    unsigned frame;

    memset(rom_data, 0, sizeof(rom_data));
    memset(save_ram, 0, sizeof(save_ram));
    rom_data[0x100] = 0xc3;
    rom_data[0x101] = 0x50;
    rom_data[0x102] = 0x01;
    rom_data[0x143] = color ? 0x80 : 0;
    rom_data[0x147] = 3;
    rom_data[0x149] = 2;
    memcpy(rom_data + 0x150, program, sizeof(program));
    assert(crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), sizeof(rom_data), &cart));
    assert(cart.color == color && cart.battery && cart.ram_size == 8192);
    assert(crazypod_gameboy_core_open(
        rom_data, sizeof(rom_data), save_ram, audio));
    for(frame = 0; frame < 8; ++frame)
        assert(crazypod_gameboy_core_frame(0, true));
    assert(save_ram[0] == 0x5a);
    assert(save_ram[2] == (color ? 0x11 : 0x01));
    assert(crazypod_gameboy_core_pixels()[0] !=
           crazypod_gameboy_core_pixels()[160]);
    assert((save_ram[1] & 15) == 15);
    assert(crazypod_gameboy_core_frame(CRAZYPOD_GB_RIGHT, true));
    assert((save_ram[1] & 1) == 0);
    assert(crazypod_gameboy_core_frame(0, false));
    assert((save_ram[1] & 15) == 15);
    assert(crazypod_gameboy_core_clock_import(clock));
    crazypod_gameboy_core_clock_advance(2);
    crazypod_gameboy_core_clock_export(clock);
    assert(clock[0] == 0 && clock[3] == 1 && clock[6] == 1);
    clock[1] = 24;
    assert(!crazypod_gameboy_core_clock_import(clock));
    crazypod_gameboy_core_close();
    assert(!crazypod_gameboy_core_frame(0, true));
}

static void run_mbc2(void)
{
    /* MBC2 RAM stores only the low nibble and mirrors every 0x200 bytes. */
    static const uint8_t program[] = {
        0x31, 0xfe, 0xff,
        0x3e, 0x0a, 0xea, 0x00, 0x00, /* enable MBC2 RAM */
        0x3e, 0xab, 0xea, 0x00, 0xa0, /* write low nibble */
        0xfa, 0x00, 0xa0, 0xea, 0x00, 0xc0, /* read back */
        0x18, 0xfe
    };
    struct crazypod_gameboy_cartridge cart;
    unsigned frame;

    memset(rom_data, 0, sizeof(rom_data));
    memset(save_ram, 0xff, sizeof(save_ram));
    rom_data[0x100] = 0xc3;
    rom_data[0x101] = 0x50;
    rom_data[0x102] = 0x01;
    rom_data[0x147] = 0x06;
    rom_data[0x149] = 0;
    memcpy(rom_data + 0x150, program, sizeof(program));
    assert(crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), sizeof(rom_data), &cart));
    assert(cart.mapper == 2 && cart.battery && cart.ram_size == 512);
    assert(crazypod_gameboy_core_open(
        rom_data, sizeof(rom_data), save_ram, audio));
    for(frame = 0; frame < 8; ++frame)
        assert(crazypod_gameboy_core_frame(0, false));
    assert(save_ram[0] == 0x0b);
    assert(save_ram[0x200] == 0xff);
    crazypod_gameboy_core_close();
}

int main(void)
{
    struct crazypod_gameboy_cartridge cart;

    assert(crazypod_gameboy_path_supported("/MiniApps/Games/Test.GBC"));
    assert(crazypod_gameboy_path_supported("/MiniApps/Games/Test.gb"));
    assert(!crazypod_gameboy_path_supported("/MiniApps/Games/Test.gba"));
    assert(!crazypod_gameboy_path_supported(".g"));
    assert(!crazypod_gameboy_path_supported(NULL));
    run_cartridge(false);
    run_cartridge(true);
    run_mbc2();
    assert(audio_samples > 0);
    /* An illegal opcode stops before subsequent SRAM writes execute. */
    rom_data[0x150] = 0xd3;
    memset(save_ram, 0, sizeof(save_ram));
    assert(crazypod_gameboy_core_open(
        rom_data, sizeof(rom_data), save_ram, audio));
    assert(!crazypod_gameboy_core_frame(0, true));
    assert(save_ram[0] == 0);
    crazypod_gameboy_core_close();
    assert(!crazypod_gameboy_cartridge_probe(rom_data, 20, 20, &cart));
    assert(!crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), 1000, &cart));
    rom_data[0x148] = 9;
    assert(!crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), sizeof(rom_data), &cart));
    rom_data[0x148] = 0;
    rom_data[0x147] = 0xfc;
    assert(!crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), sizeof(rom_data), &cart));
    rom_data[0x147] = 1;
    rom_data[0x149] = 1;
    assert(crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), sizeof(rom_data), &cart));
    assert(cart.ram_size == 2048);
    rom_data[0x148] = 0x52;
    assert(crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), 72u * 16u * 1024u, &cart));
    assert(cart.rom_size == 72u * 16u * 1024u);
    puts("Game Boy core: GB/GBC boot, SRAM, input, RTC and validation pass");
    return 0;
}
