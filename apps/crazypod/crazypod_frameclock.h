#ifndef CRAZYPOD_FRAMECLOCK_H
#define CRAZYPOD_FRAMECLOCK_H

#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>

/* Run at the iPod 6G kernel's maximum scheduling rate. The LCD driver still
 * waits for TE/FMARK when the panel exposes a usable synchronization edge. */
#define CRAZYPOD_TARGET_FPS HZ
#define CRAZYPOD_FRAME_BUDGET_US \
    (1000000u / CRAZYPOD_TARGET_FPS)

enum crazypod_render_source {
    CRAZYPOD_RENDER_HOME = 0,
    CRAZYPOD_RENDER_MUSIC,
};

struct crazypod_present_diagnostics {
    uint32_t queue_requests;
    uint32_t coalesced_requests;
    uint32_t presents;
    uint32_t full_presents;
    uint32_t deadline_misses;
    uint32_t max_late_ticks;
    uint32_t present_timeouts;
    uint32_t last_present_us;
    uint32_t max_present_us;
    uint32_t home_renders;
    uint32_t home_render_timeouts;
    uint32_t last_home_render_us;
    uint32_t max_home_render_us;
    uint32_t music_renders;
    uint32_t music_render_timeouts;
    uint32_t last_music_render_us;
    uint32_t max_music_render_us;
};

struct crazypod_frameclock {
    long next_tick;
    int tick_error;
};

void crazypod_frameclock_reset(struct crazypod_frameclock *clock, long now);
bool crazypod_frameclock_due(const struct crazypod_frameclock *clock, long now);
void crazypod_frameclock_schedule_next(struct crazypod_frameclock *clock,
                                       long now);
uint32_t crazypod_monotonic_usec(void);

void crazypod_present_init(long now);
void crazypod_present_queue_rect(int x, int y, int width, int height);
void crazypod_present_queue_full(void);
/* Commit an already-rendered queued frame before a synchronous operation. */
void crazypod_present_now(void);
void crazypod_present_tick(void);
uint32_t crazypod_present_sequence(void);
long crazypod_present_last_tick(void);
void crazypod_present_note_render(
    enum crazypod_render_source source, uint32_t duration_us);
void crazypod_present_get_diagnostics(
    struct crazypod_present_diagnostics *diagnostics);

#ifdef SIMULATOR
uint32_t crazypod_present_framebuffer_crc(void);
#endif

#endif

#endif
