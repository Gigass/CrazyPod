#include "crazypod_coverflow_motion.h"

#define FLOW_ONE CRAZYPOD_FLOW_ONE
#define FLOW_STEP_USEC 8333u

/* Tracking follower: critically damped, omega = 12 rad/s. */
#define FLOW_TRACK_STIFFNESS 144
#define FLOW_TRACK_DAMPING 24
/* Soft snap spring: critically damped, omega = 10 rad/s. */
#define FLOW_SNAP_OMEGA 10
#define FLOW_SNAP_STIFFNESS 100
#define FLOW_SNAP_DAMPING 20
/* Remote steps: target filter rate and follower, omega = 16 rad/s. */
#define FLOW_STEP_RATE 10
#define FLOW_STEP_STIFFNESS 256
#define FLOW_STEP_DAMPING 32
/* Friction coast: exponential approach to the album the release velocity
 * would stop at, x(t) = stop - (stop - x0) * exp(-t / tau). The nominal
 * time constant is 0.45 s; tau is then solved so the coast starts at exactly
 * the release velocity, which keeps position and velocity continuous. */
#define FLOW_COAST_NOMINAL_TAU_Q16 29491
#define FLOW_COAST_TAU_MIN_USEC 120000
#define FLOW_COAST_TAU_MAX_USEC 900000
#define FLOW_COAST_MIN_VELOCITY (FLOW_ONE * 3)
#define FLOW_COAST_END_POSITION (FLOW_ONE / 64)
/* The exponential tail alone crawls for over a second. Keep at least this
 * speed until the last few percent of an album, then ramp it to zero. */
#define FLOW_COAST_FLOOR_VELOCITY FLOW_ONE
#define FLOW_COAST_FLOOR_ZONE (FLOW_ONE * 3 / 20)
/* Below this speed a soft snap must still keep moving forward. */
#define FLOW_SNAP_FORWARD_VELOCITY (FLOW_ONE / 2)
#define FLOW_MAX_VELOCITY (20L * FLOW_ONE)
#define FLOW_SNAP_POSITION (FLOW_ONE / 64)
#define FLOW_STEP_SETTLE_POSITION (FLOW_ONE / 64)
#define FLOW_SNAP_VELOCITY (FLOW_ONE / 4)
/* Wheel speed window and gain curve. */
#define FLOW_SPEED_WINDOW_USEC 120000u
#define FLOW_SPEED_MIN_USEC 40000u
#define FLOW_GAIN_SLOW_CPS 24
#define FLOW_GAIN_FAST_CPS 96
#define FLOW_GAIN_MAX_Q16 (FLOW_ONE * 2)

static int32_t clamp_position(
    const struct crazypod_flow_motion *motion, int32_t value)
{
    if(value < 0)
        return 0;
    if(value > motion->max_position_q16)
        return motion->max_position_q16;
    return value;
}

static int32_t nearest_album_q16(
    const struct crazypod_flow_motion *motion, int32_t value)
{
    return clamp_position(
        motion, ((value + FLOW_ONE / 2) >> 16) * FLOW_ONE);
}

static int32_t abs_q16(int32_t value)
{
    return value < 0 ? -value : value;
}

void crazypod_flow_motion_reset(
    struct crazypod_flow_motion *motion, int32_t position_q16,
    int32_t max_position_q16)
{
    motion->max_position_q16 = max_position_q16 < 0 ? 0 : max_position_q16;
    motion->position_q16 = clamp_position(motion, position_q16);
    motion->velocity_q16 = 0;
    motion->target_q16 = motion->position_q16;
    motion->aim_q16 = motion->position_q16;
    motion->mode = CRAZYPOD_FLOW_IDLE;
    motion->clicks = 0;
    motion->sample_count = 0;
}

void crazypod_flow_motion_set_limit(
    struct crazypod_flow_motion *motion, int32_t max_position_q16)
{
    motion->max_position_q16 = max_position_q16 < 0 ? 0 : max_position_q16;
}

int32_t crazypod_flow_motion_gain_q16(int clicks_per_second)
{
    int64_t t_q16;
    int64_t smooth_q16;

    if(clicks_per_second <= FLOW_GAIN_SLOW_CPS)
        return FLOW_ONE;
    if(clicks_per_second >= FLOW_GAIN_FAST_CPS)
        return FLOW_GAIN_MAX_Q16;
    t_q16 = ((int64_t)(clicks_per_second - FLOW_GAIN_SLOW_CPS) *
             FLOW_ONE) /
        (FLOW_GAIN_FAST_CPS - FLOW_GAIN_SLOW_CPS);
    /* smoothstep: slow turning stays exact, fast turning ramps in. */
    smooth_q16 = (t_q16 * t_q16 >> 16) *
        (3 * FLOW_ONE - 2 * t_q16) >> 16;
    return (int32_t)(FLOW_ONE +
        (smooth_q16 * (FLOW_GAIN_MAX_Q16 - FLOW_ONE) >> 16));
}

static void push_wheel_sample(
    struct crazypod_flow_motion *motion, uint32_t now_usec)
{
    int i;

    if(motion->sample_count == CRAZYPOD_FLOW_WHEEL_SAMPLES) {
        for(i = 1; i < CRAZYPOD_FLOW_WHEEL_SAMPLES; ++i) {
            motion->sample_usec[i - 1] = motion->sample_usec[i];
            motion->sample_clicks[i - 1] = motion->sample_clicks[i];
        }
        --motion->sample_count;
    }
    motion->sample_usec[motion->sample_count] = now_usec;
    motion->sample_clicks[motion->sample_count] = motion->clicks;
    ++motion->sample_count;
}

static int wheel_speed_cps(
    const struct crazypod_flow_motion *motion, uint32_t now_usec)
{
    int i;

    for(i = 0; i < motion->sample_count; ++i) {
        uint32_t age = now_usec - motion->sample_usec[i];

        if(age <= FLOW_SPEED_WINDOW_USEC) {
            int32_t clicks = motion->clicks - motion->sample_clicks[i];
            int64_t speed;

            if(clicks < 0)
                clicks = -clicks;
            if(age < FLOW_SPEED_MIN_USEC)
                age = FLOW_SPEED_MIN_USEC;
            speed = (int64_t)clicks * 1000000 / age;
            return speed > 100000 ? 100000 : (int)speed;
        }
    }
    return 0;
}

void crazypod_flow_motion_touch(
    struct crazypod_flow_motion *motion, uint32_t now_usec)
{
    motion->mode = CRAZYPOD_FLOW_TRACK;
    motion->target_q16 = motion->position_q16;
    motion->clicks = 0;
    motion->sample_count = 0;
    push_wheel_sample(motion, now_usec);
}

void crazypod_flow_motion_wheel(
    struct crazypod_flow_motion *motion, uint32_t now_usec,
    int delta_clicks)
{
    int32_t gain_q16;

    if(delta_clicks == 0)
        return;
    if(motion->mode != CRAZYPOD_FLOW_TRACK)
        crazypod_flow_motion_touch(motion, now_usec);
    motion->clicks += delta_clicks;
    push_wheel_sample(motion, now_usec);
    gain_q16 = crazypod_flow_motion_gain_q16(
        wheel_speed_cps(motion, now_usec));
    motion->target_q16 = clamp_position(
        motion,
        motion->target_q16 +
        (int32_t)((int64_t)delta_clicks * gain_q16 /
                  CRAZYPOD_FLOW_CLICKS_PER_ALBUM));
}

static void enter_snap(struct crazypod_flow_motion *motion)
{
    int32_t target = nearest_album_q16(
        motion,
        motion->position_q16 + motion->velocity_q16 / FLOW_SNAP_OMEGA);

    /* A drifting cover must not be pulled back to the album it just
       passed; only a nearly stationary one may reverse. */
    if(motion->velocity_q16 > FLOW_SNAP_FORWARD_VELOCITY &&
       target <= motion->position_q16)
        target = clamp_position(
            motion,
            ((motion->position_q16 >> 16) + 1) * FLOW_ONE);
    else if(motion->velocity_q16 < -FLOW_SNAP_FORWARD_VELOCITY &&
            target >= motion->position_q16)
        target = clamp_position(
            motion,
            (((motion->position_q16 + FLOW_ONE - 1) >> 16) - 1) *
                FLOW_ONE);
    motion->target_q16 = target;
    motion->mode = CRAZYPOD_FLOW_SNAP;
}

static void enter_coast(struct crazypod_flow_motion *motion)
{
    int32_t stop = nearest_album_q16(
        motion,
        motion->position_q16 +
        (int32_t)((int64_t)motion->velocity_q16 *
                  FLOW_COAST_NOMINAL_TAU_Q16 >> 16));
    int64_t distance = (int64_t)stop - motion->position_q16;
    int64_t tau_usec;

    if(distance == 0 ||
       (distance < 0) != (motion->velocity_q16 < 0)) {
        enter_snap(motion);
        return;
    }
    tau_usec = distance * 1000000 / motion->velocity_q16;
    if(tau_usec < FLOW_COAST_TAU_MIN_USEC)
        tau_usec = FLOW_COAST_TAU_MIN_USEC;
    else if(tau_usec > FLOW_COAST_TAU_MAX_USEC)
        tau_usec = FLOW_COAST_TAU_MAX_USEC;
    motion->target_q16 = stop;
    motion->coast_tau_usec = (uint32_t)tau_usec;
    motion->mode = CRAZYPOD_FLOW_COAST;
}

void crazypod_flow_motion_release(struct crazypod_flow_motion *motion)
{
    if(motion->mode != CRAZYPOD_FLOW_TRACK)
        return;
    if(abs_q16(motion->velocity_q16) >= FLOW_COAST_MIN_VELOCITY)
        enter_coast(motion);
    else
        enter_snap(motion);
}

void crazypod_flow_motion_step_to(
    struct crazypod_flow_motion *motion, int album)
{
    if(motion->mode != CRAZYPOD_FLOW_STEP)
        motion->target_q16 = motion->position_q16;
    motion->aim_q16 = clamp_position(motion, album * FLOW_ONE);
    motion->mode = CRAZYPOD_FLOW_STEP;
}

static void spring_step(
    struct crazypod_flow_motion *motion, int32_t goal_q16,
    int stiffness, int damping, uint32_t step_usec)
{
    int64_t acceleration =
        (int64_t)(goal_q16 - motion->position_q16) * stiffness -
        (int64_t)motion->velocity_q16 * damping;

    motion->velocity_q16 +=
        (int32_t)(acceleration * step_usec / 1000000);
    if(motion->velocity_q16 > FLOW_MAX_VELOCITY)
        motion->velocity_q16 = FLOW_MAX_VELOCITY;
    else if(motion->velocity_q16 < -FLOW_MAX_VELOCITY)
        motion->velocity_q16 = -FLOW_MAX_VELOCITY;
}

static void settle_if_done(
    struct crazypod_flow_motion *motion, int32_t goal_q16,
    int32_t position_window)
{
    if(abs_q16(goal_q16 - motion->position_q16) <= position_window &&
       abs_q16(motion->velocity_q16) <= FLOW_SNAP_VELOCITY) {
        motion->position_q16 = goal_q16;
        motion->velocity_q16 = 0;
        motion->target_q16 = goal_q16;
        motion->aim_q16 = goal_q16;
        motion->mode = CRAZYPOD_FLOW_IDLE;
    }
}

static void advance_step(
    struct crazypod_flow_motion *motion, uint32_t step_usec)
{
    switch(motion->mode) {
    case CRAZYPOD_FLOW_TRACK:
        spring_step(motion, motion->target_q16,
                    FLOW_TRACK_STIFFNESS, FLOW_TRACK_DAMPING, step_usec);
        break;
    case CRAZYPOD_FLOW_COAST:
        /* Velocity is proportional to the distance left, so it falls off
           smoothly and reaches the stop album without any hand-over. */
        {
            int64_t distance =
                (int64_t)motion->target_q16 - motion->position_q16;
            int64_t magnitude = distance < 0 ? -distance : distance;
            int64_t speed = magnitude * 1000000 / motion->coast_tau_usec;
            int64_t floor_speed = magnitude >= FLOW_COAST_FLOOR_ZONE
                ? FLOW_COAST_FLOOR_VELOCITY
                : FLOW_COAST_FLOOR_VELOCITY * magnitude /
                    FLOW_COAST_FLOOR_ZONE;

            if(floor_speed > speed)
                speed = floor_speed;
            motion->velocity_q16 =
                (int32_t)(distance < 0 ? -speed : speed);
        }
        break;
    case CRAZYPOD_FLOW_SNAP:
        spring_step(motion, motion->target_q16,
                    FLOW_SNAP_STIFFNESS, FLOW_SNAP_DAMPING, step_usec);
        break;
    case CRAZYPOD_FLOW_STEP:
        motion->target_q16 += (int32_t)(
            (int64_t)(motion->aim_q16 - motion->target_q16) *
            FLOW_STEP_RATE * step_usec / 1000000);
        spring_step(motion, motion->target_q16,
                    FLOW_STEP_STIFFNESS, FLOW_STEP_DAMPING, step_usec);
        break;
    case CRAZYPOD_FLOW_IDLE:
        return;
    }

    motion->position_q16 += (int32_t)(
        (int64_t)motion->velocity_q16 * step_usec / 1000000);
    if(motion->position_q16 < 0) {
        motion->position_q16 = 0;
        if(motion->velocity_q16 < 0)
            motion->velocity_q16 = 0;
    }
    else if(motion->position_q16 > motion->max_position_q16) {
        motion->position_q16 = motion->max_position_q16;
        if(motion->velocity_q16 > 0)
            motion->velocity_q16 = 0;
    }

    switch(motion->mode) {
    case CRAZYPOD_FLOW_COAST:
        if(abs_q16(motion->target_q16 - motion->position_q16) <=
           FLOW_COAST_END_POSITION) {
            motion->position_q16 = motion->target_q16;
            motion->velocity_q16 = 0;
            motion->mode = CRAZYPOD_FLOW_IDLE;
        }
        break;
    case CRAZYPOD_FLOW_SNAP:
        settle_if_done(motion, motion->target_q16, FLOW_SNAP_POSITION);
        break;
    case CRAZYPOD_FLOW_STEP:
        settle_if_done(
            motion, motion->aim_q16, FLOW_STEP_SETTLE_POSITION);
        break;
    default:
        break;
    }
}

void crazypod_flow_motion_advance(
    struct crazypod_flow_motion *motion, uint32_t elapsed_usec)
{
    while(elapsed_usec > 0 && motion->mode != CRAZYPOD_FLOW_IDLE) {
        uint32_t step_usec = elapsed_usec;

        if(step_usec > FLOW_STEP_USEC)
            step_usec = FLOW_STEP_USEC;
        advance_step(motion, step_usec);
        elapsed_usec -= step_usec;
    }
}

bool crazypod_flow_motion_moving(
    const struct crazypod_flow_motion *motion)
{
    switch(motion->mode) {
    case CRAZYPOD_FLOW_IDLE:
        return false;
    case CRAZYPOD_FLOW_TRACK:
        return motion->velocity_q16 != 0 ||
            motion->position_q16 != motion->target_q16;
    default:
        return true;
    }
}

int32_t crazypod_flow_motion_destination(
    const struct crazypod_flow_motion *motion)
{
    switch(motion->mode) {
    case CRAZYPOD_FLOW_STEP:
        return motion->aim_q16;
    case CRAZYPOD_FLOW_SNAP:
    case CRAZYPOD_FLOW_COAST:
        return motion->target_q16;
    default:
        return nearest_album_q16(
            motion,
            motion->position_q16 +
            motion->velocity_q16 * 7 / 20);
    }
}
