#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "game2048.c"

static int persisted_size;

static uint32_t fake_monotonic_ms(void)
{
    return 1234u;
}

static int fake_state_write(const void *data, size_t size)
{
    assert(data != NULL);
    persisted_size = (int)size;
    return (int)size;
}

static void set_board(const int32_t board[16])
{
    cp_game2048_store(board);
    state_score = 0;
    state_moves = 0;
    state_seed = 1;
    state_won = 0;
    state_gameOver = 0;
}

static int occupied_cells(void)
{
    const int32_t board[16] = {
        state_cell0, state_cell1, state_cell2, state_cell3,
        state_cell4, state_cell5, state_cell6, state_cell7,
        state_cell8, state_cell9, state_cell10, state_cell11,
        state_cell12, state_cell13, state_cell14, state_cell15,
    };
    int occupied = 0;
    int index;

    for(index = 0; index < 16; ++index)
        occupied += board[index] != 0;
    return occupied;
}

int main(void)
{
    static const struct cp_native_host_api api = {
        .abi_major = CP_NATIVE_ABI_MAJOR,
        .abi_minor = CP_NATIVE_ABI_MINOR,
        .struct_size = sizeof(api),
        .monotonic_ms = fake_monotonic_ms,
        .state_write = fake_state_write,
    };
    const int32_t merge_twice[16] = {
        1, 1, 1, 1,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    };
    const int32_t blocked[16] = {
        1, 2, 1, 2,
        2, 1, 2, 1,
        1, 2, 1, 2,
        2, 1, 2, 1,
    };

    host = &api;
    set_board(merge_twice);
    cp_game2048_move(CP_GAME2048_LEFT);
    assert(state_cell0 == 2);
    assert(state_cell1 == 2);
    assert(state_score == 8);
    assert(state_moves == 1);
    assert(occupied_cells() == 3);
    assert(persisted_size == (int)sizeof(struct cp_game2048_record));

    persisted_size = 0;
    set_board(blocked);
    cp_game2048_move(CP_GAME2048_LEFT);
    assert(state_score == 0);
    assert(state_moves == 0);
    assert(state_gameOver == 1);
    assert(occupied_cells() == 16);
    assert(persisted_size == (int)sizeof(struct cp_game2048_record));
    return 0;
}
