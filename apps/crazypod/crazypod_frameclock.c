#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "kernel.h"
#include "lcd.h"
#include "system.h"

#include "crazypod_frameclock.h"

#ifdef SIMULATOR
#include "crc32.h"

extern struct frame_buffer_t lcd_framebuffer_default;
#endif

static struct crazypod_frameclock present_clock;
static bool present_initialized;
static bool present_pending;
static int present_x1;
static int present_y1;
static int present_x2;
static int present_y2;
static uint32_t present_sequence;
static long present_tick;
static struct crazypod_present_diagnostics present_diagnostics;
#ifdef SIMULATOR
static uint32_t simulator_present_crc;
#endif

void crazypod_frameclock_reset(struct crazypod_frameclock *clock, long now)
{
    clock->next_tick = now;
    clock->tick_error = 0;
}

bool crazypod_frameclock_due(const struct crazypod_frameclock *clock, long now)
{
    return !TIME_BEFORE(now, clock->next_tick);
}

void crazypod_frameclock_schedule_next(struct crazypod_frameclock *clock,
                                       long now)
{
    do {
        int interval = HZ / CRAZYPOD_TARGET_FPS;

        clock->tick_error += HZ % CRAZYPOD_TARGET_FPS;
        if(clock->tick_error >= CRAZYPOD_TARGET_FPS) {
            ++interval;
            clock->tick_error -= CRAZYPOD_TARGET_FPS;
        }
        if(interval < 1)
            interval = 1;
        clock->next_tick += interval;
    } while(!TIME_BEFORE(now, clock->next_tick));
}

uint32_t crazypod_monotonic_usec(void)
{
#ifdef SIMULATOR
    return (uint32_t)(((uint64_t)(uint32_t)current_tick * 1000000u) / HZ);
#else
    return (uint32_t)USEC_TIMER;
#endif
}

void crazypod_present_init(long now)
{
    crazypod_frameclock_reset(&present_clock, now);
    present_initialized = true;
    present_pending = false;
    memset(&present_diagnostics, 0, sizeof(present_diagnostics));
}

void crazypod_present_queue_rect(int x, int y, int width, int height)
{
    int x2;
    int y2;

    if(!present_initialized)
        crazypod_present_init(current_tick);
    if(width <= 0 || height <= 0)
        return;
    if(x < 0) {
        width += x;
        x = 0;
    }
    if(y < 0) {
        height += y;
        y = 0;
    }
    if(x >= LCD_WIDTH || y >= LCD_HEIGHT || width <= 0 || height <= 0)
        return;
    if(x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if(y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - y;

    ++present_diagnostics.queue_requests;
    x2 = x + width - 1;
    y2 = y + height - 1;
    if(!present_pending) {
        present_x1 = x;
        present_y1 = y;
        present_x2 = x2;
        present_y2 = y2;
        present_pending = true;
        return;
    }
    ++present_diagnostics.coalesced_requests;
    if(x < present_x1)
        present_x1 = x;
    if(y < present_y1)
        present_y1 = y;
    if(x2 > present_x2)
        present_x2 = x2;
    if(y2 > present_y2)
        present_y2 = y2;
}

void crazypod_present_queue_full(void)
{
    crazypod_present_queue_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

void crazypod_present_now(void)
{
    uint32_t duration_us;
    uint32_t started_us;
    bool full;

    if(!present_pending)
        return;

    full = present_x1 == 0 && present_y1 == 0 &&
        present_x2 == LCD_WIDTH - 1 &&
        present_y2 == LCD_HEIGHT - 1;
    started_us = crazypod_monotonic_usec();
    lcd_update_rect(present_x1, present_y1,
                    present_x2 - present_x1 + 1,
                    present_y2 - present_y1 + 1);
    duration_us = crazypod_monotonic_usec() - started_us;
    present_pending = false;
    ++present_sequence;
    present_tick = current_tick;
    ++present_diagnostics.presents;
    if(full)
        ++present_diagnostics.full_presents;
    present_diagnostics.last_present_us = duration_us;
    if(duration_us > present_diagnostics.max_present_us)
        present_diagnostics.max_present_us = duration_us;
    if(duration_us > CRAZYPOD_FRAME_BUDGET_US)
        ++present_diagnostics.present_timeouts;
#ifdef SIMULATOR
    simulator_present_crc = crc_32(
        lcd_framebuffer_default.data,
        (uint32_t)(
            lcd_framebuffer_default.elems * sizeof(fb_data)),
        0xffffffffu);
#endif
}

void crazypod_present_tick(void)
{
    long now = current_tick;
    uint32_t late_ticks;

    if(!present_pending ||
       !crazypod_frameclock_due(&present_clock, now))
        return;

    if(TIME_AFTER(now, present_clock.next_tick)) {
        late_ticks = (uint32_t)(now - present_clock.next_tick);
        ++present_diagnostics.deadline_misses;
        if(late_ticks > present_diagnostics.max_late_ticks)
            present_diagnostics.max_late_ticks = late_ticks;
    }
    crazypod_present_now();
    crazypod_frameclock_schedule_next(&present_clock, now);
}

uint32_t crazypod_present_sequence(void)
{
    return present_sequence;
}

long crazypod_present_last_tick(void)
{
    return present_tick;
}

void crazypod_present_note_render(
    enum crazypod_render_source source, uint32_t duration_us)
{
    if(source == CRAZYPOD_RENDER_HOME) {
        ++present_diagnostics.home_renders;
        present_diagnostics.last_home_render_us = duration_us;
        if(duration_us > present_diagnostics.max_home_render_us)
            present_diagnostics.max_home_render_us = duration_us;
        if(duration_us > CRAZYPOD_FRAME_BUDGET_US)
            ++present_diagnostics.home_render_timeouts;
    }
    else if(source == CRAZYPOD_RENDER_MUSIC) {
        ++present_diagnostics.music_renders;
        present_diagnostics.last_music_render_us = duration_us;
        if(duration_us > present_diagnostics.max_music_render_us)
            present_diagnostics.max_music_render_us = duration_us;
        if(duration_us > CRAZYPOD_FRAME_BUDGET_US)
            ++present_diagnostics.music_render_timeouts;
    }
}

void crazypod_present_get_diagnostics(
    struct crazypod_present_diagnostics *diagnostics)
{
    if(diagnostics != NULL)
        *diagnostics = present_diagnostics;
}

#ifdef SIMULATOR
uint32_t crazypod_present_framebuffer_crc(void)
{
    return simulator_present_crc;
}
#endif

#endif
