#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "crazypod_miniapp_native.h"

static void verify_constants(void)
{
    assert(CP_NATIVE_ABI_MAJOR == 1u);
    assert(CP_NATIVE_ABI_MINOR == 20u);
    assert(CP_NATIVE_ABI_REJECTED_MINOR == 14u);
    assert(CP_UI_FONT_CONDENSED_16 > CP_UI_FONT_DISPLAY);
    assert(CP_UI_FONT_SERIF_28 > CP_UI_FONT_CONDENSED_32);
    assert(CP_UI_FONT_TECHNICAL_16 > CP_UI_FONT_TECHNICAL_8);
    assert(CP_UI_FONT_SYSTEM > CP_UI_FONT_UNIBIT_16);
    assert(CP_UI_FONT_SERIF == CP_UI_FONT_SYSTEM + 1);
    assert(CP_UI_FONT_MONO == CP_UI_FONT_SERIF + 1);
    assert(CP_NATIVE_PACKAGE_FORMAT == 5u);
    assert(CP_NATIVE_REACT_PROFILE == 1u);
    assert(CP_NATIVE_CAP_FILES == (1u << 4));
    assert(CP_NATIVE_CAP_SERVICES == (1u << 5));
    assert(CP_NATIVE_CAP_DIAGNOSTICS == (1u << 6));
    assert(CP_NATIVE_SERVICE_DIAGNOSTICS == 2u);
    assert(CP_NATIVE_SERVICE_PAYLOAD_MAX == 1024u);
    assert(CP_NATIVE_SYSTEM_STATUS == 1u);
    assert(CP_NATIVE_SYSTEM_STATUS_LEGACY_COUNT == 5u);
    assert(CP_NATIVE_SYSTEM_STATUS_COUNT == 6u);
    assert(CP_NOW_PLAYING_INFO_COUNT == 10u);
    assert(sizeof(struct cp_native_system_info) == 12u);
    assert(CP_UI_OBJECT_SCREEN == 1);
    assert(CP_UI_OBJECT_MODAL == CP_UI_OBJECT_SOUND_WAVE + 1);
    assert(CP_UI_OBJECT_TYPE_COUNT < 256);
    assert(CP_UI_PROP_COUNT < 65536);
    assert(CP_UI_PROP_IMAGE_SOURCE == CP_UI_PROP_FONT + 1);
    assert(CP_UI_PROP_FONT_SIZE == CP_UI_PROP_FONT_SOURCE + 1);
    assert(CP_UI_PROP_FONT_WEIGHT == CP_UI_PROP_FONT_SIZE + 1);
    assert(CP_UI_PROP_FONT_STYLE == CP_UI_PROP_FONT_WEIGHT + 1);
    assert(CP_UI_PROP_LINE_HEIGHT == CP_UI_PROP_FONT_STYLE + 1);
    assert(CP_UI_PROP_WAVE_PRIMARY_COLOR == CP_UI_PROP_LINE_HEIGHT + 1);
    assert(CP_UI_PROP_WAVE_SECONDARY_COLOR ==
           CP_UI_PROP_WAVE_PRIMARY_COLOR + 1);
    assert(CP_UI_PROP_WAVE_HIGHLIGHT_COLOR ==
           CP_UI_PROP_WAVE_SECONDARY_COLOR + 1);
    assert(CP_UI_PROP_ADAPTIVE_LYRICS ==
           CP_UI_PROP_WAVE_HIGHLIGHT_COLOR + 1);
    assert(CP_UI_PROP_COUNT == CP_UI_PROP_ADAPTIVE_LYRICS + 1);
    assert(CP_UI_SOUND_WAVE_TORRENT == 0);
    assert(CP_UI_SOUND_WAVE_RADIAL_SPECTRUM == 1);
    assert(CP_UI_SOUND_WAVE_LIQUID_RIBBON == 2);
    assert(CP_UI_SOUND_WAVE_VINYL_GROOVE == 3);
    assert(CP_UI_SOUND_WAVE_MINI_LED_METER == 4);
    assert(CP_UI_SOUND_WAVE_PARTICLE_PULSE == 5);
    assert(CP_UI_SOUND_WAVE_STYLE_COUNT == 6);
    assert(CP_UI_SOUND_WAVE_BAR == 0);
    assert(CP_UI_SOUND_WAVE_BALL == 1);
}

static void verify_layout(void)
{
    assert(offsetof(struct cp_native_ui_api, begin_update) == 8u);
    assert(offsetof(struct cp_native_host_api, ui) >= 12u);
    assert(offsetof(struct cp_native_host_api, file_size) >
           offsetof(struct cp_native_host_api, log));
    assert(offsetof(struct cp_native_host_api, file_read) ==
           offsetof(struct cp_native_host_api, file_size) +
               sizeof(((struct cp_native_host_api *)0)->file_size));
    assert(offsetof(struct cp_native_host_api, file_write) ==
           offsetof(struct cp_native_host_api, file_read) +
               sizeof(((struct cp_native_host_api *)0)->file_read));
    assert(offsetof(struct cp_native_host_api, file_remove) ==
           offsetof(struct cp_native_host_api, file_write) +
               sizeof(((struct cp_native_host_api *)0)->file_write));
    assert(offsetof(struct cp_native_host_api, service_call) ==
           offsetof(struct cp_native_host_api, file_remove) +
               sizeof(((struct cp_native_host_api *)0)->file_remove));
    assert(sizeof(struct cp_native_host_api) ==
           offsetof(struct cp_native_host_api, service_call) +
               sizeof(((struct cp_native_host_api *)0)->service_call));
    assert(sizeof(struct cp_input_event) == 8u);
    assert(sizeof(struct cp_resource_info) == 20u);
    assert(sizeof(struct cp_native_ui_animation) == 24u);
    assert(CP_NOW_PLAYING_SNAPSHOT_BASE_SIZE == 420u);
    assert(offsetof(struct cp_now_playing_snapshot, level_left) == 420u);
    assert(offsetof(struct cp_now_playing_snapshot, level_right) == 424u);
    assert(sizeof(struct cp_now_playing_snapshot) == 428u);
    assert(sizeof(struct cp_now_playing_queue_state) == 16u);
    assert(sizeof(struct cp_now_playing_queue_item_request) == 8u);
    assert(sizeof(struct cp_now_playing_queue_item) == 400u);
    assert(sizeof(struct cp_now_playing_lyrics_request) == 8u);
    assert(sizeof(struct cp_now_playing_lyrics_window) == 400u);
    assert(sizeof(struct cp_now_playing_lyrics_context_request) == 12u);
    assert(sizeof(struct cp_now_playing_lyrics_context) == 28u);
    assert(sizeof(struct cp_now_playing_lyric_line_request) == 8u);
    assert(sizeof(struct cp_now_playing_lyric_line) == 780u);
    assert(sizeof(struct cp_now_playing_lyric_line) <=
           CP_NATIVE_SERVICE_PAYLOAD_MAX);
    assert(sizeof(struct cp_diagnostics_snapshot) == 44u);
    assert(sizeof(struct cp_diagnostics_log_request) == 8u);
    assert(sizeof(struct cp_diagnostics_log_entry) == 144u);
    assert(CP_NATIVE_HOST_V1_SIZE ==
           sizeof(struct cp_native_host_api));
    assert(CP_NATIVE_UI_V1_SIZE ==
           sizeof(struct cp_native_ui_api));
    assert(CP_NATIVE_APP_OPS_V1_SIZE ==
           sizeof(struct cp_native_app_ops));
}

int main(void)
{
    verify_constants();
    verify_layout();
    return 0;
}
