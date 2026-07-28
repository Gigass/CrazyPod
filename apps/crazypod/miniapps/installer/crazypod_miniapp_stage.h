#ifndef CRAZYPOD_MINIAPP_STAGE_H
#define CRAZYPOD_MINIAPP_STAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "crazypod_cpk_reader.h"
#include "../../crazypod_miniapps.h"

void crazypod_miniapp_stage_recover_all(void);
bool crazypod_miniapp_stage_recover_id(const char *id);
bool crazypod_miniapp_stage_has_space(uint32_t extracted_size);
int crazypod_miniapp_stage_publish(
    const struct cpk_reader *reader,
    const struct crazypod_miniapp_metadata *metadata,
    struct crazypod_miniapp_metadata *verified_metadata);

#endif
