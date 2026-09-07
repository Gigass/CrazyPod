#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "appevents.h"
#include "audio.h"
#include "backlight.h"
#include "events.h"
#include "kernel.h"
#include "panic.h"
#include "playlist.h"
#include "settings.h"

#include "../../crazypod_artwork.h"
#include "../../crazypod_coverflow.h"
#include "../../crazypod_l10n.h"
#include "../../crazypod_music.h"
#include "../../crazypod_playlist.h"
#include "../../crazypod_runtime_font.h"
#include "../../crazypod_state.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../features/photos/crazypod_photos_feature.h"
#include "../navigation/crazypod_render_scheduler.h"
#include "../navigation/crazypod_ui_routes.h"
#include "../presentation/crazypod_menu_list.h"
#include "../presentation/crazypod_preview_motion.h"
#include "../shell/crazypod_lock_screen.h"
#include "../shell/crazypod_now_capsule.h"
#include "../shell/crazypod_notification.h"
#include "../shell/crazypod_shell.h"
#include "crazypod_app_launcher.h"
#include "crazypod_choice_coordinator.h"
#include "crazypod_menu_preview.h"
#include "crazypod_playback.h"

#define PREVIOUS_RESTART_THRESHOLD_MS 5000
#define SEEK_MAX_STEP_PERCENT 3
#define SEEK_MIN_STEP_MS 500u
#define PLAYBACK_COMMAND_STACK_SIZE (DEFAULT_STACK_SIZE + 0x1000)
/* Font prewarming walks the runtime-font/cache path and keeps several
 * metadata snapshots alive at once.  The default firmware stack is only
 * 0x400 bytes on non-hosted targets, so the small extra stack used here can
 * overflow on the first track transition.  Keep this in line with the
 * tagcache worker's background stack budget. */
#define LOCK_METADATA_WARM_STACK_SIZE (DEFAULT_STACK_SIZE + 0x4000)
#define LOCK_METADATA_WARM_EVENT 1
#define LOCK_TITLE_FONT_SIZE 15
#define LOCK_ARTIST_FONT_SIZE 12
#define LOCK_ALBUM_FONT_SIZE 10
#define PLAYBACK_SEARCH_QUERY_SIZE 64

enum crazypod_playback_command {
    CRAZYPOD_PLAYBACK_COMMAND_PAUSE = 1,
    CRAZYPOD_PLAYBACK_COMMAND_RESUME,
    CRAZYPOD_PLAYBACK_COMMAND_STOP,
    CRAZYPOD_PLAYBACK_COMMAND_NEXT,
    CRAZYPOD_PLAYBACK_COMMAND_PREVIOUS,
    CRAZYPOD_PLAYBACK_COMMAND_SELECT,
    CRAZYPOD_PLAYBACK_COMMAND_SELECT_MUSIC,
    CRAZYPOD_PLAYBACK_COMMAND_SYNC_PLAY_STATE,
};

struct crazypod_music_selection_request {
    enum crazypod_music_scope scope;
    int group_index;
    int selected_index;
    unsigned generation;
    bool shuffle_all;
    char query[PLAYBACK_SEARCH_QUERY_SIZE];
};

struct crazypod_lock_playback_cache {
    char path[MAX_PATH];
    char warmed_path[MAX_PATH];
    char warming_path[MAX_PATH];
    char title[96];
    char artist[72];
    char album[72];
    uint32_t elapsed_ms;
    uint32_t length_ms;
    long captured_tick;
    int requested_queue_index;
    int requested_playing;
    bool valid;
    bool elapsed_advancing;
};

static struct mutex lock_playback_mutex;
static struct event_queue playback_command_queue;
static long playback_command_stack[
    PLAYBACK_COMMAND_STACK_SIZE / sizeof(long)];
static bool playback_command_thread_started;
static struct event_queue lock_metadata_warm_queue;
static long lock_metadata_warm_stack[
    LOCK_METADATA_WARM_STACK_SIZE / sizeof(long)];
static bool lock_metadata_warm_thread_started;
static struct crazypod_lock_playback_cache lock_playback;
static struct crazypod_music_selection_request music_selection;
static unsigned music_selection_generation;
static bool music_selection_pending;
static bool music_selection_failed;
static bool play_state_command_queued;
static bool stop_command_queued;

static struct {
    struct crazypod_playback_host host;
    unsigned preview_generation;
    unsigned menu_generation;
    unsigned track_queue_generation;
    unsigned track_catalog_generation;
    int track_queue_index;
    int track_library_index;
    int track_album_index;
    long last_album_warm;
    uint32_t unlock_present_sequence;
    uint32_t seek_target_ms;
    uint32_t seek_length_ms;
    uint32_t seek_step_ms;
    int seek_direction;
    bool seeking;
    bool unlock_refresh_pending;
    bool unlock_capsule_entry;
} playback;

static struct route_state *current_route(void)
{
    return crazypod_ui_routes_current();
}

static int current_track_index(void)
{
    unsigned queue_generation = crazypod_queue_generation();
    unsigned catalog_generation =
        crazypod_music_scan_generation();
    int queue_index = crazypod_queue_index();

    if(!crazypod_music_catalog_ready()) {
        playback.track_queue_generation = queue_generation;
        playback.track_catalog_generation = catalog_generation;
        playback.track_queue_index = queue_index;
        playback.track_library_index = -1;
        playback.track_album_index = -1;
        return -1;
    }
    if(playback.track_queue_generation != queue_generation ||
       playback.track_catalog_generation != catalog_generation ||
       playback.track_queue_index != queue_index) {
        char path[MAX_PATH];
        struct crazypod_track track;
        bool have_track;
        int album_count;
        int i;

        playback.track_queue_generation = queue_generation;
        playback.track_catalog_generation = catalog_generation;
        playback.track_queue_index = queue_index;
        playback.track_library_index =
            crazypod_queue_copy_path(queue_index, path, sizeof(path))
                ? crazypod_music_find_track(path) : -1;
        playback.track_album_index = -1;
        have_track = crazypod_music_copy_track(
            playback.track_library_index, &track);
        album_count = crazypod_music_album_count();
        for(i = 0; have_track && i < album_count; ++i) {
            struct crazypod_album album;

            if(crazypod_music_copy_album(i, &album) &&
               strcmp(album.title, track.album) == 0 &&
               strcmp(album.artist, track.album_artist) == 0) {
                playback.track_album_index = i;
                break;
            }
        }
    }
    return playback.track_library_index;
}

static bool copy_current_track(struct crazypod_track *track)
{
    return crazypod_music_copy_track(current_track_index(), track);
}

static bool copy_track_at_queue_index(int queue_index,
                                      struct crazypod_track *track)
{
    char path[MAX_PATH];

    return crazypod_queue_copy_path(queue_index, path, sizeof(path)) &&
        crazypod_music_copy_track(crazypod_music_find_track(path), track);
}

static bool copy_track_at_path(const char *path,
                               struct crazypod_track *track)
{
    return crazypod_music_copy_track(crazypod_music_find_track(path), track);
}

static void copy_lock_playback_text(
    char *destination, size_t size, const char *source)
{
    snprintf(destination, size, "%s", source != NULL ? source : "");
}

static void request_lock_metadata_warm(const char *path)
{
    bool post = false;

    if(!lock_metadata_warm_thread_started ||
       path == NULL || path[0] == '\0')
        return;
    mutex_lock(&lock_playback_mutex);
    if(strcmp(lock_playback.warmed_path, path) != 0 &&
       strcmp(lock_playback.warming_path, path) != 0) {
        copy_lock_playback_text(
            lock_playback.warming_path,
            sizeof(lock_playback.warming_path), path);
        post = true;
    }
    mutex_unlock(&lock_playback_mutex);
    if(post)
        queue_post(
            &lock_metadata_warm_queue,
            LOCK_METADATA_WARM_EVENT, 0);
}

static void lock_playback_track_event(
    unsigned short id, void *event_data)
{
    const struct track_event *event = event_data;
    const struct mp3entry *id3;
    char path[MAX_PATH];
    bool playing;

    (void)id;
    if(event == NULL || event->id3 == NULL)
        return;
    id3 = event->id3;
    playing =
        (audio_status() & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) ==
            AUDIO_STATUS_PLAY;
    mutex_lock(&lock_playback_mutex);
    copy_lock_playback_text(
        lock_playback.path, sizeof(lock_playback.path), id3->path);
    copy_lock_playback_text(
        lock_playback.title, sizeof(lock_playback.title), id3->title);
    copy_lock_playback_text(
        lock_playback.artist, sizeof(lock_playback.artist), id3->artist);
    copy_lock_playback_text(
        lock_playback.album, sizeof(lock_playback.album), id3->album);
    lock_playback.elapsed_ms = id3->elapsed > 0
        ? (uint32_t)id3->elapsed : 0;
    lock_playback.length_ms = id3->length > 0
        ? (uint32_t)id3->length : 0;
    lock_playback.captured_tick = current_tick;
    lock_playback.valid = true;
    lock_playback.elapsed_advancing = playing;
    lock_playback.warmed_path[0] = '\0';
    copy_lock_playback_text(path, sizeof(path), lock_playback.path);
    if(lock_playback.requested_queue_index >= 0) {
        char requested_path[MAX_PATH];

        if(crazypod_queue_copy_path(
               lock_playback.requested_queue_index,
               requested_path, sizeof(requested_path)) &&
           strcmp(requested_path, lock_playback.path) == 0)
            lock_playback.requested_queue_index = -1;
    }
    mutex_unlock(&lock_playback_mutex);
    request_lock_metadata_warm(path);
}

static void lock_metadata_warm_thread(void)
{
    struct queue_event event;

    while(true) {
        struct crazypod_lock_playback_cache cached;
        struct crazypod_track track;
        bool have_track;
        const char *path;
        const char *title;
        const char *artist;
        const char *album;

        queue_wait(&lock_metadata_warm_queue, &event);
        if(event.id != LOCK_METADATA_WARM_EVENT)
            continue;
        mutex_lock(&lock_playback_mutex);
        cached = lock_playback;
        mutex_unlock(&lock_playback_mutex);
        if(cached.warming_path[0] == '\0')
            continue;
        path = cached.warming_path;
        have_track = copy_track_at_path(path, &track);
        title = have_track ? track.title :
            strcmp(path, cached.path) == 0 ? cached.title : "";
        artist = have_track ? track.artist :
            strcmp(path, cached.path) == 0 ? cached.artist : "";
        album = have_track ? track.album :
            strcmp(path, cached.path) == 0 ? cached.album : "";
        crazypod_runtime_font_prewarm_text(
            LOCK_TITLE_FONT_SIZE, title);
        crazypod_runtime_font_prewarm_text(
            LOCK_ARTIST_FONT_SIZE, artist);
        crazypod_runtime_font_prewarm_text(
            LOCK_ALBUM_FONT_SIZE, album);
        mutex_lock(&lock_playback_mutex);
        copy_lock_playback_text(
            lock_playback.warmed_path,
            sizeof(lock_playback.warmed_path), path);
        if(strcmp(lock_playback.warming_path, path) == 0)
            lock_playback.warming_path[0] = '\0';
        mutex_unlock(&lock_playback_mutex);
    }
}

static void playback_command_thread(void)
{
    struct queue_event event;

    while(true) {
        int requested_playing;

        queue_wait(&playback_command_queue, &event);
        switch(event.id) {
        case CRAZYPOD_PLAYBACK_COMMAND_PAUSE:
            audio_pause();
            break;
        case CRAZYPOD_PLAYBACK_COMMAND_RESUME:
            audio_resume();
            break;
        case CRAZYPOD_PLAYBACK_COMMAND_STOP:
            mutex_lock(&lock_playback_mutex);
            stop_command_queued = false;
            mutex_unlock(&lock_playback_mutex);
            audio_stop();
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
            break;
        case CRAZYPOD_PLAYBACK_COMMAND_NEXT:
            crazypod_playback_next();
            break;
        case CRAZYPOD_PLAYBACK_COMMAND_PREVIOUS:
            crazypod_playback_previous_or_restart();
            break;
        case CRAZYPOD_PLAYBACK_COMMAND_SELECT:
            if(event.data >= 0 && event.data < crazypod_queue_count()) {
                playlist_start((int)event.data, 0, 0);
                crazypod_state_forget_resume();
                crazypod_state_mark_dirty();
            }
            break;
        case CRAZYPOD_PLAYBACK_COMMAND_SELECT_MUSIC:
        {
            struct crazypod_music_selection_request request;
            bool started;

            mutex_lock(&lock_playback_mutex);
            request = music_selection;
            music_selection_pending = false;
            mutex_unlock(&lock_playback_mutex);
            if(request.shuffle_all)
                started = crazypod_music_shuffle_all(
                    (unsigned int)current_tick);
            else
                started = request.scope == CRAZYPOD_SCOPE_ALL ||
                    request.scope == CRAZYPOD_SCOPE_ARTIST ||
                    request.scope == CRAZYPOD_SCOPE_ALBUM ||
                    request.scope == CRAZYPOD_SCOPE_PLAYLIST
                        ? crazypod_music_play(
                            request.scope, request.group_index,
                            request.selected_index)
                        : crazypod_music_play_search(
                            request.query, request.selected_index);
            if(started) {
                crazypod_state_forget_resume();
                crazypod_state_mark_dirty();
            }
            else {
                mutex_lock(&lock_playback_mutex);
                if(request.generation == music_selection_generation)
                    music_selection_failed = true;
                mutex_unlock(&lock_playback_mutex);
            }
            break;
        }
        case CRAZYPOD_PLAYBACK_COMMAND_SYNC_PLAY_STATE:
        {
            bool playing;

            mutex_lock(&lock_playback_mutex);
            playing = lock_playback.requested_playing != 0;
            play_state_command_queued = false;
            mutex_unlock(&lock_playback_mutex);
            if(playing)
                audio_resume();
            else
                audio_pause();
            break;
        }
        default:
            continue;
        }
        requested_playing =
            (audio_status() & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) ==
                AUDIO_STATUS_PLAY;
        mutex_lock(&lock_playback_mutex);
        if(lock_playback.requested_playing == requested_playing)
            lock_playback.requested_playing = -1;
        mutex_unlock(&lock_playback_mutex);
    }
}

static unsigned menu_artwork_signature(void)
{
    unsigned signature = 2166136261u;
    bool flow = crazypod_shell_product_active() &&
        crazypod_ui_routes_depth() > 0 &&
        current_route()->route == MUSIC_ROUTE_MENU &&
        current_route()->selected == 1;
    int first = flow
        ? CRAZYPOD_MENU_PREVIEW_FLOW_SLOT_BASE : 0;
    int count = flow ? 3 : 7;
    int slot;

    for(slot = first; slot < first + count; ++slot) {
        signature ^=
            crazypod_artwork_slot_generation(slot);
        signature *= 16777619u;
    }
    signature ^= crazypod_artwork_slot_generation(
        CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    return signature * 16777619u;
}

void crazypod_playback_configure(
    const struct crazypod_playback_host *host)
{
    if(host != NULL)
        playback.host = *host;
}

void crazypod_playback_initialize(void)
{
    playback.preview_generation =
        crazypod_artwork_slot_generation(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
    playback.menu_generation =
        menu_artwork_signature();
    playback.track_queue_generation = (unsigned)-1;
    playback.track_catalog_generation = (unsigned)-1;
    playback.track_queue_index = -1;
    playback.track_library_index = -1;
    playback.track_album_index = -1;
    playback.last_album_warm = 0;
    playback.unlock_present_sequence = 0;
    playback.seek_target_ms = 0;
    playback.seek_length_ms = 0;
    playback.seek_step_ms = 0;
    playback.seek_direction = 0;
    playback.seeking = false;
    playback.unlock_refresh_pending = false;
    playback.unlock_capsule_entry = false;
    mutex_init(&lock_playback_mutex);
    lock_playback = (struct crazypod_lock_playback_cache){0};
    lock_playback.requested_queue_index = -1;
    lock_playback.requested_playing = -1;
    memset(&music_selection, 0, sizeof(music_selection));
    music_selection_generation = 0;
    music_selection_pending = false;
    music_selection_failed = false;
    play_state_command_queued = false;
    stop_command_queued = false;
    if(!lock_metadata_warm_thread_started) {
        unsigned int thread_id;

        queue_init(&lock_metadata_warm_queue, false);
        thread_id = create_thread(
            lock_metadata_warm_thread, lock_metadata_warm_stack,
            sizeof(lock_metadata_warm_stack), 0,
            "crazypod lock font"
            IF_PRIO(, PRIORITY_BACKGROUND)
            IF_COP(, CPU));
        lock_metadata_warm_thread_started = thread_id != 0;
    }
    add_event(
        PLAYBACK_EVENT_TRACK_CHANGE, lock_playback_track_event);
    add_event(
        PLAYBACK_EVENT_CUR_TRACK_READY, lock_playback_track_event);
    if(!playback_command_thread_started) {
        unsigned int thread_id;

        queue_init(&playback_command_queue, false);
        thread_id = create_thread(
            playback_command_thread, playback_command_stack,
            sizeof(playback_command_stack), 0,
            "crazypod playback"
            IF_PRIO(, PRIORITY_BACKGROUND)
            IF_COP(, CPU));
        playback_command_thread_started = thread_id != 0;
        if(!playback_command_thread_started)
            panicf("playback command thread");
    }
}

bool crazypod_playback_commands_ready(void)
{
    return playback_command_thread_started;
}

void crazypod_playback_headphone_changed(bool inserted)
{
    static bool headphone_caused_pause = true;
    int status;

    if(global_settings.unplug_mode) {
        status = audio_status();
        if(inserted) {
            backlight_on();
            if((status & AUDIO_STATUS_PLAY) &&
               headphone_caused_pause &&
               global_settings.unplug_mode > 1) {
                audio_resume();
                crazypod_now_playing_overlay_refresh_after_playback();
            }
            headphone_caused_pause = false;
        }
        else if((status & AUDIO_STATUS_PLAY) &&
                !(status & AUDIO_STATUS_PAUSE)) {
            headphone_caused_pause = true;
            audio_pause();
            crazypod_now_playing_overlay_refresh_after_playback();
        }
    }
#ifdef HAVE_SPEAKER
    audio_enable_speaker(global_settings.speaker_mode);
#endif
}

int crazypod_playback_initial_album_index(void)
{
    struct crazypod_track track;

    if(!copy_current_track(&track) || playback.track_album_index < 0)
        return 0;
    return playback.track_album_index;
}

void crazypod_playback_toggle(void)
{
    int status = audio_status();

    if(crazypod_queue_count() <= 0) {
        bool started = false;
        bool deferred = false;

        if(crazypod_music_track_count() > 0) {
            if(playback_command_thread_started) {
                started = crazypod_playback_select_music_async(
                    CRAZYPOD_SCOPE_ALL, 0, 0, NULL);
                deferred = started;
            }
            else
                started = crazypod_music_play(
                    CRAZYPOD_SCOPE_ALL, 0, 0);
        }
        if(started && !deferred) {
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
        }
        return;
    }
    if(status & AUDIO_STATUS_PAUSE)
        audio_resume();
    else if(status & AUDIO_STATUS_PLAY)
        audio_pause();
    else
        playlist_start(
            crazypod_queue_index(),
            crazypod_state_take_resume_elapsed(), 0);
    crazypod_now_playing_overlay_refresh_after_playback();
}

static bool start_adjacent_from_restored_queue(int direction)
{
    int index;

    if((audio_status() & AUDIO_STATUS_PLAY) != 0)
        return false;
    index = playlist_next(direction);
    if(index >= 0) {
        playlist_start(index, 0, 0);
        crazypod_state_forget_resume();
        crazypod_state_mark_dirty();
    }
    return true;
}

void crazypod_playback_next(void)
{
    if(crazypod_queue_count() <= 0 ||
       start_adjacent_from_restored_queue(1))
        return;
    audio_next();
}

void crazypod_playback_previous_or_restart(void)
{
    const struct mp3entry *id3;

    if(crazypod_queue_count() <= 0 ||
       start_adjacent_from_restored_queue(-1))
        return;
    id3 = audio_current_track();
    if(id3 != NULL &&
       id3->elapsed >= PREVIOUS_RESTART_THRESHOLD_MS) {
        audio_pre_ff_rewind();
        audio_ff_rewind(0);
    }
    else
        audio_prev();
}

bool crazypod_playback_seek_begin(int direction)
{
    const struct mp3entry *id3 = audio_current_track();
    uint32_t initial_step;

    if(direction == 0 || id3 == NULL || id3->length <= 0 ||
       (audio_status() & AUDIO_STATUS_PLAY) == 0)
        return false;
    playback.seek_length_ms = (uint32_t)id3->length;
    playback.seek_target_ms = id3->elapsed > 0
        ? (uint32_t)id3->elapsed : 0;
    if(playback.seek_target_ms >= playback.seek_length_ms)
        playback.seek_target_ms = playback.seek_length_ms - 1;
    initial_step = 1000u *
        (uint32_t)global_settings.ff_rewind_min_step;
    playback.seek_step_ms = initial_step < SEEK_MIN_STEP_MS
        ? SEEK_MIN_STEP_MS : initial_step;
    playback.seek_direction = direction > 0 ? 1 : -1;
    playback.seeking = true;
    audio_pre_ff_rewind();
    return true;
}

void crazypod_playback_seek_step(void)
{
    uint32_t remaining;
    uint32_t maximum_step;
    uint32_t step;
    unsigned int shift;

    if(!playback.seeking)
        return;
    remaining = playback.seek_direction > 0
        ? playback.seek_length_ms - 1 - playback.seek_target_ms
        : playback.seek_target_ms;
    maximum_step = remaining * SEEK_MAX_STEP_PERCENT / 100u;
    if(maximum_step < SEEK_MIN_STEP_MS)
        maximum_step = SEEK_MIN_STEP_MS;
    step = playback.seek_step_ms;
    if(step > maximum_step)
        step = maximum_step;
    if(step > remaining)
        step = remaining;
    if(playback.seek_direction > 0)
        playback.seek_target_ms += step;
    else
        playback.seek_target_ms -= step;
    shift = (unsigned int)global_settings.ff_rewind_accel + 3u;
    playback.seek_step_ms += playback.seek_step_ms >> shift;
}

void crazypod_playback_seek_finish(void)
{
    if(!playback.seeking)
        return;
    audio_ff_rewind((long)playback.seek_target_ms);
    playback.seeking = false;
    playback.seek_direction = 0;
    crazypod_state_mark_dirty();
    crazypod_now_playing_overlay_refresh_after_playback();
}

static int adjacent_queue_index(int index, int direction)
{
    int count = crazypod_queue_count();

    if(count <= 0)
        return -1;
    if(global_settings.repeat_mode == REPEAT_ONE)
        return index;
    index += direction;
    if(index >= 0 && index < count)
        return index;
    if(global_settings.repeat_mode != REPEAT_ALL)
        return -1;
    return index < 0 ? count - 1 : 0;
}

static uint32_t cached_lock_elapsed(const char *path, bool playing)
{
    uint32_t elapsed = 0;

    mutex_lock(&lock_playback_mutex);
    if(lock_playback.valid && path != NULL &&
       strcmp(lock_playback.path, path) == 0) {
        elapsed = lock_playback.elapsed_ms;
        if(lock_playback.elapsed_advancing &&
           TIME_AFTER(current_tick, lock_playback.captured_tick)) {
            uint64_t delta = (uint64_t)
                (current_tick - lock_playback.captured_tick) * 1000;

            elapsed += (uint32_t)(delta / HZ);
        }
        if(lock_playback.elapsed_advancing != playing) {
            lock_playback.elapsed_ms = elapsed;
            lock_playback.captured_tick = current_tick;
            lock_playback.elapsed_advancing = playing;
        }
    }
    mutex_unlock(&lock_playback_mutex);
    return elapsed;
}

void crazypod_playback_toggle_async(void)
{
    bool playing;
    bool post = false;

    if(!playback_command_thread_started)
        return;
    mutex_lock(&lock_playback_mutex);
    playing = lock_playback.requested_playing >= 0
        ? lock_playback.requested_playing != 0
        : (audio_status() & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) ==
            AUDIO_STATUS_PLAY;
    lock_playback.requested_playing = !playing;
    if(!play_state_command_queued) {
        play_state_command_queued = true;
        post = true;
    }
    mutex_unlock(&lock_playback_mutex);
    if(post)
        queue_post(&playback_command_queue,
                   CRAZYPOD_PLAYBACK_COMMAND_SYNC_PLAY_STATE, 0);
}

void crazypod_playback_stop_async(void)
{
    bool post = false;

    if(!playback_command_thread_started)
        return;
    mutex_lock(&lock_playback_mutex);
    lock_playback.requested_playing = false;
    if(!stop_command_queued) {
        stop_command_queued = true;
        post = true;
    }
    mutex_unlock(&lock_playback_mutex);
    if(post)
        queue_post(&playback_command_queue,
                   CRAZYPOD_PLAYBACK_COMMAND_STOP, 0);
}

void crazypod_playback_next_async(void)
{
    char path[MAX_PATH];
    int index;

    if(!playback_command_thread_started)
        return;
    mutex_lock(&lock_playback_mutex);
    index = lock_playback.requested_queue_index >= 0
        ? lock_playback.requested_queue_index
        : crazypod_queue_index();
    index = adjacent_queue_index(index, 1);
    if(index >= 0)
        lock_playback.requested_queue_index = index;
    mutex_unlock(&lock_playback_mutex);
    if(index >= 0)
        queue_post(
            &playback_command_queue,
            CRAZYPOD_PLAYBACK_COMMAND_NEXT, 0);
    if(index >= 0 &&
       crazypod_queue_copy_path(index, path, sizeof(path)))
        request_lock_metadata_warm(path);
}

void crazypod_playback_previous_or_restart_async(void)
{
    char path[MAX_PATH];
    bool playing;
    int current_index;
    int requested_index;

    if(!playback_command_thread_started)
        return;
    mutex_lock(&lock_playback_mutex);
    current_index = lock_playback.requested_queue_index >= 0
        ? lock_playback.requested_queue_index
        : crazypod_queue_index();
    playing = lock_playback.requested_playing >= 0
        ? lock_playback.requested_playing != 0
        : (audio_status() & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) ==
            AUDIO_STATUS_PLAY;
    mutex_unlock(&lock_playback_mutex);
    requested_index = cached_lock_elapsed(
            crazypod_queue_copy_path(
                current_index, path, sizeof(path)) ? path : NULL,
            playing) >=
            PREVIOUS_RESTART_THRESHOLD_MS
        ? current_index : adjacent_queue_index(current_index, -1);
    if(requested_index < 0)
        return;
    mutex_lock(&lock_playback_mutex);
    lock_playback.requested_queue_index = requested_index;
    mutex_unlock(&lock_playback_mutex);
    queue_post(
        &playback_command_queue,
        CRAZYPOD_PLAYBACK_COMMAND_PREVIOUS, 0);
    if(crazypod_queue_copy_path(
           requested_index, path, sizeof(path)))
        request_lock_metadata_warm(path);
}

void crazypod_playback_select_async(int queue_index)
{
    if(!playback_command_thread_started ||
       queue_index < 0 || queue_index >= crazypod_queue_count())
        return;
    mutex_lock(&lock_playback_mutex);
    lock_playback.requested_queue_index = queue_index;
    lock_playback.requested_playing = true;
    mutex_unlock(&lock_playback_mutex);
    queue_post(
        &playback_command_queue,
        CRAZYPOD_PLAYBACK_COMMAND_SELECT, queue_index);
}

bool crazypod_playback_select_music_async(
    enum crazypod_music_scope scope, int group_index,
    int selected_index, const char *query)
{
    bool post = false;

    if(!playback_command_thread_started)
        return false;
    if(scope != CRAZYPOD_SCOPE_ALL &&
       scope != CRAZYPOD_SCOPE_ARTIST &&
       scope != CRAZYPOD_SCOPE_ALBUM &&
       scope != CRAZYPOD_SCOPE_PLAYLIST &&
       scope != CRAZYPOD_SCOPE_SEARCH)
        return false;
    mutex_lock(&lock_playback_mutex);
    ++music_selection_generation;
    music_selection.scope = scope;
    music_selection.group_index = group_index;
    music_selection.selected_index = selected_index;
    music_selection.generation = music_selection_generation;
    music_selection.shuffle_all = false;
    snprintf(music_selection.query, sizeof(music_selection.query),
             "%s", query != NULL ? query : "");
    music_selection_failed = false;
    if(!music_selection_pending) {
        music_selection_pending = true;
        post = true;
    }
    mutex_unlock(&lock_playback_mutex);
    if(post)
        queue_post(&playback_command_queue,
                   CRAZYPOD_PLAYBACK_COMMAND_SELECT_MUSIC, 0);
    return true;
}

bool crazypod_playback_shuffle_all_async(void)
{
    bool post = false;

    if(!playback_command_thread_started)
        return false;
    mutex_lock(&lock_playback_mutex);
    ++music_selection_generation;
    music_selection.scope = CRAZYPOD_SCOPE_ALL;
    music_selection.group_index = 0;
    music_selection.selected_index = 0;
    music_selection.generation = music_selection_generation;
    music_selection.shuffle_all = true;
    music_selection.query[0] = '\0';
    music_selection_failed = false;
    if(!music_selection_pending) {
        music_selection_pending = true;
        post = true;
    }
    mutex_unlock(&lock_playback_mutex);
    if(post)
        queue_post(&playback_command_queue,
                   CRAZYPOD_PLAYBACK_COMMAND_SELECT_MUSIC, 0);
    return true;
}

void crazypod_playback_refresh_lock_screen(void)
{
    struct crazypod_lock_playback_cache cached;
    struct crazypod_track track;
    bool have_track;
    const lv_image_dsc_t *artwork = NULL;
    const char *artwork_path = NULL;
    const char *track_path = NULL;
    unsigned artwork_generation = 0;
    int status = audio_status();
    bool active;
    bool playing;
    struct crazypod_lock_media_snapshot snapshot = {0};

    mutex_lock(&lock_playback_mutex);
    cached = lock_playback;
    mutex_unlock(&lock_playback_mutex);
    have_track = cached.valid
        ? copy_track_at_path(cached.path, &track)
        : copy_track_at_queue_index(
            crazypod_queue_index(), &track);
    track_path = cached.valid ? cached.path :
        have_track ? track.path : NULL;
    request_lock_metadata_warm(track_path);
    active = track_path != NULL && track_path[0] != '\0' &&
        (status & AUDIO_STATUS_PLAY) != 0;
    playing = cached.requested_playing >= 0
        ? cached.requested_playing != 0
        : (status & AUDIO_STATUS_PAUSE) == 0;
    if(active) {
        crazypod_now_playing_artwork_sync();
        artwork = crazypod_now_playing_artwork_committed(
            &artwork_path, &artwork_generation);
        if(track_path == NULL || artwork_path == NULL ||
           strcmp(track_path, artwork_path) != 0)
            artwork = NULL;
        snapshot.active = true;
        snapshot.playing = playing;
        snapshot.metadata_ready =
            strcmp(cached.warmed_path, track_path) == 0;
        snapshot.track_path = track_path;
        snapshot.title = have_track && track.title[0] != '\0'
            ? track.title : cached.title;
        snapshot.artist = have_track && track.artist[0] != '\0'
            ? track.artist : cached.artist;
        snapshot.album = have_track && track.album[0] != '\0'
            ? track.album : cached.album;
        snapshot.elapsed_ms = cached_lock_elapsed(
            track_path, playing);
        snapshot.length_ms = have_track
            ? track.duration_ms : cached.length_ms;
        if(snapshot.length_ms > 0 &&
           snapshot.elapsed_ms > snapshot.length_ms)
            snapshot.elapsed_ms = snapshot.length_ms;
        snapshot.artwork = artwork;
        snapshot.artwork_generation = artwork_generation;
    }
    crazypod_lock_screen_update_media(&snapshot);
}

void crazypod_playback_update_timer(lv_timer_t *timer)
{
    struct crazypod_track track;
    bool have_track;
    bool music_selection_error;
    struct mp3entry *id3;

    (void)timer;
    if(!is_backlight_on(true))
        return;
    if(crazypod_lock_screen_is_locked()) {
        crazypod_playback_refresh_lock_screen();
        return;
    }
    if(playback.unlock_refresh_pending)
        return;
    mutex_lock(&lock_playback_mutex);
    music_selection_error = music_selection_failed;
    music_selection_failed = false;
    mutex_unlock(&lock_playback_mutex);
    if(music_selection_error)
        crazypod_notification_show(
            CRAZYPOD_NOTIFICATION_ERROR, CP_TR("No track available"));
    if(crazypod_music_library_update())
        return;
    crazypod_app_launcher_process_pending();
    have_track = copy_current_track(&track);
    id3 = audio_current_track();
    crazypod_now_playing_artwork_sync();
    crazypod_now_capsule_update(
        have_track ? &track : NULL,
        id3 != NULL ? (uint32_t)id3->elapsed : 0,
        id3 != NULL ? (uint32_t)id3->length : 0);
    if(crazypod_ui_routes_depth() <= 0 ||
       current_route()->route != MUSIC_ROUTE_NOW_PLAYING)
        return;
    if(crazypod_now_playing_theme_open())
        return;
    if(have_track &&
       strcmp(
           crazypod_now_playing_feature_rendered_track_path(),
           track.path) != 0) {
        enum crazypod_now_playing_overlay overlay =
            crazypod_now_playing_overlay_kind();

        playback.host.render(false);
        crazypod_now_playing_overlay_restore(overlay);
        return;
    }
    if(!have_track &&
       crazypod_now_playing_feature_rendered_track_path()[0] != '\0') {
        enum crazypod_now_playing_overlay overlay =
            crazypod_now_playing_overlay_kind();

        playback.host.render(false);
        crazypod_now_playing_overlay_restore(overlay);
        return;
    }
    crazypod_now_playing_overlay_refresh_tick();
    if(id3 != NULL)
        crazypod_now_playing_feature_update_playback(
            playback.seeking
                ? playback.seek_target_ms
                : (uint32_t)id3->elapsed,
            (uint32_t)id3->length);
}

void crazypod_playback_request_refresh_after_unlock(
    uint32_t present_sequence, bool animate_capsule)
{
    playback.unlock_present_sequence = present_sequence;
    playback.unlock_refresh_pending = true;
    playback.unlock_capsule_entry = animate_capsule;
    if(animate_capsule)
        crazypod_now_capsule_prepare_entry();
}

bool crazypod_playback_refresh_after_unlock_pending(void)
{
    return playback.unlock_refresh_pending;
}

void crazypod_playback_service_after_unlock(
    uint32_t present_sequence)
{
    struct crazypod_track track;
    bool have_track;
    struct mp3entry *id3;
    bool animate_capsule;

    if(!playback.unlock_refresh_pending ||
       present_sequence == playback.unlock_present_sequence)
        return;
    animate_capsule = playback.unlock_capsule_entry;
    playback.unlock_refresh_pending = false;
    playback.unlock_capsule_entry = false;
    if(!animate_capsule)
        return;

    have_track = copy_current_track(&track);
    id3 = audio_current_track();
    crazypod_now_capsule_update(
        have_track ? &track : NULL,
        id3 != NULL ? (uint32_t)id3->elapsed : 0,
        id3 != NULL ? (uint32_t)id3->length : 0);
    crazypod_now_capsule_start_entry();
}

void crazypod_playback_process_artwork(void)
{
    struct crazypod_track track;
    bool have_track;
    unsigned generation;

    if(playback.unlock_refresh_pending)
        return;
    crazypod_now_playing_artwork_sync();
    have_track = copy_current_track(&track);
    crazypod_now_capsule_poll_artwork(have_track ? &track : NULL);
    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0 ||
       crazypod_music_library_loading() ||
       crazypod_render_scheduler_blocked() ||
       crazypod_coverflow_active() ||
       current_route()->route >= DIY_ROUTE_MENU)
        return;
    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING &&
       crazypod_now_playing_overlay_visible())
        return;
    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING &&
       crazypod_now_playing_theme_open())
        return;
    if(current_route()->route == MUSIC_ROUTE_NOW_PLAYING) {
        if(!crazypod_now_playing_artwork_changed())
            return;
    }
    else if(current_route()->route == MUSIC_ROUTE_MENU) {
        generation = menu_artwork_signature();
        if(generation == playback.menu_generation ||
           crazypod_preview_motion_active())
            return;
        playback.menu_generation = generation;
        crazypod_menu_preview_render(current_route(), false);
        return;
    }
    else {
        generation = crazypod_artwork_slot_generation(
            CRAZYPOD_PREVIEW_ARTWORK_SLOT);
        if(generation == playback.preview_generation)
            return;
        if(crazypod_menu_list_matches(
               current_route()->route) &&
           crazypod_menu_preview_is_music_route(
               current_route()->route) &&
           crazypod_preview_motion_active())
            return;
        playback.preview_generation = generation;
    }
    if(crazypod_menu_list_matches(current_route()->route) &&
       crazypod_menu_preview_is_music_route(
           current_route()->route))
        crazypod_menu_preview_render(current_route(), false);
    else
        playback.host.render(false);
}

void crazypod_playback_process_media(void)
{
    enum crazypod_feature_media_update update =
        CRAZYPOD_FEATURE_MEDIA_NONE;
    enum crazypod_route route;

    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0 ||
       crazypod_choice_coordinator_visible())
        return;
    route = current_route()->route;
    if(route >= PHOTOS_ROUTE_MENU &&
       route <= PHOTOS_ROUTE_DETAIL)
        update = crazypod_photos_feature_poll_media(
            route, crazypod_render_scheduler_blocked(),
            crazypod_preview_motion_active());
    else if(route == DIY_ROUTE_WALLPAPER_FILES ||
            route == DIY_ROUTE_WALLPAPER_CROP)
        update = crazypod_customize_feature_poll_media(
            route, crazypod_render_scheduler_blocked());
    if(update == CRAZYPOD_FEATURE_MEDIA_PREVIEW)
        crazypod_menu_preview_render(current_route(), false);
    else if(update == CRAZYPOD_FEATURE_MEDIA_ROUTE)
        playback.host.render(false);
}

void crazypod_playback_tick_wave(long now)
{
    bool active = crazypod_shell_product_active() &&
        crazypod_ui_routes_depth() > 0 &&
        current_route()->route == MUSIC_ROUTE_NOW_PLAYING;

    if(!crazypod_now_playing_theme_open())
        crazypod_now_playing_feature_tick_wave(now, active);
}

void crazypod_playback_sync_album_flow(void)
{
    int index;

    if(!crazypod_shell_product_active() ||
       crazypod_ui_routes_depth() <= 0 ||
       !crazypod_coverflow_active() ||
       current_route()->route != MUSIC_ROUTE_ALBUM_FLOW)
        return;
    index = crazypod_music_feature_sync_album_flow();
    if(index >= 0)
        current_route()->selected = index;
}

void crazypod_playback_warm_album_flow(
    long now, bool locked)
{
    if(!crazypod_shell_product_active() || locked ||
       crazypod_ui_routes_depth() <= 0 ||
       crazypod_coverflow_active() ||
       crazypod_render_scheduler_preview_pending() ||
       crazypod_preview_motion_active() ||
       current_route()->route != MUSIC_ROUTE_MENU ||
       current_route()->selected != 1)
        return;
    if(playback.last_album_warm != 0 &&
       TIME_BEFORE(
           now, playback.last_album_warm + HZ / 10))
        return;
    playback.last_album_warm = now;
    (void)crazypod_coverflow_warm(
        crazypod_playback_initial_album_index());
}

#endif
