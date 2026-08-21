#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "buflib.h"
#include "crazypod_audio_memory_policy.h"

#define TEST_POOL_SIZE (8u * 1024u * 1024u)
#define LATE_ALLOCATION_COUNT 64u
#define LATE_ALLOCATION_SIZE (32u * 1024u)
#define OVERSIZED_ALLOCATION (3u * 1024u * 1024u)

static union buflib_data test_pool[
    TEST_POOL_SIZE / sizeof(union buflib_data)];
static int reserved_handles[CRAZYPOD_RUNTIME_HANDLE_HEADROOM];
static int late_handles[LATE_ALLOCATION_COUNT];
static bool playback_running;
static int shrink_requests;

void __assert(const char *file, int line, const char *expression)
{
    fprintf(stderr, "%s:%d: assertion failed: %s\n",
        file, line, expression);
    abort();
}

void panicf(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    abort();
}

static int audio_shrink(
    int handle, unsigned hints, void *start, size_t size)
{
    (void)handle;
    (void)hints;
    (void)start;
    (void)size;
    ++shrink_requests;
    return crazypod_audio_buffer_may_shrink(playback_running)
        ? BUFLIB_CB_OK : BUFLIB_CB_CANNOT_SHRINK;
}

int main(void)
{
    static struct buflib_callbacks audio_ops = {
        .move_callback = NULL,
        .shrink_callback = audio_shrink,
        .sync_callback = NULL,
    };
    struct buflib_context context;
    unsigned char *audio_data;
    size_t audio_size;
    size_t headroom;
    unsigned index;
    int headroom_handle;
    int audio_handle;
    int oversized_handle;

    buflib_init(&context, test_pool, sizeof(test_pool));
    for (index = 0; index < CRAZYPOD_RUNTIME_HANDLE_HEADROOM; ++index)
    {
        reserved_handles[index] = buflib_alloc(&context, 1);
        assert(reserved_handles[index] > 0);
    }
    headroom = crazypod_audio_runtime_headroom(
        buflib_allocatable(&context));
    assert(headroom > LATE_ALLOCATION_COUNT * LATE_ALLOCATION_SIZE);
    headroom_handle = buflib_alloc_ex(
        &context, headroom, &buflib_ops_locked);
    assert(headroom_handle > 0);
    audio_handle = buflib_alloc_maximum(
        &context, &audio_size, &audio_ops);
    assert(audio_handle > 0);
    assert(audio_size >= 3u * 1024u * 1024u);
    buflib_free(&context, headroom_handle);
    while (index > 0)
        buflib_free(&context, reserved_handles[--index]);

    audio_data = buflib_get_data(&context, audio_handle);
    audio_data[0] = 0x5a;
    playback_running = true;
    for (index = 0; index < LATE_ALLOCATION_COUNT; ++index)
    {
        late_handles[index] = buflib_alloc(
            &context, LATE_ALLOCATION_SIZE);
        assert(late_handles[index] > 0);
    }
    assert(shrink_requests == 0);

    oversized_handle = buflib_alloc(
        &context, OVERSIZED_ALLOCATION);
    assert(oversized_handle < 0);
    assert(shrink_requests > 0);
    assert(buflib_get_data(&context, audio_handle) == audio_data);
    assert(audio_data[0] == 0x5a);

    puts("crazypod audio buflib integration tests passed");
    return 0;
}
