#ifndef CRAZYPOD_MINIAPP_TEXT_PROMPT_SERVICE_H
#define CRAZYPOD_MINIAPP_TEXT_PROMPT_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int crazypod_miniapp_text_prompt_service_call(
    uint32_t operation, const void *request, size_t request_size,
    void *response, size_t response_capacity);
void crazypod_miniapp_text_prompt_reset(void);
bool crazypod_miniapp_text_prompt_visible(void);
void crazypod_miniapp_text_prompt_move(int steps);
void crazypod_miniapp_text_prompt_select(void);
void crazypod_miniapp_text_prompt_cancel(void);
const char *crazypod_miniapp_text_prompt_title(void);
const char *crazypod_miniapp_text_prompt_value(void);
const char *crazypod_miniapp_text_prompt_choice(void);

#endif
