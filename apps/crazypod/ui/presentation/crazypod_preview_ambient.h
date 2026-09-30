#ifndef CRAZYPOD_PREVIEW_AMBIENT_H
#define CRAZYPOD_PREVIEW_AMBIENT_H

#include <stdbool.h>

#include "lvgl.h"

/*
 * Ambient life for a settled menu preview. One LVGL animation drives every
 * registered object, so the cost is a single timer regardless of how many
 * parts move. A session is time-boxed: motion ramps in, runs, then coasts to
 * the exact rest pose so the CPU can drop its boost when the user is idle.
 * Any wheel input re-renders the preview and starts a new session.
 *
 * Every channel is optional; zero means unused. Angles are LVGL tenths of a
 * degree, scale is 1/256, positions are pixels relative to the object's
 * parent, and phase is degrees.
 */
struct crazypod_preview_ambient_spec {
    int period_ms;
    int phase;
    /* Sine offsets. dy_phase 90 turns dx/dy into a circular orbit. */
    int dx;
    int dy;
    int dy_phase;
    /* Sine rotation and scale pulse around the rest pose. */
    int rock;
    int pulse;
    /* Whole signed turns over the session: spin up, cruise, coast down. */
    int spin_turns;
    /* Sine opacity between opa_lo and opa_hi when opa_hi > opa_lo. */
    int opa_lo;
    int opa_hi;
    /* Repeating x sweep in absolute parent coordinates, e.g. a gloss.
     * phase staggers the cycle; ripples use it the same way. */
    int sweep_from;
    int sweep_to;
    int sweep_ms;
    /* Expanding ripple: scale from..to while opacity fades from ripple_opa. */
    int ripple_from;
    int ripple_to;
    int ripple_opa;
};

/* Drop every registered object without touching it. */
void crazypod_preview_ambient_reset(void);
/* Register an object at its current pose; call after final styles are set. */
void crazypod_preview_ambient_add(
    lv_obj_t *object, const struct crazypod_preview_ambient_spec *spec);
/* Begin a session driven by anchor; false when nothing is registered. */
bool crazypod_preview_ambient_start(lv_obj_t *anchor);
/* End the session. restore snaps objects to rest; otherwise they stay put. */
void crazypod_preview_ambient_stop(bool restore);
bool crazypod_preview_ambient_active(void);

#endif
