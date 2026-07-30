#ifndef CRAZYPOD_GAME2048_ENGINE_H
#define CRAZYPOD_GAME2048_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAME2048_STATE_MAGIC 0x43503234ul /* CP24 */
#define GAME2048_STATE_VERSION 1u
#define GAME2048_BOARD_CELLS 16u
#define GAME2048_HISTORY_LIMIT 10u
#define GAME2048_TARGET_EXPONENT 11u

enum game2048_direction {
    GAME2048_UP = 0,
    GAME2048_DOWN,
    GAME2048_LEFT,
    GAME2048_RIGHT
};

enum game2048_session_phase {
    GAME2048_SESSION_NONE = 0,
    GAME2048_SESSION_ACTIVE,
    GAME2048_SESSION_WIN_PROMPT,
    GAME2048_SESSION_FAILED,
    GAME2048_SESSION_COMPLETED
};

enum game2048_result {
    GAME2048_RESULT_WIN = 0,
    GAME2048_RESULT_FAILED,
    GAME2048_RESULT_POST_WIN_FAILED,
    GAME2048_RESULT_ABANDONED
};

struct game2048_settings {
    uint8_t auto_continue;
    uint8_t show_timer;
};

struct game2048_session {
    uint8_t board[GAME2048_BOARD_CELLS];
    uint8_t phase;
    uint8_t reached_2048;
    uint8_t clock_running;
    uint8_t reserved;
    uint16_t moves;
    uint16_t reached_2048_move;
    uint32_t id;
    uint32_t score;
    uint32_t started_epoch;
    uint32_t elapsed_ms;
    uint32_t active_since_ms;
};

struct game2048_record {
    uint8_t board[GAME2048_BOARD_CELLS];
    uint8_t result;
    uint8_t reached_2048;
    uint16_t moves;
    uint32_t id;
    uint32_t score;
    uint32_t started_epoch;
    uint32_t ended_epoch;
    uint32_t elapsed_seconds;
};

struct game2048_model {
    struct game2048_settings settings;
    struct game2048_session session;
    struct game2048_record history[GAME2048_HISTORY_LIMIT];
    uint8_t history_count;
    uint8_t reserved[3];
    uint32_t next_session_id;
    uint32_t high_score;
    uint32_t rng_state;
};

struct game2048_disk_state {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    struct game2048_settings settings;
    uint8_t history_count;
    uint8_t reserved0;
    uint32_t next_session_id;
    uint32_t high_score;
    uint32_t rng_state;
    struct game2048_session session;
    struct game2048_record history[GAME2048_HISTORY_LIMIT];
    uint32_t checksum;
};

void game2048_model_init(struct game2048_model *model);
bool game2048_start_new(struct game2048_model *model,
                        uint32_t epoch_seconds,
                        uint32_t monotonic_ms);
bool game2048_move(struct game2048_model *model,
                   enum game2048_direction direction,
                   uint32_t epoch_seconds,
                   uint32_t monotonic_ms);
bool game2048_slide_board(
    uint8_t board[GAME2048_BOARD_CELLS],
    enum game2048_direction direction, uint32_t *score_gained);
bool game2048_can_move(const struct game2048_model *model);
bool game2048_continue_after_win(struct game2048_model *model,
                                 uint32_t epoch_seconds,
                                 uint32_t monotonic_ms);
bool game2048_finish_win(struct game2048_model *model,
                         uint32_t epoch_seconds,
                         uint32_t monotonic_ms);
bool game2048_abandon(struct game2048_model *model,
                      uint32_t epoch_seconds,
                      uint32_t monotonic_ms);
void game2048_resume_clock(struct game2048_model *model,
                           uint32_t monotonic_ms);
void game2048_pause_clock(struct game2048_model *model,
                          uint32_t monotonic_ms);
uint32_t game2048_elapsed_seconds(
    const struct game2048_model *model, uint32_t monotonic_ms);
uint8_t game2048_max_exponent(const uint8_t *board);
uint32_t game2048_tile_value(uint8_t exponent);
const struct game2048_record *game2048_history_get(
    const struct game2048_model *model, unsigned index);
void game2048_clear_history(struct game2048_model *model);
void game2048_reset_high_score(struct game2048_model *model);
void game2048_pack(const struct game2048_model *model,
                   struct game2048_disk_state *disk,
                   uint32_t monotonic_ms);
bool game2048_unpack(struct game2048_model *model,
                     const struct game2048_disk_state *disk,
                     size_t size);

#endif
