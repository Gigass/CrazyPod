#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "crazypod_coverflow_motion.h"

#define ONE CRAZYPOD_FLOW_ONE
#define FRAME_USEC 20000u

static struct crazypod_flow_motion m;

static void run(uint32_t usec)
{
    while(usec >= FRAME_USEC) {
        crazypod_flow_motion_advance(&m, FRAME_USEC);
        usec -= FRAME_USEC;
    }
}

/* Spin the wheel at a constant speed, advancing the model every frame. */
static uint32_t spin(uint32_t now, int clicks_per_second, int frames)
{
    int frame;
    int carry = 0;

    for(frame = 0; frame < frames; ++frame) {
        int whole;

        carry += clicks_per_second * (int)(FRAME_USEC / 1000);
        whole = carry / 1000;
        carry -= whole * 1000;
        now += FRAME_USEC;
        if(whole != 0)
            crazypod_flow_motion_wheel(&m, now, whole);
        crazypod_flow_motion_advance(&m, FRAME_USEC);
    }
    return now;
}

static int32_t settle(void)
{
    int guard;

    for(guard = 0; guard < 1000 && crazypod_flow_motion_moving(&m);
        ++guard)
        crazypod_flow_motion_advance(&m, FRAME_USEC);
    return m.position_q16;
}

static void test_gain_curve(void)
{
    int32_t previous = ONE;
    int speed;

    assert(crazypod_flow_motion_gain_q16(0) == ONE);
    assert(crazypod_flow_motion_gain_q16(24) == ONE);
    assert(crazypod_flow_motion_gain_q16(96) == 2 * ONE);
    assert(crazypod_flow_motion_gain_q16(5000) == 2 * ONE);
    for(speed = 24; speed <= 96; ++speed) {
        int32_t gain = crazypod_flow_motion_gain_q16(speed);

        assert(gain >= previous);
        previous = gain;
    }
}

static void test_slow_turn_is_exact(void)
{
    uint32_t now = 1000;
    int i;

    crazypod_flow_motion_reset(&m, 10 * ONE, 100 * ONE);
    crazypod_flow_motion_touch(&m, now);
    for(i = 0; i < 12; ++i) {
        now += 200000;
        crazypod_flow_motion_wheel(&m, now, 1);
    }
    /* Twelve clicks are one album, up to fixed-point truncation. */
    assert(labs((long)m.target_q16 - 11 * ONE) <= 16);
}

static void test_start_eases_in(void)
{
    uint32_t now = 1000;
    int32_t first;
    int32_t second;

    crazypod_flow_motion_reset(&m, 10 * ONE, 100 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 60, 1);
    first = m.velocity_q16;
    now = spin(now, 60, 3);
    second = m.velocity_q16;
    /* Velocity ramps up instead of jumping to the wheel speed. */
    assert(first > 0);
    assert(second > first);
}

static void test_release_keeps_velocity_and_coasts(void)
{
    uint32_t now = 1000;
    int32_t before;
    int32_t after_release;
    int32_t start;
    int32_t stop;

    crazypod_flow_motion_reset(&m, 10 * ONE, 100 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 80, 30);
    start = m.position_q16;
    before = m.velocity_q16;
    assert(before > 3 * ONE);
    crazypod_flow_motion_release(&m);
    after_release = m.velocity_q16;
    assert(after_release == before);
    assert(m.mode == CRAZYPOD_FLOW_COAST);

    /* Friction, not a brake: one frame later it is still moving fast. */
    run(FRAME_USEC);
    assert(m.velocity_q16 > before * 9 / 10);
    stop = settle();
    assert(stop % ONE == 0);
    assert(stop > start + ONE);
    assert(m.mode == CRAZYPOD_FLOW_IDLE);
}

static void test_coast_distance_grows_with_speed(void)
{
    uint32_t now = 1000;
    int32_t slow_stop;
    int32_t fast_stop;

    crazypod_flow_motion_reset(&m, 10 * ONE, 500 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 40, 30);
    crazypod_flow_motion_release(&m);
    slow_stop = settle() - 10 * ONE;

    crazypod_flow_motion_reset(&m, 10 * ONE, 500 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 120, 30);
    crazypod_flow_motion_release(&m);
    fast_stop = settle() - 10 * ONE;
    assert(fast_stop > slow_stop);
}

static void test_monotonic_and_no_reverse(void)
{
    uint32_t now = 1000;
    int32_t previous;
    int guard;

    crazypod_flow_motion_reset(&m, 10 * ONE, 500 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 100, 30);
    crazypod_flow_motion_release(&m);
    previous = m.position_q16;
    for(guard = 0; guard < 1000 && crazypod_flow_motion_moving(&m);
        ++guard) {
        crazypod_flow_motion_advance(&m, FRAME_USEC);
        assert(m.velocity_q16 >= 0);
        assert(m.position_q16 >= previous);
        previous = m.position_q16;
    }
    assert(!crazypod_flow_motion_moving(&m));
}

static void test_slow_release_snaps_softly(void)
{
    uint32_t now = 1000;
    int32_t start;
    int guard;
    int32_t previous;

    crazypod_flow_motion_reset(&m, 10 * ONE, 100 * ONE);
    crazypod_flow_motion_touch(&m, now);
    crazypod_flow_motion_wheel(&m, now + 10000, 5);
    run(200000);
    start = m.position_q16;
    assert(start > 10 * ONE && start < 11 * ONE);
    crazypod_flow_motion_release(&m);
    assert(m.mode == CRAZYPOD_FLOW_SNAP);
    previous = start;
    for(guard = 0; guard < 500 && crazypod_flow_motion_moving(&m);
        ++guard) {
        crazypod_flow_motion_advance(&m, FRAME_USEC);
        assert(m.position_q16 >= previous - 2);
        previous = m.position_q16;
    }
    assert(m.position_q16 == 10 * ONE || m.position_q16 == 11 * ONE);
}

static void test_pause_before_lift_does_not_fling(void)
{
    uint32_t now = 1000;

    crazypod_flow_motion_reset(&m, 10 * ONE, 500 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 100, 30);
    /* Finger rests on the wheel for a while. */
    run(400000);
    assert(m.velocity_q16 < ONE);
    crazypod_flow_motion_release(&m);
    assert(m.mode == CRAZYPOD_FLOW_SNAP);
}

static void test_limits(void)
{
    uint32_t now = 1000;

    crazypod_flow_motion_reset(&m, 2 * ONE, 5 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 200, 60);
    crazypod_flow_motion_release(&m);
    assert(settle() == 5 * ONE);
    assert(m.position_q16 <= m.max_position_q16);

    crazypod_flow_motion_touch(&m, now);
    now = spin(now, -200, 120);
    crazypod_flow_motion_release(&m);
    assert(settle() == 0);
}

static void test_step_eases_in_and_out(void)
{
    int32_t first_frame;
    int32_t peak = 0;
    int guard;

    crazypod_flow_motion_reset(&m, 10 * ONE, 100 * ONE);
    crazypod_flow_motion_step_to(&m, 11);
    crazypod_flow_motion_advance(&m, FRAME_USEC);
    first_frame = m.velocity_q16;
    /* Ease-in: no instant kick. */
    assert(first_frame < ONE);
    for(guard = 0; guard < 500 && crazypod_flow_motion_moving(&m);
        ++guard) {
        crazypod_flow_motion_advance(&m, FRAME_USEC);
        if(m.velocity_q16 > peak)
            peak = m.velocity_q16;
        assert(m.position_q16 <= 11 * ONE);
    }
    assert(peak > first_frame);
    assert(m.position_q16 == 11 * ONE);
    assert(m.mode == CRAZYPOD_FLOW_IDLE);
}

static void test_touch_stops_coast_smoothly(void)
{
    uint32_t now = 1000;
    int guard;

    crazypod_flow_motion_reset(&m, 10 * ONE, 500 * ONE);
    crazypod_flow_motion_touch(&m, now);
    now = spin(now, 100, 30);
    crazypod_flow_motion_release(&m);
    run(100000);
    crazypod_flow_motion_touch(&m, now + 100000);
    for(guard = 0; guard < 100; ++guard)
        crazypod_flow_motion_advance(&m, FRAME_USEC);
    assert(m.velocity_q16 < ONE / 8 && m.velocity_q16 > -ONE / 8);
    assert(!crazypod_flow_motion_moving(&m) ||
           m.mode == CRAZYPOD_FLOW_TRACK);
}

int main(void)
{
    test_gain_curve();
    test_slow_turn_is_exact();
    test_start_eases_in();
    test_release_keeps_velocity_and_coasts();
    test_coast_distance_grows_with_speed();
    test_monotonic_and_no_reverse();
    test_slow_release_snaps_softly();
    test_pause_before_lift_does_not_fling();
    test_limits();
    test_step_eases_in_and_out();
    test_touch_stops_coast_smoothly();
    puts("crazypod_coverflow_motion_host_test: ok");
    return 0;
}
