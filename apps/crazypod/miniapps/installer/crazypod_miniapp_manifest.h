#ifndef CRAZYPOD_MINIAPP_MANIFEST_H
#define CRAZYPOD_MINIAPP_MANIFEST_H

#include <stddef.h>
#include <stdbool.h>

#include "../../crazypod_miniapps.h"

#define CRAZYPOD_MINIAPP_MANIFEST_MAX CP_NATIVE_MANIFEST_MAX

int crazypod_miniapp_manifest_parse(
    char *buffer, size_t size,
    struct crazypod_miniapp_metadata *metadata);
bool crazypod_miniapp_text_valid(
    const char *text, bool allow_space);

#endif
