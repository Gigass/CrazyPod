#ifndef TEST_CRAZYPOD_CORE_ALLOC_H
#define TEST_CRAZYPOD_CORE_ALLOC_H

#include <stdbool.h>
#include <stddef.h>

int core_alloc(size_t size);
int core_free(int handle);
bool core_shrink(int handle, void *new_start, size_t new_size);
void core_pin(int handle);
void core_unpin(int handle);
void *core_get_data(int handle);

int test_core_alloc_active_handles(void);
int test_core_alloc_pin_count(void);
void test_core_alloc_fail_next(void);

#endif
