#ifndef CRAZYPOD_MINIAPP_MODAL_H
#define CRAZYPOD_MINIAPP_MODAL_H

#include <stdbool.h>
#include <stdint.h>

#include "../../crazypod_miniapps.h"

void crazypod_miniapp_modal_open(void);
void crazypod_miniapp_modal_close(void);
int crazypod_miniapp_modal_text_input(
    uint32_t request_id, const char *title,
    const char *initial_value, uint16_t max_bytes);
int crazypod_miniapp_modal_choice(
    uint32_t request_id, const char *title,
    const struct cp_ui_choice_item *items,
    uint16_t item_count, int16_t selected_index);
int crazypod_miniapp_modal_confirm(
    uint32_t request_id, const char *title,
    const char *message, const char *confirm_label);
int crazypod_miniapp_modal_poll(struct cp_ui_result *result);
int crazypod_miniapp_modal_cancel(uint32_t request_id);
bool crazypod_miniapp_modal_event(const struct cp_input_event *event);
void crazypod_miniapp_modal_render(struct cp_scene *scene);
bool crazypod_miniapp_modal_take_changed(void);

#endif
