#ifndef CRAZYPOD_COLLATION_H
#define CRAZYPOD_COLLATION_H

#include <stdbool.h>

typedef const char *(*crazypod_collation_title_provider)(
    int index, void *context);

char crazypod_collation_initial(const char *text);
int crazypod_collation_compare(const char *left, const char *right);
bool crazypod_collation_section_target(
    int count, int current, int direction,
    crazypod_collation_title_provider title_at,
    void *context, int *target, char *key);

#endif
