#include <assert.h>
#include <stdlib.h>

#include "core_alloc.h"

#define TEST_CORE_HANDLE_COUNT 64

struct test_core_block {
    void *data;
    int pins;
};

static struct test_core_block blocks[TEST_CORE_HANDLE_COUNT];
static bool fail_next_alloc;

static bool valid_handle(int handle)
{
    return handle > 0 && handle < TEST_CORE_HANDLE_COUNT &&
        blocks[handle].data != NULL;
}

int core_alloc(size_t size)
{
    int handle;

    if(fail_next_alloc) {
        fail_next_alloc = false;
        return -1;
    }

    for(handle = 1; handle < TEST_CORE_HANDLE_COUNT; ++handle) {
        if(blocks[handle].data == NULL) {
            blocks[handle].data = malloc(size);
            blocks[handle].pins = 0;
            return blocks[handle].data != NULL ? handle : -1;
        }
    }
    return -1;
}

int core_free(int handle)
{
    if(!valid_handle(handle))
        return 0;
    assert(blocks[handle].pins == 0);
    free(blocks[handle].data);
    blocks[handle].data = NULL;
    return 0;
}

bool core_shrink(int handle, void *new_start, size_t new_size)
{
    void *data;

    if(!valid_handle(handle) || new_start != NULL ||
       blocks[handle].pins != 0 || new_size == 0)
        return false;
    data = realloc(blocks[handle].data, new_size);
    if(data == NULL)
        return false;
    blocks[handle].data = data;
    return true;
}

void core_pin(int handle)
{
    assert(valid_handle(handle));
    ++blocks[handle].pins;
}

void core_unpin(int handle)
{
    assert(valid_handle(handle));
    assert(blocks[handle].pins > 0);
    --blocks[handle].pins;
}

void *core_get_data(int handle)
{
    assert(valid_handle(handle));
    return blocks[handle].data;
}

int test_core_alloc_active_handles(void)
{
    int count = 0;
    int handle;

    for(handle = 1; handle < TEST_CORE_HANDLE_COUNT; ++handle) {
        if(blocks[handle].data != NULL)
            ++count;
    }
    return count;
}

int test_core_alloc_pin_count(void)
{
    int count = 0;
    int handle;

    for(handle = 1; handle < TEST_CORE_HANDLE_COUNT; ++handle)
        count += blocks[handle].pins;
    return count;
}

void test_core_alloc_fail_next(void)
{
    fail_next_alloc = true;
}
