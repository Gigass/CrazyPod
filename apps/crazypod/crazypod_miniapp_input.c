#include "crazypod_miniapp_input.h"

static bool wheel_type(uint8_t type)
{
    return type == CP_INPUT_WHEEL_CLOCKWISE ||
           type == CP_INPUT_WHEEL_COUNTERCLOCKWISE;
}

static bool opposite_wheel(uint8_t first, uint8_t second)
{
    return wheel_type(first) && wheel_type(second) && first != second;
}

void crazypod_miniapp_input_reset(
    struct crazypod_miniapp_input_queue *queue)
{
    if(queue == NULL)
        return;
    queue->head = 0;
    queue->count = 0;
}

bool crazypod_miniapp_input_push_wheel(
    struct crazypod_miniapp_input_queue *queue,
    const struct cp_input_event *event)
{
    unsigned tail;

    if(queue == NULL || event == NULL ||
       event->struct_size < sizeof(*event) ||
       !wheel_type(event->type) || event->steps == 0)
        return false;

    if(queue->count > 0) {
        unsigned last = (queue->head + queue->count - 1u) %
                        CRAZYPOD_MINIAPP_INPUT_CAPACITY;

        if(opposite_wheel(queue->events[last].type, event->type)) {
            queue->head = 0;
            queue->count = 0;
        }
    }

    if(queue->count >= CRAZYPOD_MINIAPP_INPUT_CAPACITY)
        return false;

    tail = (queue->head + queue->count) %
           CRAZYPOD_MINIAPP_INPUT_CAPACITY;
    queue->events[tail] = *event;
    ++queue->count;
    return true;
}

bool crazypod_miniapp_input_next(
    struct crazypod_miniapp_input_queue *queue, bool frame_due,
    struct cp_input_event *event)
{
    if(queue == NULL || event == NULL || !frame_due ||
       queue->count == 0)
        return false;

    *event = queue->events[queue->head];
    queue->head = (queue->head + 1u) %
                  CRAZYPOD_MINIAPP_INPUT_CAPACITY;
    --queue->count;
    if(queue->count == 0)
        queue->head = 0;
    return true;
}

unsigned crazypod_miniapp_input_count(
    const struct crazypod_miniapp_input_queue *queue)
{
    return queue == NULL ? 0 : queue->count;
}
