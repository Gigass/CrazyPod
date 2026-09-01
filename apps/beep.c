/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (c) 2011 Michael Sevakis
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
#include "config.h"
#include "system.h"
#include "settings.h"
#include "pcm.h"
#include "pcm_mixer.h"
#include "misc.h"
#include "fixedpoint.h"

/** Beep generation, CPU optimized **/
#include "asm/beep.c"

static uint32_t beep_phase;     /* Phase of square wave generator */
static uint32_t beep_step;      /* Step of square wave generator on each sample */
#ifdef BEEP_GENERIC
static int16_t  beep_amplitude; /* Amplitude of square wave generator */
#else
/* Optimized routines do XOR with phase sign bit in both channels at once */
static uint32_t beep_amplitude; /* Amplitude of square wave generator */
#endif
static int beep_count;          /* Number of samples remaining to generate */

#define BEEP_COUNT(fs, duration) ((fs) / 1000 * (duration))

/* Reserve enough static space for keyclick to fit in worst case */
#define BEEP_BUF_COUNT  BEEP_COUNT(PLAY_SAMPR_MAX, KEYCLICK_DURATION)
static int16_t beep_buf[BEEP_BUF_COUNT*2] IBSS_ATTR __attribute__((aligned(4)));

static const int16_t *pcm_effect_samples;
static size_t pcm_effect_frame_count;
static uint32_t pcm_effect_phase;
static uint32_t pcm_effect_step;
static int16_t pcm_effect_buf[MIX_FRAME_SAMPLES * 2]
    IBSS_ATTR __attribute__((aligned(4)));

/* Callback to generate the beep frames - also don't want inlining of
   call below in beep_play */
static void __attribute__((noinline))
beep_get_more(const void **start, size_t *size)
{
    int count = beep_count;

    if (count > 0)
    {
        count = MIN(count, BEEP_BUF_COUNT);
        beep_count -= count;
        *start = beep_buf;
        *size = count * 2 * sizeof (int16_t);
        beep_generate((void *)beep_buf, count, &beep_phase,
                      beep_step, beep_amplitude);
    }
}

/* Generates a constant square wave sound with a given frequency in Hertz for
   a duration in milliseconds */
void beep_play(unsigned int frequency, unsigned int duration,
               unsigned int amplitude)
{
    mixer_channel_stop(PCM_MIXER_CHAN_BEEP);

    if (frequency == 0 || duration == 0 || amplitude == 0)
        return;

    if (amplitude > INT16_MAX)
        amplitude = INT16_MAX;

    /* Setup the parameters for the square wave generator */
    uint32_t fout = mixer_get_frequency();
    beep_phase = 0;
    beep_step = fp_div(frequency, fout, 32);
    beep_count = BEEP_COUNT(fout, duration);

#ifdef BEEP_GENERIC
    beep_amplitude = amplitude;
#else
    /* Optimized routines do XOR with phase sign bit in both channels at once */
    beep_amplitude = amplitude | (amplitude << 16); /* Word:|AMP16|AMP16| */
#endif

    /* If it fits - avoid cb overhead */
    const void *start;
    size_t size;

    /* Generate first frame here */
    beep_get_more(&start, &size);

    mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, MIX_AMP_UNITY);
    static struct mixer_play_cbs cbs;
    cbs.get_more = beep_count ? beep_get_more : NULL;
    mixer_channel_play_data(PCM_MIXER_CHAN_BEEP, &cbs, start, size);
}

static void pcm_effect_get_more(const void **start, size_t *size)
{
    int frames = 0;

    while(frames < MIX_FRAME_SAMPLES) {
        uint32_t index = pcm_effect_phase >> 15;
        uint32_t fraction = pcm_effect_phase & 0x7fffu;
        int32_t first;
        int32_t second;
        int16_t sample;

        if(index >= pcm_effect_frame_count)
            break;
        first = pcm_effect_samples[index];
        second = index + 1 < pcm_effect_frame_count
            ? pcm_effect_samples[index + 1] : first;
        sample = (int16_t)(
            first + ((second - first) * (int32_t)fraction >> 15));
        pcm_effect_buf[frames * 2] = sample;
        pcm_effect_buf[frames * 2 + 1] = sample;
        pcm_effect_phase += pcm_effect_step;
        ++frames;
    }

    if(frames <= 0) {
        *start = NULL;
        *size = 0;
        return;
    }
    *start = pcm_effect_buf;
    *size = (size_t)frames * 2 * sizeof(int16_t);
}

void beep_play_pcm(const int16_t *samples, size_t frame_count,
                   unsigned int sample_rate,
                   unsigned int mixer_amplitude)
{
    static struct mixer_play_cbs callbacks;
    const void *start = NULL;
    size_t size = 0;
    unsigned int output_rate;

    mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    if(samples == NULL || frame_count == 0 ||
       sample_rate == 0 || mixer_amplitude == 0)
        return;

    output_rate = mixer_get_frequency();
    if(output_rate == 0)
        return;
    pcm_effect_samples = samples;
    pcm_effect_frame_count = frame_count;
    pcm_effect_phase = 0;
    pcm_effect_step = (uint32_t)(
        ((uint64_t)sample_rate << 15) / output_rate);
    if(pcm_effect_step == 0)
        pcm_effect_step = 1;

    pcm_effect_get_more(&start, &size);
    mixer_channel_set_amplitude(
        PCM_MIXER_CHAN_BEEP, mixer_amplitude);
    callbacks.get_more =
        (pcm_effect_phase >> 15) < pcm_effect_frame_count
            ? pcm_effect_get_more : NULL;
    mixer_channel_play_data(
        PCM_MIXER_CHAN_BEEP, &callbacks, start, size);
}
