#include "crazypod_gameboy_core.h"
#include "crazypod_gameboy_core_compat.h"
#include "../../plugins/rockboy/cpu-gb.h"
#include "../../plugins/rockboy/hw.h"
#include "../../plugins/rockboy/mem.h"
#include "../../plugins/rockboy/lcd-gb.h"
#include "../../plugins/rockboy/regs.h"
#include "../../plugins/rockboy/pcm.h"
#include "../../plugins/rockboy/sound.h"
#include "../../plugins/rockboy/rtc-gb.h"
#include "../../plugins/rockboy/fb.h"

struct options options;
struct pcm pcm;
struct fb fb;
static uint16_t pixels[160 * 144];
static int16_t samples[2048];
static void (*submit_audio)(const int16_t *, size_t);
static bool running;
static bool draw_frame;
static unsigned rtc_frame_phase;

void die(char *message, ...)
{
    (void)message;
    running = false;
    rtc_frame_phase = 0;
}

int rockboy_pcm_submit(void)
{
    if(submit_audio != NULL && pcm.pos > 0)
        submit_audio(samples, (size_t)pcm.pos);
    pcm.pos = 0;
    return 1;
}

void crazypod_gameboy_scanline(
    int line, const uint8_t *indices, const fb_data *palette)
{
    int x;

    if(!draw_frame || line < 0 || line >= 144)
        return;
    for(x = 0; x < 160; ++x)
        pixels[line * 160 + x] = palette[indices[x] & 63];
}

bool crazypod_gameboy_core_open(
    uint8_t *data, size_t size, uint8_t *save_ram,
    void (*audio)(const int16_t *, size_t))
{
    struct crazypod_gameboy_cartridge cart;

    running = false;
    if(save_ram == NULL ||
       !crazypod_gameboy_cartridge_probe(data, size, size, &cart))
        return false;
    memset(&cpu, 0, sizeof(cpu));
    memset(&hw, 0, sizeof(hw));
    memset(&ram, 0, sizeof(ram));
    memset(&mbc, 0, sizeof(mbc));
    memset(&rom, 0, sizeof(rom));
    memset(&rtc, 0, sizeof(rtc));
    memset(&snd, 0, sizeof(snd));
    memset(&scan, 0, sizeof(scan));
    memset(pixels, 0, sizeof(pixels));
    memset(&pcm, 0, sizeof(pcm));
    rom.bank = (void *)data;
    ram.sbank = (void *)save_ram;
    ram.loaded = 1;
    mbc.type = cart.mapper;
    mbc.batt = cart.battery;
    mbc.romsize = cart.rom_size / 16384;
    /* A safe scratch bank also covers writes by RAM-less cartridges. */
    mbc.ramsize = cart.ram_size > 0 ?
        (cart.ram_size + 8191) / 8192 : 1;
    mbc.ram_bytes = (int)cart.ram_size;
    mbc.rombank = 1;
    hw.cgb = cart.color;
    rtc.batt = cart.clock;
    pcm.hz = 44100;
    pcm.stereo = 1;
    pcm.len = sizeof(samples) / sizeof(samples[0]);
    pcm.buf = samples;
    submit_audio = audio;
    options.pal = 0;
    options.sound = 1;
    fb.enabled = 1;
    hw_reset();
    lcd_reset();
    cpu_reset();
    mbc_reset();
    sound_reset();
    set_pal();
    /* ROM+RAM cartridges have no RAM-enable register. */
    if(cart.mapper == 0 && cart.ram_size > 0) {
        mbc.enableram = 1;
        mem_updatemap();
    }
    running = true;
    return true;
}

bool crazypod_gameboy_core_frame(uint8_t buttons, bool render)
{
    unsigned steps = 0;
    unsigned bit;

    if(!running)
        return false;
    for(bit = 1; bit <= 128; bit <<= 1)
        pad_set(bit, (buttons & bit) != 0);
    draw_frame = render;
    fb.enabled = render;
    lcd_begin();
    cpu_emulate(2280);
    while(running && R_LY > 0 && R_LY < 144 && ++steps < 2048)
        cpu_emulate(cpu.lcdc > 0 ? cpu.lcdc : 1);
    /* 70224 dots are one emulated frame.  Convert the nominal 59.7275 Hz
     * frame rate to the RTC's 60 Hz tick without accumulating drift. */
    rtc_frame_phase += 70224u * 60u;
    while(rtc_frame_phase >= 4194304u) {
        rtc_tick();
        rtc_frame_phase -= 4194304u;
    }
    sound_mix();
    if(!(R_LCDC & 0x80))
        cpu_emulate(32832);
    while(running && R_LY > 0 && ++steps < 2048)
        cpu_emulate(cpu.lcdc > 0 ? cpu.lcdc : 1);
    if(steps >= 2048)
        running = false;
    return running;
}

const uint16_t *crazypod_gameboy_core_pixels(void)
{
    return pixels;
}

void crazypod_gameboy_core_close(void)
{
    running = false;
    rtc_frame_phase = 0;
    submit_audio = NULL;
    pcm.buf = NULL;
    rom.bank = NULL;
    ram.sbank = NULL;
    memset(mbc.rmap, 0, sizeof(mbc.rmap));
    memset(mbc.wmap, 0, sizeof(mbc.wmap));
}

void crazypod_gameboy_core_clock_export(uint32_t clock[8])
{
    clock[0] = rtc.d; clock[1] = rtc.h; clock[2] = rtc.m;
    clock[3] = rtc.s; clock[4] = rtc.t; clock[5] = rtc.stop;
    clock[6] = rtc.carry; clock[7] = 0;
}

bool crazypod_gameboy_core_clock_import(const uint32_t clock[8])
{
    if(clock[0] > 511 || clock[1] > 23 || clock[2] > 59 ||
       clock[3] > 59 || clock[4] > 59 || clock[5] > 1 || clock[6] > 1)
        return false;
    rtc.d = clock[0]; rtc.h = clock[1]; rtc.m = clock[2];
    rtc.s = clock[3]; rtc.t = clock[4]; rtc.stop = clock[5];
    rtc.carry = clock[6];
    return true;
}

void crazypod_gameboy_core_clock_advance(uint32_t seconds)
{
    uint64_t total;

    if(rtc.stop)
        return;
    total = (uint64_t)rtc.d * 86400 + rtc.h * 3600 + rtc.m * 60 +
        rtc.s + seconds;
    if(total / 86400 >= 512)
        rtc.carry = 1;
    rtc.d = (total / 86400) % 512;
    rtc.h = (total / 3600) % 24;
    rtc.m = (total / 60) % 60;
    rtc.s = total % 60;
}
