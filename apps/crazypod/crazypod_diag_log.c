#include "crazypod_diag_log.h"

#ifdef CRAZYPOD_DIAG_LOG

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "kernel.h"

#define DIAG_LOG_PATH "/.crazypod/diag.log"
/* Small enough to paste into a message, large enough for a session. */
#define DIAG_LOG_LIMIT (48 * 1024)

void crazypod_diag_log(const char *tag, const char *format, ...)
{
    char line[192];
    va_list arguments;
    int prefix;
    int written;
    int fd;

    if(tag == NULL || format == NULL)
        return;
    if(!dir_exists("/.crazypod") && mkdir("/.crazypod") < 0)
        return;
    prefix = snprintf(line, sizeof(line), "%ld %s ",
                      (long)(current_tick / HZ), tag);
    if(prefix < 0 || (size_t)prefix >= sizeof(line))
        return;
    va_start(arguments, format);
    written = vsnprintf(line + prefix, sizeof(line) - prefix - 1,
                        format, arguments);
    va_end(arguments);
    if(written < 0)
        return;
    written += prefix;
    if((size_t)written > sizeof(line) - 2)
        written = (int)sizeof(line) - 2;
    line[written++] = '\n';

    /* Start over rather than grow without bound: the interesting lines
     * are the ones from the run the owner is about to report. */
    fd = open(DIAG_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(fd < 0)
        return;
    if(filesize(fd) > DIAG_LOG_LIMIT) {
        close(fd);
        fd = open(DIAG_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd < 0)
            return;
    }
    write(fd, line, (size_t)written);
    close(fd);
}

#endif
