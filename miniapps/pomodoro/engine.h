#ifndef CRAZYPOD_POMODORO_ENGINE_H
#define CRAZYPOD_POMODORO_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POMODORO_STATE_MAGIC 0x4350504ful /* CPPO */
#define POMODORO_STATE_VERSION 1u

#define POMODORO_MIN_MINUTES 1u
#define POMODORO_MAX_MINUTES 99u
#define POMODORO_MIN_ROUNDS 1u
#define POMODORO_MAX_ROUNDS 12u

enum pomodoro_phase {
    POMODORO_PHASE_FOCUS = 0,
    POMODORO_PHASE_SHORT_BREAK,
    POMODORO_PHASE_LONG_BREAK
};

enum pomodoro_run_state {
    POMODORO_READY = 0,
    POMODORO_RUNNING,
    POMODORO_PAUSED,
    POMODORO_COMPLETE
};

struct pomodoro_config {
    uint16_t focus_minutes;
    uint16_t short_break_minutes;
    uint16_t long_break_minutes;
    uint16_t rounds;
};

struct pomodoro_model {
    struct pomodoro_config config;
    uint8_t phase;
    uint8_t run_state;
    uint16_t focuses_in_cycle;
    uint32_t deadline_epoch;
    uint32_t remaining_seconds;
    uint32_t alarm_token;
    uint32_t next_alarm_token;
};

struct pomodoro_disk_state {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint16_t focus_minutes;
    uint16_t short_break_minutes;
    uint16_t long_break_minutes;
    uint16_t rounds;
    uint8_t phase;
    uint8_t run_state;
    uint16_t focuses_in_cycle;
    uint32_t deadline_epoch;
    uint32_t remaining_seconds;
    uint32_t alarm_token;
    uint32_t next_alarm_token;
    uint32_t checksum;
};

void pomodoro_model_init(struct pomodoro_model *model);
bool pomodoro_config_valid(const struct pomodoro_config *config);
bool pomodoro_model_set_config(struct pomodoro_model *model,
                               const struct pomodoro_config *config);

uint32_t pomodoro_phase_duration(const struct pomodoro_model *model);
uint32_t pomodoro_remaining_at(const struct pomodoro_model *model,
                               uint32_t now_epoch);
unsigned pomodoro_round_number(const struct pomodoro_model *model);
enum pomodoro_phase
pomodoro_next_phase(const struct pomodoro_model *model);

bool pomodoro_start(struct pomodoro_model *model, uint32_t now_epoch);
bool pomodoro_pause(struct pomodoro_model *model, uint32_t now_epoch);
bool pomodoro_resume(struct pomodoro_model *model, uint32_t now_epoch);
bool pomodoro_tick(struct pomodoro_model *model, uint32_t now_epoch);
bool pomodoro_alarm_fired(struct pomodoro_model *model, uint32_t token);
bool pomodoro_reset(struct pomodoro_model *model);
bool pomodoro_skip(struct pomodoro_model *model);
bool pomodoro_advance(struct pomodoro_model *model);

void pomodoro_pack(const struct pomodoro_model *model,
                   struct pomodoro_disk_state *disk);
bool pomodoro_unpack(struct pomodoro_model *model,
                     const struct pomodoro_disk_state *disk,
                     size_t size);

#endif
