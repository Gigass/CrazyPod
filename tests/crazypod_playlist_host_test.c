#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core_alloc.h"
#include "crazypod_playlist.h"
#include "playlist.h"
#include "settings.h"

struct user_settings global_settings;
struct system_status global_status;

static int play_count;
static bool read_ipod_music;

#define LARGE_QUEUE_LENGTH 2501

static char large_paths[LARGE_QUEUE_LENGTH][32];
static const char *large_path_pointers[LARGE_QUEUE_LENGTH];

void audio_stop(void)
{
}

void audio_play(unsigned long elapsed, unsigned long offset)
{
    (void)elapsed;
    (void)offset;
    ++play_count;
}

void audio_resume(void)
{
}

bool crazypod_state_read_ipod_music(void)
{
    return read_ipod_music;
}

static const char *current_path(void)
{
    static char path[MAX_PATH];

    return crazypod_queue_copy_path(
        crazypod_queue_index(), path, sizeof(path)) ? path : NULL;
}

int main(void)
{
    static const char *const paths[] = {
        "/music/a.mp3",
        "/music/b.mp3",
        "/music/c.mp3",
        "/music/d.mp3",
    };

    playlist_init();
    assert(crazypod_queue_replace_shuffled(paths, 4, 123u));
    assert(crazypod_queue_shuffle());
    assert(global_settings.playlist_shuffle);
    assert(crazypod_queue_index() == 0);
    assert(strcmp(current_path(), paths[0]) != 0);
    assert(play_count == 1);

    playlist_init();
    assert(crazypod_queue_replace(paths, 4, 2));
    {
        char copied_path[MAX_PATH];

        assert(crazypod_queue_copy_path(2, copied_path,
                                        sizeof(copied_path)));
        assert(crazypod_queue_replace(paths, 2, 0));
        assert(strcmp(copied_path, paths[2]) == 0);
    }
    assert(crazypod_queue_replace(paths, 4, 2));
    crazypod_queue_set_shuffle(true);
    assert(strcmp(current_path(), paths[2]) == 0);
    assert(crazypod_queue_replace(paths, 4, 1));
    assert(strcmp(current_path(), paths[1]) == 0);

    crazypod_queue_restore_begin();
    assert(!crazypod_queue_restore_add(
        "/iPod_Control/Music/F00/blocked.mp3"));
    assert(crazypod_queue_restore_add(
        "/iPod_Control/Musical/allowed.mp3"));
    assert(crazypod_queue_count() == 1);

    playlist_init();
    assert(playlist_insert_track(
        NULL, "/iPod_Control/Music/F00/blocked.mp3", 0,
        false, false) == -1);
    assert(crazypod_queue_count() == 0);

    read_ipod_music = true;
    assert(crazypod_queue_restore_add(
        "/iPod_Control/Music/F00/allowed.mp3"));
    playlist_init();
    assert(playlist_insert_track(
        NULL, "/iPod_Control/Music/F00/allowed.mp3", 0,
        false, false) == 0);
    assert(crazypod_queue_count() == 1);
    read_ipod_music = false;
    playlist_init();

    {
        int index;

        for(index = 0; index < LARGE_QUEUE_LENGTH; ++index) {
            snprintf(large_paths[index], sizeof(large_paths[index]),
                     "/music/%04d.mp3", index);
            large_path_pointers[index] = large_paths[index];
        }
        assert(crazypod_queue_replace(
            large_path_pointers, LARGE_QUEUE_LENGTH, 2200));
        assert(crazypod_queue_count() == LARGE_QUEUE_LENGTH);
        assert(crazypod_queue_index() == 2200);
        {
            char path[MAX_PATH];

            assert(crazypod_queue_copy_path(
                LARGE_QUEUE_LENGTH - 1, path, sizeof(path)));
            assert(strcmp(path,
                          large_paths[LARGE_QUEUE_LENGTH - 1]) == 0);
        }
        assert(test_core_alloc_active_handles() == 1);
        assert(test_core_alloc_pin_count() == 1);
    }

    playlist_init();
    assert(test_core_alloc_active_handles() == 0);
    assert(test_core_alloc_pin_count() == 0);

    return 0;
}
