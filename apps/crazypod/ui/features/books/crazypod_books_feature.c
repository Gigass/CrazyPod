#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_books.h"
#include "crazypod_book_session.h"
#include "crazypod_books_actions.h"
#include "crazypod_books_screen.h"
#include "crazypod_books_workflow.h"
#include "crazypod_books_feature.h"

static bool has_continue(void)
{
    int index = crazypod_books_recent_index();
    const struct crazypod_book *book = crazypod_book_get(index);

    return book != NULL && book->progress > 0;
}

static const uint32_t page_colors[] = {
    0xE8D5A4, 0xF8F8F4, 0xDDEFE3, 0x17181D
};

static const uint32_t ink_colors[] = {
    0x302A22, 0x252525, 0x24382D, 0xECECF1
};

int crazypod_books_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case BOOKS_ROUTE_MENU:
        return has_continue() ? 6 : 5;
    case BOOKS_ROUTE_RECENTS:
        return crazypod_books_recent_count();
    case BOOKS_ROUTE_LIBRARY:
        return crazypod_books_count();
    case BOOKS_ROUTE_FAVORITES:
        return crazypod_books_favorite_count();
    case BOOKS_ROUTE_READER:
    case BOOKS_ROUTE_STATS:
    case BOOKS_ROUTE_INFO:
    case BOOKS_ROUTE_DELETE_CONFIRM:
        return 1;
    case BOOKS_ROUTE_ACTIONS:
        return 6;
    case BOOKS_ROUTE_CHAPTERS:
        return crazypod_book_chapter_count(state->group);
    case BOOKS_ROUTE_BOOKMARKS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);

        return book != NULL && book->bookmark > 0 ? 1 : 0;
    }
    case BOOKS_ROUTE_READING_SETTINGS:
        return 3;
    default:
        return 0;
    }
}

const char *crazypod_books_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case BOOKS_ROUTE_MENU:
        return "BOOKS";
    case BOOKS_ROUTE_RECENTS:
        return "RECENTS";
    case BOOKS_ROUTE_LIBRARY:
        return "BOOKS";
    case BOOKS_ROUTE_FAVORITES:
        return "FAVORITES";
    case BOOKS_ROUTE_READER: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);

        return book != NULL ? book->title : "BOOK";
    }
    case BOOKS_ROUTE_ACTIONS:
        return "BOOK ACTIONS";
    case BOOKS_ROUTE_CHAPTERS:
        return "CHAPTERS";
    case BOOKS_ROUTE_BOOKMARKS:
        return "BOOKMARKS";
    case BOOKS_ROUTE_DELETE_CONFIRM:
        return "DELETE BOOK";
    case BOOKS_ROUTE_STATS:
        return "READING STATS";
    case BOOKS_ROUTE_READING_SETTINGS:
        return "READING";
    case BOOKS_ROUTE_INFO:
        return "BOOK INFO";
    default:
        return "";
    }
}

static int book_index(
    const struct route_state *state, int position)
{
    if(state->route == BOOKS_ROUTE_LIBRARY)
        return position;
    if(state->route == BOOKS_ROUTE_RECENTS)
        return crazypod_books_recent_at(position);
    if(state->route == BOOKS_ROUTE_FAVORITES)
        return crazypod_books_favorite_at(position);
    return state->group;
}

bool crazypod_books_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    switch(state->route) {
    case BOOKS_ROUTE_MENU: {
        static const char *const titles[] = {
            "Recents", "Books", "Favorites", "Stats", "Reading"
        };
        bool can_continue = has_continue();
        int logical;

        if(can_continue && index == 0)
            *title = "Continue";
        else {
            logical = index - (can_continue ? 1 : 0);
            *title = logical >= 0 && logical < 5
                ? titles[logical] : "";
        }
        return true;
    }
    case BOOKS_ROUTE_RECENTS:
    case BOOKS_ROUTE_LIBRARY:
    case BOOKS_ROUTE_FAVORITES: {
        const struct crazypod_book *book =
            crazypod_book_get(book_index(state, index));

        *title = book != NULL ? book->title : "";
        return true;
    }
    case BOOKS_ROUTE_ACTIONS: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);

        *title = index == 0 ? "Read" :
            index == 1 ? "Bookmarks" :
            index == 2 ? "Chapters" :
            index == 3
                ? (book != NULL && book->favorite
                    ? "Remove Favorite" : "Favorite") :
            index == 4 ? "Info" :
            index == 5 ? "Delete" : "";
        return true;
    }
    case BOOKS_ROUTE_CHAPTERS: {
        static char chapter_title[96];
        uint32_t offset;

        *title = crazypod_book_chapter_get(
            state->group, index, chapter_title,
            sizeof(chapter_title), &offset)
                ? chapter_title : "";
        return true;
    }
    case BOOKS_ROUTE_BOOKMARKS:
        *title = "Saved Page";
        return true;
    case BOOKS_ROUTE_DELETE_CONFIRM:
        *title = "Hold Center to Delete";
        return true;
    case BOOKS_ROUTE_READING_SETTINGS:
        if(index == 0) {
            static const char *const sizes[] = {
                "Text Size: Small", "Text Size: Medium",
                "Text Size: Large"
            };

            *title = sizes[crazypod_books_font_size()];
        }
        else if(index == 1) {
            static const char *const themes[] = {
                "Page: Parchment", "Page: Light",
                "Page: Mint", "Page: Dark"
            };

            *title = themes[crazypod_books_theme()];
        }
        else
            *title = index == 2 ? "Import / Rescan" : "";
        return true;
    case BOOKS_ROUTE_STATS:
        *title = "Library Summary";
        return true;
    case BOOKS_ROUTE_INFO:
        *title = "Book Details";
        return true;
    case BOOKS_ROUTE_READER:
        *title = "Reader";
        return true;
    default:
        return false;
    }
}

bool crazypod_books_feature_activate(
    const struct route_state *state,
    const struct crazypod_books_activation_host *host)
{
    const struct crazypod_books_action action =
        crazypod_books_actions_activate(state);

    if(action.kind == CRAZYPOD_BOOKS_ACTION_UNHANDLED)
        return false;
    switch(action.kind) {
    case CRAZYPOD_BOOKS_ACTION_RENDER:
        host->render(false);
        break;
    case CRAZYPOD_BOOKS_ACTION_PUSH:
        host->push(action.route, action.group);
        break;
    case CRAZYPOD_BOOKS_ACTION_POP:
        host->pop();
        break;
    case CRAZYPOD_BOOKS_ACTION_BEGIN_READER:
        crazypod_books_workflow_begin_reader(
            action.book_index, action.offset);
        break;
    case CRAZYPOD_BOOKS_ACTION_SHOW_FONT_SIZE:
        host->show_font_size(crazypod_books_font_size());
        break;
    case CRAZYPOD_BOOKS_ACTION_SHOW_THEME:
        host->show_theme(crazypod_books_theme());
        break;
    case CRAZYPOD_BOOKS_ACTION_RESCAN:
        crazypod_books_workflow_rescan();
        break;
    case CRAZYPOD_BOOKS_ACTION_NONE:
    case CRAZYPOD_BOOKS_ACTION_UNHANDLED:
    default:
        break;
    }
    return true;
}

bool crazypod_books_feature_render(
    const struct route_state *state, lv_obj_t *parent)
{
    if(state->route == BOOKS_ROUTE_READER) {
        int theme = crazypod_books_theme();

        crazypod_books_screen_render_reader(
            parent, state->group,
            crazypod_book_session_offset(),
            crazypod_book_session_text(),
            page_colors[theme], ink_colors[theme]);
        return true;
    }
    if(state->route == BOOKS_ROUTE_STATS) {
        crazypod_books_screen_render_stats(parent);
        return true;
    }
    if(state->route != BOOKS_ROUTE_INFO)
        return false;
    crazypod_books_screen_render_info(parent, state->group);
    return true;
}

const uint32_t *crazypod_books_feature_page_colors(void)
{
    return page_colors;
}

const uint32_t *crazypod_books_feature_ink_colors(void)
{
    return ink_colors;
}

void crazypod_books_feature_reset_view(void)
{
    crazypod_books_workflow_reset_view();
}

#endif
