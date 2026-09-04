#ifndef CRAZYPOD_BOOKS_TEST_KERNEL_H
#define CRAZYPOD_BOOKS_TEST_KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_STACK_SIZE 4096
#define IF_PRIO(...)
#define IF_COP(...)

struct queue_event {
    long id;
    intptr_t data;
};

struct event_queue {
    int unused;
};

static inline void queue_init(struct event_queue *queue, bool unique)
{
    (void)queue;
    (void)unique;
}

static inline void queue_wait(
    struct event_queue *queue, struct queue_event *event)
{
    (void)queue;
    event->id = 0;
    event->data = 0;
}

static inline void queue_post(
    struct event_queue *queue, long id, intptr_t data)
{
    (void)queue;
    (void)id;
    (void)data;
}

static inline unsigned int create_thread(
    void (*function)(void), void *stack, size_t stack_size,
    int flags, const char *name)
{
    (void)function;
    (void)stack;
    (void)stack_size;
    (void)flags;
    (void)name;
    return 1;
}

static inline void sleep(int ticks)
{
    (void)ticks;
}

static inline void yield(void)
{
}

#endif
