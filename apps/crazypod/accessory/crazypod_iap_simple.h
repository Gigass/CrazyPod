#ifndef CRAZYPOD_IAP_SIMPLE_H
#define CRAZYPOD_IAP_SIMPLE_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

#ifdef IPOD_ACCESSORY_PROTOCOL
struct crazypod_iap_diagnostics {
    uint32_t received_frames;
    uint32_t checksum_errors;
    uint32_t frame_timeouts;
    uint32_t queue_overflows;
    uint32_t remote_state_overflows;
    uint32_t uart_overruns;
    uint32_t uart_parity_errors;
    uint32_t uart_frame_errors;
    uint32_t uart_breaks;
    uint32_t uart_tx_timeouts;
};

bool crazypod_iap_simple_handle_event(long event, intptr_t data);
bool crazypod_iap_simple_accessory_present(void);
bool crazypod_iap_simple_dock_connected(void);
bool crazypod_iap_simple_take_dock_connected(void);
void crazypod_iap_simple_get_diagnostics(
    struct crazypod_iap_diagnostics *diagnostics);
#else
static inline bool crazypod_iap_simple_handle_event(
    long event, intptr_t data)
{
    (void)event;
    (void)data;
    return false;
}
static inline bool crazypod_iap_simple_accessory_present(void)
{
    return false;
}
static inline bool crazypod_iap_simple_dock_connected(void)
{
    return false;
}
static inline bool crazypod_iap_simple_take_dock_connected(void)
{
    return false;
}
#endif

#endif
