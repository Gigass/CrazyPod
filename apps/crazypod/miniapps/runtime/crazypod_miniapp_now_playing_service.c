#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "pcm.h"
#include "pcm_mixer.h"
#include "playlist.h"
#include "settings.h"

#include "../../../../miniapps/sdk/crazypod_miniapp_native.h"
#include "../../crazypod_lyrics.h"
#include "../../crazypod_music.h"
#include "../../crazypod_playlist.h"
#include "../../crazypod_state.h"
#include "../../ui/app/crazypod_playback.h"
#include "../../ui/features/now_playing/crazypod_now_playing_feature.h"
#include "crazypod_miniapp_now_playing_service.h"

bool crazypod_miniapp_now_playing_copy_track(struct crazypod_track *track)
{
    char path[MAX_PATH];

    return crazypod_queue_copy_path(
            crazypod_queue_index(), path, sizeof(path)) &&
        crazypod_music_copy_track(crazypod_music_find_track(path), track);
}

static uint32_t track_revision(const struct crazypod_track *track)
{
    uint32_t value = crazypod_queue_generation();

    value = value * 16777619u ^
        (uint32_t)(crazypod_queue_index() + 1);
    if(track != NULL)
        value = value * 16777619u ^
            crazypod_now_playing_artwork_committed_generation();
    return value;
}

static void copy_text(char *destination, size_t capacity,
                      const char *source)
{
    if(source == NULL)
        source = "";
    snprintf(destination, capacity, "%s", source);
}

static int snapshot_call(
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    static struct pcm_peaks playback_peaks;
    struct cp_now_playing_snapshot snapshot;
    struct crazypod_track track;
    bool have_track = crazypod_miniapp_now_playing_copy_track(&track);
    const struct mp3entry *id3 = audio_current_track();
    size_t response_size;
    int audio = audio_status();

    if(request != NULL || request_size != 0 || response == NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(response_capacity < CP_NOW_PLAYING_SNAPSHOT_BASE_SIZE)
        return CP_NATIVE_ERROR_LIMIT;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.struct_size = sizeof(snapshot);
    snapshot.revision = track_revision(have_track ? &track : NULL);
    snapshot.elapsed_ms = id3 != NULL
        ? (uint32_t)id3->elapsed : 0;
    snapshot.length_ms = id3 != NULL
        ? (uint32_t)id3->length : 0;
    snapshot.status = (audio & AUDIO_STATUS_PLAY) == 0
        ? CP_NOW_PLAYING_STOPPED
        : (audio & AUDIO_STATUS_PAUSE) != 0
            ? CP_NOW_PLAYING_PAUSED : CP_NOW_PLAYING_PLAYING;
    snapshot.volume = global_status.volume;
    snapshot.repeat_mode = crazypod_queue_repeat();
    snapshot.shuffle = crazypod_queue_shuffle() ? 1 : 0;
    snapshot.favorite = have_track &&
        crazypod_music_track_is_favorite(track.path) ? 1 : 0;
    mixer_channel_calculate_peaks(
        PCM_MIXER_CHAN_PLAYBACK, &playback_peaks);
    snapshot.level_left = playback_peaks.left >= 32767u
        ? 1000u : playback_peaks.left * 1000u / 32767u;
    snapshot.level_right = playback_peaks.right >= 32767u
        ? 1000u : playback_peaks.right * 1000u / 32767u;
    copy_text(snapshot.title, sizeof(snapshot.title),
              have_track ? track.title : "");
    copy_text(snapshot.artist, sizeof(snapshot.artist),
              have_track ? track.artist : "");
    copy_text(snapshot.album, sizeof(snapshot.album),
              have_track ? track.album : "");
    response_size = response_capacity < sizeof(snapshot)
        ? response_capacity : sizeof(snapshot);
    memcpy(response, &snapshot, response_size);
    return (int)response_size;
}

static int queue_state_call(
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    struct cp_now_playing_queue_state state;

    if(request != NULL || request_size != 0 || response == NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(response_capacity < sizeof(state))
        return CP_NATIVE_ERROR_LIMIT;
    memset(&state, 0, sizeof(state));
    state.struct_size = sizeof(state);
    state.generation = crazypod_queue_generation();
    state.count = crazypod_queue_count();
    state.current_index = crazypod_queue_index();
    memcpy(response, &state, sizeof(state));
    return sizeof(state);
}

static int queue_item_call(
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    const struct cp_now_playing_queue_item_request *item_request = request;
    struct cp_now_playing_queue_item item;
    struct crazypod_track track;
    char path[MAX_PATH];

    if(item_request == NULL ||
       request_size != sizeof(*item_request) ||
       item_request->struct_size < sizeof(*item_request) ||
       response == NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(response_capacity < sizeof(item))
        return CP_NATIVE_ERROR_LIMIT;
    if(item_request->index < 0 ||
       item_request->index >= crazypod_queue_count())
        return CP_NATIVE_ERROR_ARGUMENT;
    if(!crazypod_queue_copy_path(
           item_request->index, path, sizeof(path)))
        return CP_NATIVE_ERROR_STATE;
    if(!crazypod_music_copy_track(crazypod_music_find_track(path), &track))
        return CP_NATIVE_ERROR_STATE;
    memset(&item, 0, sizeof(item));
    item.struct_size = sizeof(item);
    item.generation = crazypod_queue_generation();
    item.index = item_request->index;
    item.current = item.index == crazypod_queue_index() ? 1 : 0;
    copy_text(item.title, sizeof(item.title), track.title);
    copy_text(item.artist, sizeof(item.artist), track.artist);
    copy_text(item.album, sizeof(item.album), track.album);
    memcpy(response, &item, sizeof(item));
    return sizeof(item);
}

static int lyrics_window_call(
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    const struct cp_now_playing_lyrics_request *lyrics_request = request;
    struct cp_now_playing_lyrics_window window;
    struct crazypod_track track;
    bool have_track = crazypod_miniapp_now_playing_copy_track(&track);
    const char *previous = "";
    const char *current = "";
    const char *next = "";

    if(lyrics_request == NULL ||
       request_size != sizeof(*lyrics_request) ||
       lyrics_request->struct_size < sizeof(*lyrics_request) ||
       response == NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(response_capacity < sizeof(window))
        return CP_NATIVE_ERROR_LIMIT;
    memset(&window, 0, sizeof(window));
    window.struct_size = sizeof(window);
    window.revision = track_revision(have_track ? &track : NULL);
    window.current_line = -1;
    if(have_track && crazypod_lyrics_load(track.path)) {
        window.available = 1;
        window.current_line =
            crazypod_lyrics_current_line(lyrics_request->elapsed_ms);
        crazypod_lyrics_window(
            lyrics_request->elapsed_ms, &previous, &current, &next);
    }
    copy_text(window.previous, sizeof(window.previous), previous);
    copy_text(window.current, sizeof(window.current), current);
    copy_text(window.next, sizeof(window.next), next);
    memcpy(response, &window, sizeof(window));
    return sizeof(window);
}

static int lyrics_context_call(
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    const struct cp_now_playing_lyrics_context_request *lyrics_request =
        request;
    struct cp_now_playing_lyrics_context context;
    struct crazypod_track track;
    bool have_track = crazypod_miniapp_now_playing_copy_track(&track);

    if(lyrics_request == NULL ||
       request_size != sizeof(*lyrics_request) ||
       lyrics_request->struct_size < sizeof(*lyrics_request) ||
       lyrics_request->anchor_line < -1 || response == NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(response_capacity < sizeof(context))
        return CP_NATIVE_ERROR_LIMIT;
    memset(&context, 0, sizeof(context));
    context.struct_size = sizeof(context);
    context.revision = track_revision(have_track ? &track : NULL);
    context.current_line = -1;
    if(have_track)
        (void)crazypod_lyrics_load(track.path);
    context.status = crazypod_lyrics_get_status();
    context.available = crazypod_lyrics_available() ? 1 : 0;
    context.synchronized = crazypod_lyrics_synchronized() ? 1 : 0;
    context.line_count = crazypod_lyrics_display_page_count(
        CP_NOW_PLAYING_LYRIC_TEXT_CAPACITY);
    if(context.line_count > 0) {
        if(lyrics_request->anchor_line >= 0)
            context.current_line = lyrics_request->anchor_line;
        else if(context.synchronized)
            context.current_line = crazypod_lyrics_current_line(
                lyrics_request->elapsed_ms);
        else
            context.current_line = 0;
        if(context.current_line < 0)
            context.current_line = 0;
        if(context.current_line >= context.line_count)
            context.current_line = context.line_count - 1;
    }
    memcpy(response, &context, sizeof(context));
    return sizeof(context);
}

static int lyric_line_call(
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    const struct cp_now_playing_lyric_line_request *line_request = request;
    struct cp_now_playing_lyric_line line;
    struct crazypod_track track;
    bool have_track = crazypod_miniapp_now_playing_copy_track(&track);

    if(line_request == NULL ||
       request_size != sizeof(*line_request) ||
       line_request->struct_size < sizeof(*line_request) ||
       line_request->index < 0 || response == NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(response_capacity < sizeof(line))
        return CP_NATIVE_ERROR_LIMIT;
    if(!have_track || !crazypod_lyrics_load(track.path))
        return CP_NATIVE_ERROR_STATE;
    memset(&line, 0, sizeof(line));
    line.struct_size = sizeof(line);
    line.revision = track_revision(&track);
    line.index = line_request->index;
    if(!crazypod_lyrics_copy_display_page(
           line.index, line.text, sizeof(line.text)))
        return CP_NATIVE_ERROR_ARGUMENT;
    memcpy(response, &line, sizeof(line));
    return sizeof(line);
}

static int seek_to(int64_t target)
{
    const struct mp3entry *id3 = audio_current_track();

    if(id3 == NULL || id3->length <= 0)
        return CP_NATIVE_ERROR_STATE;
    if(target < 0)
        target = 0;
    if((uint64_t)target >= (uint64_t)id3->length)
        target = (int64_t)id3->length - 1;
    audio_pre_ff_rewind();
    audio_ff_rewind((long)target);
    crazypod_state_mark_dirty();
    return CP_NATIVE_OK;
}

static int set_favorite(int value)
{
    struct crazypod_track track;
    bool favorite;

    if(value != 0 && value != 1)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(!crazypod_miniapp_now_playing_copy_track(&track))
        return CP_NATIVE_ERROR_STATE;
    favorite = crazypod_music_track_is_favorite(track.path);
    if(favorite != (value != 0) &&
       !crazypod_music_toggle_favorite(track.path))
        return CP_NATIVE_ERROR_STATE;
    return CP_NATIVE_OK;
}

static int set_playback_mode(int mode)
{
    if(mode < CP_NOW_PLAYING_MODE_NORMAL ||
       mode > CP_NOW_PLAYING_MODE_REPEAT_ONE)
        return CP_NATIVE_ERROR_ARGUMENT;
    crazypod_queue_set_shuffle(false);
    crazypod_queue_set_repeat(REPEAT_OFF);
    if(mode == CP_NOW_PLAYING_MODE_SHUFFLE)
        crazypod_queue_set_shuffle(true);
    else if(mode == CP_NOW_PLAYING_MODE_REPEAT_ALL)
        crazypod_queue_set_repeat(REPEAT_ALL);
    else if(mode == CP_NOW_PLAYING_MODE_REPEAT_ONE)
        crazypod_queue_set_repeat(REPEAT_ONE);
    crazypod_state_mark_dirty();
    return CP_NATIVE_OK;
}

static int command_call(
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    const struct cp_now_playing_command_request *command = request;
    struct crazypod_track track;
    const struct mp3entry *id3;

    if(response != NULL || response_capacity != 0 ||
       command == NULL || request_size != sizeof(*command) ||
       command->struct_size < sizeof(*command))
        return CP_NATIVE_ERROR_ARGUMENT;
    switch(command->command) {
    case CP_NOW_PLAYING_COMMAND_TOGGLE:
        crazypod_playback_toggle();
        break;
    case CP_NOW_PLAYING_COMMAND_PREVIOUS:
        crazypod_playback_previous_or_restart();
        break;
    case CP_NOW_PLAYING_COMMAND_NEXT:
        crazypod_playback_next();
        break;
    case CP_NOW_PLAYING_COMMAND_ADJUST_VOLUME:
        crazypod_now_playing_adjust_volume(command->value);
        break;
    case CP_NOW_PLAYING_COMMAND_SEEK:
        return seek_to(command->value);
    case CP_NOW_PLAYING_COMMAND_TOGGLE_FAVORITE:
        if(!crazypod_miniapp_now_playing_copy_track(&track) ||
           !crazypod_music_toggle_favorite(track.path))
            return CP_NATIVE_ERROR_STATE;
        break;
    case CP_NOW_PLAYING_COMMAND_CYCLE_MODE:
        crazypod_now_playing_overlay_cycle_playback_mode();
        break;
    case CP_NOW_PLAYING_COMMAND_SET_FAVORITE:
        return set_favorite(command->value);
    case CP_NOW_PLAYING_COMMAND_SET_MODE:
        return set_playback_mode(command->value);
    case CP_NOW_PLAYING_COMMAND_SEEK_BY:
        id3 = audio_current_track();
        if(id3 == NULL)
            return CP_NATIVE_ERROR_STATE;
        return seek_to((int64_t)id3->elapsed + command->value);
    case CP_NOW_PLAYING_COMMAND_PLAY_QUEUE_ITEM:
        if(command->value < 0 ||
           command->value >= crazypod_queue_count())
            return CP_NATIVE_ERROR_ARGUMENT;
        playlist_start(command->value, 0, 0);
        crazypod_state_forget_resume();
        crazypod_state_mark_dirty();
        break;
    default:
        return CP_NATIVE_ERROR_UNSUPPORTED;
    }
    return CP_NATIVE_OK;
}

int crazypod_miniapp_now_playing_service_call(
    uint32_t operation,
    const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    if(operation == CP_NATIVE_NOW_PLAYING_SNAPSHOT)
        return snapshot_call(
            request, request_size, response, response_capacity);
    if(operation == CP_NATIVE_NOW_PLAYING_COMMAND)
        return command_call(
            request, request_size, response, response_capacity);
    if(operation == CP_NATIVE_NOW_PLAYING_QUEUE_STATE)
        return queue_state_call(
            request, request_size, response, response_capacity);
    if(operation == CP_NATIVE_NOW_PLAYING_QUEUE_ITEM)
        return queue_item_call(
            request, request_size, response, response_capacity);
    if(operation == CP_NATIVE_NOW_PLAYING_LYRICS_WINDOW)
        return lyrics_window_call(
            request, request_size, response, response_capacity);
    if(operation == CP_NATIVE_NOW_PLAYING_LYRICS_CONTEXT)
        return lyrics_context_call(
            request, request_size, response, response_capacity);
    if(operation == CP_NATIVE_NOW_PLAYING_LYRIC_LINE)
        return lyric_line_call(
            request, request_size, response, response_capacity);
    return CP_NATIVE_ERROR_UNSUPPORTED;
}

#endif
