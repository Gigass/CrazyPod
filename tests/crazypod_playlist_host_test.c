#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "crazypod_playlist.h"
#include "playlist.h"
#include "settings.h"

struct user_settings global_settings;
struct system_status global_status;

static int play_count;

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

static const char *current_path(void)
{
    return crazypod_queue_path(crazypod_queue_index());
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
    crazypod_queue_replace_shuffled(paths, 4, 123u);
    assert(crazypod_queue_shuffle());
    assert(global_settings.playlist_shuffle);
    assert(crazypod_queue_index() == 0);
    assert(strcmp(current_path(), paths[0]) != 0);
    assert(play_count == 1);

    playlist_init();
    crazypod_queue_replace(paths, 4, 2);
    crazypod_queue_set_shuffle(true);
    assert(strcmp(current_path(), paths[2]) == 0);
    crazypod_queue_replace(paths, 4, 1);
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

    return 0;
}
