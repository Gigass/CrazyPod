#ifndef CRAZYPOD_COVERFLOW_MOTION_H
#define CRAZYPOD_COVERFLOW_MOTION_H

#include <stdbool.h>
#include <stdint.h>

/* Pure CoverFlow motion model. It owns position and velocity in Q16 album
 * units and knows nothing about rendering, the LCD or the click wheel driver,
 * so the feel can be tested on the host.
 *
 * Finger down (TRACK): wheel clicks move a target through a speed-dependent
 * gain; the displayed position follows it with a stiff critically damped
 * follower, so there is a short ease-in and no velocity-estimator lag.
 * Finger up: the displayed velocity is kept and the cover glides to the
 * album that friction would stop at (COAST); a slow release uses a soft
 * critically damped spring instead (SNAP).
 * Discrete remote steps (STEP) ease in and out through a filtered target. */

#define CRAZYPOD_FLOW_ONE (1L << 16)
#define CRAZYPOD_FLOW_CLICKS_PER_ALBUM 12
#define CRAZYPOD_FLOW_WHEEL_SAMPLES 8

enum crazypod_flow_mode {
    CRAZYPOD_FLOW_IDLE = 0,
    CRAZYPOD_FLOW_TRACK,
    CRAZYPOD_FLOW_COAST,
    CRAZYPOD_FLOW_SNAP,
    CRAZYPOD_FLOW_STEP,
};

struct crazypod_flow_motion {
    int32_t position_q16;
    int32_t velocity_q16;
    int32_t target_q16;
    int32_t aim_q16;
    int32_t max_position_q16;
    uint32_t coast_tau_usec;
    enum crazypod_flow_mode mode;
    int32_t clicks;
    int sample_count;
    uint32_t sample_usec[CRAZYPOD_FLOW_WHEEL_SAMPLES];
    int32_t sample_clicks[CRAZYPOD_FLOW_WHEEL_SAMPLES];
};

void crazypod_flow_motion_reset(
    struct crazypod_flow_motion *motion, int32_t position_q16,
    int32_t max_position_q16);
void crazypod_flow_motion_set_limit(
    struct crazypod_flow_motion *motion, int32_t max_position_q16);
void crazypod_flow_motion_touch(
    struct crazypod_flow_motion *motion, uint32_t now_usec);
void crazypod_flow_motion_wheel(
    struct crazypod_flow_motion *motion, uint32_t now_usec,
    int delta_clicks);
void crazypod_flow_motion_release(struct crazypod_flow_motion *motion);
void crazypod_flow_motion_step_to(
    struct crazypod_flow_motion *motion, int album);
void crazypod_flow_motion_advance(
    struct crazypod_flow_motion *motion, uint32_t elapsed_usec);
/* True while the displayed position can still change on its own. */
bool crazypod_flow_motion_moving(
    const struct crazypod_flow_motion *motion);
/* Album position where the current motion is heading. */
int32_t crazypod_flow_motion_destination(
    const struct crazypod_flow_motion *motion);
/* Wheel gain for a wheel speed in clicks per second, in Q16. */
int32_t crazypod_flow_motion_gain_q16(int clicks_per_second);

#endif
