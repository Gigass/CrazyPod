#ifndef CRAZYPOD_COLLATION_H
#define CRAZYPOD_COLLATION_H

#include <stdbool.h>

typedef const char *(*crazypod_collation_title_provider)(
    int index, void *context);

/* Case-insensitive substring match over ASCII letters, which is what a
 * wheel-typed search query is. An empty query matches everything. */
bool crazypod_collation_contains(const char *text, const char *query);
char crazypod_collation_initial(const char *text);
int crazypod_collation_compare(const char *left, const char *right);
bool crazypod_collation_section_target(
    int count, int current, int direction,
    crazypod_collation_title_provider title_at,
    void *context, int *target, char *key);

#endif
