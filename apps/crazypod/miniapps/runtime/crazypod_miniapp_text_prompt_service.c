#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "string-extra.h"

#include "../../../../miniapps/sdk/crazypod_miniapp_native.h"
#include "crazypod_miniapp_text_prompt_service.h"

#define TEXT_PROMPT_CHOICE_COUNT 40
#define TEXT_PROMPT_SPACE 36
#define TEXT_PROMPT_BACKSPACE 37
#define TEXT_PROMPT_CANCEL 38
#define TEXT_PROMPT_DONE 39

static const char prompt_characters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static struct {
    char title[CP_TEXT_PROMPT_TITLE_CAPACITY];
    char value[CP_TEXT_PROMPT_VALUE_CAPACITY];
    uint32_t maximum_bytes;
    int choice;
    bool active;
    bool completed;
    bool accepted;
} prompt;

void crazypod_miniapp_text_prompt_reset(void)
{
    memset(&prompt, 0, sizeof(prompt));
}

bool crazypod_miniapp_text_prompt_visible(void)
{
    return prompt.active;
}

const char *crazypod_miniapp_text_prompt_title(void)
{
    return prompt.title;
}

const char *crazypod_miniapp_text_prompt_value(void)
{
    return prompt.value;
}

const char *crazypod_miniapp_text_prompt_choice(void)
{
    static char character[2];

    if(prompt.choice >= 0 && prompt.choice < TEXT_PROMPT_SPACE) {
        character[0] = prompt_characters[prompt.choice];
        character[1] = '\0';
        return character;
    }
    if(prompt.choice == TEXT_PROMPT_SPACE)
        return "SPACE";
    if(prompt.choice == TEXT_PROMPT_BACKSPACE)
        return "DELETE";
    if(prompt.choice == TEXT_PROMPT_CANCEL)
        return "CANCEL";
    return "DONE";
}

void crazypod_miniapp_text_prompt_move(int steps)
{
    if(!prompt.active)
        return;
    prompt.choice = (prompt.choice + steps) % TEXT_PROMPT_CHOICE_COUNT;
    if(prompt.choice < 0)
        prompt.choice += TEXT_PROMPT_CHOICE_COUNT;
}

static void finish(bool accepted)
{
    prompt.active = false;
    prompt.completed = true;
    prompt.accepted = accepted;
}

void crazypod_miniapp_text_prompt_cancel(void)
{
    if(prompt.active)
        finish(false);
}

static void backspace(void)
{
    size_t length = strlen(prompt.value);

    if(length == 0)
        return;
    do {
        --length;
    } while(length > 0 &&
            ((uint8_t)prompt.value[length] & 0xc0u) == 0x80u);
    prompt.value[length] = '\0';
}

void crazypod_miniapp_text_prompt_select(void)
{
    size_t length;

    if(!prompt.active)
        return;
    if(prompt.choice == TEXT_PROMPT_BACKSPACE) {
        backspace();
        return;
    }
    if(prompt.choice == TEXT_PROMPT_CANCEL) {
        finish(false);
        return;
    }
    if(prompt.choice == TEXT_PROMPT_DONE) {
        finish(true);
        return;
    }
    length = strlen(prompt.value);
    if(length >= prompt.maximum_bytes)
        return;
    prompt.value[length] = prompt.choice == TEXT_PROMPT_SPACE
        ? ' ' : prompt_characters[prompt.choice];
    prompt.value[length + 1] = '\0';
}

static int start_prompt(
    const void *request, size_t request_size)
{
    const struct cp_text_prompt_request *start = request;

    if(start == NULL || request_size != sizeof(*start) ||
       start->struct_size != sizeof(*start) ||
       start->maximum_bytes == 0 ||
       start->maximum_bytes >= CP_TEXT_PROMPT_VALUE_CAPACITY ||
       memchr(start->title, '\0', sizeof(start->title)) == NULL ||
       memchr(start->initial_value, '\0',
              sizeof(start->initial_value)) == NULL ||
       strlen(start->initial_value) > start->maximum_bytes)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(prompt.active)
        return CP_NATIVE_ERROR_STATE;
    memset(&prompt, 0, sizeof(prompt));
    strlcpy(prompt.title, start->title, sizeof(prompt.title));
    strlcpy(prompt.value, start->initial_value, sizeof(prompt.value));
    prompt.maximum_bytes = start->maximum_bytes;
    prompt.active = true;
    return CP_NATIVE_OK;
}

static int poll_prompt(void *response, size_t response_capacity)
{
    struct cp_text_prompt_result result;

    if(response == NULL || response_capacity < sizeof(result))
        return CP_NATIVE_ERROR_ARGUMENT;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.active = prompt.active;
    result.completed = prompt.completed;
    result.accepted = prompt.accepted;
    strlcpy(result.value, prompt.value, sizeof(result.value));
    memcpy(response, &result, sizeof(result));
    prompt.completed = false;
    return (int)sizeof(result);
}

int crazypod_miniapp_text_prompt_service_call(
    uint32_t operation, const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    if(operation == CP_NATIVE_TEXT_PROMPT_START) {
        if(response != NULL || response_capacity != 0)
            return CP_NATIVE_ERROR_ARGUMENT;
        return start_prompt(request, request_size);
    }
    if(operation == CP_NATIVE_TEXT_PROMPT_POLL) {
        if(request != NULL || request_size != 0)
            return CP_NATIVE_ERROR_ARGUMENT;
        return poll_prompt(response, response_capacity);
    }
    if(operation == CP_NATIVE_TEXT_PROMPT_CANCEL) {
        if(request != NULL || request_size != 0 ||
           response != NULL || response_capacity != 0)
            return CP_NATIVE_ERROR_ARGUMENT;
        crazypod_miniapp_text_prompt_cancel();
        return CP_NATIVE_OK;
    }
    return CP_NATIVE_ERROR_UNSUPPORTED;
}

#endif
