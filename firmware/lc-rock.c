/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2010 by Thomas Martitz
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
#include <stdint.h>
#include "system.h"
#include "kernel.h"
#include "file.h"
#include "debug.h"
#include "load_code.h"

#define LC_READ_CHUNK_SIZE (16u * 1024u)

/* load binary blob from disk to memory, returning a handle */
void * lc_open(const char *filename, unsigned char *buf, size_t buf_size)
{
    int fd = open(filename, O_RDONLY);
    struct lc_header hdr;
    off_t file_size;
    uintptr_t buffer_start;
    uintptr_t buffer_end;
    uintptr_t load_start;
    uintptr_t image_end;
    size_t image_span;
    size_t disk_span;
    size_t required_span;
    size_t total_read;

    if (fd < 0 || buf == NULL || buf_size < sizeof(hdr))
    {
        DEBUGF("Could not open file");
        goto error;
    }

#if NUM_CORES > 1
    /* Make sure COP cache is flushed and invalidated before loading */
    {
        int my_core = switch_core(CURRENT_CORE ^ 1);
        switch_core(my_core);
    }
#endif

    /* read the header to obtain the load address */
    if (read(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr))
    {
        DEBUGF("Could not read binary header");
        goto error_fd;
    }

    file_size = filesize(fd);
    if (file_size < (off_t)sizeof(hdr) ||
        (uintmax_t)file_size > (uintmax_t)SIZE_MAX)
    {
        DEBUGF("Invalid binary size");
        goto error_fd;
    }

    buffer_start = (uintptr_t)buf;
    if (buf_size > UINTPTR_MAX - buffer_start)
        goto error_fd;
    buffer_end = buffer_start + buf_size;
    load_start = (uintptr_t)hdr.load_addr;
    image_end = (uintptr_t)hdr.end_addr;
    if (load_start < buffer_start || load_start >= buffer_end ||
        image_end < load_start || image_end > buffer_end)
    {
        DEBUGF("Invalid binary memory range");
        goto error_fd;
    }

    disk_span = (size_t)file_size;
    image_span = (size_t)(image_end - load_start);
    required_span = MAX(disk_span, image_span);
    if (required_span > (size_t)(buffer_end - load_start))
    {
        DEBUGF("Binary doesn't fit into memory");
        goto error_fd;
    }

    /* go back to beginning to load the whole thing (incl. header) */
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        DEBUGF("lseek failed");
        goto error_fd;
    }

    /* The header has the address where the image is linked. Read only the
     * stored bytes; the caller owns BSS validation and clearing. */
    total_read = 0;
    while (total_read < disk_span)
    {
        size_t remaining = disk_span - total_read;
        size_t amount = MIN(remaining, LC_READ_CHUNK_SIZE);
        ssize_t count = read(fd, hdr.load_addr + total_read,
                             amount);
        if (count <= 0)
        {
            DEBUGF("Short read while loading binary");
            goto error_fd;
        }
        total_read += (size_t)count;
        yield();
    }
    close(fd);

    /* commit dcache and discard icache */
    commit_discard_idcache();
    /* return a pointer the header, reused by lc_get_header() */
    return hdr.load_addr;

error_fd:
    close(fd);
error:
    return NULL;
}
