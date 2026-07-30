#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../game2048/engine.h"

static unsigned nonzero_count(const uint8_t *board)
{
    unsigned count = 0;
    unsigned index;

    for(index = 0; index < GAME2048_BOARD_CELLS; ++index)
        if(board[index] != 0)
            ++count;
    return count;
}

static void test_slide_rules(void)
{
    uint8_t board[GAME2048_BOARD_CELLS] = {
        1, 1, 1, 1,
        1, 1, 2, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };
    uint32_t score = 0;

    assert(game2048_slide_board(board, GAME2048_LEFT, &score));
    assert(board[0] == 2);
    assert(board[1] == 2);
    assert(board[2] == 0);
    assert(board[3] == 0);
    assert(board[4] == 2);
    assert(board[5] == 2);
    assert(board[6] == 0);
    assert(score == 12);

    memset(board, 0, sizeof(board));
    board[0] = 1;
    board[4] = 1;
    board[8] = 1;
    board[12] = 1;
    score = 0;
    assert(game2048_slide_board(board, GAME2048_DOWN, &score));
    assert(board[0] == 0);
    assert(board[4] == 0);
    assert(board[8] == 2);
    assert(board[12] == 2);
    assert(score == 8);
}

static void test_new_game_and_invalid_move(void)
{
    struct game2048_model model;
    uint8_t before[GAME2048_BOARD_CELLS];

    game2048_model_init(&model);
    assert(game2048_start_new(&model, 1000, 500));
    assert(model.session.phase == GAME2048_SESSION_ACTIVE);
    assert(model.session.id == 1);
    assert(nonzero_count(model.session.board) == 2);

    memset(model.session.board, 0, sizeof(model.session.board));
    model.session.board[0] = 1;
    memcpy(before, model.session.board, sizeof(before));
    assert(!game2048_move(&model, GAME2048_UP, 1001, 600));
    assert(memcmp(before, model.session.board, sizeof(before)) == 0);
    assert(model.session.moves == 0);
    assert(model.session.score == 0);
}

static void test_win_continue_and_finish(void)
{
    struct game2048_model model;

    game2048_model_init(&model);
    assert(game2048_start_new(&model, 1000, 0));
    memset(model.session.board, 0, sizeof(model.session.board));
    model.session.board[0] = 10;
    model.session.board[1] = 10;
    assert(game2048_move(&model, GAME2048_LEFT, 1001, 1000));
    assert(model.session.reached_2048);
    assert(model.session.phase == GAME2048_SESSION_WIN_PROMPT);
    assert(model.session.score == 2048);
    assert(model.high_score == 2048);
    assert(model.history_count == 0);

    assert(game2048_continue_after_win(&model, 1001, 1100));
    assert(model.session.phase == GAME2048_SESSION_ACTIVE);

    model.session.phase = GAME2048_SESSION_WIN_PROMPT;
    game2048_pause_clock(&model, 1200);
    assert(game2048_finish_win(&model, 1002, 1200));
    assert(model.session.phase == GAME2048_SESSION_COMPLETED);
    assert(model.history_count == 1);
    assert(model.history[0].result == GAME2048_RESULT_WIN);
    assert(model.history[0].score == 2048);
}

static void test_failure_is_recorded_once(void)
{
    static const uint8_t dead_board[GAME2048_BOARD_CELLS] = {
        1, 2, 1, 2,
        2, 1, 2, 1,
        1, 2, 1, 2,
        2, 1, 2, 1
    };
    struct game2048_model model;

    game2048_model_init(&model);
    assert(game2048_start_new(&model, 2000, 0));
    memcpy(model.session.board, dead_board, sizeof(dead_board));
    assert(game2048_move(&model, GAME2048_LEFT, 2001, 1000));
    assert(model.session.phase == GAME2048_SESSION_FAILED);
    assert(model.history_count == 1);
    assert(model.history[0].result == GAME2048_RESULT_FAILED);
    assert(!game2048_move(&model, GAME2048_RIGHT, 2002, 2000));
    assert(model.history_count == 1);
}

static void test_recent_history_limit(void)
{
    struct game2048_model model;
    uint32_t first_retained;
    unsigned index;

    game2048_model_init(&model);
    for(index = 0; index < 11; ++index) {
        assert(game2048_start_new(
            &model, 3000 + index, index * 100u));
        assert(game2048_abandon(
            &model, 3000 + index, index * 100u + 50u));
    }
    assert(model.history_count == GAME2048_HISTORY_LIMIT);
    assert(model.history[0].id == 11);
    first_retained = model.history[GAME2048_HISTORY_LIMIT - 1u].id;
    assert(first_retained == 2);
    assert(game2048_history_get(&model, 10) == NULL);
}

static void test_persistence_and_checksum(void)
{
    struct game2048_model model;
    struct game2048_model loaded;
    struct game2048_disk_state disk;

    game2048_model_init(&model);
    assert(game2048_start_new(&model, 4000, 500));
    model.settings.auto_continue = 1;
    model.session.score = 512;
    model.high_score = 1024;
    game2048_pack(&model, &disk, 2500);
    assert(game2048_unpack(&loaded, &disk, sizeof(disk)));
    assert(loaded.settings.auto_continue == 1);
    assert(loaded.session.score == 512);
    assert(loaded.session.elapsed_ms == 2000);
    assert(!loaded.session.clock_running);
    assert(loaded.high_score == 1024);

    disk.history_count ^= 1u;
    assert(!game2048_unpack(&loaded, &disk, sizeof(disk)));
}

int main(void)
{
    test_slide_rules();
    test_new_game_and_invalid_move();
    test_win_continue_and_finish();
    test_failure_is_recorded_once();
    test_recent_history_limit();
    test_persistence_and_checksum();
    puts("game2048 tests passed");
    return 0;
}
