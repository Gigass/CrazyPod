#ifndef CRAZYPOD_CPK_VERIFIER_H
#define CRAZYPOD_CPK_VERIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crazypod_cpk_reader.h"

bool crazypod_cpk_assets_valid(const struct cpk_reader *reader);
bool crazypod_cpk_profile_valid(const struct cpk_reader *reader);
int crazypod_cpk_verify_crc(
    const struct cpk_reader *reader, int entry);
int crazypod_cpk_read_entry(
    const struct cpk_reader *reader, int entry,
    void *buffer, size_t capacity);
bool crazypod_cpk_icon_valid(const struct cpk_reader *reader);
int crazypod_cpk_extract_entry(
    const struct cpk_reader *reader, int entry,
    const char *path);

#endif
