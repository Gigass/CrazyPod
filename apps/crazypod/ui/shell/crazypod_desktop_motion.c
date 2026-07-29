#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>

#include "button.h"
#include "kernel.h"

#include "../../crazypod_frameclock.h"
#include "crazypod_desktop_motion.h"

#define DESKTOP_POSITION_ONE (1L << 16)
#define DESKTOP_RELEASE_STIFFNESS 400
#define DESKTOP_RELEASE_DAMPING 40
#define DESKTOP_RELEASE_PROJECTION_TICKS \
    (((HZ * 32) / 100) > 0 ? ((HZ * 32) / 100) : 1)
#define DESKTOP_RELEASE_MAX_SPEED_Q16 \
    (14 * DESKTOP_POSITION_ONE)
#define DESKTOP_SNAP_POSITION (DESKTOP_POSITION_ONE / 512)
#define DESKTOP_SNAP_VELOCITY (DESKTOP_POSITION_ONE / 20)
#define DESKTOP_WHEEL_POSITIONS 96
#define DESKTOP_WHEEL_CLICKS_PER_ITEM 12
#define DESKTOP_WHEEL_FEEDBACK_CLICKS 4
#define DESKTOP_WHEEL_RELEASE_TICKS \
    (((HZ * 6) / 100) > 0 ? ((HZ * 6) / 100) : 1)

static struct crazypod_frameclock motion_clock;
static bool input_enabled;
static bool moving;
static bool render_dirty;
static bool wheel_tracking;
static int wheel_position;
static int wheel_feedback_accumulator;
static int wheel_feedback_direction;
static int motion_direction;
static int gesture_min_item;
static int32_t target_q16;
static int32_t position_q16;
static int32_t velocity_q16;
static long last_physics;
static long wheel_last_seen;
static long wheel_last_motion;

static int clamp_item(int item, int item_count)
{
    if(item < 0)
        return 0;
    if(item >= item_count)
        return item_count > 0 ? item_count - 1 : 0;
    return item;
}

static int center_item(int item_count)
{
    return clamp_item(
        (position_q16 + DESKTOP_POSITION_ONE / 2) >> 16,
        item_count);
}

static int32_t maximum_position(int item_count)
{
    return clamp_item(item_count - 1, item_count) *
        DESKTOP_POSITION_ONE;
}

static int32_t clamp_velocity(int32_t velocity)
{
    if(velocity > DESKTOP_RELEASE_MAX_SPEED_Q16)
        return DESKTOP_RELEASE_MAX_SPEED_Q16;
    if(velocity < -DESKTOP_RELEASE_MAX_SPEED_Q16)
        return -DESKTOP_RELEASE_MAX_SPEED_Q16;
    return velocity;
}

static void stop_wheel_tracking(long now)
{
    wheel_tracking = false;
    wheel_position = -1;
    wheel_last_seen = 0;
    wheel_last_motion = 0;
    last_physics = now;
}

void crazypod_desktop_motion_initialize(long now, int selected)
{
    target_q16 = selected * DESKTOP_POSITION_ONE;
    position_q16 = target_q16;
    velocity_q16 = 0;
    moving = false;
    input_enabled = false;
    render_dirty = false;
    motion_direction = 1;
    gesture_min_item = selected;
    stop_wheel_tracking(now);
    wheel_feedback_accumulator = 0;
    wheel_feedback_direction = 0;
    crazypod_frameclock_reset(&motion_clock, now);
}

void crazypod_desktop_motion_set_input_enabled(
    long now, bool enabled, bool restore_wheel_events,
    int item_count)
{
    int selected;

    if(input_enabled == enabled)
        return;
    input_enabled = enabled;
    wheel_feedback_accumulator = 0;
    wheel_feedback_direction = 0;
    if(enabled) {
        stop_wheel_tracking(now);
#ifdef HAVE_WHEEL_POSITION
        wheel_send_events(false);
#endif
        return;
    }
#ifdef HAVE_WHEEL_POSITION
    if(restore_wheel_events)
        wheel_send_events(true);
#else
    (void)restore_wheel_events;
#endif
    selected = center_item(item_count);
    target_q16 = selected * DESKTOP_POSITION_ONE;
    position_q16 = target_q16;
    velocity_q16 = 0;
    moving = false;
    render_dirty = true;
    stop_wheel_tracking(now);
}

void crazypod_desktop_motion_select(
    long now, int selected, bool animated)
{
    bool was_active = moving || wheel_tracking;

    stop_wheel_tracking(now);
    target_q16 = selected * DESKTOP_POSITION_ONE;
    motion_direction =
        target_q16 < position_q16 ? -1 : 1;
    gesture_min_item = selected;
    if(animated) {
        moving =
            position_q16 != target_q16 ||
            velocity_q16 != 0;
        render_dirty = moving;
        if(!was_active)
            crazypod_frameclock_reset(&motion_clock, now);
        return;
    }
    position_q16 = target_q16;
    velocity_q16 = 0;
    moving = false;
    render_dirty = true;
}

#ifdef HAVE_WHEEL_POSITION
static void release_wheel(long now, int item_count)
{
    int32_t projected_q16;
    int target_item;

    wheel_tracking = false;
    wheel_position = -1;
    projected_q16 =
        position_q16 +
        velocity_q16 *
            DESKTOP_RELEASE_PROJECTION_TICKS / HZ;
    target_item =
        (projected_q16 + DESKTOP_POSITION_ONE / 2) >> 16;
    if(motion_direction > 0) {
        if(target_item < gesture_min_item)
            target_item = gesture_min_item;
    }
    else if(target_item > gesture_min_item) {
        target_item = gesture_min_item;
    }
    target_item = clamp_item(target_item, item_count);
    target_q16 = target_item * DESKTOP_POSITION_ONE;
    moving =
        position_q16 != target_q16 ||
        velocity_q16 != 0;
    render_dirty = true;
    last_physics = now;
}
#endif

static void sample_wheel(long now, int item_count)
{
#ifdef HAVE_WHEEL_POSITION
    int current = wheel_status();
    int32_t maximum_q16 = maximum_position(item_count);

    if(current >= 0) {
        current %= DESKTOP_WHEEL_POSITIONS;
        if(!wheel_tracking) {
            wheel_tracking = true;
            moving = false;
            wheel_position = current;
            wheel_last_motion = now;
            velocity_q16 = 0;
            gesture_min_item = center_item(item_count);
        }
        else {
            int delta = current - wheel_position;

            if(delta < -DESKTOP_WHEEL_POSITIONS / 2)
                delta += DESKTOP_WHEEL_POSITIONS;
            else if(delta > DESKTOP_WHEEL_POSITIONS / 2)
                delta -= DESKTOP_WHEEL_POSITIONS;
            if(delta != 0) {
                long elapsed = now - wheel_last_motion;
                int32_t next_q16;
                int32_t instant_velocity_q16;

                if(elapsed < 1)
                    elapsed = 1;
                if(wheel_feedback_accumulator != 0 &&
                   (wheel_feedback_accumulator < 0) !=
                       (delta < 0))
                    wheel_feedback_accumulator = 0;
                wheel_feedback_accumulator += delta;
                if(wheel_feedback_accumulator >=
                   DESKTOP_WHEEL_FEEDBACK_CLICKS ||
                   wheel_feedback_accumulator <=
                   -DESKTOP_WHEEL_FEEDBACK_CLICKS) {
                    wheel_feedback_direction =
                        wheel_feedback_accumulator < 0 ? -1 : 1;
                    wheel_feedback_accumulator %=
                        DESKTOP_WHEEL_FEEDBACK_CLICKS;
                }
                next_q16 =
                    position_q16 +
                    delta * DESKTOP_POSITION_ONE /
                        DESKTOP_WHEEL_CLICKS_PER_ITEM;
                if(next_q16 < 0)
                    position_q16 = 0;
                else if(next_q16 > maximum_q16)
                    position_q16 = maximum_q16;
                else
                    position_q16 = (int32_t)next_q16;
                instant_velocity_q16 =
                    delta * DESKTOP_POSITION_ONE /
                        DESKTOP_WHEEL_CLICKS_PER_ITEM *
                        HZ / elapsed;
                instant_velocity_q16 =
                    clamp_velocity(instant_velocity_q16);
                if(velocity_q16 != 0 &&
                   (velocity_q16 < 0) ==
                       (instant_velocity_q16 < 0))
                    velocity_q16 =
                        (velocity_q16 * 3 +
                         instant_velocity_q16 * 5) >> 3;
                else
                    velocity_q16 = instant_velocity_q16;
                motion_direction = delta < 0 ? -1 : 1;
                gesture_min_item = center_item(item_count);
                wheel_position = current;
                wheel_last_motion = now;
                render_dirty = true;
            }
        }
        wheel_last_seen = now;
        last_physics = now;
        target_q16 = position_q16;
        return;
    }
    if(wheel_tracking &&
       !TIME_BEFORE(now,
                    wheel_last_seen +
                        DESKTOP_WHEEL_RELEASE_TICKS))
        release_wheel(now, item_count);
#else
    (void)now;
    (void)item_count;
#endif
}

static void advance(long now, int item_count)
{
    long elapsed = now - last_physics;
    int32_t maximum_q16 = maximum_position(item_count);

    if(elapsed < 1)
        elapsed = 1;
    if(elapsed > 8)
        elapsed = 8;
    while(elapsed-- > 0) {
        int32_t error_q16 =
            target_q16 - position_q16;
        int32_t acceleration_q16 =
            error_q16 *
                DESKTOP_RELEASE_STIFFNESS -
            velocity_q16 *
                DESKTOP_RELEASE_DAMPING;

        velocity_q16 +=
            acceleration_q16 / HZ;
        velocity_q16 =
            clamp_velocity(velocity_q16);
        position_q16 += velocity_q16 / HZ;
        if(position_q16 < 0) {
            position_q16 = 0;
            if(velocity_q16 < 0)
                velocity_q16 = 0;
        }
        else if(position_q16 > maximum_q16) {
            position_q16 = maximum_q16;
            if(velocity_q16 > 0)
                velocity_q16 = 0;
        }
    }
    last_physics = now;
    {
        int32_t error_q16 =
            target_q16 - position_q16;
        int32_t absolute_error_q16 =
            error_q16 < 0 ? -error_q16 : error_q16;
        int32_t absolute_velocity_q16 =
            velocity_q16 < 0 ? -velocity_q16 : velocity_q16;

        if(absolute_error_q16 <= DESKTOP_SNAP_POSITION &&
           absolute_velocity_q16 <= DESKTOP_SNAP_VELOCITY) {
            position_q16 = target_q16;
            velocity_q16 = 0;
            moving = false;
        }
    }
}

bool crazypod_desktop_motion_tick(long now, int item_count)
{
    bool advanced = false;
    bool frame_due;

    if(input_enabled)
        sample_wheel(now, item_count);
    frame_due =
        crazypod_frameclock_due(&motion_clock, now);
    if(frame_due && moving && !wheel_tracking) {
        advance(now, item_count);
        advanced = true;
    }
    if(!frame_due ||
       (!render_dirty && !moving && !advanced))
        return false;
    crazypod_frameclock_schedule_next(&motion_clock, now);
    render_dirty = false;
    return true;
}

bool crazypod_desktop_motion_active(void)
{
    return moving || wheel_tracking;
}

int crazypod_desktop_motion_position_q8(void)
{
    return (position_q16 + 128) >> 8;
}

int crazypod_desktop_motion_center(int item_count)
{
    return center_item(item_count);
}

int crazypod_desktop_motion_take_wheel_feedback(void)
{
    int direction = wheel_feedback_direction;

    wheel_feedback_direction = 0;
    return direction;
}

#endif
