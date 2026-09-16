#include "config.h"

#ifdef HAVE_CRAZYPOD_UI

#include "../../../crazypod_audiobooks.h"
#include "../../../crazypod_books.h"
#include "crazypod_books_actions.h"
#include "crazypod_books_feature.h"

static struct crazypod_books_action action(
    enum crazypod_books_action_kind kind)
{
    const struct crazypod_books_action result = {
        .kind = kind,
        .book_index = -1,
    };
    return result;
}

static struct crazypod_books_action push(
    enum crazypod_route route, int group)
{
    struct crazypod_books_action result =
        action(CRAZYPOD_BOOKS_ACTION_PUSH);

    result.route = route;
    result.group = group;
    return result;
}

static struct crazypod_books_action begin_reader(
    int book_index, uint32_t offset)
{
    struct crazypod_books_action result =
        action(CRAZYPOD_BOOKS_ACTION_BEGIN_READER);

    result.book_index = book_index;
    result.offset = offset;
    return result;
}

static bool has_continue(void)
{
    struct crazypod_books_recent_entry entry;

    return crazypod_books_feature_continue_entry(&entry);
}

static struct crazypod_books_action play_audiobook(int index)
{
    struct crazypod_books_action result =
        action(CRAZYPOD_BOOKS_ACTION_PLAY_AUDIOBOOK);

    result.book_index = index;
    return result;
}

static int route_book_index(
    const struct route_state *state, int position)
{
    if(state->route == BOOKS_ROUTE_LIBRARY)
        return position;
    if(state->route == BOOKS_ROUTE_RECENTS) {
        struct crazypod_books_recent_entry entry;

        return crazypod_books_feature_recent_at(position, &entry) &&
               !entry.audiobook ? entry.index : -1;
    }
    if(state->route == BOOKS_ROUTE_FAVORITES) {
        struct crazypod_books_recent_entry entry;

        return crazypod_books_feature_favorite_at(position, &entry) &&
               !entry.audiobook ? entry.index : -1;
    }
    return state->group;
}

struct crazypod_books_action crazypod_books_actions_activate(
    const struct route_state *state)
{
    switch(state->route) {
    case BOOKS_ROUTE_MENU: {
        int logical = state->selected;

        if(has_continue() && state->selected == 0) {
            struct crazypod_books_recent_entry entry;
            const struct crazypod_book *book;

            if(!crazypod_books_feature_continue_entry(&entry))
                return action(CRAZYPOD_BOOKS_ACTION_NONE);
            if(entry.audiobook)
                return play_audiobook(entry.index);
            book = crazypod_book_get(entry.index);
            return book != NULL
                ? begin_reader(entry.index, book->progress)
                : action(CRAZYPOD_BOOKS_ACTION_NONE);
        }
        logical -= has_continue() ? 1 : 0;
        if(logical == 0)
            return push(BOOKS_ROUTE_RECENTS, -1);
        if(logical == 1)
            return push(BOOKS_ROUTE_LIBRARY, -1);
        if(logical == 2)
            return push(BOOKS_ROUTE_AUDIOBOOKS, -1);
        if(logical == 3)
            return push(BOOKS_ROUTE_FAVORITES, -1);
        if(logical == 4) {
            crazypod_books_feature_clear_query();
            return push(BOOKS_ROUTE_SEARCH, -1);
        }
        if(logical == 5)
            return push(BOOKS_ROUTE_STATS, -1);
        if(logical == 6)
            return push(BOOKS_ROUTE_READING_SETTINGS, -1);
        return action(CRAZYPOD_BOOKS_ACTION_NONE);
    }
    case BOOKS_ROUTE_AUDIOBOOKS:
        return crazypod_audiobook_get(state->selected) != NULL
            ? play_audiobook(state->selected)
            : action(CRAZYPOD_BOOKS_ACTION_NONE);
    case BOOKS_ROUTE_RECENTS:
    case BOOKS_ROUTE_FAVORITES: {
        struct crazypod_books_recent_entry entry;
        bool found = state->route == BOOKS_ROUTE_RECENTS
            ? crazypod_books_feature_recent_at(state->selected, &entry)
            : crazypod_books_feature_favorite_at(
                  state->selected, &entry);

        if(found && entry.audiobook)
            return play_audiobook(entry.index);
    }
    /* Fall through: a text book in Recents or Favorites. */
    case BOOKS_ROUTE_LIBRARY: {
        int index = route_book_index(state, state->selected);

        return index >= 0
            ? push(BOOKS_ROUTE_ACTIONS, index)
            : action(CRAZYPOD_BOOKS_ACTION_NONE);
    }
    case BOOKS_ROUTE_READER:
        return push(BOOKS_ROUTE_ACTIONS, state->group);
    case BOOKS_ROUTE_ACTIONS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);

        if(book == NULL)
            return action(CRAZYPOD_BOOKS_ACTION_POP);
        if(state->selected == 0)
            return begin_reader(state->group, book->progress);
        if(state->selected == 1)
            return push(BOOKS_ROUTE_BOOKMARKS, state->group);
        if(state->selected == 2)
            return push(BOOKS_ROUTE_CHAPTERS, state->group);
        if(state->selected == 3) {
            return action(crazypod_book_toggle_favorite(
                    state->group)
                ? CRAZYPOD_BOOKS_ACTION_RENDER
                : CRAZYPOD_BOOKS_ACTION_FAILED);
        }
        if(state->selected == 4)
            return push(BOOKS_ROUTE_INFO, state->group);
        return push(BOOKS_ROUTE_DELETE_CONFIRM, state->group);
    }
    case BOOKS_ROUTE_CHAPTERS: {
        uint32_t offset;

        return crazypod_book_chapter_get(
                   state->group, state->selected, NULL, 0, &offset)
            ? begin_reader(state->group, offset)
            : action(CRAZYPOD_BOOKS_ACTION_NONE);
    }
    case BOOKS_ROUTE_BOOKMARKS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);

        return book != NULL &&
               book->bookmark != CRAZYPOD_BOOKMARK_NONE
            ? begin_reader(state->group, book->bookmark)
            : action(CRAZYPOD_BOOKS_ACTION_NONE);
    }
    case BOOKS_ROUTE_SEARCH:
        if(crazypod_books_feature_search_key(state->selected))
            return action(CRAZYPOD_BOOKS_ACTION_RENDER);
        return crazypod_books_feature_query()[0] != '\0'
            ? push(BOOKS_ROUTE_SEARCH_RESULTS, -1)
            : action(CRAZYPOD_BOOKS_ACTION_NONE);
    case BOOKS_ROUTE_SEARCH_RESULTS: {
        struct crazypod_books_recent_entry entry;

        if(!crazypod_books_feature_search_at(
               crazypod_books_feature_query(), state->selected, &entry))
            return action(CRAZYPOD_BOOKS_ACTION_NONE);
        if(entry.audiobook)
            return play_audiobook(entry.index);
        return push(BOOKS_ROUTE_ACTIONS, entry.index);
    }
    case BOOKS_ROUTE_READING_SETTINGS:
        if(state->selected == 0)
            return action(CRAZYPOD_BOOKS_ACTION_SHOW_FONT_SIZE);
        if(state->selected == 1)
            return action(CRAZYPOD_BOOKS_ACTION_SHOW_THEME);
        return action(CRAZYPOD_BOOKS_ACTION_NONE);
    case BOOKS_ROUTE_STATS:
    case BOOKS_ROUTE_INFO:
    case BOOKS_ROUTE_DELETE_CONFIRM:
        return action(CRAZYPOD_BOOKS_ACTION_NONE);
    default:
        return action(CRAZYPOD_BOOKS_ACTION_UNHANDLED);
    }
}

#endif
