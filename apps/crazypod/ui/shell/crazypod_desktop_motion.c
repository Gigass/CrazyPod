#include "config.h"

#ifdef IPOD_6G

#include "../../crazypod_frameclock.h"
#include "crazypod_desktop_motion.h"

#define DESKTOP_MOTION_SIM_FPS 60

static struct crazypod_frameclock motion_clock;
static bool moving;
static int target_q8;
static int position_q8;
static int velocity_q8;
static int step_accumulator;

void crazypod_desktop_motion_initialize(long now, int selected)
{
    target_q8 = selected * 256;
    position_q8 = target_q8;
    velocity_q8 = 0;
    moving = false;
    step_accumulator = 0;
    crazypod_frameclock_reset(&motion_clock, now);
}

void crazypod_desktop_motion_select(
    long now, int selected, bool animated)
{
    target_q8 = selected * 256;
    if(animated) {
        moving = true;
        step_accumulator = 0;
        crazypod_frameclock_reset(&motion_clock, now);
        return;
    }
    position_q8 = target_q8;
    velocity_q8 = 0;
    moving = false;
    step_accumulator = 0;
}

static bool advance(void)
{
    int delta = target_q8 - position_q8;

    if(delta == 0 && velocity_q8 == 0) {
        moving = false;
        return false;
    }
    velocity_q8 = velocity_q8 * 8 / 16 + delta * 3 / 16;
    if(velocity_q8 == 0 && delta != 0)
        velocity_q8 = delta < 0 ? -1 : 1;
    position_q8 += velocity_q8;
    delta = target_q8 - position_q8;
    if((delta < 0 ? -delta : delta) <= 2 &&
       (velocity_q8 < 0 ? -velocity_q8 : velocity_q8) <= 2) {
        position_q8 = target_q8;
        velocity_q8 = 0;
        moving = false;
    }
    return moving;
}

bool crazypod_desktop_motion_tick(long now)
{
    bool changed = false;

    if(!moving || !crazypod_frameclock_due(&motion_clock, now))
        return false;
    step_accumulator += DESKTOP_MOTION_SIM_FPS;
    do {
        changed = true;
        if(!advance())
            break;
        step_accumulator -= CRAZYPOD_TARGET_FPS;
    } while(step_accumulator >= CRAZYPOD_TARGET_FPS);
    if(moving)
        crazypod_frameclock_schedule_next(&motion_clock, now);
    else
        step_accumulator = 0;
    return changed;
}

bool crazypod_desktop_motion_active(void)
{
    return moving;
}

int crazypod_desktop_motion_position_q8(void)
{
    return position_q8;
}

#endif
