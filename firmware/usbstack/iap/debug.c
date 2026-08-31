/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2025 by Sho Tanimoto
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "logf.h"
#include "system.h"
#include "tick.h"

void iap_lcd_scatter(const char* fmt, ...) {
    va_list ap;
    char    buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), (char*)fmt, ap);
    va_end(ap);

    logf("%s", buf);
}

static unsigned long timestamp_epoch;

unsigned long iap_debug_timestamp(void) {
    return current_tick - timestamp_epoch;
}

void iap_debug_reset_timestamp(void) {
    timestamp_epoch = current_tick;
}
