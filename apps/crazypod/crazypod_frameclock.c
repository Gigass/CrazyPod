#include "config.h"

#ifdef IPOD_6G

#include "kernel.h"
#include "lcd.h"

#include "crazypod_frameclock.h"

static struct crazypod_frameclock present_clock;
static bool present_initialized;
static bool present_pending;
static int present_x1;
static int present_y1;
static int present_x2;
static int present_y2;

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

void crazypod_present_init(long now)
{
    crazypod_frameclock_reset(&present_clock, now);
    present_initialized = true;
    present_pending = false;
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
    if(!present_pending)
        return;

    lcd_update_rect(present_x1, present_y1,
                    present_x2 - present_x1 + 1,
                    present_y2 - present_y1 + 1);
    present_pending = false;
}

void crazypod_present_tick(void)
{
    long now = current_tick;

    if(!present_pending ||
       !crazypod_frameclock_due(&present_clock, now))
        return;

    crazypod_present_now();
    crazypod_frameclock_schedule_next(&present_clock, now);
}

#endif
