#ifndef CRAZYPOD_SIMULATOR_SNAPSHOT_H
#define CRAZYPOD_SIMULATOR_SNAPSHOT_H

#include <stdbool.h>
#include <stdint.h>

#include "../../crazypod_apps.h"
#include "../navigation/crazypod_ui_routes.h"

struct crazypod_simulator_snapshot_host {
    void (*show_power_prompt)(void);
    void (*open_app)(enum crazypod_app_id id);
    void (*open_root_route)(enum crazypod_route route);
    void (*push_route)(enum crazypod_route route, int group);
    void (*pop_route)(void);
    void (*render)(bool transition);
    void (*activate_selected)(void);
    void (*begin_note_composer)(uint32_t id, bool resume_draft);
    void (*show_calendar_day)(int date);
};

bool crazypod_simulator_snapshot_prepare(
    const struct crazypod_simulator_snapshot_host *host);
long crazypod_simulator_snapshot_settle_ticks(void);
void crazypod_simulator_snapshot_write_profile(void);

#endif
