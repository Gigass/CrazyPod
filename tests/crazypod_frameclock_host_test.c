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
static int lcd_sync_mode;
static bool lcd_sync_submit_succeeds;

static void record_lcd_update(
    int x, int y, int width, int height, int sync_mode)
{
    ++lcd_calls;
    lcd_x = x;
    lcd_y = y;
    lcd_width = width;
    lcd_height = height;
    lcd_sync_mode = sync_mode;
    test_usec_timer += lcd_duration_us;
}

void lcd_update_rect(int x, int y, int width, int height)
{
    record_lcd_update(x, y, width, height, 0);
}

bool lcd_update_rect_frame_sync(
    int x, int y, int width, int height)
{
    record_lcd_update(x, y, width, height, 1);
    return lcd_sync_submit_succeeds;
}

bool lcd_update_rect_music_sync(
    int x, int y, int width, int height)
{
    record_lcd_update(x, y, width, height, 2);
    return lcd_sync_submit_succeeds;
}

bool lcd_update_full_sync(void)
{
    record_lcd_update(0, 0, LCD_WIDTH, LCD_HEIGHT, 0);
    return lcd_sync_submit_succeeds;
}

static void reset_lcd(void)
{
    lcd_duration_us = 0;
    lcd_calls = 0;
    lcd_x = 0;
    lcd_y = 0;
    lcd_width = 0;
    lcd_height = 0;
    lcd_sync_mode = 0;
    lcd_sync_submit_succeeds = true;
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

static void test_partial_sync_routes(void)
{
    reset_lcd();
    crazypod_present_init(0);

    crazypod_present_queue_home_rect(0, 20, LCD_WIDTH, 100);
    crazypod_present_now();
    assert(lcd_calls == 1);
    assert(lcd_sync_mode == 1);

    crazypod_present_queue_music_rect(0, 40, LCD_WIDTH, 154);
    crazypod_present_now();
    assert(lcd_calls == 2);
    assert(lcd_sync_mode == 2);
}

static void test_home_sync_does_not_merge_playback_capsule(void)
{
    reset_lcd();
    crazypod_present_init(0);

    /* LVGL flushes before the native Home renderer in the UI loop. */
    crazypod_present_queue_rect(261, 186, 42, 42);
    crazypod_present_queue_home_rect(0, 40, LCD_WIDTH, 103);
    crazypod_present_now();

    assert(lcd_calls == 1);
    assert(lcd_sync_mode == 1);
    assert(lcd_x == 0);
    assert(lcd_y == 40);
    assert(lcd_width == LCD_WIDTH);
    assert(lcd_height == 103);

    /* The capsule remains independent and is committed afterward. */
    crazypod_present_now();
    assert(lcd_calls == 2);
    assert(lcd_sync_mode == 0);
    assert(lcd_x == 261);
    assert(lcd_y == 186);
    assert(lcd_width == 42);
    assert(lcd_height == 42);
}

static void test_home_motion_keeps_priority_over_deferred_lvgl(void)
{
    reset_lcd();
    crazypod_present_init(0);

    crazypod_present_queue_rect(261, 186, 42, 42);
    crazypod_present_queue_home_rect(0, 40, LCD_WIDTH, 103);
    crazypod_present_now();
    assert(lcd_sync_mode == 1);

    /* A new motion frame arrives while the capsule is still deferred. */
    crazypod_present_queue_home_rect(0, 40, LCD_WIDTH, 103);
    crazypod_present_now();
    assert(lcd_calls == 2);
    assert(lcd_sync_mode == 1);

    crazypod_present_now();
    assert(lcd_calls == 3);
    assert(lcd_sync_mode == 0);
    assert(lcd_x == 261);
    assert(lcd_y == 186);
}

static void test_full_frame_cannot_discard_home_sync(void)
{
    reset_lcd();
    crazypod_present_init(0);

    crazypod_present_queue_home_rect(0, 40, LCD_WIDTH, 103);
    crazypod_present_queue_full();
    crazypod_present_now();
    assert(lcd_calls == 1);
    assert(lcd_sync_mode == 1);

    crazypod_present_now();
    assert(lcd_calls == 2);
    assert(lcd_sync_mode == 0);
    assert(lcd_width == LCD_WIDTH);
    assert(lcd_height == LCD_HEIGHT);

    reset_lcd();
    crazypod_present_init(0);
    crazypod_present_queue_full();
    crazypod_present_queue_home_rect(0, 40, LCD_WIDTH, 103);
    crazypod_present_now();
    assert(lcd_calls == 1);
    assert(lcd_sync_mode == 1);
    crazypod_present_now();
    assert(lcd_calls == 2);
    assert(lcd_sync_mode == 0);
}

static void test_failed_home_sync_retries_without_consuming_frame(void)
{
    struct crazypod_present_diagnostics diagnostics;
    uint32_t sequence;

    reset_lcd();
    crazypod_present_init(0);
    sequence = crazypod_present_sequence();
    lcd_sync_submit_succeeds = false;
    crazypod_present_queue_home_rect(0, 40, LCD_WIDTH, 103);
    crazypod_present_tick();

    assert(lcd_calls == 1);
    assert(crazypod_present_sequence() == sequence);
    crazypod_present_get_diagnostics(&diagnostics);
    assert(diagnostics.presents == 0);
    assert(diagnostics.sync_submit_failures == 1);

    lcd_sync_submit_succeeds = true;
    test_current_tick = 1;
    crazypod_present_tick();
    assert(lcd_calls == 2);
    assert(lcd_sync_mode == 1);
    assert(crazypod_present_sequence() == sequence + 1);
    crazypod_present_get_diagnostics(&diagnostics);
    assert(diagnostics.presents == 1);
    assert(diagnostics.sync_submit_failures == 1);
}

static void test_failed_full_sync_retries_without_consuming_frame(void)
{
    struct crazypod_present_diagnostics diagnostics;
    uint32_t sequence;

    reset_lcd();
    crazypod_present_init(0);
    sequence = crazypod_present_sequence();
    lcd_sync_submit_succeeds = false;
    crazypod_present_queue_full();
    crazypod_present_tick();

    assert(lcd_calls == 1);
    assert(crazypod_present_sequence() == sequence);
    crazypod_present_get_diagnostics(&diagnostics);
    assert(diagnostics.presents == 0);
    assert(diagnostics.sync_submit_failures == 1);

    lcd_sync_submit_succeeds = true;
    test_current_tick = 1;
    crazypod_present_tick();
    assert(lcd_calls == 2);
    assert(crazypod_present_sequence() == sequence + 1);
    crazypod_present_get_diagnostics(&diagnostics);
    assert(diagnostics.presents == 1);
    assert(diagnostics.full_presents == 1);
    assert(diagnostics.sync_submit_failures == 1);
}

static void test_home_touch_defers_ordinary_lvgl_until_release(void)
{
    reset_lcd();
    crazypod_present_init(0);
    crazypod_present_set_home_interaction(true);

    crazypod_present_queue_rect(70, 225, 100, 3);
    crazypod_present_tick();
    assert(lcd_calls == 0);

    crazypod_present_queue_home_rect(0, 40, LCD_WIDTH, 103);
    crazypod_present_tick();
    assert(lcd_calls == 1);
    assert(lcd_sync_mode == 1);

    test_current_tick = 1;
    crazypod_present_tick();
    assert(lcd_calls == 1);

    crazypod_present_set_home_interaction(false);
    crazypod_present_tick();
    assert(lcd_calls == 2);
    assert(lcd_sync_mode == 0);
    assert(lcd_x == 70);
    assert(lcd_y == 225);
    assert(lcd_width == 100);
    assert(lcd_height == 3);
}

int main(void)
{
    test_frameclock_cadence();
    test_present_coalesces_to_full_frame();
    test_present_deadline_and_timeout_diagnostics();
    test_full_frame_bypasses_software_gate();
    test_partial_sync_routes();
    test_home_sync_does_not_merge_playback_capsule();
    test_home_motion_keeps_priority_over_deferred_lvgl();
    test_full_frame_cannot_discard_home_sync();
    test_failed_home_sync_retries_without_consuming_frame();
    test_failed_full_sync_retries_without_consuming_frame();
    test_home_touch_defers_ordinary_lvgl_until_release();
    return 0;
}
