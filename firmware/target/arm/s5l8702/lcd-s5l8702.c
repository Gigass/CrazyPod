/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: lcd-nano2g.c 28868 2010-12-21 06:59:17Z Buschel $
 *
 * Copyright (C) 2009 by Dave Chapman
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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"

#include "kernel.h"
#include "system.h"
#include "cpu.h"
#include "lcd.h"
#ifdef IPOD_6G
#include "pmu-target.h"
#endif
#include "backlight-target.h"

#include "s5l87xx.h"
#include "clocking-s5l8702.h"
#include "dma-s5l8702.h"
#include "lcd-s5l8702.h"
#include "lcd-target.h"


// TODO TODO TODO: HAVE_LCD_ENABLE

/* Switch command/frame mode:
 *
 * XXX: WIP, TBC
 *
 * Configures MPU interface and pixel mode.
 *
 * Frame mode:
 *   6g:
 *       16-bit 8080-series parallel MPU interface
 *       pixel mode: RBG565/1-transfer
 *       APB[15:0] -> D[17:10,8:1]
 *   nano3g:
 *       9-bit 8080-series parallel PMU interface (TBC)
 *       pixel mode: RGB565/2-transfers (TBC)
 *       APB[15:0] -> D[8:1] + D[8:1] (TBC)
 *
 * Command mode:
 *   LCD HW that uses 8-bit command set (6g, nano3g):
 *       8-bit 8080-series parallel MPU interface
 *       APB[7:0] -> D[8:1]
 *   LCD HW that uses 16-bit command set (6g):
 *       18-bit 8080-series parallel MPU interface (TBC)
 *       APB[15:0] -> D[17:10,8:1]
 *
 * See ILI9320, ILI9326, ILI9340 DS.
 */
/* XXX: see ili9340, pg.27 for MCU interface selection,
 *      bit31 could be the one that selects interface D[7:0] or D[8:1] for 8-bit), Type-I uses D[7:0] and Type-II uses D[17:10] (ILI9340)
 *                                                                           9-bit               D[8:0]                D[17:9]
 *                                                                          16-bit               D[15:0]               D[17:10],D[8:1]
 *                                                                          18-bit               D[17:0]               D[17:0]
 *      bit24 could be the one that selects interface D[8:1] or D[17:10]
 *                                          MCU_interface Mode          Pins            Write                           Read
 * #define LCD_MODE_8      0x80000c20       8080 MCU 8-bit bus iface    D[8:1]          APB[7:0] -> D[8:1]              D[8:1] -> LCD_DBUFF[8:1]
 * #define LCD_MODE_9      0x81100db8                9-bit
 * #define LCD_MODE_16     0x80100db0               16-bit              D[17:10,8:1]    APB[15:0] -> D[17:10,8:1]
 * #define LCD_MODE_18     0x80000da8               18-bit              D[17:0]         APB[17:0] -> D[17:0] (TBC)
 *
 * #define LCD_MODE_8      0x80000c20          // LCD_MODE_P8T1        // paralled 8-bit, 1 transfer, TBC: DB[17:10] or DB[8:1] ???
 * #define LCD_MODE_9      0x81100db8          // LCD_MODE_P9T2        // TBC: DB[8:1] or DB[17:10] ???, 2-transfers or 1.5-transfers, RGB565 or RBG666 ???
 * #define LCD_MODE_16     0x80100db0          // LCD_MODE_P16T1       // TBC: DB[17:10,8:1]
 * #define LCD_MODE_18     0x80000da8          // LCD_MODE_P18T1       // TBC: DB[17:10,8:1]
 */

#define LCD_MODE_P8     0x80000c20          // LCD_MODE_P8T1        // paralled 8-bit, 1 transfer, TBC: DB[17:10] or DB[8:1] ???
#define LCD_MODE_P8b    0x80100c20          // TODO: see if it influences, so far we are using it in nano3g with 0x80000c20 and it seems to be working fine
                                            // It may be a "pixel format" setting for 8 bit, which would only affect DATA and not CMD ???
#define LCD_MODE_P9     0x81100db8          // LCD_MODE_P9T2        // TBC: DB[8:1] or DB[17:10] ???, 2-transfers or 1.5-transfers, RGB565 or RBG666 ???
#define LCD_MODE_P16    0x80100db0          // LCD_MODE_P16T1       // TBC: DB[17:10,8:1]
#define LCD_MODE_P18    0x80000da8          // LCD_MODE_P18T1       // TBC: DB[17:10,8:1]

#define LCD_MODE_S8     0x41000c20          // nano4g
#define LCD_MODE_S9     0x41100db8          // nano4g



#ifdef S5L_LCD_WITH_CMDSET16
/* LCD type 16-bit register defines */
#define R_HORIZ_GRAM_ADDR_SET     0x200
#define R_VERT_GRAM_ADDR_SET      0x201
#define R_WRITE_DATA_TO_GRAM      0x202
#define R_HORIZ_ADDR_START_POS    0x210
#define R_HORIZ_ADDR_END_POS      0x211
#define R_VERT_ADDR_START_POS     0x212
#define R_VERT_ADDR_END_POS       0x213
#endif

/* LCD type 8-bit register defines */
#define R_COLUMN_ADDR_SET         0x2a
#define R_ROW_ADDR_SET            0x2b
#define R_MEMORY_WRITE            0x2c
#ifdef IPOD_6G
#define R_GET_SCANLINE            0x45
#endif


/** globals **/

int lcd_type; /* also needed in debug-s5l8702.c */
static struct mutex lcd_mutex;
static uint16_t lcd_dblbuf[LCD_HEIGHT][LCD_WIDTH] CACHEALIGN_ATTR;
static bool lcd_ispowered;

/* TODO: there is no info about these specific drivers,
   some useful info on similar HW such as ILI9320, ILI9326, ILI9340 and */

static struct lcd_info_rec *lcd_info;
static void (*lcd_run_seq)(void*);

/* LCD_CONFIG values for command/frame modes */
static uint32_t lcd_cmd_mode IDATA_ATTR;
static uint32_t lcd_frame_mode IDATA_ATTR;


// TODO
/* Each target must define a list containing all supported LCD types */
// extern struct lcd_info_rec lcd_info_list[];
// static struct lcd_info_rec lcd_info_list[];


/* DMA configuration */

/* One single transfer at once, needed LLIs:
 *   screen_size / (DMAC_LLI_MAX_COUNT << swidth) =
 *   (320*240*2) / (4095*2) = 19
 */
#define LCD_DMA_TSKBUF_SZ   1   /* N tasks, MUST be pow2 */
#define LCD_DMA_LLIBUF_SZ   32  /* N LLIs, MUST be pow2 */

static struct dmac_tsk lcd_dma_tskbuf[LCD_DMA_TSKBUF_SZ];
static struct dmac_lli volatile \
            lcd_dma_llibuf[LCD_DMA_LLIBUF_SZ] CACHEALIGN_ATTR;

#ifdef IPOD_6G
static volatile uint32_t lcd_dma_started_us;
static volatile uint32_t lcd_dma_transfers;
static volatile uint32_t lcd_dma_last_us;
static volatile uint32_t lcd_dma_max_us;
static volatile uint32_t lcd_dma_last_start_phase_us;
static volatile uint32_t lcd_dma_last_end_phase_us;
static volatile uint32_t lcd_dma_last_start_delay_us;
static volatile bool lcd_dma_te_synchronized;
static bool lcd_te_phase_armed;
static uint32_t lcd_te_last_target_us;
static uint32_t lcd_last_frame_edge_us;
static uint32_t lcd_te_period_us;
static void displaylcd_dma_complete(void *cb_data);
#endif

static struct dmac_ch lcd_dma_ch =
{
    .dmac = &s5l8702_dmac0,
    .prio = DMAC_CH_PRIO(4),
#ifdef IPOD_6G
    .cb_fn = displaylcd_dma_complete,
#else
    .cb_fn = NULL,
#endif

    .tskbuf = lcd_dma_tskbuf,
    .tskbuf_mask = LCD_DMA_TSKBUF_SZ - 1,
    .queue_mode = QUEUE_NORMAL,

    .llibuf = lcd_dma_llibuf,
    .llibuf_mask = LCD_DMA_LLIBUF_SZ - 1,
    .llibuf_bus = DMAC_MASTER_AHB1,
};

static struct dmac_ch_cfg lcd_dma_ch_cfg =
{
    .srcperi = S5L8702_DMAC0_PERI_MEM,
    .dstperi = S5L8702_DMAC0_PERI_LCD_WR,
    .sbsize  = DMACCxCONTROL_BSIZE_1,
    .dbsize  = DMACCxCONTROL_BSIZE_1,
    .swidth  = DMACCxCONTROL_WIDTH_16,
    .dwidth  = DMACCxCONTROL_WIDTH_16,
    .sbus    = DMAC_MASTER_AHB1,
    .dbus    = DMAC_MASTER_AHB1,
    .sinc    = DMACCxCONTROL_INC_ENABLE,
    .dinc    = DMACCxCONTROL_INC_DISABLE,
    .prot    = DMAC_PROT_CACH | DMAC_PROT_BUFF | DMAC_PROT_PRIV,
    .lli_xfer_max_count = DMAC_LLI_MAX_COUNT,
};


/*** clocks ***/

// TODO: In mks5lboot --mkraw put a command to specify the address of the binary,
// for example --address 0x6000, it must be greater than 0x310 which would be the default address,
// pass this address (0x310..128Kb) in the dfu_options flag

// TODO: to lcd-target.c
static void lcd_target_enable_clocks(bool enable)
{
    clockgate_enable(CLOCKGATE_LCD, enable);
#ifdef IPOD_NANO4G
    clockgate_enable(CLOCKGATE_LCD_2, enable);
#endif
}


/*** LCD controller - low level functions ***/

static void s5l_lcd_write_config(uint32_t config) ICODE_ATTR;
static void s5l_lcd_write_config(uint32_t config)
{
    while (!(LCD_STATUS & 0x2));
    udelay(1);
    LCD_CON = config;
}

static void s5l_lcd_write_cmd(uint16_t cmd) ICODE_ATTR;
static void s5l_lcd_write_cmd(uint16_t cmd)
{
    while (LCD_STATUS & 0x10);
    LCD_WCMD = cmd;
}

static void s5l_lcd_write_data(uint16_t data) ICODE_ATTR;
static void s5l_lcd_write_data(uint16_t data)
{
    while (LCD_STATUS & 0x10);
    LCD_WDATA = data;
}

static void s5l_lcd_send_cmd8(uint8_t cmd, int len, uint8_t *data) ICODE_ATTR;
static void s5l_lcd_send_cmd8(uint8_t cmd, int len, uint8_t *data)
{
    s5l_lcd_write_cmd(cmd);
    while (len--)
        s5l_lcd_write_data(*data++);
}

#if defined(S5L_LCD_WITH_READID) || defined(IPOD_6G)
static void s5l_lcd_recv_cmd8(uint8_t cmd, int len, uint8_t *buf)
{
    s5l_lcd_write_cmd(cmd);
    while (len--) {
        while (!(LCD_STATUS & 0x2));
        LCD_RDATA = 0;
        while (!(LCD_STATUS & 1));
        *buf++ = LCD_DBUFF >> 1;
    }
}
#endif

#define s5l_lcd_set_command_mode()  s5l_lcd_write_config(lcd_cmd_mode)
#define s5l_lcd_set_frame_mode()    s5l_lcd_write_config(lcd_frame_mode)

static void s5l_lcd_run_seq8(void *seq8)
{
    uint8_t *seq = seq8;

    while (1) switch (*seq++)
    {
        case CMD:
        {
            uint8_t cmd = *seq++;
            int len = *seq++;
            s5l_lcd_send_cmd8(cmd, len, seq);
            seq += len;
            break;
        }
        case SLEEP:
            sleep(*seq++);
            break;
        case END:
        default:
            /* bye */
            return;
    }
}

#ifdef S5L_LCD_WITH_CMDSET16
static void s5l_lcd_write_reg(uint16_t cmd, uint16_t data) ICODE_ATTR;
static void s5l_lcd_write_reg(uint16_t cmd, uint16_t data)
{
    s5l_lcd_write_cmd(cmd);
    s5l_lcd_write_data(data);
}

static void s5l_lcd_run_seq16(void *seq16)
{
    uint16_t *seq = seq16;
    int action, param;
    uint16_t reg;

    while (1)
    {
        action = *seq & 0xff;
        param = *seq++ >> 8;

        switch (action)
        {
            case CMD:
                s5l_lcd_write_cmd(*seq++);
                break;
            case MREG:
                reg = *seq++;
                while (param--)
                    s5l_lcd_write_reg(reg++, *seq++);
                break;
            case SLEEP:
                sleep(param);
                break;
            case DELAY:
                udelay(param<<6);
                break;
            case END:
            default:
                /* bye */
                return;
        }
    }
}
#endif /* S5L_LCD_WITH_CMDSET16 */


/*** Update functions ***/

/* Update the display.
   This must be called after all other LCD functions that change the display. */
void lcd_update(void)
{
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static void displaylcd_setup(int x, int y, int width, int height) ICODE_ATTR;
static void displaylcd_setup(int x, int y, int width, int height)
{
    int xe = (x + width) - 1;           /* max horiz */
    int ye = (y + height) - 1;          /* max vert */

    s5l_lcd_set_command_mode();

#ifdef S5L_LCD_WITH_CMDSET16
    if (lcd_info->cmdset == LCD_CMDSET_16BIT)
    {
        s5l_lcd_write_reg(R_HORIZ_ADDR_START_POS, x);
        s5l_lcd_write_reg(R_HORIZ_ADDR_END_POS,   xe);
        s5l_lcd_write_reg(R_VERT_ADDR_START_POS,  y);
        s5l_lcd_write_reg(R_VERT_ADDR_END_POS,    ye);

        s5l_lcd_write_reg(R_HORIZ_GRAM_ADDR_SET,  x);
        s5l_lcd_write_reg(R_VERT_GRAM_ADDR_SET,   y);

        s5l_lcd_write_cmd(R_WRITE_DATA_TO_GRAM);
    }
    else
#endif
    {
        uint8_t col[] = { x >> 8, x & 0xff, xe >> 8, xe & 0xff };
        uint8_t row[] = { y >> 8, y & 0xff, ye >> 8, ye & 0xff };
        s5l_lcd_send_cmd8(R_COLUMN_ADDR_SET, 4, col);
        s5l_lcd_send_cmd8(R_ROW_ADDR_SET, 4, row);

        s5l_lcd_write_cmd(R_MEMORY_WRITE);
    }
}

static void displaylcd_prepare_dma(void)
{
    s5l_lcd_set_frame_mode();
    commit_dcache();
}

static void displaylcd_start_dma(int pixels)
{
#ifdef IPOD_6G
    lcd_dma_started_us = USEC_TIMER;
    lcd_dma_te_synchronized = lcd_te_phase_armed;
    if (lcd_te_phase_armed && lcd_te_period_us != 0)
    {
        uint32_t phase =
            (lcd_dma_started_us - lcd_last_frame_edge_us) %
            lcd_te_period_us;

        lcd_dma_last_start_phase_us = phase;
        lcd_dma_last_start_delay_us = phase >= lcd_te_last_target_us
            ? phase - lcd_te_last_target_us
            : lcd_te_period_us + phase - lcd_te_last_target_us;
    }
    lcd_te_phase_armed = false;
#endif
    dmac_ch_queue(&lcd_dma_ch, lcd_dblbuf,
            (void*)S5L8702_DADDR_PERI_LCD_WR, pixels*2, NULL);
}

static void displaylcd_dma(int pixels)
{
    displaylcd_prepare_dma();
    displaylcd_start_dma(pixels);
}

#ifdef IPOD_6G
static void displaylcd_dma_complete(void *cb_data)
{
    uint32_t elapsed;

    (void)cb_data;
    elapsed = USEC_TIMER - lcd_dma_started_us;
    lcd_dma_last_us = elapsed;
    if (elapsed > lcd_dma_max_us)
        lcd_dma_max_us = elapsed;
    if (lcd_dma_te_synchronized && lcd_te_period_us != 0)
        lcd_dma_last_end_phase_us =
            (USEC_TIMER - lcd_last_frame_edge_us) % lcd_te_period_us;
    lcd_dma_te_synchronized = false;
    ++lcd_dma_transfers;
}
#endif

// TODO: wait if there is a DMA transfer in progress
static void displaylcd_wait_dma(void)
{
    while (dmac_ch_running(&lcd_dma_ch))
        yield();
}

#ifdef IPOD_6G
/*
 * iPod 6G panels can emit a short TE/FMARK pulse. S5L LCD status bit 8 is
 * the candidate input route, but its public documentation is incomplete;
 * validate its edges and pulse duty before trusting it. The 8-bit panels
 * also expose their gate scan counter through DCS 45h, retained as a
 * verified fallback when the marker route is unavailable.
 */
#define LCD_STATUS_FRAME_MARKER          0x100
#define LCD_FRAME_SYNC_POLL_US               5
#define LCD_FRAME_SYNC_TIMEOUT_US        25000
#define LCD_FRAME_SYNC_PROBE_US          25000
#define LCD_SCAN_SYNC_POLL_US               20
#define LCD_SCAN_SYNC_PROBE_TIMEOUT_US   40000
#define LCD_SCAN_SYNC_WRAP_LINES            64
#define LCD_RCMD_READY_TIMEOUT_US          1000
#define LCD_TE_INPUT_PROBE_US             120000
#define LCD_TE_INPUT_POLL_US                   5
#define LCD_TE_EXPECTED_PERIOD_US          14720
#define LCD_TE_MIN_PERIOD_US               12000
#define LCD_TE_MAX_PERIOD_US               18000
#define LCD_TE_INPUT_BITS                    160
#define LCD_TE_GPIO_SOURCE                      7
#define LCD_TE_GPIO_GROUP                       6
#define LCD_TE_GPIO_BIT                         7
#define LCD_TE_GPIO_MASK              (1u << LCD_TE_GPIO_BIT)
#define LCD_TE_MIN_PULSE_US                    20
#define LCD_TE_MAX_PULSE_US                  4000
#define LCD_TE_MAX_PERIOD_SPREAD_US           500
#define LCD_TE_HOME_GUARD_LINES                 12
#define LCD_TE_MUSIC_GUARD_LINES                 2
#define LCD_TE_PHASE_RELOCK_FRAMES             128

static enum lcd_frame_sync_method lcd_frame_sync_method;
static bool lcd_frame_marker_idle_high;
static struct lcd_frame_sync_diagnostics lcd_frame_sync_diagnostics;
static bool lcd_te_input_probe_complete;
static bool lcd_te_input_valid;
static bool lcd_te_active_high;
static bool lcd_te_phase_locked;
static unsigned lcd_te_phase_frames;
static uint32_t lcd_te_probe_high[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_transitions[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_last_edge[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_min_edge[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_last_rise[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_period_count[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_period_sum[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_min_period[LCD_TE_INPUT_BITS];
static uint32_t lcd_te_probe_max_period[LCD_TE_INPUT_BITS];

static unsigned displaylcd_te_probe_index(unsigned source, unsigned bit)
{
    if (source == 0)
        return bit;
    return 32 + ((source - 1) * 8) + bit;
}

static unsigned displaylcd_te_probe_source_bits(unsigned source)
{
    return source == 0 ? 32 : 8;
}

static uint32_t displaylcd_te_probe_read(unsigned source)
{
    if (source == 0)
        return LCD_INTCON;
    return PDAT(source - 1) & 0xff;
}

static uint32_t displaylcd_te_candidate_score(unsigned index)
{
    uint32_t count = lcd_te_probe_period_count[index];
    uint32_t average;
    uint32_t error;

    if (count < 2)
        return UINT32_MAX;
    average = lcd_te_probe_period_sum[index] / count;
    if (average < LCD_TE_MIN_PERIOD_US ||
        average > LCD_TE_MAX_PERIOD_US)
        return UINT32_MAX;
    error = average > LCD_TE_EXPECTED_PERIOD_US
        ? average - LCD_TE_EXPECTED_PERIOD_US
        : LCD_TE_EXPECTED_PERIOD_US - average;
    return error;
}

static void displaylcd_te_probe_add_candidate(
    unsigned source, unsigned bit, unsigned index)
{
    struct lcd_te_probe_candidate candidate;
    uint32_t score = displaylcd_te_candidate_score(index);
    unsigned count = lcd_frame_sync_diagnostics.te_probe_candidate_count;
    unsigned insert;

    if (score == UINT32_MAX)
        return;

    candidate.source = source;
    candidate.bit = bit;
    candidate.transitions = lcd_te_probe_transitions[index];
    candidate.high_samples = lcd_te_probe_high[index];
    candidate.period_count = lcd_te_probe_period_count[index];
    candidate.average_period_us =
        lcd_te_probe_period_sum[index] / candidate.period_count;
    candidate.min_period_us = lcd_te_probe_min_period[index];
    candidate.max_period_us = lcd_te_probe_max_period[index];
    candidate.min_edge_interval_us = lcd_te_probe_min_edge[index];

    insert = count;
    if (insert > LCD_TE_PROBE_CANDIDATES)
        insert = LCD_TE_PROBE_CANDIDATES;
    while (insert > 0)
    {
        const struct lcd_te_probe_candidate *previous =
            &lcd_frame_sync_diagnostics.te_probe_candidates[insert - 1];
        uint32_t previous_error = previous->average_period_us >
            LCD_TE_EXPECTED_PERIOD_US
            ? previous->average_period_us - LCD_TE_EXPECTED_PERIOD_US
            : LCD_TE_EXPECTED_PERIOD_US - previous->average_period_us;

        if (previous_error <= score)
            break;
        if (insert < LCD_TE_PROBE_CANDIDATES)
            lcd_frame_sync_diagnostics.te_probe_candidates[insert] =
                *previous;
        --insert;
    }
    if (insert < LCD_TE_PROBE_CANDIDATES)
        lcd_frame_sync_diagnostics.te_probe_candidates[insert] = candidate;
    if (count < LCD_TE_PROBE_CANDIDATES)
        lcd_frame_sync_diagnostics.te_probe_candidate_count = count + 1;
}

static void displaylcd_validate_gpio_te(void)
{
    unsigned index = displaylcd_te_probe_index(
        LCD_TE_GPIO_SOURCE, LCD_TE_GPIO_BIT);
    uint32_t period_count = lcd_te_probe_period_count[index];
    uint32_t high_samples = lcd_te_probe_high[index];
    uint32_t samples = lcd_frame_sync_diagnostics.te_probe_samples;
    uint32_t average_period = period_count == 0 ? 0
        : lcd_te_probe_period_sum[index] / period_count;
    uint32_t period_spread =
        lcd_te_probe_max_period[index] -
        lcd_te_probe_min_period[index];
    bool narrow_high = high_samples > 0 &&
        high_samples < samples / 3;
    bool narrow_low = high_samples > (samples * 2) / 3 &&
        high_samples < samples;

    lcd_te_input_valid = period_count >= 2 &&
        lcd_te_probe_transitions[index] >= 6 &&
        average_period >= LCD_TE_MIN_PERIOD_US &&
        average_period <= LCD_TE_MAX_PERIOD_US &&
        period_spread <= LCD_TE_MAX_PERIOD_SPREAD_US &&
        lcd_te_probe_min_edge[index] >= LCD_TE_MIN_PULSE_US &&
        lcd_te_probe_min_edge[index] <= LCD_TE_MAX_PULSE_US &&
        (narrow_high || narrow_low);
    lcd_te_active_high = narrow_high;
    lcd_frame_sync_diagnostics.te_sync_input_valid =
        lcd_te_input_valid ? 1 : 0;
    lcd_frame_sync_diagnostics.te_sync_active_high =
        lcd_te_active_high ? 1 : 0;
    if (lcd_te_input_valid)
    {
        lcd_te_period_us = average_period;
        lcd_frame_sync_method = LCD_FRAME_SYNC_GPIO_TE;
    }
}

static void displaylcd_probe_te_inputs(void)
{
    uint32_t previous[1 + LCD_TE_PROBE_GPIO_GROUPS];
    uint32_t started;
    uint32_t deadline;
    unsigned source;
    unsigned bit;

    if (lcd_te_input_probe_complete)
        return;
    lcd_te_input_probe_complete = true;

    memset(lcd_te_probe_high, 0, sizeof(lcd_te_probe_high));
    memset(lcd_te_probe_transitions, 0,
        sizeof(lcd_te_probe_transitions));
    memset(lcd_te_probe_last_edge, 0,
        sizeof(lcd_te_probe_last_edge));
    memset(lcd_te_probe_min_edge, 0,
        sizeof(lcd_te_probe_min_edge));
    memset(lcd_te_probe_last_rise, 0,
        sizeof(lcd_te_probe_last_rise));
    memset(lcd_te_probe_period_count, 0,
        sizeof(lcd_te_probe_period_count));
    memset(lcd_te_probe_period_sum, 0,
        sizeof(lcd_te_probe_period_sum));
    memset(lcd_te_probe_min_period, 0,
        sizeof(lcd_te_probe_min_period));
    memset(lcd_te_probe_max_period, 0,
        sizeof(lcd_te_probe_max_period));
    memset(lcd_frame_sync_diagnostics.te_probe_candidates, 0,
        sizeof(lcd_frame_sync_diagnostics.te_probe_candidates));
    lcd_frame_sync_diagnostics.te_probe_candidate_count = 0;

    previous[0] = displaylcd_te_probe_read(0);
    lcd_frame_sync_diagnostics.te_probe_intcon_first = previous[0];
    lcd_frame_sync_diagnostics.te_probe_intcon_or = previous[0];
    lcd_frame_sync_diagnostics.te_probe_intcon_and = previous[0];
    for (source = 1; source <= LCD_TE_PROBE_GPIO_GROUPS; ++source)
    {
        unsigned group = source - 1;

        previous[source] = displaylcd_te_probe_read(source);
        lcd_frame_sync_diagnostics.te_probe_gpio_pcon[group] =
            PCON(group);
        lcd_frame_sync_diagnostics.te_probe_gpio_first[group] =
            previous[source];
        lcd_frame_sync_diagnostics.te_probe_gpio_or[group] =
            previous[source];
        lcd_frame_sync_diagnostics.te_probe_gpio_and[group] =
            previous[source];
    }

    started = USEC_TIMER;
    deadline = started + LCD_TE_INPUT_PROBE_US;
    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        uint32_t now;

        udelay(LCD_TE_INPUT_POLL_US);
        now = USEC_TIMER;
        ++lcd_frame_sync_diagnostics.te_probe_samples;
        for (source = 0; source <= LCD_TE_PROBE_GPIO_GROUPS; ++source)
        {
            uint32_t current = displaylcd_te_probe_read(source);
            uint32_t changed = current ^ previous[source];
            unsigned bits = displaylcd_te_probe_source_bits(source);

            if (source == 0)
            {
                lcd_frame_sync_diagnostics.te_probe_intcon_or |= current;
                lcd_frame_sync_diagnostics.te_probe_intcon_and &= current;
                lcd_frame_sync_diagnostics.te_probe_intcon_changed |=
                    changed;
            }
            else
            {
                unsigned group = source - 1;

                lcd_frame_sync_diagnostics.te_probe_gpio_or[group] |=
                    current;
                lcd_frame_sync_diagnostics.te_probe_gpio_and[group] &=
                    current;
                lcd_frame_sync_diagnostics.te_probe_gpio_changed[group] |=
                    changed;
            }

            for (bit = 0; bit < bits; ++bit)
            {
                uint32_t mask = 1u << bit;
                unsigned index =
                    displaylcd_te_probe_index(source, bit);

                if (current & mask)
                    ++lcd_te_probe_high[index];
                if (changed & mask)
                {
                    uint32_t edge_interval =
                        now - lcd_te_probe_last_edge[index];

                    ++lcd_te_probe_transitions[index];
                    if (lcd_te_probe_last_edge[index] != 0 &&
                        (lcd_te_probe_min_edge[index] == 0 ||
                         edge_interval < lcd_te_probe_min_edge[index]))
                        lcd_te_probe_min_edge[index] = edge_interval;
                    lcd_te_probe_last_edge[index] = now;

                    if (current & mask)
                    {
                        uint32_t period =
                            now - lcd_te_probe_last_rise[index];

                        if (lcd_te_probe_last_rise[index] != 0)
                        {
                            ++lcd_te_probe_period_count[index];
                            lcd_te_probe_period_sum[index] += period;
                            if (lcd_te_probe_min_period[index] == 0 ||
                                period < lcd_te_probe_min_period[index])
                                lcd_te_probe_min_period[index] = period;
                            if (period > lcd_te_probe_max_period[index])
                                lcd_te_probe_max_period[index] = period;
                        }
                        lcd_te_probe_last_rise[index] = now;
                    }
                }
            }
            previous[source] = current;
        }
    }

    lcd_frame_sync_diagnostics.te_probe_elapsed_us =
        USEC_TIMER - started;
    lcd_frame_sync_diagnostics.te_probe_intcon_last = previous[0];
    for (source = 1; source <= LCD_TE_PROBE_GPIO_GROUPS; ++source)
        lcd_frame_sync_diagnostics.te_probe_gpio_last[source - 1] =
            previous[source];

    for (source = 0; source <= LCD_TE_PROBE_GPIO_GROUPS; ++source)
    {
        unsigned bits = displaylcd_te_probe_source_bits(source);

        for (bit = 0; bit < bits; ++bit)
        {
            unsigned index = displaylcd_te_probe_index(source, bit);

            displaylcd_te_probe_add_candidate(source, bit, index);
        }
    }
    displaylcd_validate_gpio_te();
}

static void displaylcd_reset_frame_sync(void)
{
    lcd_frame_sync_method = lcd_te_input_valid
        ? LCD_FRAME_SYNC_GPIO_TE : LCD_FRAME_SYNC_PROBING;
    lcd_frame_marker_idle_high = false;
    lcd_last_frame_edge_us = 0;
    lcd_te_phase_locked = false;
    lcd_te_phase_frames = 0;
    lcd_te_phase_armed = false;
}

static void displaylcd_note_frame_edge(void);

static bool displaylcd_gpio_te_active(void)
{
    bool high = (PDAT(LCD_TE_GPIO_GROUP) & LCD_TE_GPIO_MASK) != 0;

    return high == lcd_te_active_high;
}

static bool displaylcd_wait_gpio_te(void)
{
    unsigned deadline = USEC_TIMER + LCD_FRAME_SYNC_TIMEOUT_US;

    /* Finish any pulse already active, then wait for a fresh frame edge. */
    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        if (!displaylcd_gpio_te_active())
            break;
        udelay(LCD_FRAME_SYNC_POLL_US);
    }
    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        if (displaylcd_gpio_te_active())
        {
            displaylcd_note_frame_edge();
            lcd_te_phase_locked = true;
            lcd_te_phase_frames = 0;
            return true;
        }
        udelay(LCD_FRAME_SYNC_POLL_US);
    }
    return false;
}

static void displaylcd_note_wait_duration(uint32_t started)
{
    lcd_frame_sync_diagnostics.last_wait_us = USEC_TIMER - started;
    if (lcd_frame_sync_diagnostics.last_wait_us >
        lcd_frame_sync_diagnostics.max_wait_us)
        lcd_frame_sync_diagnostics.max_wait_us =
            lcd_frame_sync_diagnostics.last_wait_us;
}

static void displaylcd_wait_te_phase(
    int y, int height, unsigned guard_lines)
{
    uint32_t started = USEC_TIMER;
    uint32_t now;
    uint32_t phase;
    uint32_t target_phase;
    uint32_t target_time;
    unsigned bottom = y + height;

    lcd_te_phase_armed = false;
    ++lcd_frame_sync_diagnostics.waits;
    if (lcd_frame_sync_method != LCD_FRAME_SYNC_GPIO_TE ||
        lcd_te_period_us == 0)
    {
        displaylcd_note_wait_duration(started);
        return;
    }

    if (!lcd_te_phase_locked ||
        lcd_te_phase_frames >= LCD_TE_PHASE_RELOCK_FRAMES)
    {
        if (!displaylcd_wait_gpio_te())
        {
            ++lcd_frame_sync_diagnostics.timeouts;
            lcd_frame_sync_method = LCD_FRAME_SYNC_UNAVAILABLE;
            displaylcd_note_wait_duration(started);
            return;
        }
        ++lcd_frame_sync_diagnostics.te_phase_relocks;
    }

    if (bottom > LCD_HEIGHT)
        bottom = LCD_HEIGHT;
    bottom += guard_lines;
    if (bottom > LCD_HEIGHT)
        bottom = LCD_HEIGHT;
    target_phase = (lcd_te_period_us * bottom) / LCD_HEIGHT;

    now = USEC_TIMER;
    phase = now - lcd_last_frame_edge_us;
    if (phase >= lcd_te_period_us)
    {
        uint32_t periods = phase / lcd_te_period_us;

        lcd_last_frame_edge_us += periods * lcd_te_period_us;
        phase -= periods * lcd_te_period_us;
    }
    target_time = lcd_last_frame_edge_us + target_phase;
    if (phase > target_phase)
    {
        lcd_last_frame_edge_us += lcd_te_period_us;
        target_time = lcd_last_frame_edge_us + target_phase;
    }

    while (TIME_BEFORE(USEC_TIMER, target_time))
        udelay(LCD_FRAME_SYNC_POLL_US);

    lcd_frame_sync_diagnostics.last_te_phase_us =
        USEC_TIMER - lcd_last_frame_edge_us;
    lcd_frame_sync_diagnostics.last_te_target_us = target_phase;
    lcd_te_last_target_us = target_phase;
    lcd_te_phase_armed = true;
    ++lcd_frame_sync_diagnostics.gpio_te_waits;
    ++lcd_frame_sync_diagnostics.te_phase_waits;
    ++lcd_te_phase_frames;
    displaylcd_note_wait_duration(started);
}

static void displaylcd_note_frame_edge(void)
{
    uint32_t now = USEC_TIMER;

    if (lcd_last_frame_edge_us != 0)
    {
        uint32_t interval = now - lcd_last_frame_edge_us;

        ++lcd_frame_sync_diagnostics.edge_intervals;
        lcd_frame_sync_diagnostics.last_edge_interval_us = interval;
        if (lcd_frame_sync_diagnostics.min_edge_interval_us == 0 ||
            interval < lcd_frame_sync_diagnostics.min_edge_interval_us)
            lcd_frame_sync_diagnostics.min_edge_interval_us = interval;
    }
    lcd_last_frame_edge_us = now;
}

static bool displaylcd_probe_frame_marker(void)
{
    unsigned deadline = USEC_TIMER + LCD_FRAME_SYNC_PROBE_US;
    unsigned high_samples = 0;
    unsigned low_samples = 0;
    unsigned transitions = 0;
    uint32_t status = LCD_STATUS;
    uint32_t status_or = status;
    uint32_t status_and = status;
    bool previous = (status & LCD_STATUS_FRAME_MARKER) != 0;

    lcd_frame_sync_diagnostics.marker_probe_status_first = status;

    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        bool current;

        udelay(LCD_FRAME_SYNC_POLL_US);
        status = LCD_STATUS;
        status_or |= status;
        status_and &= status;
        current = (status & LCD_STATUS_FRAME_MARKER) != 0;
        if (current)
            ++high_samples;
        else
            ++low_samples;
        if (current != previous)
            ++transitions;
        previous = current;
    }

    lcd_frame_sync_diagnostics.marker_probe_samples =
        high_samples + low_samples;
    lcd_frame_sync_diagnostics.marker_probe_high_samples = high_samples;
    lcd_frame_sync_diagnostics.marker_probe_low_samples = low_samples;
    lcd_frame_sync_diagnostics.marker_probe_transitions = transitions;
    lcd_frame_sync_diagnostics.marker_probe_status_last = status;
    lcd_frame_sync_diagnostics.marker_probe_status_or = status_or;
    lcd_frame_sync_diagnostics.marker_probe_status_and = status_and;

    if (transitions < 2 || high_samples < 3 || low_samples < 3)
        return false;

    /* FMARK is a pulse, so its inactive level is the majority sample. */
    if (high_samples > low_samples)
    {
        if (low_samples * 4 >= high_samples)
            return false;
        lcd_frame_marker_idle_high = true;
    }
    else
    {
        if (high_samples * 4 >= low_samples)
            return false;
        lcd_frame_marker_idle_high = false;
    }
    return true;
}

static bool displaylcd_wait_frame_marker(void)
{
    unsigned deadline = USEC_TIMER + LCD_FRAME_SYNC_TIMEOUT_US;

    /* Never reuse a pulse that was already active when this wait began. */
    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        bool high = (LCD_STATUS & LCD_STATUS_FRAME_MARKER) != 0;

        if (high == lcd_frame_marker_idle_high)
            break;
        udelay(LCD_FRAME_SYNC_POLL_US);
    }

    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        bool high = (LCD_STATUS & LCD_STATUS_FRAME_MARKER) != 0;

        if (high != lcd_frame_marker_idle_high)
        {
            displaylcd_note_frame_edge();
            return true;
        }
        udelay(LCD_FRAME_SYNC_POLL_US);
    }
    return false;
}

static unsigned displaylcd_get_scanline(uint32_t *raw)
{
    uint8_t data[3];

    /* The first byte returned by a DBI read is a dummy byte. */
    s5l_lcd_recv_cmd8(R_GET_SCANLINE, 3, data);
    if (raw != NULL)
        *raw = ((uint32_t)data[0] << 16) |
            ((uint32_t)data[1] << 8) | data[2];
    return ((data[1] & 0x03) << 8) | data[2];
}

static bool displaylcd_wait_status(uint32_t mask, uint32_t value)
{
    unsigned deadline = USEC_TIMER + LCD_RCMD_READY_TIMEOUT_US;

    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        if ((LCD_STATUS & mask) == value)
            return true;
    }
    return false;
}

static bool displaylcd_get_scanline_rcmd(
    unsigned *scanline, uint32_t *raw)
{
    uint8_t data[3];
    unsigned index;

    if (!displaylcd_wait_status(0x10, 0))
        return false;
    LCD_RCMD = R_GET_SCANLINE;
    for (index = 0; index < 3; ++index)
    {
        if (!displaylcd_wait_status(0x2, 0x2))
            return false;
        LCD_RDATA = 0;
        if (!displaylcd_wait_status(0x1, 0x1))
            return false;
        data[index] = LCD_DBUFF >> 1;
    }
    *raw = ((uint32_t)data[0] << 16) |
        ((uint32_t)data[1] << 8) | data[2];
    *scanline = ((data[1] & 0x03) << 8) | data[2];
    return true;
}

static void displaylcd_probe_scanline_rcmd(void)
{
    unsigned deadline = USEC_TIMER + LCD_SCAN_SYNC_PROBE_TIMEOUT_US;
    unsigned previous;
    uint32_t raw;

    if (!displaylcd_get_scanline_rcmd(&previous, &raw))
    {
        ++lcd_frame_sync_diagnostics.rcmd_probe_timeouts;
        return;
    }

    lcd_frame_sync_diagnostics.rcmd_probe_samples = 1;
    lcd_frame_sync_diagnostics.rcmd_probe_changes = 0;
    lcd_frame_sync_diagnostics.rcmd_probe_wraps = 0;
    lcd_frame_sync_diagnostics.rcmd_probe_first = previous;
    lcd_frame_sync_diagnostics.rcmd_probe_last = previous;
    lcd_frame_sync_diagnostics.rcmd_probe_min = previous;
    lcd_frame_sync_diagnostics.rcmd_probe_max = previous;
    lcd_frame_sync_diagnostics.rcmd_probe_raw_first = raw;
    lcd_frame_sync_diagnostics.rcmd_probe_raw_last = raw;
    lcd_frame_sync_diagnostics.rcmd_probe_raw_or = raw;
    lcd_frame_sync_diagnostics.rcmd_probe_raw_and = raw;

    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        unsigned current;
        int delta;

        udelay(LCD_SCAN_SYNC_POLL_US);
        if (!displaylcd_get_scanline_rcmd(&current, &raw))
        {
            ++lcd_frame_sync_diagnostics.rcmd_probe_timeouts;
            return;
        }
        delta = (int)current - (int)previous;
        ++lcd_frame_sync_diagnostics.rcmd_probe_samples;
        if (current != previous)
            ++lcd_frame_sync_diagnostics.rcmd_probe_changes;
        if (delta >= LCD_SCAN_SYNC_WRAP_LINES ||
            delta <= -LCD_SCAN_SYNC_WRAP_LINES)
            ++lcd_frame_sync_diagnostics.rcmd_probe_wraps;
        if (current < lcd_frame_sync_diagnostics.rcmd_probe_min)
            lcd_frame_sync_diagnostics.rcmd_probe_min = current;
        if (current > lcd_frame_sync_diagnostics.rcmd_probe_max)
            lcd_frame_sync_diagnostics.rcmd_probe_max = current;
        lcd_frame_sync_diagnostics.rcmd_probe_last = current;
        lcd_frame_sync_diagnostics.rcmd_probe_raw_last = raw;
        lcd_frame_sync_diagnostics.rcmd_probe_raw_or |= raw;
        lcd_frame_sync_diagnostics.rcmd_probe_raw_and &= raw;
        previous = current;
    }
}

static bool displaylcd_wait_scanline(bool probe)
{
    s5l_lcd_set_command_mode();

    unsigned timeout = probe ? LCD_SCAN_SYNC_PROBE_TIMEOUT_US
                             : LCD_FRAME_SYNC_TIMEOUT_US;
    unsigned deadline = USEC_TIMER + timeout;
    uint32_t raw;
    unsigned previous = displaylcd_get_scanline(&raw);
    int direction = 0;
    unsigned stable_steps = 0;

    if (probe)
    {
        lcd_frame_sync_diagnostics.scanline_probe_samples = 1;
        lcd_frame_sync_diagnostics.scanline_probe_changes = 0;
        lcd_frame_sync_diagnostics.scanline_probe_wraps = 0;
        lcd_frame_sync_diagnostics.scanline_probe_first = previous;
        lcd_frame_sync_diagnostics.scanline_probe_last = previous;
        lcd_frame_sync_diagnostics.scanline_probe_min = previous;
        lcd_frame_sync_diagnostics.scanline_probe_max = previous;
        lcd_frame_sync_diagnostics.scanline_probe_raw_first = raw;
        lcd_frame_sync_diagnostics.scanline_probe_raw_last = raw;
        lcd_frame_sync_diagnostics.scanline_probe_raw_or = raw;
        lcd_frame_sync_diagnostics.scanline_probe_raw_and = raw;
    }

    while (TIME_BEFORE(USEC_TIMER, deadline))
    {
        udelay(LCD_SCAN_SYNC_POLL_US);

        unsigned current = displaylcd_get_scanline(&raw);
        int delta = (int)current - (int)previous;

        if (probe)
        {
            ++lcd_frame_sync_diagnostics.scanline_probe_samples;
            if (current != previous)
                ++lcd_frame_sync_diagnostics.scanline_probe_changes;
            if (current <
                lcd_frame_sync_diagnostics.scanline_probe_min)
                lcd_frame_sync_diagnostics.scanline_probe_min = current;
            if (current >
                lcd_frame_sync_diagnostics.scanline_probe_max)
                lcd_frame_sync_diagnostics.scanline_probe_max = current;
            lcd_frame_sync_diagnostics.scanline_probe_last = current;
            lcd_frame_sync_diagnostics.scanline_probe_raw_last = raw;
            lcd_frame_sync_diagnostics.scanline_probe_raw_or |= raw;
            lcd_frame_sync_diagnostics.scanline_probe_raw_and &= raw;
        }

        if (delta >= LCD_SCAN_SYNC_WRAP_LINES ||
            delta <= -LCD_SCAN_SYNC_WRAP_LINES)
        {
            if (probe)
                ++lcd_frame_sync_diagnostics.scanline_probe_wraps;
            if (!probe || stable_steps >= 3)
            {
                displaylcd_note_frame_edge();
                return true;
            }

            /* Probe started on a boundary; validate one full cycle. */
            direction = 0;
            stable_steps = 0;
        }
        else if (delta != 0)
        {
            int step_direction = delta > 0 ? 1 : -1;

            if (direction == 0 || direction == step_direction)
            {
                direction = step_direction;
                stable_steps++;
            }
            else
            {
                direction = step_direction;
                stable_steps = 1;
            }
        }

        previous = current;
    }
    return false;
}

static void displaylcd_wait_frame_start(void)
{
    unsigned started = USEC_TIMER;
    bool scanline_probe = false;
    bool synced = false;

    ++lcd_frame_sync_diagnostics.waits;
    if (lcd_frame_sync_method == LCD_FRAME_SYNC_GPIO_TE)
    {
        synced = displaylcd_wait_gpio_te();
        if (synced)
            ++lcd_frame_sync_diagnostics.gpio_te_waits;
        else
        {
            ++lcd_frame_sync_diagnostics.timeouts;
            lcd_frame_sync_method = LCD_FRAME_SYNC_UNAVAILABLE;
        }
    }
    if (lcd_frame_sync_method == LCD_FRAME_SYNC_PROBING)
    {
        if (displaylcd_probe_frame_marker())
            lcd_frame_sync_method = LCD_FRAME_SYNC_MARKER;
        else if (lcd_info->cmdset == LCD_CMDSET_8BIT)
        {
            lcd_frame_sync_method = LCD_FRAME_SYNC_SCANLINE;
            scanline_probe = true;
        }
        else
        {
            ++lcd_frame_sync_diagnostics.timeouts;
            lcd_frame_sync_method = LCD_FRAME_SYNC_UNAVAILABLE;
        }
    }

    if (lcd_frame_sync_method == LCD_FRAME_SYNC_MARKER)
    {
        synced = displaylcd_wait_frame_marker();
        if (synced)
            ++lcd_frame_sync_diagnostics.marker_waits;
        else
        {
            ++lcd_frame_sync_diagnostics.timeouts;
            lcd_frame_sync_method = lcd_info->cmdset == LCD_CMDSET_8BIT
                ? LCD_FRAME_SYNC_SCANLINE
                : LCD_FRAME_SYNC_UNAVAILABLE;
            scanline_probe =
                lcd_frame_sync_method == LCD_FRAME_SYNC_SCANLINE;
        }
    }

    if (!synced && lcd_frame_sync_method == LCD_FRAME_SYNC_SCANLINE)
    {
        synced = displaylcd_wait_scanline(scanline_probe);
        if (synced)
            ++lcd_frame_sync_diagnostics.scanline_waits;
        else
        {
            ++lcd_frame_sync_diagnostics.timeouts;
            if (scanline_probe)
                displaylcd_probe_scanline_rcmd();
            lcd_frame_sync_method = LCD_FRAME_SYNC_UNAVAILABLE;
        }
    }

    lcd_frame_sync_diagnostics.last_wait_us = USEC_TIMER - started;
    if (lcd_frame_sync_diagnostics.last_wait_us >
        lcd_frame_sync_diagnostics.max_wait_us)
        lcd_frame_sync_diagnostics.max_wait_us =
            lcd_frame_sync_diagnostics.last_wait_us;
}

void lcd_get_frame_sync_diagnostics(
    struct lcd_frame_sync_diagnostics *diagnostics)
{
    if (diagnostics == NULL)
        return;
    *diagnostics = lcd_frame_sync_diagnostics;
    diagnostics->panel_type = lcd_type;
    diagnostics->method = lcd_frame_sync_method;
    diagnostics->dma_transfers = lcd_dma_transfers;
    diagnostics->last_dma_us = lcd_dma_last_us;
    diagnostics->max_dma_us = lcd_dma_max_us;
    diagnostics->last_dma_start_phase_us =
        lcd_dma_last_start_phase_us;
    diagnostics->last_dma_end_phase_us =
        lcd_dma_last_end_phase_us;
    diagnostics->last_dma_start_delay_us =
        lcd_dma_last_start_delay_us;
}
#endif

static void displaylcd_update_rect(
    int, int, int, int, int) ICODE_ATTR;
static void displaylcd_update_rect(
    int x, int y, int width, int height, int phase_guard_lines)
{
    int pixels = width * height;
    bool frame_sync = phase_guard_lines >= 0;
    fb_data* p = FBADDR(x,y);
    uint16_t* out = lcd_dblbuf[0];

    /* FIXME: ISR()->panicf()->lcd_update() blocks forever */
    mutex_lock(&lcd_mutex);
    if (lcd_ispowered)
    {
        // XXX: We contemplate the case where the LCD has entered SLEEP,
        //      the s5l_lcd_set_command_mode() writes LCD_CONFIG
        // XXX: It is also possible that if the LCD clockgate has been disabled,
        //      the LCD_CONFIG = xxx may still be blocked by the while(status == ok),
        //      since it does not check the same status bit as when writing command/data

        displaylcd_wait_dma();

        /* Copy display bitmap to hardware */
        if (LCD_WIDTH == width) {
            /* Write all lines at once */
            memcpy(out, p, pixels * 2);
        } else {
            int rows = height;

            do {
                /* Write a single line */
                memcpy(out, p, width * 2);
                p += LCD_WIDTH;
                out += width;
            } while (--rows);
        }

#ifdef IPOD_6G
        if (!frame_sync && lcd_info->seq_frame_sync != NULL &&
            x == 0 && y == 0 &&
            width == LCD_WIDTH && height == LCD_HEIGHT)
            displaylcd_wait_frame_start();
#endif

        displaylcd_setup(x, y, width, height);
#ifdef IPOD_6G
        if (frame_sync)
        {
            displaylcd_prepare_dma();
            displaylcd_wait_te_phase(
                y, height, (unsigned)phase_guard_lines);
            displaylcd_start_dma(pixels);
        }
        else
#endif
            displaylcd_dma(pixels);
    }
    mutex_unlock(&lcd_mutex);
}

/* Update a fraction of the display. */
void lcd_update_rect(int x, int y, int width, int height)
{
    displaylcd_update_rect(x, y, width, height, -1);
}

#ifdef IPOD_6G
void lcd_update_rect_frame_sync(int x, int y, int width, int height)
{
    displaylcd_update_rect(x, y, width, height,
        lcd_info->seq_frame_sync != NULL ? LCD_TE_HOME_GUARD_LINES : -1);
}

void lcd_update_rect_music_sync(int x, int y, int width, int height)
{
    displaylcd_update_rect(x, y, width, height,
        lcd_info->seq_frame_sync != NULL ? LCD_TE_MUSIC_GUARD_LINES : -1);
}
#endif

/* Line write helper function for lcd_yuv_blit. Writes two lines of yuv420. */
extern void lcd_write_yuv420_lines(unsigned char const * const src[3],
                                   uint16_t* outbuf,
                                   int width,
                                   int stride);

/* Blit a YUV bitmap directly to the LCD */
void lcd_blit_yuv(unsigned char * const src[3],
                  int src_x, int src_y, int stride,
                  int x, int y, int width, int height) ICODE_ATTR;
void lcd_blit_yuv(unsigned char * const src[3],
                  int src_x, int src_y, int stride,
                  int x, int y, int width, int height)
{
    unsigned int z;
    unsigned char const * yuv_src[3];

    width = (width + 1) & ~1;       /* ensure width is even */

    int pixels = width * height;
    uint16_t* out = lcd_dblbuf[0];

    z = stride * src_y;
    yuv_src[0] = src[0] + z + src_x;
    yuv_src[1] = src[1] + (z >> 2) + (src_x >> 1);
    yuv_src[2] = src[2] + (yuv_src[1] - src[1]);

    /* TODO: ISR()->panicf()->lcd_update() blocks forever */
    mutex_lock(&lcd_mutex);
    if (lcd_ispowered)
    {
        displaylcd_wait_dma();

        displaylcd_setup(x, y, width, height);

        height >>= 1;

        do {
            lcd_write_yuv420_lines(yuv_src, out, width, stride);
            yuv_src[0] += stride << 1;
            yuv_src[1] += stride >> 1; /* Skip down one chroma line */
            yuv_src[2] += stride >> 1;
            out += width << 1;
        } while (--height);

        displaylcd_dma(pixels);
    }
    mutex_unlock(&lcd_mutex);
}


/*** hardware configuration ***/

int lcd_default_contrast(void)
{
    return 0x1f;
}

void lcd_set_contrast(int val)
{
    (void)val;
}

void lcd_set_invert_display(bool yesno)
{
    (void)yesno;
}

void lcd_set_flip(bool yesno)
{
    (void)yesno;
}

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
bool lcd_active(void)
{
    return lcd_ispowered;
}
#endif

#if defined(HAVE_LCD_SHUTDOWN) || defined(HAVE_LCD_SLEEP)
static void lcd_powersave(void)
{
    mutex_lock(&lcd_mutex);

    displaylcd_wait_dma();
                                   // XXX: Do not change modes while data is being sent
    s5l_lcd_set_command_mode();
    lcd_run_seq(lcd_info->seq_sleep);

    lcd_target_enable_clocks(false);

    lcd_ispowered = false;

    mutex_unlock(&lcd_mutex);
}
#endif /* HAVE_LCD_SHUTDOWN || HAVE_LCD_SLEEP */

#ifdef HAVE_LCD_SHUTDOWN
void lcd_shutdown(void)
{
    backlight_hw_kill();  /* Kill the backlight, instantly. */
    lcd_powersave();
}
#endif

#ifdef HAVE_LCD_SLEEP
void lcd_sleep(void)
{
    lcd_powersave();
}

void lcd_awake(void)
{
    mutex_lock(&lcd_mutex);

    lcd_target_enable_clocks(true);
                                 // XXX: Do not change modes while data is being sent
    s5l_lcd_set_command_mode();
    lcd_run_seq(lcd_info->seq_awake);
#ifdef IPOD_6G
    if (lcd_info->seq_frame_sync != NULL)
    {
        lcd_run_seq(lcd_info->seq_frame_sync);
        displaylcd_reset_frame_sync();
        displaylcd_probe_te_inputs();
    }
#endif
    lcd_ispowered = true;       // XXX: we have to put the lcd_ispowered before the lcd_update()

    lcd_update();               // XXX: update the display and wait for the update to finish before returning,

    displaylcd_wait_dma();      //      we should really do sleep_out + lcd_update + display_on,
                                //      we can put a command in the sequence to do it

    mutex_unlock(&lcd_mutex);
    send_event(LCD_EVENT_ACTIVATION, NULL);
}
#endif

#ifdef S5L_LCD_WITH_READID
// TODO: protect it with mutex and while(dma) if you want to call it from other places (e.g. DEBUG)
void lcd_read_display_id(int mpuiface, uint8_t *lcd_id)
{
    s5l_lcd_write_config(
            (mpuiface == LCD_MPUIFACE_SERIAL) ? LCD_MODE_S8 : LCD_MODE_P8);
    s5l_lcd_recv_cmd8(4, 4, lcd_id);
}
#endif

/* LCD init */
void lcd_init_device(void)
{
    mutex_init(&lcd_mutex);

    lcd_target_enable_clocks(true);
#if defined(IPOD_6G) || defined(IPOD_NANO3G)
    LCD_PHTIME = 0x33;
#elif defined(IPOD_NANO4G) && defined(BOOTLOADER)
    cg16_config(&CG16_LCD, true, CG16_SEL_PLL0, 16, 1, 0x0);
    s5l_lcd_write_config(LCD_MODE_S9);
    cg16_config(&CG16_LCD, false, CG16_SEL_PLL0, 16, 1, 0x0);
    LCD_PHTIME = 0x11;
#endif

    lcd_info = lcd_target_get_info();
    lcd_type = lcd_info->lcd_type;

    /* select mode to send commands */
#ifdef S5L_LCD_WITH_CMDSET16
    if (lcd_info->cmdset == LCD_CMDSET_16BIT)
    {
        lcd_cmd_mode = LCD_MODE_P18;
        lcd_run_seq = s5l_lcd_run_seq16;
    }
    else /* LCD_CMDSET_8BIT */
#endif
    {
        if (lcd_info->mpuiface == LCD_MPUIFACE_SERIAL)
            lcd_cmd_mode = LCD_MODE_S8;
        else
            lcd_cmd_mode = LCD_MODE_P8;
        lcd_run_seq = s5l_lcd_run_seq8;
    }

    /* select mode to send RGB565 data */
    if (lcd_info->mpuiface == LCD_MPUIFACE_PAR9)
        lcd_frame_mode = LCD_MODE_P9;
    else if (lcd_info->mpuiface == LCD_MPUIFACE_PAR18)
        lcd_frame_mode = LCD_MODE_P16;
    else /* LCD_MPUIFACE_SERIAL */
        lcd_frame_mode = LCD_MODE_S9;

    s5l_lcd_set_command_mode();

    /* Configure DMA channel */                             // TODO: this right after mutex_init()
    dmac_ch_init(&lcd_dma_ch, &lcd_dma_ch_cfg);

#ifdef BOOTLOADER
    lcd_run_seq(lcd_info->seq_init);
#endif
#ifdef IPOD_6G
    if (lcd_info->seq_frame_sync != NULL)
    {
        lcd_run_seq(lcd_info->seq_frame_sync);
        displaylcd_reset_frame_sync();
        displaylcd_probe_te_inputs();
    }
#endif
    lcd_ispowered = true;
}
