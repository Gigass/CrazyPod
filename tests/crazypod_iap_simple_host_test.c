#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "audio.h"
#include "button.h"
#include "iap.h"
#include "serial.h"
#include "settings.h"

#include "crazypod_iap_simple.h"

long current_tick;
struct system_status global_status;

static int test_audio_status;
static int test_accessory_present;
static int configured_bitrate = -1;
static long posted_event;
static intptr_t posted_data;
static int posted_count;
static unsigned char transmitted[512];
static size_t transmitted_length;

int audio_status(void)
{
    return test_audio_status;
}

int pmu_accessory_present(void)
{
    return test_accessory_present;
}

void button_queue_post(long id, intptr_t data)
{
    posted_event = id;
    posted_data = data;
    ++posted_count;
}

int tx_rdy(void)
{
    return 1;
}

void tx_writec(unsigned char value)
{
    assert(transmitted_length < sizeof(transmitted));
    transmitted[transmitted_length++] = value;
}

void serial_bitrate(int rate)
{
    configured_bitrate = rate;
}

static void reset_transport_capture(void)
{
    transmitted_length = 0;
    memset(transmitted, 0, sizeof(transmitted));
}

static void feed_packet(
    const unsigned char *payload, size_t length, bool valid_checksum)
{
    unsigned int checksum = (unsigned int)length;
    size_t index;

    assert(length > 0 && length <= 255);
    (void)iap_getc(0xff);
    (void)iap_getc(0x55);
    (void)iap_getc((unsigned char)length);
    for(index = 0; index < length; ++index) {
        checksum += payload[index];
        (void)iap_getc(payload[index]);
    }
    (void)iap_getc((unsigned char)(0u - checksum) +
                   (valid_checksum ? 0 : 1));
}

static void handle_posted_packet(void)
{
    assert(posted_count > 0);
    --posted_count;
    assert(crazypod_iap_simple_handle_event(
        posted_event, posted_data));
}

static void assert_valid_transmit_frame(void)
{
    unsigned int checksum = 0;
    size_t index;

    assert(transmitted_length >= 4);
    assert(transmitted[0] == 0xff);
    assert(transmitted[1] == 0x55);
    assert(transmitted[2] + 4u == transmitted_length);
    for(index = 2; index < transmitted_length; ++index)
        checksum += transmitted[index];
    assert((checksum & 0xffu) == 0);
}

static size_t assert_transmit_frame_at(
    size_t offset, const unsigned char *payload, size_t payload_length)
{
    unsigned int checksum = 0;
    size_t frame_length = payload_length + 4;
    size_t index;

    assert(offset + frame_length <= transmitted_length);
    assert(transmitted[offset] == 0xff);
    assert(transmitted[offset + 1] == 0x55);
    assert(transmitted[offset + 2] == payload_length);
    assert(memcmp(&transmitted[offset + 3], payload,
                  payload_length) == 0);
    for(index = offset + 2; index < offset + frame_length; ++index)
        checksum += transmitted[index];
    assert((checksum & 0xffu) == 0);
    return offset + frame_length;
}

int main(void)
{
    struct crazypod_iap_diagnostics before;
    struct crazypod_iap_diagnostics after;
    int index;
    static const unsigned char menu_press[] = {
        0x02, 0x00, 0x00, 0x00, 0x40
    };
    static const unsigned char release[] = {
        0x02, 0x00, 0x00, 0x00, 0x00
    };
    static const unsigned char volume_down_press[] = {
        0x02, 0x00, 0x04
    };
    static const unsigned char select_audio[] = {
        0x02, 0x04, 0x00, 0x00, 0x80
    };
    static const unsigned char request_name[] = { 0x00, 0x07 };
    static const unsigned char name_response[] = {
        0x00, 0x08, 'C', 'r', 'a', 'z', 'y', 'P', 'o', 'd', 0x00
    };
    static const unsigned char request_lingo3_version[] = {
        0x00, 0x0f, 0x03
    };
    static const unsigned char identify_display_remote[] = {
        0x00, 0x13,
        0x00, 0x00, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x06,
        0x00, 0x00, 0x02, 0x00
    };
    static const unsigned char identify_general_only[] = {
        0x00, 0x13,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x00
    };
    static const unsigned char auth_certificate[] = {
        0x00, 0x15, 0x02, 0x00, 0x00, 0x00, 0xaa
    };
    static const unsigned char auth_signature[] = {
        0x00, 0x18, 0xaa
    };
    static const unsigned char display_notifications[] = {
        0x03, 0x08, 0x00, 0x00, 0x00, 0x01
    };
    static const unsigned char display_volume_request[] = {
        0x03, 0x0c, 0x04
    };

    iap_setup(0);
    assert(configured_bitrate == 0);
    assert(!crazypod_iap_simple_handle_event(123, 0));
    assert(!crazypod_iap_simple_accessory_present());
    assert(!crazypod_iap_simple_dock_connected());
    assert(!crazypod_iap_simple_take_dock_connected());

    test_accessory_present = 1;
    assert(crazypod_iap_simple_accessory_present());
    feed_packet(identify_general_only,
                sizeof(identify_general_only), true);
    handle_posted_packet();
    assert(!crazypod_iap_simple_dock_connected());
    assert(!crazypod_iap_simple_take_dock_connected());

    feed_packet(menu_press, sizeof(menu_press), false);
    assert(posted_count == 0);
    crazypod_iap_simple_get_diagnostics(&after);
    assert(after.checksum_errors == 1);

    feed_packet(menu_press, sizeof(menu_press), true);
    handle_posted_packet();
    assert(remote_control_rx() == BUTTON_RC_MENU);

    feed_packet(release, sizeof(release), true);
    handle_posted_packet();
    assert(remote_control_rx() == BUTTON_RC_MENU);
    assert(remote_control_rx() == BUTTON_NONE);
    assert(remote_control_rx() == BUTTON_NONE);

    feed_packet(volume_down_press, sizeof(volume_down_press), true);
    handle_posted_packet();
    assert(iap_repeatbtn == 2);
    assert(remote_control_rx() == BUTTON_RC_VOL_DOWN);
    assert(iap_repeatbtn == 1);
    feed_packet(volume_down_press, sizeof(volume_down_press), true);
    handle_posted_packet();
    assert(iap_repeatbtn == 1);
    feed_packet(release, sizeof(release), true);
    handle_posted_packet();
    assert(remote_control_rx() == BUTTON_RC_VOL_DOWN);
    assert(remote_control_rx() == BUTTON_NONE);
    assert(remote_control_rx() == BUTTON_NONE);

    reset_transport_capture();
    feed_packet(select_audio, sizeof(select_audio), true);
    handle_posted_packet();
    assert(remote_control_rx() == BUTTON_RC_SELECT);
    assert_valid_transmit_frame();
    assert(transmitted[2] == 4);
    assert(memcmp(&transmitted[3],
                  (const unsigned char[]){ 0x02, 0x01, 0x00, 0x04 },
                  4) == 0);

    (void)remote_control_rx();
    feed_packet(release, sizeof(release), true);
    handle_posted_packet();
    assert(remote_control_rx() == BUTTON_NONE);
    assert(remote_control_rx() == BUTTON_NONE);

    /* A complete double tap can arrive in one UART service burst. Preserve
     * every press and release state long enough for the button driver to
     * observe it instead of collapsing the burst to BUTTON_NONE. */
    feed_packet(volume_down_press, sizeof(volume_down_press), true);
    feed_packet(release, sizeof(release), true);
    feed_packet(volume_down_press, sizeof(volume_down_press), true);
    feed_packet(release, sizeof(release), true);
    handle_posted_packet();
    assert(remote_control_rx() == BUTTON_RC_VOL_DOWN);
    assert(remote_control_rx() == BUTTON_RC_VOL_DOWN);
    assert(remote_control_rx() == BUTTON_NONE);
    assert(remote_control_rx() == BUTTON_NONE);
    assert(remote_control_rx() == BUTTON_RC_VOL_DOWN);
    assert(remote_control_rx() == BUTTON_RC_VOL_DOWN);
    assert(remote_control_rx() == BUTTON_NONE);
    assert(remote_control_rx() == BUTTON_NONE);

    reset_transport_capture();
    feed_packet(request_name, sizeof(request_name), true);
    handle_posted_packet();
    assert_valid_transmit_frame();
    assert(transmitted[2] == sizeof(name_response));
    assert(memcmp(&transmitted[3], name_response,
                  sizeof(name_response)) == 0);

    reset_transport_capture();
    feed_packet(request_lingo3_version,
                sizeof(request_lingo3_version), true);
    handle_posted_packet();
    assert_valid_transmit_frame();
    assert(memcmp(&transmitted[3],
                  (const unsigned char[]){
                      0x00, 0x10, 0x03, 0x01, 0x05
                  }, 5) == 0);

    reset_transport_capture();
    feed_packet(identify_display_remote,
                sizeof(identify_display_remote), true);
    handle_posted_packet();
    index = 0;
    index = (int)assert_transmit_frame_at(
        (size_t)index,
        (const unsigned char[]){ 0x00, 0x02, 0x00, 0x13 }, 4);
    index = (int)assert_transmit_frame_at(
        (size_t)index,
        (const unsigned char[]){ 0x00, 0x14 }, 2);
    assert((size_t)index == transmitted_length);

    reset_transport_capture();
    feed_packet(auth_certificate, sizeof(auth_certificate), true);
    handle_posted_packet();
    index = 0;
    index = (int)assert_transmit_frame_at(
        (size_t)index,
        (const unsigned char[]){ 0x00, 0x16, 0x00 }, 3);
    index = (int)assert_transmit_frame_at(
        (size_t)index,
        (const unsigned char[]){
            0x00, 0x17,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x01
        }, 23);
    assert((size_t)index == transmitted_length);

    reset_transport_capture();
    feed_packet(auth_signature, sizeof(auth_signature), true);
    handle_posted_packet();
    assert_transmit_frame_at(
        0, (const unsigned char[]){ 0x00, 0x19, 0x00 }, 3);
    assert(crazypod_iap_simple_dock_connected());
    assert(crazypod_iap_simple_take_dock_connected());
    assert(!crazypod_iap_simple_take_dock_connected());

    reset_transport_capture();
    feed_packet(display_notifications,
                sizeof(display_notifications), true);
    handle_posted_packet();
    assert_transmit_frame_at(
        0, (const unsigned char[]){ 0x03, 0x00, 0x00, 0x08 }, 4);

    global_status.volume = -25;
    reset_transport_capture();
    feed_packet(display_volume_request,
                sizeof(display_volume_request), true);
    handle_posted_packet();
    assert_transmit_frame_at(
        0, (const unsigned char[]){
            0x03, 0x0d, 0x04, 0x00, 172
        }, 5);

    /* A burst uses one wake event and is drained in FIFO order. The ninth
     * frame is counted instead of being silently discarded. */
    reset_transport_capture();
    crazypod_iap_simple_get_diagnostics(&before);
    for(index = 0; index < 8; ++index)
        feed_packet(request_name, sizeof(request_name), true);
    assert(posted_count == 1);
    feed_packet(request_name, sizeof(request_name), true);
    assert(posted_count == 1);
    handle_posted_packet();
    assert(transmitted_length == 8 * (sizeof(name_response) + 4));
    crazypod_iap_simple_get_diagnostics(&after);
    assert(after.received_frames == before.received_frames + 8);
    assert(after.queue_overflows == before.queue_overflows + 1);

    iap_report_rx_error(
        IAP_RX_ERROR_OVERRUN | IAP_RX_ERROR_FRAME);
    crazypod_iap_simple_get_diagnostics(&after);
    assert(after.uart_overruns == 1);
    assert(after.uart_frame_errors == 1);
    assert(after.uart_parity_errors == 0);
    assert(after.uart_breaks == 0);

    /* An incomplete frame that stalls is abandoned and diagnosed. */
    (void)iap_getc(0xff);
    (void)iap_getc(0x55);
    (void)iap_getc(2);
    current_tick += HZ;
    (void)iap_getc(0xff);
    crazypod_iap_simple_get_diagnostics(&after);
    assert(after.frame_timeouts == 1);

    iap_reset_state();
    assert(!crazypod_iap_simple_dock_connected());
    assert(!crazypod_iap_simple_take_dock_connected());

    return 0;
}
