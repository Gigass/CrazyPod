#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../sdk/crazypod_miniapp.h"

struct cp_host_api_v1_prefix {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t (*epoch_seconds)(void);
    uint32_t (*monotonic_ms)(void);
    int (*state_read)(void *buffer, size_t capacity);
    int (*state_write)(const void *buffer, size_t size);
    int (*alarm_set)(uint32_t deadline_epoch, uint32_t token);
    void (*alarm_cancel)(void);
    bool (*alarm_fired)(uint32_t *token);
    void (*alarm_acknowledge)(void);
    void (*format_number)(double value, char *buffer, size_t capacity);
};

static void mock_format_duration(
    uint32_t seconds, char *buffer, size_t capacity)
{
    (void)seconds;
    if(capacity > 0)
        buffer[0] = '\0';
}

static int mock_now_playing(struct cp_now_playing *info)
{
    (void)info;
    return 0;
}

static void test_stable_v1_prefix(void)
{
    struct cp_host_api host;

    assert(CP_HOST_API_V1_SIZE ==
           sizeof(struct cp_host_api_v1_prefix));
    memset(&host, 0, sizeof(host));
    host.struct_size = CP_HOST_API_V1_SIZE;
    host.capabilities = CP_CAP_FORMAT_DURATION;
    host.format_duration = mock_format_duration;
    assert(!CP_HOST_HAS(
        &host, CP_CAP_FORMAT_DURATION, format_duration));

    host.struct_size = sizeof(host);
    assert(CP_HOST_HAS(
        &host, CP_CAP_FORMAT_DURATION, format_duration));
    host.capabilities = 0;
    assert(!CP_HOST_HAS(
        &host, CP_CAP_FORMAT_DURATION, format_duration));
}

static void test_revision_two_draw_commands(void)
{
    struct cp_scene scene;
    struct cp_draw_command *divider;
    struct cp_draw_command *progress;

    cp_scene_reset(&scene);
    divider = cp_scene_add(&scene, CP_DRAW_DIVIDER);
    progress = cp_scene_add(&scene, CP_DRAW_PROGRESS);
    assert(divider != NULL);
    assert(progress != NULL);
    assert(divider->type == CP_DRAW_DIVIDER);
    assert(progress->type == CP_DRAW_PROGRESS);
    assert(scene.command_count == 2);
}

static void test_revision_three_tail_and_bitmap(void)
{
    struct cp_host_api host;
    struct cp_scene scene;
    struct cp_draw_command *bitmap;

    memset(&host, 0, sizeof(host));
    host.struct_size = offsetof(struct cp_host_api, now_playing);
    host.capabilities = CP_CAP_NOW_PLAYING;
    host.now_playing = mock_now_playing;
    assert(!CP_HOST_HAS(
        &host, CP_CAP_NOW_PLAYING, now_playing));
    host.struct_size = sizeof(host);
    assert(CP_HOST_HAS(
        &host, CP_CAP_NOW_PLAYING, now_playing));

    cp_scene_reset(&scene);
    bitmap = cp_scene_add(&scene, CP_DRAW_BITMAP);
    assert(bitmap != NULL);
    cp_text_copy(bitmap->text, sizeof(bitmap->text), "badge");
    assert(bitmap->type == CP_DRAW_BITMAP);
    assert(strcmp(bitmap->text, "badge") == 0);
}

int main(void)
{
    test_stable_v1_prefix();
    test_revision_two_draw_commands();
    test_revision_three_tail_and_bitmap();
    puts("mini-app SDK extension tests: OK");
    return 0;
}
