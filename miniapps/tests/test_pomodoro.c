#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../pomodoro/engine.h"
#include "../sdk/crazypod_miniapp.h"

extern const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host);

static uint32_t mock_epoch;
static uint32_t mock_monotonic;
static unsigned char mock_state[sizeof(struct pomodoro_disk_state)];
static size_t mock_state_size;
static unsigned mock_write_count;
static bool mock_alarm_active;
static uint32_t mock_alarm_deadline;
static uint32_t mock_alarm_token;
static unsigned mock_alarm_set_count;
static unsigned mock_alarm_cancel_count;
static unsigned mock_alarm_ack_count;
static unsigned mock_toast_count;
static char mock_toast_text[CP_MINIAPP_TOAST_TEXT_SIZE];

static uint32_t host_epoch_seconds(void)
{
    return mock_epoch;
}

static uint32_t host_monotonic_ms(void)
{
    return mock_monotonic;
}

static int host_state_read(void *buffer, size_t capacity)
{
    if(mock_state_size == 0)
        return 0;
    if(capacity < mock_state_size)
        return -1;
    memcpy(buffer, mock_state, mock_state_size);
    return (int)mock_state_size;
}

static int host_state_write(const void *buffer, size_t size)
{
    if(size > sizeof(mock_state))
        return -1;
    memcpy(mock_state, buffer, size);
    mock_state_size = size;
    ++mock_write_count;
    return 0;
}

static int host_alarm_set(uint32_t deadline_epoch, uint32_t token)
{
    mock_alarm_active = true;
    mock_alarm_deadline = deadline_epoch;
    mock_alarm_token = token;
    ++mock_alarm_set_count;
    return 0;
}

static void host_alarm_cancel(void)
{
    mock_alarm_active = false;
    ++mock_alarm_cancel_count;
}

static bool host_alarm_fired(uint32_t *token)
{
    if(!mock_alarm_active ||
       (int32_t)(mock_epoch - mock_alarm_deadline) < 0)
        return false;
    if(token != NULL)
        *token = mock_alarm_token;
    return true;
}

static void host_alarm_acknowledge(void)
{
    mock_alarm_active = false;
    ++mock_alarm_ack_count;
}

static void host_format_duration(uint32_t seconds, char *buffer,
                                 size_t capacity)
{
    snprintf(buffer, capacity, "%lu:%02lu",
             (unsigned long)(seconds / 60u),
             (unsigned long)(seconds % 60u));
}

static int host_ui_toast(const char *text, uint32_t duration_ms)
{
    (void)duration_ms;
    snprintf(mock_toast_text, sizeof(mock_toast_text), "%s", text);
    ++mock_toast_count;
    return 0;
}

static const struct cp_host_api mock_host = {
    .abi_version = CP_MINIAPP_ABI_VERSION,
    .struct_size = sizeof(struct cp_host_api),
    .epoch_seconds = host_epoch_seconds,
    .monotonic_ms = host_monotonic_ms,
    .state_read = host_state_read,
    .state_write = host_state_write,
    .alarm_set = host_alarm_set,
    .alarm_cancel = host_alarm_cancel,
    .alarm_fired = host_alarm_fired,
    .alarm_acknowledge = host_alarm_acknowledge,
    .capabilities =
        CP_CAP_FORMAT_DURATION |
        CP_CAP_UI_TOAST |
        CP_CAP_DRAW_PROGRESS,
    .format_duration = host_format_duration,
    .ui_toast = host_ui_toast
};

static void reset_host(void)
{
    mock_epoch = 1000;
    mock_monotonic = 500;
    memset(mock_state, 0, sizeof(mock_state));
    mock_state_size = 0;
    mock_write_count = 0;
    mock_alarm_active = false;
    mock_alarm_deadline = 0;
    mock_alarm_token = 0;
    mock_alarm_set_count = 0;
    mock_alarm_cancel_count = 0;
    mock_alarm_ack_count = 0;
    mock_toast_count = 0;
    mock_toast_text[0] = '\0';
}

static struct cp_input_event input_event(enum cp_input_type type,
                                         unsigned steps)
{
    struct cp_input_event event;

    memset(&event, 0, sizeof(event));
    event.struct_size = sizeof(event);
    event.type = (uint8_t)type;
    event.steps = (uint8_t)steps;
    return event;
}

static bool scene_has_text(const struct cp_scene *scene, const char *text)
{
    unsigned index;

    for(index = 0; index < scene->command_count; ++index) {
        const struct cp_draw_command *command = &scene->commands[index];

        if(command->type == CP_DRAW_TEXT &&
           strcmp(command->text, text) == 0)
            return true;
    }
    return false;
}

static const struct cp_draw_command *
scene_find(const struct cp_scene *scene, enum cp_draw_type type,
           int x, int y)
{
    unsigned index;

    for(index = 0; index < scene->command_count; ++index) {
        const struct cp_draw_command *command = &scene->commands[index];

        if(command->type == type && command->x == x && command->y == y)
            return command;
    }
    return NULL;
}

static void test_defaults(void)
{
    struct pomodoro_model model;

    pomodoro_model_init(&model);
    assert(model.config.focus_minutes == 25);
    assert(model.config.short_break_minutes == 5);
    assert(model.config.long_break_minutes == 15);
    assert(model.config.rounds == 4);
    assert(model.phase == POMODORO_PHASE_FOCUS);
    assert(model.run_state == POMODORO_READY);
    assert(model.remaining_seconds == 25u * 60u);
    assert(pomodoro_round_number(&model) == 1);
    assert(pomodoro_next_phase(&model) == POMODORO_PHASE_SHORT_BREAK);
}

static void test_pause_resume_and_manual_advance(void)
{
    struct pomodoro_model model;
    uint32_t first_token;

    pomodoro_model_init(&model);
    assert(pomodoro_start(&model, 1000));
    first_token = model.alarm_token;
    assert(first_token != 0);
    assert(model.deadline_epoch == 2500);
    assert(pomodoro_remaining_at(&model, 1100) == 1400);
    assert(pomodoro_pause(&model, 1100));
    assert(model.run_state == POMODORO_PAUSED);
    assert(model.remaining_seconds == 1400);
    assert(pomodoro_resume(&model, 2000));
    assert(model.alarm_token != first_token);
    assert(model.deadline_epoch == 3400);
    assert(!pomodoro_tick(&model, 3399));
    assert(pomodoro_tick(&model, 3400));
    assert(model.run_state == POMODORO_COMPLETE);
    assert(model.phase == POMODORO_PHASE_FOCUS);
    assert(model.focuses_in_cycle == 0);
    assert(pomodoro_advance(&model));
    assert(model.run_state == POMODORO_READY);
    assert(model.phase == POMODORO_PHASE_SHORT_BREAK);
    assert(model.focuses_in_cycle == 1);
    assert(model.remaining_seconds == 5u * 60u);
}

static void test_alarm_token_is_authoritative(void)
{
    struct pomodoro_model model;
    uint32_t token;

    pomodoro_model_init(&model);
    assert(pomodoro_start(&model, 50));
    token = model.alarm_token;
    assert(!pomodoro_alarm_fired(&model, token + 1u));
    assert(model.run_state == POMODORO_RUNNING);
    assert(pomodoro_alarm_fired(&model, token));
    assert(model.run_state == POMODORO_COMPLETE);
    assert(!pomodoro_advance(NULL));
}

static void test_four_round_cycle(void)
{
    struct pomodoro_model model;
    struct pomodoro_config config = { 1, 1, 2, 4 };
    uint32_t now = 100;
    unsigned round;

    pomodoro_model_init(&model);
    assert(pomodoro_model_set_config(&model, &config));
    for(round = 1; round <= 4; ++round) {
        enum pomodoro_phase expected_break =
            round == 4 ? POMODORO_PHASE_LONG_BREAK :
                         POMODORO_PHASE_SHORT_BREAK;

        assert(pomodoro_round_number(&model) == round);
        assert(pomodoro_start(&model, now));
        now += 60;
        assert(pomodoro_tick(&model, now));
        assert(model.phase == POMODORO_PHASE_FOCUS);
        assert(pomodoro_advance(&model));
        assert(model.phase == expected_break);
        assert(model.focuses_in_cycle == round);
        assert(pomodoro_start(&model, now));
        now += round == 4 ? 120 : 60;
        assert(pomodoro_tick(&model, now));
        assert(pomodoro_advance(&model));
        assert(model.phase == POMODORO_PHASE_FOCUS);
    }
    assert(model.focuses_in_cycle == 0);
    assert(pomodoro_round_number(&model) == 1);
}

static void test_config_skip_and_reset(void)
{
    struct pomodoro_model model;
    struct pomodoro_config config = { 30, 6, 20, 3 };
    struct pomodoro_config invalid = { 0, 5, 15, 4 };

    pomodoro_model_init(&model);
    assert(!pomodoro_model_set_config(&model, &invalid));
    assert(pomodoro_model_set_config(&model, &config));
    assert(model.remaining_seconds == 30u * 60u);
    assert(pomodoro_start(&model, 0));
    assert(pomodoro_skip(&model));
    assert(model.phase == POMODORO_PHASE_SHORT_BREAK);
    assert(model.focuses_in_cycle == 0);
    assert(model.remaining_seconds == 6u * 60u);
    assert(pomodoro_start(&model, 10));
    assert(pomodoro_reset(&model));
    assert(model.phase == POMODORO_PHASE_SHORT_BREAK);
    assert(model.run_state == POMODORO_READY);
    assert(model.remaining_seconds == 6u * 60u);
}

static void test_persistence_round_trip(void)
{
    struct pomodoro_model model;
    struct pomodoro_model loaded;
    struct pomodoro_disk_state disk;

    pomodoro_model_init(&model);
    assert(pomodoro_start(&model, 1234));
    pomodoro_pack(&model, &disk);
    memset(&loaded, 0, sizeof(loaded));
    assert(pomodoro_unpack(&loaded, &disk, sizeof(disk)));
    assert(loaded.run_state == POMODORO_RUNNING);
    assert(loaded.deadline_epoch == model.deadline_epoch);
    assert(loaded.alarm_token == model.alarm_token);
    disk.focus_minutes ^= 1u;
    assert(!pomodoro_unpack(&loaded, &disk, sizeof(disk)));
}

static void test_scene_a_and_background_deadline(void)
{
    const struct cp_miniapp_ops *ops;
    struct cp_scene scene;
    struct cp_input_event event;
    const struct cp_draw_command *panel;
    const struct cp_draw_command *ring;
    const struct cp_draw_command *progress;
    uint32_t deadline;
    uint32_t token;
    unsigned cancel_count;

    reset_host();
    ops = cp_miniapp_entry(&mock_host);
    assert(ops != NULL);
    assert(strcmp(ops->id, "pomodoro") == 0);
    ops->open();
    ops->render(&scene);

    panel = scene_find(&scene, CP_DRAW_RECT, 10, 40);
    assert(panel != NULL);
    assert(panel->width == 300 && panel->height == 188);
    assert(panel->radius == 12);
    ring = scene_find(&scene, CP_DRAW_RING, 22, 54);
    assert(ring != NULL);
    assert(ring->width == 152 && ring->height == 152);
    assert(ring->track_width == 5 && ring->progress_width == 7);
    progress = scene_find(&scene, CP_DRAW_PROGRESS, 43, 174);
    assert(progress != NULL);
    assert(progress->width == 110 && progress->height == 5);
    assert(progress->maximum == 25 * 60);
    assert(scene_has_text(&scene, "FOCUS"));
    assert(scene_has_text(&scene, "25:00"));
    assert(scene_has_text(&scene, "START"));
    assert(scene_has_text(&scene, "SETUP"));

    event = input_event(CP_INPUT_PLAY, 1);
    assert(ops->event(&event));
    assert(mock_alarm_active);
    assert(mock_alarm_set_count == 1);
    deadline = mock_alarm_deadline;
    token = mock_alarm_token;
    assert(deadline == mock_epoch + 25u * 60u);
    assert(token != 0);
    ops->render(&scene);
    assert(scene_has_text(&scene, "RUNNING"));
    assert(scene_has_text(&scene, "PAUSE"));
    assert(scene_has_text(&scene, "SKIP"));
    assert(scene_has_text(&scene, "RESET"));

    cancel_count = mock_alarm_cancel_count;
    ops->close();
    assert(mock_alarm_active);
    assert(mock_alarm_cancel_count == cancel_count);
    assert(mock_state_size == sizeof(struct pomodoro_disk_state));

    mock_epoch += 100;
    ops->open();
    assert(mock_alarm_active);
    assert(mock_alarm_deadline == deadline);
    assert(mock_alarm_token == token);
    ops->render(&scene);
    assert(scene_has_text(&scene, "23:20"));

    mock_epoch = deadline;
    assert(ops->tick(mock_epoch, mock_monotonic));
    assert(mock_alarm_ack_count == 1);
    assert(!mock_alarm_active);
    ops->render(&scene);
    assert(scene_has_text(&scene, "COMPLETE"));
    assert(scene_has_text(&scene, "NEXT"));
    assert(scene_has_text(&scene, "RESET"));

    assert(ops->event(&event));
    ops->render(&scene);
    assert(scene_has_text(&scene, "SHORT BREAK"));
    assert(scene_has_text(&scene, "5:00"));
    assert(scene_has_text(&scene, "READY"));
}

static void test_setup_edits_every_field(void)
{
    const struct cp_miniapp_ops *ops;
    struct cp_input_event clockwise;
    struct cp_input_event counterclockwise;
    struct cp_input_event select;
    struct cp_input_event play;
    struct cp_scene scene;
    struct pomodoro_model loaded;

    reset_host();
    ops = cp_miniapp_entry(&mock_host);
    assert(ops != NULL);
    ops->open();

    clockwise = input_event(CP_INPUT_WHEEL_CLOCKWISE, 1);
    counterclockwise =
        input_event(CP_INPUT_WHEEL_COUNTERCLOCKWISE, 1);
    select = input_event(CP_INPUT_SELECT, 1);
    play = input_event(CP_INPUT_PLAY, 1);

    assert(ops->event(&clockwise)); /* Setup action. */
    assert(ops->event(&select));    /* Enter Setup. */
    ops->render(&scene);
    assert(scene_has_text(&scene, "FOCUS"));
    assert(scene_has_text(&scene, "SHORT BREAK"));
    assert(scene_has_text(&scene, "LONG BREAK"));
    assert(scene_has_text(&scene, "ROUNDS"));
    assert(scene_has_text(&scene, "DONE"));

    assert(ops->event(&select)); /* Edit focus. */
    clockwise.steps = 5;
    assert(ops->event(&clockwise));
    assert(ops->event(&select));

    clockwise.steps = 1;
    assert(ops->event(&clockwise)); /* Short. */
    assert(ops->event(&select));
    clockwise.steps = 2;
    assert(ops->event(&clockwise));
    assert(ops->event(&select));

    clockwise.steps = 1;
    assert(ops->event(&clockwise)); /* Long. */
    assert(ops->event(&select));
    clockwise.steps = 5;
    assert(ops->event(&clockwise));
    assert(ops->event(&select));

    clockwise.steps = 1;
    assert(ops->event(&clockwise)); /* Rounds. */
    assert(ops->event(&select));
    assert(ops->event(&counterclockwise));
    assert(ops->event(&select));

    assert(ops->event(&play)); /* Done is always the Setup main action. */
    assert(mock_toast_count == 1);
    assert(strcmp(mock_toast_text, "Settings saved") == 0);
    ops->render(&scene);
    assert(scene_has_text(&scene, "30:00"));
    assert(scene_has_text(&scene, "1 OF 3"));

    assert(mock_state_size == sizeof(struct pomodoro_disk_state));
    assert(pomodoro_unpack(
        &loaded, (const struct pomodoro_disk_state *)mock_state,
        mock_state_size));
    assert(loaded.config.focus_minutes == 30);
    assert(loaded.config.short_break_minutes == 7);
    assert(loaded.config.long_break_minutes == 20);
    assert(loaded.config.rounds == 3);
}

static void test_accelerated_action_always_moves(void)
{
    const struct cp_miniapp_ops *ops;
    struct cp_input_event clockwise;
    struct cp_input_event select;
    struct cp_scene scene;

    reset_host();
    ops = cp_miniapp_entry(&mock_host);
    assert(ops != NULL);
    ops->open();

    clockwise = input_event(CP_INPUT_WHEEL_CLOCKWISE, 4);
    select = input_event(CP_INPUT_SELECT, 1);
    assert(ops->event(&clockwise));
    assert(ops->event(&select));
    ops->render(&scene);
    assert(scene_has_text(&scene, "DONE"));
    assert(!mock_alarm_active);
}

static void test_accelerated_setup_navigation_is_discrete(void)
{
    const struct cp_miniapp_ops *ops;
    const struct cp_draw_command *row;
    struct cp_input_event clockwise;
    struct cp_input_event select;
    struct cp_input_event play;
    struct cp_scene scene;
    struct pomodoro_model loaded;

    reset_host();
    ops = cp_miniapp_entry(&mock_host);
    assert(ops != NULL);
    ops->open();

    clockwise = input_event(CP_INPUT_WHEEL_CLOCKWISE, 4);
    select = input_event(CP_INPUT_SELECT, 1);
    play = input_event(CP_INPUT_PLAY, 1);
    assert(ops->event(&clockwise)); /* Setup action: one row. */
    assert(ops->event(&select));    /* Enter Setup. */

    assert(ops->event(&clockwise)); /* Short: one row despite steps=4. */
    ops->render(&scene);
    row = scene_find(&scene, CP_DRAW_RECT, 20, 84);
    assert(row != NULL);
    assert((row->flags & CP_DRAW_FOCUSED) != 0);
    row = scene_find(&scene, CP_DRAW_RECT, 20, 186);
    assert(row != NULL);
    assert((row->flags & CP_DRAW_FOCUSED) == 0);

    assert(ops->event(&select)); /* Edit Short. */
    clockwise.steps = 5;
    assert(ops->event(&clockwise)); /* Value editing keeps acceleration. */
    assert(ops->event(&select));
    assert(ops->event(&play));

    assert(pomodoro_unpack(
        &loaded, (const struct pomodoro_disk_state *)mock_state,
        mock_state_size));
    assert(loaded.config.short_break_minutes == 10);
}

int main(void)
{
    test_defaults();
    test_pause_resume_and_manual_advance();
    test_alarm_token_is_authoritative();
    test_four_round_cycle();
    test_config_skip_and_reset();
    test_persistence_round_trip();
    test_scene_a_and_background_deadline();
    test_setup_edits_every_field();
    test_accelerated_action_always_moves();
    test_accelerated_setup_navigation_is_discrete();
    puts("pomodoro tests: ok");
    return 0;
}
