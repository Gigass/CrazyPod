#include <time.h>
time_t gb_test_time(time_t *value);
struct tm *get_time(void);
#define time gb_test_time
