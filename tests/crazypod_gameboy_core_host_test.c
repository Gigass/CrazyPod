#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gameboy/crazypod_gameboy_core.h"
#include "../apps/plugins/rockboy/cpu-gb.h"
#include "../apps/plugins/rockboy/cpuregs.h"
#include "../apps/plugins/rockboy/mem.h"
#include "../apps/plugins/rockboy/regs.h"
#include "../apps/plugins/rockboy/rtc-gb.h"

static uint8_t rom_data[32768];
static uint8_t banked_rom[4u * 1024u * 1024u];
static uint8_t save_ram[CRAZYPOD_GAMEBOY_RAM_MAX];
static unsigned audio_samples;

static uint8_t bank_marker(unsigned bank)
{
    return (uint8_t)(0x5a ^ bank ^ (bank >> 8));
}

static void prepare_banked_rom(
    size_t size, uint8_t rom_code, uint8_t type, uint8_t ram_code)
{
    size_t bank;

    assert(size <= sizeof(banked_rom) && size % 16384u == 0);
    memset(banked_rom, 0, size);
    for(bank = 0; bank < size / 16384u; ++bank)
        banked_rom[bank * 16384u] = bank_marker((unsigned)bank);
    banked_rom[0x148] = rom_code;
    banked_rom[0x147] = type;
    banked_rom[0x149] = ram_code;
}

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
    rom_data[0x14d] = color ? 0 : 1;
    rom_data[0x149] = 2;
    memcpy(rom_data + 0x150, program, sizeof(program));
    assert(crazypod_gameboy_cartridge_probe(
        rom_data, sizeof(rom_data), sizeof(rom_data), &cart));
    assert(cart.color == color && cart.battery && cart.ram_size == 8192);
    assert(crazypod_gameboy_core_open(
        rom_data, sizeof(rom_data), save_ram, audio));
    assert(A == (color ? 0x11 : 0x01));
    assert(F == (color ? 0x80 : 0xb0));
    assert(mem_read(0xff00) == 0xcf);
    assert(mem_read(0xff07) == 0xf8);
    assert(mem_read(0xff0f) == 0xe1);
    if(color) {
        assert(B == 0 && C == 0 && D == 0xff && E == 0x56);
        assert(H == 0 && L == 0x0d);
        assert(mem_read(0xff4d) == 0x7e);
        assert(mem_read(0xff4f) == 0xfe);
        assert(mem_read(0xff51) == 0xff);
        assert(mem_read(0xff70) == 0xf8);
        mem_write(0xff68, 0x80);
        mem_write(0xff69, 0x12);
        assert(mem_read(0xff68) == 0x81);
        assert(mem_read(0xff69) == 0);
        mem_write(0xff6a, 0x80);
        mem_write(0xff6b, 0x34);
        assert(mem_read(0xff6a) == 0x81);
        assert(mem_read(0xff6b) == 0);
    }
    else {
        assert(B == 0 && C == 0x13 && D == 0 && E == 0xd8);
        assert(H == 0x01 && L == 0x4d);
    }
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
    clock[7] = 123456;
    assert(crazypod_gameboy_core_clock_import(clock));
    crazypod_gameboy_core_clock_export(clock);
    assert(clock[7] == 123456 && rtc.regs[0] == rtc.s);
    clock[7] = 4194304;
    assert(!crazypod_gameboy_core_clock_import(clock));
    clock[7] = 0;
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

static void run_mapper_banking(void)
{
    static const uint8_t nintendo_logo[48] = {
        0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d,
        0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
        0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99,
        0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
        0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e
    };
    struct crazypod_gameboy_cartridge cart;

    prepare_banked_rom(64u * 1024u, 1, 0x19, 0);
    assert(crazypod_gameboy_core_open(
        banked_rom, 64u * 1024u, save_ram, NULL));
    assert(mem_read(0x4000) == bank_marker(1));
    mem_write(0x2000, 0);
    assert(mbc.rombank == 0);
    assert(mem_read(0x4000) == bank_marker(0));
    crazypod_gameboy_core_close();

    prepare_banked_rom(64u * 1024u, 1, 0x05, 0);
    assert(crazypod_gameboy_core_open(
        banked_rom, 64u * 1024u, save_ram, NULL));
    mem_write(0x0100, 2);
    assert(mem_read(0x4000) == bank_marker(2));
    mem_write(0x4100, 3);
    assert(mem_read(0x4000) == bank_marker(2));
    crazypod_gameboy_core_close();

    prepare_banked_rom(1024u * 1024u, 5, 0x01, 0);
    assert(crazypod_gameboy_core_open(
        banked_rom, 1024u * 1024u, save_ram, NULL));
    mem_write(0x4000, 1);
    assert(mem_read(0x0000) == bank_marker(0));
    assert(mem_read(0x4000) == bank_marker(33));
    mem_write(0x6000, 1);
    assert(mem_read(0x0000) == bank_marker(32));
    assert(mem_read(0x4000) == bank_marker(33));
    mem_write(0x2000, 2);
    assert(mem_read(0x4000) == bank_marker(34));
    crazypod_gameboy_core_close();

    prepare_banked_rom(1024u * 1024u, 5, 0x01, 0);
    memcpy(banked_rom + 16u * 16384u + 0x104u,
           nintendo_logo, sizeof(nintendo_logo));
    assert(crazypod_gameboy_core_open(
        banked_rom, 1024u * 1024u, save_ram, NULL));
    assert(mbc.mbc1_multicart);
    mem_write(0x4000, 1);
    mem_write(0x6000, 1);
    assert(mem_read(0x0000) == bank_marker(16));
    assert(mem_read(0x4000) == bank_marker(17));
    crazypod_gameboy_core_close();

    prepare_banked_rom(4u * 1024u * 1024u, 7, 0x10, 5);
    assert(crazypod_gameboy_cartridge_probe(
        banked_rom, sizeof(banked_rom), sizeof(banked_rom), &cart));
    assert(cart.mbc30 && cart.ram_size == 64u * 1024u);
    memset(save_ram, 0xff, sizeof(save_ram));
    assert(crazypod_gameboy_core_open(
        banked_rom, sizeof(banked_rom), save_ram, NULL));
    mem_write(0x2000, 0x80);
    assert(mem_read(0x4000) == bank_marker(128));
    mem_write(0x0000, 0x0a);
    mem_write(0x4000, 7);
    mem_write(0xa000, 0x33);
    assert(save_ram[7u * 8192u] == 0x33);
    crazypod_gameboy_core_close();

    /* MBC30 is inferred from the extended layout, not only type 0x10. */
    banked_rom[0x147] = 0x13;
    assert(crazypod_gameboy_cartridge_probe(
        banked_rom, sizeof(banked_rom), sizeof(banked_rom), &cart));
    assert(cart.mbc30 && cart.battery && !cart.clock);
}

static void run_cpu_edge_cases(void)
{
    memset(rom_data, 0, sizeof(rom_data));
    rom_data[0x147] = 0;
    rom_data[0x148] = 0;
    rom_data[0x149] = 0;
    assert(crazypod_gameboy_core_open(
        rom_data, sizeof(rom_data), save_ram, NULL));
    assert(F == 0x80);

    rom_data[0x150] = 0x76; /* HALT */
    rom_data[0x151] = 0x0c; /* INC C */
    PC = 0x150;
    C = 0;
    IME = IMA = 0;
    IF = 0;
    IE = 1;
    assert(cpu_emulate(10) == 10);
    assert(cpu.halt && PC == 0x151 && C == 0);
    IF = 1;
    assert(cpu_emulate(2) == 2);
    assert(!cpu.halt && PC == 0x152 && C == 1);

    cpu_reset();
    rom_data[0x150] = 0x76; /* HALT with a pending interrupt */
    rom_data[0x151] = 0x04; /* INC B, fetched twice by the HALT bug */
    PC = 0x150;
    B = 0;
    IF = IE = 1;
    assert(cpu_emulate(6) == 6);
    assert(B == 2 && PC == 0x152);

    cpu.div = 255;
    cpu.tim = 511;
    R_DIV = 0x12;
    mem_write(0xff04, 0x99);
    assert(R_DIV == 0 && cpu.div == 0 && cpu.tim == 0);
    crazypod_gameboy_core_close();
}

static void run_layout_validation(void)
{
    uint8_t header[0x150] = { 0 };
    struct crazypod_gameboy_cartridge cart;

    header[0x147] = 0x00;
    header[0x148] = 1;
    assert(!crazypod_gameboy_cartridge_probe(
        header, sizeof(header), 64u * 1024u, &cart));
    header[0x147] = 0x05;
    header[0x148] = 4;
    assert(!crazypod_gameboy_cartridge_probe(
        header, sizeof(header), 512u * 1024u, &cart));
    header[0x147] = 0x01;
    header[0x148] = 7;
    assert(!crazypod_gameboy_cartridge_probe(
        header, sizeof(header), 4u * 1024u * 1024u, &cart));
    header[0x147] = 0x12;
    header[0x148] = 6;
    header[0x149] = 4;
    assert(!crazypod_gameboy_cartridge_probe(
        header, sizeof(header), 2u * 1024u * 1024u, &cart));
    header[0x147] = 0x1d;
    header[0x148] = 8;
    header[0x149] = 4;
    assert(!crazypod_gameboy_cartridge_probe(
        header, sizeof(header), 8u * 1024u * 1024u, &cart));
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
    run_mapper_banking();
    run_cpu_edge_cases();
    run_layout_validation();
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
    rom_data[0x147] = 2;
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
