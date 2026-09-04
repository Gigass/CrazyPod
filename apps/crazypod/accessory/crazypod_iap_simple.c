#include "config.h"

#if defined(IPOD_6G) && defined(IPOD_ACCESSORY_PROTOCOL)

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#ifdef CRAZYPOD_IAP_DIAGNOSTICS
#include <stdio.h>
#endif

#include "audio.h"
#include "button.h"
#ifdef CRAZYPOD_IAP_DIAGNOSTICS
#include "dir.h"
#include "file.h"
#endif
#include "iap.h"
#include "kernel.h"
#include "pmu-target.h"
#include "queue.h"
#include "serial.h"
#include "settings.h"
#include "system.h"

#include "crazypod_iap_simple.h"

#define CRAZYPOD_IAP_RX_MAX 768
#define CRAZYPOD_IAP_RX_SLOTS 8
#define CRAZYPOD_IAP_REMOTE_STATE_SLOTS 16
#define CRAZYPOD_IAP_TX_MAX 64
#define CRAZYPOD_IAP_TX_WAIT_TICKS (HZ / 5)
#define CRAZYPOD_IAP_FRAMES_PER_EVENT 2
#define CRAZYPOD_IAP_PACKET_EVENT \
    MAKE_SYS_EVENT(SYS_EVENT_CLS_PRIVATE, 0x35)
#define CRAZYPOD_IAP_BUTTON_TIMEOUT_TICKS 3
#ifdef CRAZYPOD_IAP_DIAGNOSTICS
#define CRAZYPOD_IAP_RAW_CAPTURE_PATH \
    "/.crazypod/iap-raw.log"
#define CRAZYPOD_IAP_RAW_CAPTURE_MAX_RECORDS 512
#endif

enum crazypod_iap_frame_state {
    CRAZYPOD_IAP_SYNC,
    CRAZYPOD_IAP_SOF,
    CRAZYPOD_IAP_LENGTH,
    CRAZYPOD_IAP_LENGTH_HIGH,
    CRAZYPOD_IAP_LENGTH_LOW,
    CRAZYPOD_IAP_PAYLOAD,
    CRAZYPOD_IAP_CHECKSUM,
};

enum crazypod_iap_auth_state {
    CRAZYPOD_IAP_AUTH_NONE,
    CRAZYPOD_IAP_AUTH_CERT_REQUESTED,
    CRAZYPOD_IAP_AUTH_CERT_RECEIVING,
    CRAZYPOD_IAP_AUTH_CHALLENGE_SENT,
    CRAZYPOD_IAP_AUTHENTICATED,
};

struct crazypod_iap_frame {
    enum crazypod_iap_frame_state state;
    uint16_t length;
    uint16_t count;
    unsigned int checksum;
    long deadline;
    unsigned char payload[CRAZYPOD_IAP_RX_MAX];
};

struct crazypod_iap_slot {
    uint16_t length;
    unsigned char payload[CRAZYPOD_IAP_RX_MAX];
};

static struct crazypod_iap_frame rx_frame;
static struct crazypod_iap_slot rx_slots[CRAZYPOD_IAP_RX_SLOTS];
static volatile unsigned int rx_read_slot;
static volatile unsigned int rx_write_slot;
static volatile unsigned int rx_slot_count;
static volatile bool rx_event_pending;
static struct crazypod_iap_diagnostics diagnostics;
static bool iap_started;
static int iap_rate_setting;
static unsigned char last_packet[CRAZYPOD_IAP_RX_MAX];
static uint16_t last_packet_length;
static long remote_button_deadline;
static unsigned long remote_state_slots
    [CRAZYPOD_IAP_REMOTE_STATE_SLOTS];
static unsigned int remote_state_read_slot;
static unsigned int remote_state_write_slot;
static unsigned int remote_state_count;
static enum crazypod_iap_auth_state auth_state;
static unsigned char auth_next_section;
static unsigned char auth_max_section;
static uint32_t accessory_lingoes;
static volatile bool dock_connected;
static volatile bool dock_connected_pending;
static bool legacy_response_pending;
static long legacy_response_due;
#ifdef CRAZYPOD_IAP_DIAGNOSTICS
static bool raw_capture_initialized;
static unsigned int raw_capture_records;
#endif

unsigned long iap_remotebtn;
unsigned int iap_timeoutbtn;
int iap_repeatbtn;

static void crazypod_iap_serial_tx(const unsigned char *buffer, int length)
{
    long deadline = current_tick + CRAZYPOD_IAP_TX_WAIT_TICKS;
    bool ready;
    int index;

    for(index = 0; index < length; ++index) {
        while(!(ready = tx_rdy()) &&
              TIME_BEFORE(current_tick, deadline))
            yield();
        if(!ready) {
            ++diagnostics.uart_tx_timeouts;
            return;
        }
        tx_writec(buffer[index]);
    }
}

void (*iap_transport_send)(const unsigned char *buffer, int length) =
    crazypod_iap_serial_tx;

static void send_packet(const unsigned char *payload, int length)
{
    unsigned char frame[CRAZYPOD_IAP_TX_MAX + 4];
    unsigned int checksum;
    int index;

    if(payload == NULL || length <= 0 ||
       length > CRAZYPOD_IAP_TX_MAX)
        return;

    frame[0] = 0xff;
    frame[1] = 0x55;
    frame[2] = (unsigned char)length;
    checksum = (unsigned int)length;
    for(index = 0; index < length; ++index) {
        frame[3 + index] = payload[index];
        checksum += payload[index];
    }
    frame[3 + length] = (unsigned char)(0u - checksum);
    iap_transport_send(frame, length + 4);
}

static void send_general_ack(unsigned char command, unsigned char status)
{
    const unsigned char payload[] = { 0x00, 0x02, status, command };

    send_packet(payload, sizeof(payload));
}

static void send_remote_ack(unsigned char command, unsigned char status)
{
    const unsigned char payload[] = { 0x02, 0x01, status, command };

    send_packet(payload, sizeof(payload));
}

static void send_display_remote_ack(
    unsigned char command, unsigned char status)
{
    const unsigned char payload[] = { 0x03, 0x00, status, command };

    send_packet(payload, sizeof(payload));
}

static void send_string_response(unsigned char command, const char *value)
{
    unsigned char payload[CRAZYPOD_IAP_TX_MAX];
    size_t value_length;

    value_length = strlen(value);
    if(value_length > sizeof(payload) - 3)
        value_length = sizeof(payload) - 3;
    payload[0] = 0x00;
    payload[1] = command;
    memcpy(&payload[2], value, value_length);
    payload[2 + value_length] = '\0';
    send_packet(payload, (int)value_length + 3);
}

static void put_u32(unsigned char *buffer, uint32_t value)
{
    buffer[0] = (unsigned char)(value >> 24);
    buffer[1] = (unsigned char)(value >> 16);
    buffer[2] = (unsigned char)(value >> 8);
    buffer[3] = (unsigned char)value;
}

static void start_authentication(void)
{
    const unsigned char request[] = { 0x00, 0x14 };

    auth_state = CRAZYPOD_IAP_AUTH_CERT_REQUESTED;
    auth_next_section = 0;
    auth_max_section = 0;
    send_packet(request, sizeof(request));
}

static void mark_authenticated(void)
{
    int irq_level;

    auth_state = CRAZYPOD_IAP_AUTHENTICATED;
    if((accessory_lingoes & (1u << 2)) == 0)
        return;
    irq_level = disable_irq_save();
    if(!dock_connected) {
        dock_connected = true;
        dock_connected_pending = true;
    }
    restore_irq(irq_level);
}

static void handle_auth_certificate(
    const unsigned char *payload, uint16_t length)
{
    unsigned int version;
    unsigned char section;
    unsigned char max_section;

    if(length < 6 ||
       (auth_state != CRAZYPOD_IAP_AUTH_CERT_REQUESTED &&
        auth_state != CRAZYPOD_IAP_AUTH_CERT_RECEIVING)) {
        send_general_ack(0x15, 0x04);
        return;
    }
    version = ((unsigned int)payload[2] << 8) | payload[3];
    section = payload[4];
    max_section = payload[5];
    if((version != 0x0100 && version != 0x0200) ||
       section != auth_next_section ||
       (auth_state == CRAZYPOD_IAP_AUTH_CERT_RECEIVING &&
        max_section != auth_max_section)) {
        send_general_ack(0x15, 0x04);
        return;
    }
    auth_max_section = max_section;
    if(section < max_section) {
        auth_state = CRAZYPOD_IAP_AUTH_CERT_RECEIVING;
        auth_next_section = section + 1;
        send_general_ack(0x15, 0x00);
    }
    else {
        static const unsigned char certificate_ack[] = {
            0x00, 0x16, 0x00
        };
        static const unsigned char challenge[] = {
            0x00, 0x17,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x01
        };

        send_packet(certificate_ack, sizeof(certificate_ack));
        send_packet(challenge, sizeof(challenge));
        auth_state = CRAZYPOD_IAP_AUTH_CHALLENGE_SENT;
    }
}

static void handle_auth_signature(void)
{
    const unsigned char authenticated[] = { 0x00, 0x19, 0x00 };

    if(auth_state != CRAZYPOD_IAP_AUTH_CHALLENGE_SENT) {
        send_general_ack(0x18, 0x04);
        return;
    }
    send_packet(authenticated, sizeof(authenticated));
    mark_authenticated();
}

static void handle_general(const unsigned char *payload, uint16_t length)
{
    unsigned char command;

    if(length < 2)
        return;
    command = payload[1];
    switch(command) {
        case 0x01: /* Identify (legacy) */
            if(length >= 3 && payload[2] == 0x05) {
                legacy_response_pending = true;
                legacy_response_due = current_tick + HZ / 3;
            }
            break;

        case 0x02: /* Accessory ACK */
        case 0x28: /* ReturnAccessoryInfo */
            break;

        case 0x07: /* RequestiPodName */
            send_string_response(0x08, "CrazyPod");
            break;

        case 0x09: /* RequestiPodSoftwareVersion */
        {
            const unsigned char response[] = {
                0x00, 0x0a, 0x02, 0x00, 0x03
            };

            send_packet(response, sizeof(response));
            break;
        }

        case 0x0b: /* RequestiPodSerialNum */
            send_string_response(0x0c, "");
            break;

        case 0x0d: /* RequestiPodModelNum */
        {
            unsigned char response[12] = { 0x00, 0x0e };

            put_u32(&response[2], 0x00130200u);
            memcpy(&response[6], "MC293", 6);
            send_packet(response, sizeof(response));
            break;
        }

        case 0x0f: /* RequestLingoProtocolVersion */
            if(length >= 3 &&
               (payload[2] == 0x00 || payload[2] == 0x02
                || payload[2] == 0x03 || payload[2] == 0x05
               )) {
                const unsigned char response[] = {
                    0x00, 0x10, payload[2], 0x01,
                    payload[2] == 0x00 ? 0x09 :
                    payload[2] == 0x02 ? 0x02 :
                    payload[2] == 0x03 ? 0x05 : 0x01
                };

                send_packet(response, sizeof(response));
            }
            else
                send_general_ack(command, 0x04);
            break;

        case 0x13: /* IdentifyDeviceLingoes */
            if(length >= 14 && (payload[5] & 0x01) != 0) {
                uint32_t lingoes =
                    ((uint32_t)payload[2] << 24) |
                    ((uint32_t)payload[3] << 16) |
                    ((uint32_t)payload[4] << 8) |
                    payload[5];
                uint32_t options =
                    ((uint32_t)payload[6] << 24) |
                    ((uint32_t)payload[7] << 16) |
                    ((uint32_t)payload[8] << 8) |
                    payload[9];

                accessory_lingoes = lingoes;
                send_general_ack(command, 0x00);
                if((options & 0x03u) != 0)
                    start_authentication();
                else
                    mark_authenticated();
                if((lingoes & (1u << 5)) != 0) {
                    static const unsigned char get_accessory_info[] = {
                        0x00, 0x27, 0x00
                    };
                    static const unsigned char begin_transmission[] = {
                        0x05, 0x02
                    };

                    send_packet(get_accessory_info,
                                sizeof(get_accessory_info));
                    send_packet(begin_transmission,
                                sizeof(begin_transmission));
                }
            }
            else
                send_general_ack(command, 0x02);
            break;

        case 0x15: /* RetDevAuthenticationInfo */
            handle_auth_certificate(payload, length);
            break;

        case 0x18: /* RetDevAuthenticationSignature */
            handle_auth_signature();
            break;

        case 0x24: /* GetiPodOptions */
        {
            const unsigned char response[] = {
                0x00, 0x25, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00
            };

            send_packet(response, sizeof(response));
            break;
        }

        case 0x29: /* GetiPodPreferences */
            if(length >= 3) {
                const unsigned char response[] = {
                    0x00, 0x2a, payload[2],
                    payload[2] == 0x03 ? 0x01 : 0x00
                };

                send_packet(response, sizeof(response));
            }
            else
                send_general_ack(command, 0x04);
            break;

        case 0x2b: /* SetiPodPreferences */
            if(length < 5)
                send_general_ack(command, 0x04);
            else if(payload[2] == 0x03 && payload[3] == 0x00)
                send_general_ack(command, 0x02);
            else
                send_general_ack(command, 0x00);
            break;

        default:
            if(command != 0x00)
                send_general_ack(command, 0x04);
            break;
    }
}

static unsigned long decode_remote_buttons(
    const unsigned char *payload, uint16_t length)
{
    unsigned long buttons = BUTTON_NONE;

    if(payload[2] != 0) {
        if(payload[2] & 0x01)
            buttons |= BUTTON_RC_PLAY;
        if(payload[2] & 0x02)
            buttons |= BUTTON_RC_VOL_UP;
        if(payload[2] & 0x04)
            buttons |= BUTTON_RC_VOL_DOWN;
        if(payload[2] & 0x08)
            buttons |= BUTTON_RC_RIGHT;
        if(payload[2] & 0x10)
            buttons |= BUTTON_RC_LEFT;
    }
    else if(length >= 4 && payload[3] != 0) {
        if((payload[3] & 0x01) &&
           audio_status() != AUDIO_STATUS_PLAY)
            buttons |= BUTTON_RC_PLAY;
        if((payload[3] & 0x02) &&
           audio_status() == AUDIO_STATUS_PLAY)
            buttons |= BUTTON_RC_PLAY;
    }
    else if(length >= 5 && payload[4] != 0) {
        if((payload[4] & 0x04) &&
           audio_status() == AUDIO_STATUS_PLAY)
            buttons |= BUTTON_RC_PLAY;
        if(payload[4] & 0x10)
            buttons |= BUTTON_RC_RIGHT;
        if(payload[4] & 0x20)
            buttons |= BUTTON_RC_LEFT;
        if(payload[4] & 0x40)
            buttons |= BUTTON_RC_MENU;
        if(payload[4] & 0x80)
            buttons |= BUTTON_RC_SELECT;
    }
    else if(length >= 6 && payload[5] != 0) {
        if(payload[5] & 0x01)
            buttons |= BUTTON_RC_UP;
        if(payload[5] & 0x02)
            buttons |= BUTTON_RC_DOWN;
    }
    return buttons;
}

static void activate_remote_state_locked(unsigned long buttons)
{
    iap_remotebtn = buttons;
    iap_repeatbtn = 2;
    if(buttons == BUTTON_NONE) {
        iap_timeoutbtn = 0;
        remote_button_deadline = 0;
    }
    else {
        iap_timeoutbtn = CRAZYPOD_IAP_BUTTON_TIMEOUT_TICKS;
        remote_button_deadline = current_tick +
            CRAZYPOD_IAP_BUTTON_TIMEOUT_TICKS * HZ / 10;
    }
}

static unsigned long remote_state_tail_locked(void)
{
    unsigned int index;

    if(remote_state_count == 0)
        return iap_remotebtn;
    index = (remote_state_write_slot +
             CRAZYPOD_IAP_REMOTE_STATE_SLOTS - 1) %
        CRAZYPOD_IAP_REMOTE_STATE_SLOTS;
    return remote_state_slots[index];
}

static void enqueue_remote_state_locked(unsigned long buttons)
{
    if(buttons == remote_state_tail_locked())
        return;
    if(iap_remotebtn == BUTTON_NONE &&
       iap_repeatbtn == 0 && remote_state_count == 0) {
        activate_remote_state_locked(buttons);
        return;
    }
    if(remote_state_count >= CRAZYPOD_IAP_REMOTE_STATE_SLOTS) {
        unsigned int tail =
            (remote_state_write_slot +
             CRAZYPOD_IAP_REMOTE_STATE_SLOTS - 1) %
            CRAZYPOD_IAP_REMOTE_STATE_SLOTS;

        remote_state_slots[tail] = buttons;
        ++diagnostics.remote_state_overflows;
        return;
    }
    remote_state_slots[remote_state_write_slot] = buttons;
    remote_state_write_slot =
        (remote_state_write_slot + 1) %
        CRAZYPOD_IAP_REMOTE_STATE_SLOTS;
    ++remote_state_count;
}

static void activate_next_remote_state_locked(void)
{
    unsigned long buttons;

    if(remote_state_count == 0)
        return;
    buttons = remote_state_slots[remote_state_read_slot];
    remote_state_read_slot =
        (remote_state_read_slot + 1) %
        CRAZYPOD_IAP_REMOTE_STATE_SLOTS;
    --remote_state_count;
    activate_remote_state_locked(buttons);
}

static void apply_remote_buttons(
    const unsigned char *payload, uint16_t length)
{
    unsigned long buttons =
        decode_remote_buttons(payload, length);
    int irq_level = disable_irq_save();
    unsigned long tail = remote_state_tail_locked();

    enqueue_remote_state_locked(buttons);
    if(buttons != BUTTON_NONE || tail != BUTTON_NONE)
        remote_button_deadline = current_tick +
            CRAZYPOD_IAP_BUTTON_TIMEOUT_TICKS * HZ / 10;
    restore_irq(irq_level);
}

static void handle_simple_remote(
    const unsigned char *payload, uint16_t length)
{
    unsigned char command;

    if(length < 3) {
        if(length >= 2)
            send_remote_ack(payload[1], 0x04);
        return;
    }
    command = payload[1];
    if(command == 0x00 || command == 0x04) {
        apply_remote_buttons(payload, length);
        if(command == 0x04)
            send_remote_ack(command, 0x00);
    }
    else if(command != 0x01) {
        send_remote_ack(command, 0x04);
    }
}

static unsigned char encode_remote_volume(void)
{
    int encoded = (global_status.volume + 90) * 255 / 96;

    if(encoded < 0)
        return 0;
    if(encoded > 255)
        return 255;
    return (unsigned char)encoded;
}

static void handle_display_remote(
    const unsigned char *payload, uint16_t length)
{
    unsigned char command;

    if(length < 2)
        return;
    command = payload[1];
    if(command == 0x08 && length >= 6 &&
       auth_state == CRAZYPOD_IAP_AUTHENTICATED)
        send_display_remote_ack(command, 0x00);
    else if(command == 0x0c && length >= 3 && payload[2] == 0x04 &&
            auth_state == CRAZYPOD_IAP_AUTHENTICATED) {
        const unsigned char response[] = {
            0x03, 0x0d, 0x04, 0x00, encode_remote_volume()
        };

        send_packet(response, sizeof(response));
    }
    else if(command != 0x00)
        send_display_remote_ack(command, 0x04);
}

static void handle_rf_transmitter(
    const unsigned char *payload, uint16_t length)
{
    if(length >= 2 &&
       (payload[1] == 0x02 || payload[1] == 0x03))
        send_packet(payload, 2);
}

static void handle_packet(const unsigned char *payload, uint16_t length)
{
    if(length == 0)
        return;
    if(payload[0] == 0x00)
        handle_general(payload, length);
    else if(payload[0] == 0x02)
        handle_simple_remote(payload, length);
    else if(payload[0] == 0x03)
        handle_display_remote(payload, length);
    else if(payload[0] == 0x05)
        handle_rf_transmitter(payload, length);
}

#ifdef CRAZYPOD_IAP_DIAGNOSTICS
static void capture_raw_frame(
    const unsigned char *payload, uint16_t length)
{
    static const char header[] = "CrazyPod iAP raw capture v1\n";
    char line[256];
    int flags;
    int file;
    int position;
    uint16_t index;

    if(payload == NULL || length == 0 ||
       raw_capture_records >= CRAZYPOD_IAP_RAW_CAPTURE_MAX_RECORDS)
        return;
    flags = O_WRONLY | O_CREAT | O_APPEND;
    if(!raw_capture_initialized)
        mkdir("/.crazypod");
    file = open(CRAZYPOD_IAP_RAW_CAPTURE_PATH, flags, 0666);
    if(file < 0)
        return;
    if(!raw_capture_initialized &&
       write(file, header, sizeof(header) - 1u) !=
           (ssize_t)(sizeof(header) - 1u)) {
        close(file);
        return;
    }
    position = snprintf(
        line, sizeof(line), "%ld %u",
        current_tick, (unsigned int)length);
    for(index = 0; index < length &&
        position > 0 && position < (int)sizeof(line) - 4;
        ++index) {
        position += snprintf(
            &line[position], sizeof(line) - (size_t)position,
            " %02x", payload[index]);
    }
    if(position > 0 && position < (int)sizeof(line) - 1)
        line[position++] = '\n';
    if(position <= 0 || position > (int)sizeof(line) ||
       write(file, line, (size_t)position) != position ||
       fsync(file) < 0) {
        close(file);
        return;
    }
    close(file);
    raw_capture_initialized = true;
    ++raw_capture_records;
}
#endif

static void queue_received_frame(void)
{
    struct crazypod_iap_slot *slot;

    last_packet_length = rx_frame.length;
    memcpy(last_packet, rx_frame.payload, rx_frame.length);
    if(rx_slot_count >= CRAZYPOD_IAP_RX_SLOTS) {
        ++diagnostics.queue_overflows;
        return;
    }
    slot = &rx_slots[rx_write_slot];
    slot->length = rx_frame.length;
    memcpy(slot->payload, rx_frame.payload, rx_frame.length);
    rx_write_slot =
        (rx_write_slot + 1) % CRAZYPOD_IAP_RX_SLOTS;
    ++rx_slot_count;
    ++diagnostics.received_frames;
    if(!rx_event_pending) {
        rx_event_pending = true;
        button_queue_post(CRAZYPOD_IAP_PACKET_EVENT, 0);
    }
}

bool iap_getc(IF_IAP_MP(int port,) unsigned char value)
{
    IF_IAP_MP((void)port;)

    if(!iap_started)
        return true;
    if(rx_frame.state != CRAZYPOD_IAP_SYNC &&
       TIME_AFTER(current_tick, rx_frame.deadline)) {
        ++diagnostics.frame_timeouts;
        rx_frame.state = CRAZYPOD_IAP_SYNC;
    }

    switch(rx_frame.state) {
        case CRAZYPOD_IAP_SYNC:
            if(value == 0xff)
                rx_frame.state = CRAZYPOD_IAP_SOF;
            break;
        case CRAZYPOD_IAP_SOF:
            rx_frame.state = value == 0x55
                ? CRAZYPOD_IAP_LENGTH : CRAZYPOD_IAP_SYNC;
            if(value == 0xff)
                rx_frame.state = CRAZYPOD_IAP_SOF;
            break;
        case CRAZYPOD_IAP_LENGTH:
            rx_frame.checksum = value;
            rx_frame.count = 0;
            if(value == 0)
                rx_frame.state = CRAZYPOD_IAP_LENGTH_HIGH;
#if CRAZYPOD_IAP_RX_MAX < 256
            else if(value <= CRAZYPOD_IAP_RX_MAX) {
#else
            else {
#endif
                rx_frame.length = value;
                rx_frame.state = CRAZYPOD_IAP_PAYLOAD;
            }
#if CRAZYPOD_IAP_RX_MAX < 256
            else
                rx_frame.state = CRAZYPOD_IAP_SYNC;
#endif
            break;
        case CRAZYPOD_IAP_LENGTH_HIGH:
            rx_frame.checksum += value;
            rx_frame.length = (uint16_t)value << 8;
            rx_frame.state = CRAZYPOD_IAP_LENGTH_LOW;
            break;
        case CRAZYPOD_IAP_LENGTH_LOW:
            rx_frame.checksum += value;
            rx_frame.length |= value;
            rx_frame.state = rx_frame.length > 0 &&
                rx_frame.length <= CRAZYPOD_IAP_RX_MAX
                ? CRAZYPOD_IAP_PAYLOAD : CRAZYPOD_IAP_SYNC;
            break;
        case CRAZYPOD_IAP_PAYLOAD:
            rx_frame.payload[rx_frame.count++] = value;
            rx_frame.checksum += value;
            if(rx_frame.count == rx_frame.length)
                rx_frame.state = CRAZYPOD_IAP_CHECKSUM;
            break;
        case CRAZYPOD_IAP_CHECKSUM:
            rx_frame.checksum += value;
            if((rx_frame.checksum & 0xffu) == 0)
                queue_received_frame();
            else
                ++diagnostics.checksum_errors;
            rx_frame.state = CRAZYPOD_IAP_SYNC;
            break;
    }
    rx_frame.deadline = current_tick + MAX(1, HZ / 40);
    return rx_frame.state == CRAZYPOD_IAP_SYNC ||
        rx_frame.state == CRAZYPOD_IAP_SOF;
}

void iap_setup(int rate_setting)
{
    iap_rate_setting = rate_setting;
    if(!iap_started) {
        memset(&rx_frame, 0, sizeof(rx_frame));
        rx_frame.state = CRAZYPOD_IAP_SYNC;
        memset(rx_slots, 0, sizeof(rx_slots));
        memset(&diagnostics, 0, sizeof(diagnostics));
        rx_read_slot = 0;
        rx_write_slot = 0;
        rx_slot_count = 0;
        rx_event_pending = false;
        auth_state = CRAZYPOD_IAP_AUTH_NONE;
        auth_next_section = 0;
        auth_max_section = 0;
        accessory_lingoes = 0;
        dock_connected = false;
        dock_connected_pending = false;
        iap_started = true;
    }
    iap_bitrate_set(rate_setting);
}

void iap_bitrate_set(int rate_setting)
{
    static const int rates[] = { 0, 9600, 19200, 38400, 57600 };

    if(rate_setting < 0 ||
       rate_setting >= (int)(sizeof(rates) / sizeof(rates[0])))
        rate_setting = 0;
    iap_rate_setting = rate_setting;
    serial_bitrate(rates[rate_setting]);
}

void iap_reset_state(IF_IAP_MP_NONVOID(int port))
{
    int irq_level;

    IF_IAP_MP((void)port;)
    irq_level = disable_irq_save();
    rx_frame.state = CRAZYPOD_IAP_SYNC;
    memset(rx_slots, 0, sizeof(rx_slots));
    rx_read_slot = 0;
    rx_write_slot = 0;
    rx_slot_count = 0;
    rx_event_pending = false;
    last_packet_length = 0;
    memset(remote_state_slots, 0, sizeof(remote_state_slots));
    remote_state_read_slot = 0;
    remote_state_write_slot = 0;
    remote_state_count = 0;
    iap_remotebtn = BUTTON_NONE;
    iap_timeoutbtn = 0;
    iap_repeatbtn = 0;
    remote_button_deadline = 0;
    auth_state = CRAZYPOD_IAP_AUTH_NONE;
    auth_next_section = 0;
    auth_max_section = 0;
    accessory_lingoes = 0;
    dock_connected = false;
    dock_connected_pending = false;
    legacy_response_pending = false;
    legacy_response_due = 0;
    restore_irq(irq_level);
    iap_bitrate_set(iap_rate_setting);
}

int remote_control_rx(void)
{
    int irq_level = disable_irq_save();
    int button;

    if(iap_repeatbtn == 0 && remote_state_count > 0)
        activate_next_remote_state_locked();
    if(iap_remotebtn != BUTTON_NONE && iap_repeatbtn == 0 &&
       remote_state_count == 0 &&
       TIME_AFTER(current_tick, remote_button_deadline)) {
        iap_remotebtn = BUTTON_NONE;
        iap_timeoutbtn = 0;
        remote_button_deadline = 0;
    }
    button = (int)iap_remotebtn;
    if(iap_repeatbtn > 0)
        --iap_repeatbtn;
    restore_irq(irq_level);
    return button;
}

void iap_send_pkt(const unsigned char *payload, int length)
{
    send_packet(payload, length);
}

const unsigned char *iap_get_serbuf(void)
{
    return last_packet_length > 0 ? last_packet : NULL;
}

void iap_malloc(void)
{
}

void iap_periodic(void)
{
    static const unsigned char begin_transmission[] = {
        0x05, 0x02
    };

    if(legacy_response_pending &&
       !TIME_BEFORE(current_tick, legacy_response_due)) {
        legacy_response_pending = false;
        send_packet(begin_transmission, sizeof(begin_transmission));
    }
}

void iap_report_rx_error(unsigned char error)
{
    if(error == 0)
        return;
    if(error & IAP_RX_ERROR_OVERRUN)
        ++diagnostics.uart_overruns;
    if(error & IAP_RX_ERROR_PARITY)
        ++diagnostics.uart_parity_errors;
    if(error & IAP_RX_ERROR_FRAME)
        ++diagnostics.uart_frame_errors;
    if(error & IAP_RX_ERROR_BREAK)
        ++diagnostics.uart_breaks;
    rx_frame.state = CRAZYPOD_IAP_SYNC;
}

static void service_received_frames(void)
{
    struct crazypod_iap_slot slot;
    unsigned int serviced = 0;

    while(serviced < CRAZYPOD_IAP_FRAMES_PER_EVENT) {
        int irq_level = disable_irq_save();

        if(rx_slot_count == 0) {
            rx_event_pending = false;
            restore_irq(irq_level);
            return;
        }
        slot = rx_slots[rx_read_slot];
        rx_read_slot =
            (rx_read_slot + 1) % CRAZYPOD_IAP_RX_SLOTS;
        --rx_slot_count;
        restore_irq(irq_level);
#ifdef CRAZYPOD_IAP_DIAGNOSTICS
        capture_raw_frame(slot.payload, slot.length);
#endif
        handle_packet(slot.payload, slot.length);
        ++serviced;
    }
    button_queue_post(CRAZYPOD_IAP_PACKET_EVENT, 0);
}

void iap_handlepkt(void)
{
    service_received_frames();
    iap_periodic();
}

bool crazypod_iap_simple_handle_event(long event, intptr_t data)
{
    (void)data;
    if(event != CRAZYPOD_IAP_PACKET_EVENT)
        return false;
    service_received_frames();
    iap_periodic();
    return true;
}

bool crazypod_iap_simple_accessory_present(void)
{
    return pmu_accessory_present() != 0;
}

bool crazypod_iap_simple_dock_connected(void)
{
    int irq_level = disable_irq_save();
    bool connected = dock_connected;

    restore_irq(irq_level);
    return connected && crazypod_iap_simple_accessory_present();
}

bool crazypod_iap_simple_take_dock_connected(void)
{
    int irq_level = disable_irq_save();
    bool pending = dock_connected_pending;

    dock_connected_pending = false;
    restore_irq(irq_level);
    return pending && crazypod_iap_simple_accessory_present();
}

void crazypod_iap_simple_get_diagnostics(
    struct crazypod_iap_diagnostics *result)
{
    int irq_level;

    if(result == NULL)
        return;
    irq_level = disable_irq_save();
    *result = diagnostics;
    restore_irq(irq_level);
}

bool dbg_iap(void)
{
    return false;
}

#endif
