#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../apps/crazypod/crazypod_miniapp_input.h"
#include "../sdk/crazypod_miniapp.h"

extern const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host);

static void host_format_number(double value, char *buffer, size_t capacity)
{
    snprintf(buffer, capacity, "%.12g", value);
}

static const struct cp_host_api mock_host = {
    .abi_version = CP_MINIAPP_ABI_VERSION,
    .struct_size = sizeof(struct cp_host_api),
    .format_number = host_format_number
};

static struct cp_input_event wheel_event(enum cp_input_type type,
                                         unsigned steps)
{
    struct cp_input_event event;

    memset(&event, 0, sizeof(event));
    event.struct_size = sizeof(event);
    event.type = (uint8_t)type;
    event.steps = (uint8_t)steps;
    return event;
}

static int focused_key(const struct cp_scene *scene)
{
    unsigned index;

    for(index = 0; index < scene->command_count; ++index) {
        const struct cp_draw_command *command = &scene->commands[index];
        int row;
        int column;

        if(command->type != CP_DRAW_RECT ||
           (command->flags & CP_DRAW_FOCUSED) == 0)
            continue;
        row = (command->y - 116) / 22;
        column = (command->x - 16) / 73;
        if(row >= 0 && row < 5 && column >= 0 && column < 4)
            return row * 4 + column;
    }
    return -1;
}

static void test_burst_is_presented_one_step_per_frame(void)
{
    const struct cp_miniapp_ops *ops = cp_miniapp_entry(&mock_host);
    struct crazypod_miniapp_input_queue queue;
    struct cp_input_event input =
        wheel_event(CP_INPUT_WHEEL_CLOCKWISE, 4);
    struct cp_input_event next;
    struct cp_scene scene;
    int frame;

    assert(ops != NULL);
    crazypod_miniapp_input_reset(&queue);
    ops->open();
    ops->render(&scene);
    assert(focused_key(&scene) == 0);

    for(frame = 0; frame < CRAZYPOD_MINIAPP_INPUT_CAPACITY; ++frame)
        assert(crazypod_miniapp_input_push_wheel(&queue, &input));
    assert(crazypod_miniapp_input_count(&queue) ==
           CRAZYPOD_MINIAPP_INPUT_CAPACITY);

    assert(!crazypod_miniapp_input_next(&queue, false, &next));
    ops->render(&scene);
    assert(focused_key(&scene) == 0);

    for(frame = 1; frame <= CRAZYPOD_MINIAPP_INPUT_CAPACITY; ++frame) {
        assert(crazypod_miniapp_input_next(&queue, true, &next));
        assert(next.steps == 4);
        assert(ops->event(&next));
        ops->render(&scene);
        assert(focused_key(&scene) == frame);
        assert(!crazypod_miniapp_input_next(&queue, false, &next));
    }
    assert(crazypod_miniapp_input_count(&queue) == 0);
    ops->close();
}

static void test_queue_is_bounded_and_reversal_cancels(void)
{
    struct crazypod_miniapp_input_queue queue;
    struct cp_input_event clockwise =
        wheel_event(CP_INPUT_WHEEL_CLOCKWISE, 1);
    struct cp_input_event counterclockwise =
        wheel_event(CP_INPUT_WHEEL_COUNTERCLOCKWISE, 1);
    struct cp_input_event next;
    unsigned index;

    crazypod_miniapp_input_reset(&queue);
    for(index = 0; index < CRAZYPOD_MINIAPP_INPUT_CAPACITY; ++index)
        assert(crazypod_miniapp_input_push_wheel(&queue, &clockwise));
    assert(!crazypod_miniapp_input_push_wheel(&queue, &clockwise));
    assert(crazypod_miniapp_input_count(&queue) ==
           CRAZYPOD_MINIAPP_INPUT_CAPACITY);

    assert(crazypod_miniapp_input_push_wheel(
        &queue, &counterclockwise));
    assert(crazypod_miniapp_input_count(&queue) == 1);
    assert(crazypod_miniapp_input_next(&queue, true, &next));
    assert(next.type == CP_INPUT_WHEEL_COUNTERCLOCKWISE);

    crazypod_miniapp_input_reset(&queue);
    assert(crazypod_miniapp_input_count(&queue) == 0);
}

int main(void)
{
    test_burst_is_presented_one_step_per_frame();
    test_queue_is_bounded_and_reversal_cancels();
    puts("miniapp input pacing tests passed");
    return 0;
}
