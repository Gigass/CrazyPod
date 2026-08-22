/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id:
 *
 * Copyright © 2009 Michael Sparmann
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef __LCD_S5L8702_H__
#define __LCD_S5L8702_H__

#include <stdint.h>

#include "config.h"
#include "lcd-target.h"


/* sequence actions */
enum {
    CMD = 0,    /* send command with N data */
    SLEEP,      /* unit: RB tick */                 // TODO: the unit in ms. but using HZ
#ifdef S5L_LCD_WITH_CMDSET16
    MREG,       /* write multiple registers */
    DELAY,      /* unit: 64 us. */
#endif
    END = 0xff
};

#ifdef S5L_LCD_WITH_CMDSET16
/* pack 16-bit sequence actions */
#define CMD16           (CMD)   /* send command with no data */             // TODO: why not use MREG16(0) ???
#define MREG16(len)     (MREG | ((len) << 8))
#define SLEEP16(t)      (SLEEP | ((t) << 8))
#define DELAY16(t)      (DELAY | ((t) << 8))

/* Supported command sets */
enum {
    LCD_CMDSET_8BIT = 0,    /* 8-bit command set similar to TODO */
    LCD_CMDSET_16BIT,       /* 16-bit command set similar to TODO */
};
#endif

/* Supported MPU interfaces */
enum {
    LCD_MPUIFACE_PAR9 = 0,  /* 8080-series 9-bit parallel */
    LCD_MPUIFACE_PAR18,     /* 8080-series 18-bit parallel */
    LCD_MPUIFACE_SERIAL,    /* 3-pin SPI (TBC) */
};


// TODO: typedef struct {} lcd_info_t;
struct lcd_info_rec {
    uint8_t lcd_type;
    uint8_t mpuiface;   /* LCD_MPUIFACE_ */
#ifdef S5L_LCD_WITH_CMDSET16
    uint8_t cmdset;     /* LCD_CMDSET_ */
#endif
    /* sequences */
#if defined(HAVE_LCD_SLEEP) || defined(HAVE_LCD_SHUTDOWN)
    void *seq_sleep;
#endif
#ifdef HAVE_LCD_SLEEP
    void *seq_awake;
#endif
#ifdef BOOTLOADER
    void *seq_init;
#endif
#ifdef IPOD_6G
    /* Non-NULL only for panel types with physical TE validation. */
    void *seq_frame_sync;
#endif
};

#ifdef IPOD_6G
#define LCD_TE_PROBE_GPIO_GROUPS 16
#define LCD_TE_PROBE_CANDIDATES 8

enum lcd_frame_sync_method {
    LCD_FRAME_SYNC_PROBING = 0,
    LCD_FRAME_SYNC_MARKER,
    LCD_FRAME_SYNC_SCANLINE,
    LCD_FRAME_SYNC_GPIO_TE,
    LCD_FRAME_SYNC_UNAVAILABLE,
};

struct lcd_te_probe_candidate {
    uint32_t source;
    uint32_t bit;
    uint32_t transitions;
    uint32_t high_samples;
    uint32_t period_count;
    uint32_t average_period_us;
    uint32_t min_period_us;
    uint32_t max_period_us;
    uint32_t min_edge_interval_us;
};

struct lcd_frame_sync_diagnostics {
    int panel_type;
    enum lcd_frame_sync_method method;
    uint32_t waits;
    uint32_t marker_waits;
    uint32_t scanline_waits;
    uint32_t gpio_te_waits;
    uint32_t te_phase_waits;
    uint32_t te_phase_relocks;
    uint32_t last_te_phase_us;
    uint32_t last_te_target_us;
    uint32_t last_dma_start_phase_us;
    uint32_t last_dma_end_phase_us;
    uint32_t last_dma_start_delay_us;
    uint32_t timeouts;
    uint32_t last_wait_us;
    uint32_t max_wait_us;
    uint32_t edge_intervals;
    uint32_t last_edge_interval_us;
    uint32_t min_edge_interval_us;
    uint32_t marker_probe_samples;
    uint32_t marker_probe_high_samples;
    uint32_t marker_probe_low_samples;
    uint32_t marker_probe_transitions;
    uint32_t marker_probe_status_first;
    uint32_t marker_probe_status_last;
    uint32_t marker_probe_status_or;
    uint32_t marker_probe_status_and;
    uint32_t scanline_probe_samples;
    uint32_t scanline_probe_changes;
    uint32_t scanline_probe_wraps;
    uint32_t scanline_probe_first;
    uint32_t scanline_probe_last;
    uint32_t scanline_probe_min;
    uint32_t scanline_probe_max;
    uint32_t scanline_probe_raw_first;
    uint32_t scanline_probe_raw_last;
    uint32_t scanline_probe_raw_or;
    uint32_t scanline_probe_raw_and;
    uint32_t rcmd_probe_samples;
    uint32_t rcmd_probe_timeouts;
    uint32_t rcmd_probe_changes;
    uint32_t rcmd_probe_wraps;
    uint32_t rcmd_probe_first;
    uint32_t rcmd_probe_last;
    uint32_t rcmd_probe_min;
    uint32_t rcmd_probe_max;
    uint32_t rcmd_probe_raw_first;
    uint32_t rcmd_probe_raw_last;
    uint32_t rcmd_probe_raw_or;
    uint32_t rcmd_probe_raw_and;
    uint32_t te_probe_samples;
    uint32_t te_probe_elapsed_us;
    uint32_t te_probe_intcon_first;
    uint32_t te_probe_intcon_last;
    uint32_t te_probe_intcon_or;
    uint32_t te_probe_intcon_and;
    uint32_t te_probe_intcon_changed;
    uint32_t te_probe_gpio_pcon[LCD_TE_PROBE_GPIO_GROUPS];
    uint32_t te_probe_gpio_first[LCD_TE_PROBE_GPIO_GROUPS];
    uint32_t te_probe_gpio_last[LCD_TE_PROBE_GPIO_GROUPS];
    uint32_t te_probe_gpio_or[LCD_TE_PROBE_GPIO_GROUPS];
    uint32_t te_probe_gpio_and[LCD_TE_PROBE_GPIO_GROUPS];
    uint32_t te_probe_gpio_changed[LCD_TE_PROBE_GPIO_GROUPS];
    uint32_t te_probe_candidate_count;
    uint32_t te_sync_input_valid;
    uint32_t te_sync_active_high;
    struct lcd_te_probe_candidate
        te_probe_candidates[LCD_TE_PROBE_CANDIDATES];
    uint32_t dma_transfers;
    uint32_t last_dma_us;
    uint32_t max_dma_us;
};

void lcd_get_frame_sync_diagnostics(
    struct lcd_frame_sync_diagnostics *diagnostics);
void lcd_update_rect_frame_sync(int x, int y, int width, int height);
void lcd_update_rect_music_sync(int x, int y, int width, int height);
#endif

void lcd_awake(void);

#ifdef S5L_LCD_WITH_READID
void lcd_read_display_id(int mupiface, uint8_t *lcd_id);
#endif

extern struct lcd_info_rec* lcd_target_get_info(void);

#endif /* __LCD_S5L8702_H__ */
