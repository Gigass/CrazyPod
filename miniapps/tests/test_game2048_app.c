#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../game2048/engine.h"
#include "../sdk/crazypod_miniapp.h"

extern const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host);

static uint32_t mock_epoch = 1000;
static uint32_t mock_monotonic = 500;
static unsigned char mock_state[sizeof(struct game2048_disk_state)];
static size_t mock_state_size;
static unsigned write_count;

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
    ++write_count;
    return 0;
}

static void host_format_duration(uint32_t seconds, char *buffer,
                                 size_t capacity)
{
    snprintf(buffer, capacity, "%lu:%02lu",
             (unsigned long)(seconds / 60u),
             (unsigned long)(seconds % 60u));
}

static void host_format_datetime(
    uint32_t epoch_seconds, enum cp_datetime_format format,
    char *buffer, size_t capacity)
{
    (void)format;
    snprintf(buffer, capacity, "T%lu",
             (unsigned long)epoch_seconds);
}

static const struct cp_host_api mock_host = {
    .abi_version = CP_MINIAPP_ABI_VERSION,
    .struct_size = sizeof(struct cp_host_api),
    .epoch_seconds = host_epoch_seconds,
    .monotonic_ms = host_monotonic_ms,
    .state_read = host_state_read,
    .state_write = host_state_write,
    .capabilities =
        CP_CAP_FORMAT_DURATION |
        CP_CAP_FORMAT_DATETIME,
    .format_duration = host_format_duration,
    .format_datetime = host_format_datetime
};

static struct cp_input_event input_event(enum cp_input_type type)
{
    struct cp_input_event event;

    memset(&event, 0, sizeof(event));
    event.struct_size = sizeof(event);
    event.type = (uint8_t)type;
    event.steps = 1;
    return event;
}

static bool scene_has_text(const struct cp_scene *scene, const char *text)
{
    unsigned index;

    for(index = 0; index < scene->command_count; ++index)
        if(scene->commands[index].type == CP_DRAW_TEXT &&
           strcmp(scene->commands[index].text, text) == 0)
            return true;
    return false;
}

static void render_and_expect(
    const struct cp_miniapp_ops *ops,
    struct cp_scene *scene, const char *text)
{
    ops->render(scene);
    assert(scene->command_count <= CP_MINIAPP_MAX_COMMANDS);
    assert(scene_has_text(scene, text));
}

int main(void)
{
    const struct cp_miniapp_ops *ops =
        cp_miniapp_entry(&mock_host);
    struct cp_input_event event;
    struct cp_scene scene;
    unsigned index;

    assert(ops != NULL);
    assert(strcmp(ops->id, "game2048") == 0);
    ops->open();
    render_and_expect(ops, &scene, "NEW GAME");

    event = input_event(CP_INPUT_SELECT);
    assert(ops->event(&event));
    assert(write_count > 0);
    render_and_expect(ops, &scene, "SCORE 0");

    assert(ops->event(&event));
    render_and_expect(ops, &scene, "PAUSED");

    event = input_event(CP_INPUT_WHEEL_CLOCKWISE);
    for(index = 0; index < 4; ++index)
        assert(ops->event(&event));
    event = input_event(CP_INPUT_SELECT);
    assert(ops->event(&event));
    render_and_expect(ops, &scene, "SETTINGS");

    assert(ops->event(&event));
    event = input_event(CP_INPUT_MENU);
    assert(ops->event(&event));
    render_and_expect(ops, &scene, "PAUSED");
    assert(ops->event(&event));
    render_and_expect(ops, &scene, "SCORE 0");

    mock_monotonic = 2500;
    ops->close();
    assert(mock_state_size == sizeof(struct game2048_disk_state));

    ops->open();
    render_and_expect(ops, &scene, "CONTINUE");

    event = input_event(CP_INPUT_SELECT);
    assert(ops->event(&event));
    render_and_expect(ops, &scene, "SCORE 0");
    assert(ops->event(&event));
    render_and_expect(ops, &scene, "PAUSED");

    event = input_event(CP_INPUT_WHEEL_CLOCKWISE);
    assert(ops->event(&event));
    assert(ops->event(&event));
    event = input_event(CP_INPUT_SELECT);
    assert(ops->event(&event));
    render_and_expect(ops, &scene, "END THIS GAME?");
    event = input_event(CP_INPUT_WHEEL_CLOCKWISE);
    assert(ops->event(&event));
    event = input_event(CP_INPUT_SELECT);
    assert(ops->event(&event));
    render_and_expect(ops, &scene, "GAME OVER");
    assert(scene_has_text(&scene, "ABANDONED"));

    puts("game2048 app tests passed");
    return 0;
}
