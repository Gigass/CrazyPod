#include <stddef.h>
struct buflib_callbacks { int unused; };
extern struct buflib_callbacks buflib_ops_locked;
int core_alloc_ex(size_t size, struct buflib_callbacks *ops);
void *core_get_data(int handle);
int core_free(int handle);
