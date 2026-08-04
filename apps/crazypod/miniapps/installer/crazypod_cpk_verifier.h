#ifndef CRAZYPOD_CPK_VERIFIER_H
#define CRAZYPOD_CPK_VERIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crazypod_cpk_reader.h"

struct crazypod_miniapp_metadata;

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
int crazypod_cpk_verify_trust(
    const struct cpk_reader *reader,
    const struct crazypod_miniapp_metadata *metadata,
    const char *manifest, size_t manifest_size,
    bool allow_unsigned);

#endif
