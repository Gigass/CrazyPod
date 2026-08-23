#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "../../../crazypod_books.h"
#include "crazypod_book_session.h"
#include "crazypod_book_reader_input.h"
#include "crazypod_books_actions.h"
#include "crazypod_books_confirmation.h"
#include "crazypod_books_preview.h"
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

        return book != NULL &&
               book->bookmark != CRAZYPOD_BOOKMARK_NONE ? 1 : 0;
    }
    case BOOKS_ROUTE_READING_SETTINGS:
        return 2;
    default:
        return 0;
    }
}

const char *crazypod_books_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case BOOKS_ROUTE_MENU:
        return CP_TR("BOOKS");
    case BOOKS_ROUTE_RECENTS:
        return CP_TR("RECENTS");
    case BOOKS_ROUTE_LIBRARY:
        return CP_TR("BOOKS");
    case BOOKS_ROUTE_FAVORITES:
        return CP_TR("FAVORITES");
    case BOOKS_ROUTE_READER: {
        const struct crazypod_book *book =
            crazypod_book_get(state->group);

        return book != NULL ? book->title : CP_TR("BOOK");
    }
    case BOOKS_ROUTE_ACTIONS:
        return CP_TR("BOOK ACTIONS");
    case BOOKS_ROUTE_CHAPTERS:
        return CP_TR("CHAPTERS");
    case BOOKS_ROUTE_BOOKMARKS:
        return CP_TR("BOOKMARKS");
    case BOOKS_ROUTE_DELETE_CONFIRM:
        return CP_TR("DELETE BOOK");
    case BOOKS_ROUTE_STATS:
        return CP_TR("READING STATS");
    case BOOKS_ROUTE_READING_SETTINGS:
        return CP_TR("READING");
    case BOOKS_ROUTE_INFO:
        return CP_TR("BOOK INFO");
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
            CP_TR("Recents"), CP_TR("Books"), CP_TR("Favorites"), CP_TR("Stats"), CP_TR("Reading")
        };
        bool can_continue = has_continue();
        int logical;

        if(can_continue && index == 0)
            *title = CP_TR("Continue");
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

        *title = index == 0 ? CP_TR("Read") :
            index == 1 ? CP_TR("Bookmarks") :
            index == 2 ? CP_TR("Chapters") :
            index == 3
                ? (book != NULL && book->favorite
                    ? CP_TR("Remove Favorite") : CP_TR("Favorite")) :
            index == 4 ? CP_TR("Info") :
            index == 5 ? CP_TR("Delete") : "";
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
        *title = CP_TR("Saved Page");
        return true;
    case BOOKS_ROUTE_DELETE_CONFIRM:
        *title = CP_TR("Hold Center to Delete");
        return true;
    case BOOKS_ROUTE_READING_SETTINGS:
        if(index == 0) {
            static const char *const sizes[] = {
                CP_TR("Text Size: Small"), CP_TR("Text Size: Medium"),
                CP_TR("Text Size: Large")
            };

            *title = sizes[crazypod_books_font_size()];
        }
        else if(index == 1) {
            static const char *const themes[] = {
                CP_TR("Page: Parchment"), CP_TR("Page: Light"),
                CP_TR("Page: Mint"), CP_TR("Page: Dark")
            };

            *title = themes[crazypod_books_theme()];
        }
        else
            *title = "";
        return true;
    case BOOKS_ROUTE_STATS:
        *title = CP_TR("Library Summary");
        return true;
    case BOOKS_ROUTE_INFO:
        *title = CP_TR("Book Details");
        return true;
    case BOOKS_ROUTE_READER:
        *title = CP_TR("Reader");
        return true;
    default:
        return false;
    }
}

enum crazypod_menu_icon crazypod_books_feature_item_icon(
    const struct route_state *state, int index)
{
    static const enum crazypod_menu_icon root_icons[] = {
        CRAZYPOD_MENU_ICON_RECENTS,
        CRAZYPOD_MENU_ICON_BOOK,
        CRAZYPOD_MENU_ICON_FAVORITE,
        CRAZYPOD_MENU_ICON_STATS,
        CRAZYPOD_MENU_ICON_READING,
    };
    int logical;

    if(index < 0)
        return CRAZYPOD_MENU_ICON_NONE;
    switch(state->route) {
    case BOOKS_ROUTE_MENU:
        if(has_continue() && index == 0)
            return CRAZYPOD_MENU_ICON_READING;
        logical = index - (has_continue() ? 1 : 0);
        return logical >= 0 &&
            logical < (int)(sizeof(root_icons) / sizeof(root_icons[0]))
                ? root_icons[logical] : CRAZYPOD_MENU_ICON_NONE;
    case BOOKS_ROUTE_RECENTS:
    case BOOKS_ROUTE_LIBRARY:
        return CRAZYPOD_MENU_ICON_BOOK;
    case BOOKS_ROUTE_FAVORITES:
        return CRAZYPOD_MENU_ICON_FAVORITE;
    case BOOKS_ROUTE_ACTIONS:
        return index == 0 ? CRAZYPOD_MENU_ICON_READING :
            index == 1 ? CRAZYPOD_MENU_ICON_BOOKMARK :
            index == 2 ? CRAZYPOD_MENU_ICON_CHAPTERS :
            index == 3 ? CRAZYPOD_MENU_ICON_FAVORITE :
            index == 4 ? CRAZYPOD_MENU_ICON_DETAILS :
            index == 5 ? CRAZYPOD_MENU_ICON_TRASH :
            CRAZYPOD_MENU_ICON_NONE;
    case BOOKS_ROUTE_CHAPTERS:
        return CRAZYPOD_MENU_ICON_CHAPTERS;
    case BOOKS_ROUTE_BOOKMARKS:
        return CRAZYPOD_MENU_ICON_BOOKMARK;
    case BOOKS_ROUTE_READING_SETTINGS:
        return index == 0 ? CRAZYPOD_MENU_ICON_TEXT_SIZE :
            index == 1 ? CRAZYPOD_MENU_ICON_PAGE_THEME :
            CRAZYPOD_MENU_ICON_NONE;
    case BOOKS_ROUTE_DELETE_CONFIRM:
        return CRAZYPOD_MENU_ICON_TRASH;
    case BOOKS_ROUTE_STATS:
        return CRAZYPOD_MENU_ICON_STATS;
    case BOOKS_ROUTE_INFO:
        return CRAZYPOD_MENU_ICON_DETAILS;
    case BOOKS_ROUTE_READER:
        return CRAZYPOD_MENU_ICON_READING;
    default:
        return CRAZYPOD_MENU_ICON_NONE;
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
    case CRAZYPOD_BOOKS_ACTION_FAILED:
        host->operation_failed();
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

static struct crazypod_feature_input_context book_input_context;

static void refresh_reader(void)
{
    book_input_context.render(false);
}

static void toggle_bookmark(void)
{
    (void)crazypod_books_feature_toggle_reader_bookmark();
    book_input_context.render(false);
}

bool crazypod_books_feature_reader_page_bookmarked(void)
{
    int index = crazypod_book_session_index();
    const struct crazypod_book *book = crazypod_book_get(index);

    return book != NULL &&
        book->bookmark != CRAZYPOD_BOOKMARK_NONE &&
        book->bookmark == crazypod_book_session_offset();
}

bool crazypod_books_feature_toggle_reader_bookmark(void)
{
    return crazypod_book_toggle_bookmark(
        crazypod_book_session_index(),
        crazypod_book_session_offset());
}

bool crazypod_books_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    const struct crazypod_book_reader_input_actions actions = {
        .turn_page = crazypod_book_session_turn,
        .refresh = refresh_reader,
        .show_actions = context->activate,
        .toggle_bookmark = toggle_bookmark,
        .leave = context->pop,
    };

    if(state->route != BOOKS_ROUTE_READER)
        return false;
    book_input_context = *context;
    crazypod_book_reader_input_handle(event, &actions);
    return true;
}

void crazypod_books_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font)
{
    crazypod_books_preview_render(
        parent, state, metadata_font);
}

void crazypod_books_feature_configure_runtime(
    const struct crazypod_books_runtime_host *host)
{
    const struct crazypod_books_workflow_host internal = {
        .parent = host->parent,
        .metadata_font = host->metadata_font,
        .page_colors = host->page_colors,
        .ink_colors = host->ink_colors,
        .set_status_palette = host->set_status_palette,
        .status_foreground = host->status_foreground,
        .present = host->present,
        .render_route = host->render_route,
        .push_reader = host->push_reader,
    };

    crazypod_books_workflow_configure(&internal);
}

void crazypod_books_feature_ensure_metadata(void)
{
    crazypod_books_workflow_ensure_metadata();
}

void crazypod_books_feature_invalidate_metadata(void)
{
    crazypod_books_workflow_invalidate_metadata();
}

void crazypod_books_feature_apply_font_size(int value)
{
    crazypod_books_workflow_apply_font_size(value);
}

void crazypod_books_feature_begin_reader(
    int index, uint32_t offset)
{
    crazypod_books_workflow_begin_reader(index, offset);
}

void crazypod_books_feature_turn_page(int direction)
{
    crazypod_book_session_turn(direction);
}

struct crazypod_books_confirmation_result
crazypod_books_feature_confirm(
    const struct route_state *state)
{
    return crazypod_books_confirmation_execute(state);
}

#endif
