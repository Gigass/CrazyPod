#ifndef CRAZYPOD_GAMEBOY_CORE_COMPAT_H
#define CRAZYPOD_GAMEBOY_CORE_COMPAT_H

/* A core build uses no Rockbox plugin API, menus, allocators or IRAM. */
#include "config.h"
#include "lcd.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#undef ICODE_ATTR
#undef IBSS_ATTR
#undef IDATA_ATTR
#undef ICONST_ATTR
#define ICODE_ATTR
#define IBSS_ATTR
#define IDATA_ATTR
#define ICONST_ATTR
#ifndef BIT_N
#define BIT_N(n) (1u << (n))
#endif

/* Keep the inherited emulator's global names out of the firmware ABI. */
#define cpu crazypod_gb_cpu
#define lcd crazypod_gb_lcd
#define hw crazypod_gb_hw
#define ram crazypod_gb_ram
#define rom crazypod_gb_rom
#define mbc crazypod_gb_mbc
#define rtc crazypod_gb_rtc
#define snd crazypod_gb_snd
#define pcm crazypod_gb_pcm
#define fb crazypod_gb_fb
#define scan crazypod_gb_scan
#define plugbuf crazypod_gb_plugbuf
#define options crazypod_gb_options
#define vdest crazypod_gb_vdest
#define patpix crazypod_gb_patpix
#define patdirty crazypod_gb_patdirty
#define anydirty crazypod_gb_anydirty
#define palettes crazypod_gb_palettes
#define soundFreqRatio crazypod_gb_sound_freq_ratio
#define soundShiftClock crazypod_gb_sound_shift_clock
#define cpu_reset crazypod_gb_cpu_reset
#define cpu_timers crazypod_gb_cpu_timers
#define cpu_emulate crazypod_gb_cpu_emulate
#define lcd_reset crazypod_gb_lcd_reset
#define lcd_begin crazypod_gb_lcd_begin
#define lcd_refreshline crazypod_gb_lcd_refreshline
#define lcdc_change crazypod_gb_lcdc_change
#define lcdc_trans crazypod_gb_lcdc_trans
#define stat_trigger crazypod_gb_stat_trigger
#define hw_reset crazypod_gb_hw_reset
#define hw_interrupt crazypod_gb_hw_interrupt
#define hw_dma crazypod_gb_hw_dma
#define hw_hdma crazypod_gb_hw_hdma
#define hw_hdma_cmd crazypod_gb_hw_hdma_cmd
#define pad_refresh crazypod_gb_pad_refresh
#define pad_set crazypod_gb_pad_set
#define mbc_reset crazypod_gb_mbc_reset
#define mem_updatemap crazypod_gb_mem_updatemap
#define mem_read crazypod_gb_mem_read
#define mem_write crazypod_gb_mem_write
#define readb crazypod_gb_readb
#define readw crazypod_gb_readw
#define writeb crazypod_gb_writeb
#define writew crazypod_gb_writew
#define readhi crazypod_gb_readhi
#define writehi crazypod_gb_writehi
#define pal_write crazypod_gb_pal_write
#define pal_write_dmg crazypod_gb_pal_write_dmg
#define pal_dirty crazypod_gb_pal_dirty
#define vram_write crazypod_gb_vram_write
#define vram_dirty crazypod_gb_vram_dirty
#define set_pal crazypod_gb_set_pal
#define sound_read crazypod_gb_sound_read
#define sound_write crazypod_gb_sound_write
#define sound_dirty crazypod_gb_sound_dirty
#define sound_reset crazypod_gb_sound_reset
#define sound_mix crazypod_gb_sound_mix
#define rtc_latch crazypod_gb_rtc_latch
#define rtc_write crazypod_gb_rtc_write
#define rtc_tick crazypod_gb_rtc_tick
#define rockboy_pcm_submit crazypod_gb_pcm_submit
#define die crazypod_gb_die

struct options { int pal; int sound; };
extern struct options options;
void set_pal(void);
void die(char *message, ...);
void crazypod_gameboy_scanline(
    int line, const uint8_t *pixels, const fb_data *palette);

#endif
