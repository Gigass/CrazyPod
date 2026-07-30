#include "engine.h"

static void bytes_clear(void *buffer, size_t size)
{
    uint8_t *bytes = buffer;
    size_t index;

    for(index = 0; index < size; ++index)
        bytes[index] = 0;
}

static void bytes_copy(void *destination, const void *source, size_t size)
{
    uint8_t *output = destination;
    const uint8_t *input = source;
    size_t index;

    for(index = 0; index < size; ++index)
        output[index] = input[index];
}

static uint32_t checksum_bytes(const void *buffer, size_t size)
{
    const uint8_t *bytes = buffer;
    uint32_t hash = 2166136261u;
    size_t index;

    for(index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t game2048_tile_value(uint8_t exponent)
{
    if(exponent == 0 || exponent >= 31)
        return 0;
    return 1u << exponent;
}

uint8_t game2048_max_exponent(const uint8_t *board)
{
    uint8_t maximum = 0;
    unsigned index;

    if(board == NULL)
        return 0;
    for(index = 0; index < GAME2048_BOARD_CELLS; ++index)
        if(board[index] > maximum)
            maximum = board[index];
    return maximum;
}

static uint32_t random_next(struct game2048_model *model)
{
    uint32_t value = model->rng_state;

    if(value == 0)
        value = 0x6d2b79f5u;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    model->rng_state = value;
    return value;
}

static bool spawn_tile(struct game2048_model *model)
{
    unsigned empty_count = 0;
    unsigned target;
    unsigned index;

    for(index = 0; index < GAME2048_BOARD_CELLS; ++index)
        if(model->session.board[index] == 0)
            ++empty_count;
    if(empty_count == 0)
        return false;
    target = random_next(model) % empty_count;
    for(index = 0; index < GAME2048_BOARD_CELLS; ++index) {
        if(model->session.board[index] != 0)
            continue;
        if(target > 0) {
            --target;
            continue;
        }
        model->session.board[index] =
            random_next(model) % 10u == 0 ? 2u : 1u;
        return true;
    }
    return false;
}

static unsigned line_index(enum game2048_direction direction,
                           unsigned line, unsigned position)
{
    if(direction == GAME2048_LEFT)
        return line * 4u + position;
    if(direction == GAME2048_RIGHT)
        return line * 4u + (3u - position);
    if(direction == GAME2048_UP)
        return position * 4u + line;
    return (3u - position) * 4u + line;
}

static bool collapse_line(struct game2048_session *session,
                          enum game2048_direction direction,
                          unsigned line, uint32_t *score)
{
    uint8_t input[4];
    uint8_t output[4] = { 0, 0, 0, 0 };
    unsigned input_count = 0;
    unsigned output_count = 0;
    unsigned position;
    bool changed = false;

    for(position = 0; position < 4; ++position) {
        uint8_t value =
            session->board[line_index(direction, line, position)];

        if(value != 0)
            input[input_count++] = value;
    }
    position = 0;
    while(position < input_count) {
        uint8_t value = input[position];

        if(position + 1u < input_count &&
           input[position + 1u] == value) {
            if(value < 30u)
                ++value;
            *score += game2048_tile_value(value);
            position += 2u;
        }
        else
            ++position;
        output[output_count++] = value;
    }
    for(position = 0; position < 4; ++position) {
        unsigned index = line_index(direction, line, position);

        if(session->board[index] != output[position]) {
            session->board[index] = output[position];
            changed = true;
        }
    }
    return changed;
}

bool game2048_slide_board(
    uint8_t board[GAME2048_BOARD_CELLS],
    enum game2048_direction direction, uint32_t *score_gained)
{
    struct game2048_session session;
    uint32_t gained = 0;
    bool changed = false;
    unsigned line;

    if(board == NULL || direction > GAME2048_RIGHT)
        return false;
    bytes_clear(&session, sizeof(session));
    bytes_copy(session.board, board, sizeof(session.board));
    for(line = 0; line < 4; ++line)
        if(collapse_line(&session, direction, line, &gained))
            changed = true;
    if(changed)
        bytes_copy(board, session.board, sizeof(session.board));
    if(score_gained != NULL)
        *score_gained = gained;
    return changed;
}

static bool board_valid(const uint8_t *board)
{
    unsigned index;

    for(index = 0; index < GAME2048_BOARD_CELLS; ++index)
        if(board[index] > 30u)
            return false;
    return true;
}

bool game2048_can_move(const struct game2048_model *model)
{
    unsigned row;
    unsigned column;

    if(model == NULL)
        return false;
    for(row = 0; row < 4; ++row) {
        for(column = 0; column < 4; ++column) {
            unsigned index = row * 4u + column;
            uint8_t value = model->session.board[index];

            if(value == 0)
                return true;
            if(column + 1u < 4 &&
               model->session.board[index + 1u] == value)
                return true;
            if(row + 1u < 4 &&
               model->session.board[index + 4u] == value)
                return true;
        }
    }
    return false;
}

static void update_clock(struct game2048_model *model,
                         uint32_t monotonic_ms)
{
    if(model->session.clock_running) {
        model->session.elapsed_ms +=
            monotonic_ms - model->session.active_since_ms;
        model->session.active_since_ms = monotonic_ms;
    }
}

void game2048_resume_clock(struct game2048_model *model,
                           uint32_t monotonic_ms)
{
    if(model == NULL ||
       (model->session.phase != GAME2048_SESSION_ACTIVE &&
        model->session.phase != GAME2048_SESSION_WIN_PROMPT))
        return;
    model->session.active_since_ms = monotonic_ms;
    model->session.clock_running = 1;
}

void game2048_pause_clock(struct game2048_model *model,
                          uint32_t monotonic_ms)
{
    if(model == NULL)
        return;
    update_clock(model, monotonic_ms);
    model->session.clock_running = 0;
    model->session.active_since_ms = 0;
}

uint32_t game2048_elapsed_seconds(
    const struct game2048_model *model, uint32_t monotonic_ms)
{
    uint32_t elapsed_ms;

    if(model == NULL)
        return 0;
    elapsed_ms = model->session.elapsed_ms;
    if(model->session.clock_running)
        elapsed_ms += monotonic_ms - model->session.active_since_ms;
    return elapsed_ms / 1000u;
}

static void record_session(struct game2048_model *model,
                           enum game2048_result result,
                           uint32_t epoch_seconds,
                           uint32_t monotonic_ms)
{
    struct game2048_record *record;
    unsigned count;
    unsigned index;

    game2048_pause_clock(model, monotonic_ms);
    count = model->history_count;
    if(count >= GAME2048_HISTORY_LIMIT)
        count = GAME2048_HISTORY_LIMIT - 1u;
    for(index = count; index > 0; --index)
        model->history[index] = model->history[index - 1u];
    record = &model->history[0];
    bytes_clear(record, sizeof(*record));
    bytes_copy(record->board, model->session.board,
               sizeof(record->board));
    record->result = (uint8_t)result;
    record->reached_2048 = model->session.reached_2048;
    record->moves = model->session.moves;
    record->id = model->session.id;
    record->score = model->session.score;
    record->started_epoch = model->session.started_epoch;
    record->ended_epoch = epoch_seconds;
    record->elapsed_seconds = model->session.elapsed_ms / 1000u;
    if(model->history_count < GAME2048_HISTORY_LIMIT)
        ++model->history_count;
}

static void finish_failure(struct game2048_model *model,
                           uint32_t epoch_seconds,
                           uint32_t monotonic_ms)
{
    enum game2048_result result = model->session.reached_2048
        ? GAME2048_RESULT_POST_WIN_FAILED
        : GAME2048_RESULT_FAILED;

    record_session(model, result, epoch_seconds, monotonic_ms);
    model->session.phase = GAME2048_SESSION_FAILED;
}

void game2048_model_init(struct game2048_model *model)
{
    if(model == NULL)
        return;
    bytes_clear(model, sizeof(*model));
    model->settings.show_timer = 1;
    model->next_session_id = 1;
    model->rng_state = 0x6d2b79f5u;
}

bool game2048_start_new(struct game2048_model *model,
                        uint32_t epoch_seconds,
                        uint32_t monotonic_ms)
{
    uint32_t session_id;
    uint32_t seed;

    if(model == NULL)
        return false;
    session_id = model->next_session_id;
    if(session_id == 0)
        session_id = 1;
    model->next_session_id = session_id + 1u;
    if(model->next_session_id == 0)
        model->next_session_id = 1;
    bytes_clear(&model->session, sizeof(model->session));
    model->session.phase = GAME2048_SESSION_ACTIVE;
    model->session.id = session_id;
    model->session.started_epoch = epoch_seconds;
    seed = model->rng_state ^ epoch_seconds ^
        (monotonic_ms * 1664525u) ^ session_id;
    model->rng_state = seed != 0 ? seed : 0x6d2b79f5u;
    if(!spawn_tile(model) || !spawn_tile(model))
        return false;
    game2048_resume_clock(model, monotonic_ms);
    return true;
}

bool game2048_move(struct game2048_model *model,
                   enum game2048_direction direction,
                   uint32_t epoch_seconds,
                   uint32_t monotonic_ms)
{
    bool changed = false;
    bool reached_now = false;
    uint32_t gained = 0;

    if(model == NULL ||
       model->session.phase != GAME2048_SESSION_ACTIVE ||
       direction > GAME2048_RIGHT)
        return false;
    update_clock(model, monotonic_ms);
    changed = game2048_slide_board(
        model->session.board, direction, &gained);
    if(!changed) {
        if(!game2048_can_move(model)) {
            finish_failure(model, epoch_seconds, monotonic_ms);
            return true;
        }
        return false;
    }
    model->session.score += gained;
    ++model->session.moves;
    if(model->session.score > model->high_score)
        model->high_score = model->session.score;
    (void)spawn_tile(model);
    if(!model->session.reached_2048 &&
       game2048_max_exponent(model->session.board) >=
           GAME2048_TARGET_EXPONENT) {
        model->session.reached_2048 = 1;
        model->session.reached_2048_move = model->session.moves;
        reached_now = true;
    }
    if(reached_now && !model->settings.auto_continue) {
        model->session.phase = GAME2048_SESSION_WIN_PROMPT;
        game2048_pause_clock(model, monotonic_ms);
    }
    else if(!game2048_can_move(model))
        finish_failure(model, epoch_seconds, monotonic_ms);
    return true;
}

bool game2048_continue_after_win(struct game2048_model *model,
                                 uint32_t epoch_seconds,
                                 uint32_t monotonic_ms)
{
    if(model == NULL ||
       model->session.phase != GAME2048_SESSION_WIN_PROMPT)
        return false;
    model->session.phase = GAME2048_SESSION_ACTIVE;
    if(!game2048_can_move(model)) {
        finish_failure(model, epoch_seconds, monotonic_ms);
        return true;
    }
    game2048_resume_clock(model, monotonic_ms);
    return true;
}

bool game2048_finish_win(struct game2048_model *model,
                         uint32_t epoch_seconds,
                         uint32_t monotonic_ms)
{
    if(model == NULL ||
       model->session.phase != GAME2048_SESSION_WIN_PROMPT)
        return false;
    record_session(model, GAME2048_RESULT_WIN,
                   epoch_seconds, monotonic_ms);
    model->session.phase = GAME2048_SESSION_COMPLETED;
    return true;
}

bool game2048_abandon(struct game2048_model *model,
                      uint32_t epoch_seconds,
                      uint32_t monotonic_ms)
{
    if(model == NULL ||
       (model->session.phase != GAME2048_SESSION_ACTIVE &&
        model->session.phase != GAME2048_SESSION_WIN_PROMPT))
        return false;
    record_session(model, GAME2048_RESULT_ABANDONED,
                   epoch_seconds, monotonic_ms);
    model->session.phase = GAME2048_SESSION_COMPLETED;
    return true;
}

const struct game2048_record *game2048_history_get(
    const struct game2048_model *model, unsigned index)
{
    if(model == NULL || index >= model->history_count)
        return NULL;
    return &model->history[index];
}

void game2048_clear_history(struct game2048_model *model)
{
    if(model == NULL)
        return;
    bytes_clear(model->history, sizeof(model->history));
    model->history_count = 0;
}

void game2048_reset_high_score(struct game2048_model *model)
{
    if(model == NULL)
        return;
    model->high_score =
        model->session.phase == GAME2048_SESSION_ACTIVE ||
        model->session.phase == GAME2048_SESSION_WIN_PROMPT
            ? model->session.score : 0;
}

void game2048_pack(const struct game2048_model *model,
                   struct game2048_disk_state *disk,
                   uint32_t monotonic_ms)
{
    struct game2048_model copy;

    if(model == NULL || disk == NULL)
        return;
    copy = *model;
    game2048_pause_clock(&copy, monotonic_ms);
    bytes_clear(disk, sizeof(*disk));
    disk->magic = GAME2048_STATE_MAGIC;
    disk->version = GAME2048_STATE_VERSION;
    disk->size = (uint16_t)sizeof(*disk);
    disk->settings = copy.settings;
    disk->history_count = copy.history_count;
    disk->next_session_id = copy.next_session_id;
    disk->high_score = copy.high_score;
    disk->rng_state = copy.rng_state;
    disk->session = copy.session;
    bytes_copy(disk->history, copy.history, sizeof(disk->history));
    disk->checksum = checksum_bytes(disk, sizeof(*disk));
}

static bool record_valid(const struct game2048_record *record)
{
    return record->result <= GAME2048_RESULT_ABANDONED &&
        record->id != 0 &&
        board_valid(record->board);
}

bool game2048_unpack(struct game2048_model *model,
                     const struct game2048_disk_state *disk,
                     size_t size)
{
    struct game2048_disk_state copy;
    struct game2048_model loaded;
    uint32_t checksum;
    unsigned index;

    if(model == NULL || disk == NULL ||
       size != sizeof(*disk) ||
       disk->magic != GAME2048_STATE_MAGIC ||
       disk->version != GAME2048_STATE_VERSION ||
       disk->size != sizeof(*disk))
        return false;
    copy = *disk;
    checksum = copy.checksum;
    copy.checksum = 0;
    if(checksum != checksum_bytes(&copy, sizeof(copy)))
        return false;
    if(disk->settings.auto_continue > 1 ||
       disk->settings.show_timer > 1 ||
       disk->history_count > GAME2048_HISTORY_LIMIT ||
       disk->session.phase > GAME2048_SESSION_COMPLETED ||
       !board_valid(disk->session.board))
        return false;
    for(index = 0; index < disk->history_count; ++index)
        if(!record_valid(&disk->history[index]))
            return false;
    bytes_clear(&loaded, sizeof(loaded));
    loaded.settings = disk->settings;
    loaded.session = disk->session;
    loaded.session.clock_running = 0;
    loaded.session.active_since_ms = 0;
    loaded.history_count = disk->history_count;
    loaded.next_session_id =
        disk->next_session_id == 0 ? 1 : disk->next_session_id;
    loaded.high_score = disk->high_score;
    loaded.rng_state =
        disk->rng_state == 0 ? 0x6d2b79f5u : disk->rng_state;
    bytes_copy(loaded.history, disk->history,
               sizeof(loaded.history));
    *model = loaded;
    return true;
}
