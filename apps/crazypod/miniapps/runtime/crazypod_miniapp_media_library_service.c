#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "string-extra.h"

#include "../../crazypod_music.h"
#include "../../../../miniapps/sdk/crazypod_miniapp_native.h"
#include "crazypod_miniapp_media_library_service.h"

static int item_total(
    const struct cp_media_library_page_request *request)
{
    switch(request->kind) {
    case CP_MEDIA_LIBRARY_TRACKS:
        return crazypod_music_track_count();
    case CP_MEDIA_LIBRARY_ARTISTS:
        return crazypod_music_artist_count();
    case CP_MEDIA_LIBRARY_ALBUMS:
        return crazypod_music_album_count();
    case CP_MEDIA_LIBRARY_PLAYLISTS:
        return crazypod_music_playlist_count();
    case CP_MEDIA_LIBRARY_ARTIST_TRACKS:
        return crazypod_music_artist_track_count(request->group_index);
    case CP_MEDIA_LIBRARY_ALBUM_TRACKS:
        return crazypod_music_album_track_count(request->group_index);
    case CP_MEDIA_LIBRARY_PLAYLIST_TRACKS: {
        struct crazypod_playlist playlist;
        return crazypod_music_copy_playlist(
                request->group_index, &playlist)
            ? (int)playlist.track_count : -1;
    }
    case CP_MEDIA_LIBRARY_SEARCH_TRACKS:
        return request->query[0] != '\0'
            ? crazypod_music_search_count(request->query) : 0;
    default:
        return -1;
    }
}

static bool copy_track_at(
    const struct cp_media_library_page_request *request, int index,
    struct crazypod_track *track)
{
    switch(request->kind) {
    case CP_MEDIA_LIBRARY_TRACKS:
        return crazypod_music_copy_track(index, track);
    case CP_MEDIA_LIBRARY_ARTIST_TRACKS:
        return crazypod_music_copy_artist_track(
            request->group_index, index, track);
    case CP_MEDIA_LIBRARY_ALBUM_TRACKS:
        return crazypod_music_copy_album_track(
            request->group_index, index, track);
    case CP_MEDIA_LIBRARY_PLAYLIST_TRACKS:
        return crazypod_music_copy_playlist_track(
            request->group_index, index, track);
    case CP_MEDIA_LIBRARY_SEARCH_TRACKS:
        return crazypod_music_copy_search_track(
            request->query, index, track);
    default:
        return false;
    }
}

static bool fill_group_item(
    uint32_t kind, int index, struct cp_media_library_item *item)
{
    if(kind == CP_MEDIA_LIBRARY_ARTISTS) {
        char artist[72];
        if(!crazypod_music_copy_artist(index, artist, sizeof(artist)))
            return false;
        strlcpy(item->primary, artist, sizeof(item->primary));
        item->value = crazypod_music_artist_track_count(index);
        return item->value >= 0;
    }
    if(kind == CP_MEDIA_LIBRARY_ALBUMS) {
        struct crazypod_album album;
        if(!crazypod_music_copy_album(index, &album))
            return false;
        strlcpy(item->primary, album.title, sizeof(item->primary));
        strlcpy(item->secondary, album.artist, sizeof(item->secondary));
        item->value = (int32_t)album.track_count;
        return true;
    }
    if(kind == CP_MEDIA_LIBRARY_PLAYLISTS) {
        struct crazypod_playlist playlist;
        if(!crazypod_music_copy_playlist(index, &playlist))
            return false;
        strlcpy(item->primary, playlist.name, sizeof(item->primary));
        item->value = (int32_t)playlist.track_count;
        return true;
    }
    return false;
}

static bool fill_item(
    const struct cp_media_library_page_request *request,
    int index, struct cp_media_library_item *item)
{
    struct crazypod_track track;

    memset(item, 0, sizeof(*item));
    item->struct_size = sizeof(*item);
    item->index = index;
    if(request->kind == CP_MEDIA_LIBRARY_ARTISTS ||
       request->kind == CP_MEDIA_LIBRARY_ALBUMS ||
       request->kind == CP_MEDIA_LIBRARY_PLAYLISTS)
        return fill_group_item(request->kind, index, item);
    if(!copy_track_at(request, index, &track))
        return false;
    item->index = crazypod_music_find_track(track.path);
    item->value = (int32_t)track.duration_ms;
    item->auxiliary = ((int32_t)track.disc_number << 16) |
        track.track_number;
    strlcpy(item->primary, track.title, sizeof(item->primary));
    strlcpy(item->secondary, track.artist, sizeof(item->secondary));
    strlcpy(item->tertiary, track.album, sizeof(item->tertiary));
    return item->index >= 0;
}

int crazypod_miniapp_media_library_service_call(
    uint32_t operation, const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    const struct cp_media_library_page_request *page_request = request;
    struct cp_media_library_page page;
    int total;
    uint32_t index;

    if(operation != CP_NATIVE_MEDIA_LIBRARY_PAGE)
        return CP_NATIVE_ERROR_UNSUPPORTED;
    if(request_size != sizeof(*page_request) || page_request == NULL ||
       response == NULL || response_capacity < sizeof(page) ||
       page_request->struct_size != sizeof(*page_request) ||
       page_request->limit == 0 ||
       page_request->limit > CP_MEDIA_LIBRARY_PAGE_CAPACITY ||
       memchr(page_request->query, '\0', sizeof(page_request->query)) == NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    if(!crazypod_music_catalog_ready())
        return CP_NATIVE_ERROR_STATE;
    total = item_total(page_request);
    if(total < 0 || page_request->offset > (uint32_t)total)
        return CP_NATIVE_ERROR_ARGUMENT;
    memset(&page, 0, sizeof(page));
    page.struct_size = sizeof(page);
    page.generation = crazypod_music_scan_generation();
    page.total = (uint32_t)total;
    page.offset = page_request->offset;
    for(index = 0; index < page_request->limit &&
        page_request->offset + index < (uint32_t)total; ++index) {
        if(!fill_item(page_request,
                      (int)(page_request->offset + index),
                      &page.items[index]))
            return CP_NATIVE_ERROR_STATE;
        ++page.count;
    }
    memcpy(response, &page, sizeof(page));
    return (int)sizeof(page);
}

#endif
