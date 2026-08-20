#include <assert.h>
#include <stdint.h>

#include "crazypod_frameclock.h"

long test_current_tick;
uint32_t test_usec_timer;

static uint32_t lcd_duration_us;
static int lcd_calls;
static int lcd_x;
static int lcd_y;
static int lcd_width;
static int lcd_height;

void lcd_update_rect(int x, int y, int width, int height)
{
    ++lcd_calls;
    lcd_x = x;
    lcd_y = y;
    lcd_width = width;
    lcd_height = height;
    test_usec_timer += lcd_duration_us;
}

static void reset_lcd(void)
{
    lcd_duration_us = 0;
    lcd_calls = 0;
    lcd_x = 0;
    lcd_y = 0;
    lcd_width = 0;
    lcd_height = 0;
    test_current_tick = 0;
    test_usec_timer = 0;
}

static void test_frameclock_cadence(void)
{
    struct crazypod_frameclock clock;

    crazypod_frameclock_reset(&clock, 0);
    assert(crazypod_frameclock_due(&clock, 0));
    crazypod_frameclock_schedule_next(&clock, 0);
    assert(clock.next_tick == 1);
    assert(crazypod_frameclock_due(&clock, 1));
}

static void test_present_coalesces_to_full_frame(void)
{
    struct crazypod_present_diagnostics diagnostics;

    reset_lcd();
    lcd_duration_us = 1000;
    crazypod_present_init(0);
    crazypod_present_queue_rect(10, 20, 30, 40);
    crazypod_present_queue_rect(5, 15, 5, 5);
    crazypod_present_queue_full();
    crazypod_present_now();

    assert(lcd_calls == 1);
    assert(lcd_x == 0);
    assert(lcd_y == 0);
    assert(lcd_width == LCD_WIDTH);
    assert(lcd_height == LCD_HEIGHT);
    crazypod_present_get_diagnostics(&diagnostics);
    assert(diagnostics.queue_requests == 3);
    assert(diagnostics.coalesced_requests == 2);
    assert(diagnostics.presents == 1);
    assert(diagnostics.full_presents == 1);
    assert(diagnostics.last_present_us == 1000);
    assert(diagnostics.max_present_us == 1000);
    assert(diagnostics.present_timeouts == 0);
}

static void test_present_deadline_and_timeout_diagnostics(void)
{
    struct crazypod_present_diagnostics diagnostics;

    reset_lcd();
    crazypod_present_init(0);
    crazypod_present_queue_rect(0, 0, LCD_WIDTH, LCD_HEIGHT - 1);
    crazypod_present_tick();

    lcd_duration_us = CRAZYPOD_FRAME_BUDGET_US + 1;
    test_current_tick = 2;
    crazypod_present_queue_rect(0, 0, LCD_WIDTH, LCD_HEIGHT - 1);
    crazypod_present_tick();

    crazypod_present_note_render(
        CRAZYPOD_RENDER_HOME, CRAZYPOD_FRAME_BUDGET_US + 1);
    crazypod_present_note_render(CRAZYPOD_RENDER_MUSIC, 5000);
    crazypod_present_get_diagnostics(&diagnostics);
    assert(diagnostics.deadline_misses == 1);
    assert(diagnostics.max_late_ticks == 1);
    assert(diagnostics.present_timeouts == 1);
    assert(diagnostics.home_renders == 1);
    assert(diagnostics.home_render_timeouts == 1);
    assert(diagnostics.music_renders == 1);
    assert(diagnostics.music_render_timeouts == 0);
}

static void test_full_frame_bypasses_software_gate(void)
{
    reset_lcd();
    crazypod_present_init(0);

    crazypod_present_queue_full();
    crazypod_present_tick();
    assert(lcd_calls == 1);

    crazypod_present_queue_full();
    crazypod_present_tick();
    assert(lcd_calls == 2);
}

int main(void)
{
    test_frameclock_cadence();
    test_present_coalesces_to_full_frame();
    test_present_deadline_and_timeout_diagnostics();
    test_full_frame_bypasses_software_gate();
    return 0;
}
