#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include "kernel.h"

#include "../../../crazypod_audiobooks.h"
#include "../../../crazypod_books.h"
#include "crazypod_book_session.h"
#include "crazypod_book_reader_input.h"
#include "crazypod_books_actions.h"
#include "crazypod_books_confirmation.h"
#include "crazypod_books_preview.h"
#include "crazypod_books_screen.h"
#include "crazypod_books_workflow.h"
#include "button.h"

#include "../../../crazypod_collation.h"
#include "../../presentation/crazypod_ui_text.h"
#include "crazypod_books_feature.h"

static void ensure_audiobooks(void)
{
    if(crazypod_audiobooks_scan_needed())
        crazypod_audiobooks_scan();
}

#define RECENT_ENTRIES_MAX 16

static struct crazypod_books_recent_entry recent_entries[
    RECENT_ENTRIES_MAX];
static int recent_entry_count;

static void offer_recent(bool audiobook, int index, uint32_t sequence)
{
    int slot;

    if(sequence == 0)
        return;
    for(slot = recent_entry_count; slot > 0; --slot) {
        if(recent_entries[slot - 1].sequence >= sequence)
            break;
        if(slot < RECENT_ENTRIES_MAX)
            recent_entries[slot] = recent_entries[slot - 1];
    }
    if(slot >= RECENT_ENTRIES_MAX)
        return;
    recent_entries[slot].audiobook = audiobook;
    recent_entries[slot].index = index;
    recent_entries[slot].sequence = sequence;
    if(recent_entry_count < RECENT_ENTRIES_MAX)
        ++recent_entry_count;
}

/* Text books and audiobooks share one recency counter, so the merged
 * list is just both catalogs sorted by it. Small enough to rebuild on
 * every query. */
static void collect_recents(void)
{
    int i;

    ensure_audiobooks();
    recent_entry_count = 0;
    for(i = 0; i < crazypod_books_count(); ++i)
        offer_recent(false, i, crazypod_books_recent_sequence(i));
    for(i = 0; i < crazypod_audiobooks_count(); ++i)
        offer_recent(true, i, crazypod_audiobook_recent_sequence(i));
}

/*
 * One search over both halves of the library. Text books come first and
 * keep their catalogue order, then audiobooks, so a result's position
 * maps back to a book without a second pass over the query.
 */
static char search_query[64];

/* The same wheel-typed editor the Music and Notes searches use: letters,
 * digits, then space, backspace and the search itself. */
#define SEARCH_ACTION_COUNT 3
#define SEARCH_CHARACTER_COUNT 36

static const char *const search_characters[SEARCH_CHARACTER_COUNT] = {
    CP_TR("A"), CP_TR("B"), CP_TR("C"), CP_TR("D"), CP_TR("E"), CP_TR("F"), CP_TR("G"), CP_TR("H"), CP_TR("I"), CP_TR("J"),
    CP_TR("K"), CP_TR("L"), CP_TR("M"), CP_TR("N"), CP_TR("O"), CP_TR("P"), CP_TR("Q"), CP_TR("R"), CP_TR("S"), CP_TR("T"),
    CP_TR("U"), CP_TR("V"), CP_TR("W"), "X", CP_TR("Y"), CP_TR("Z"),
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

int crazypod_books_feature_search_key_count(void)
{
    return SEARCH_CHARACTER_COUNT + SEARCH_ACTION_COUNT;
}

const char *crazypod_books_feature_search_key_title(int index)
{
    if(index >= 0 && index < SEARCH_CHARACTER_COUNT)
        return search_characters[index];
    if(index == SEARCH_CHARACTER_COUNT)
        return CP_TR("Space");
    if(index == SEARCH_CHARACTER_COUNT + 1)
        return CP_TR("Backspace");
    if(index == SEARCH_CHARACTER_COUNT + 2)
        return CP_TR("Search");
    return "";
}

/* True when the press was a key; false when it asks for the results. */
bool crazypod_books_feature_search_key(int index)
{
    if(index >= 0 && index < SEARCH_CHARACTER_COUNT) {
        crazypod_books_feature_append_query(
            crazypod_l10n_text(search_characters[index]));
        return true;
    }
    if(index == SEARCH_CHARACTER_COUNT) {
        crazypod_books_feature_append_query(" ");
        return true;
    }
    if(index == SEARCH_CHARACTER_COUNT + 1) {
        crazypod_books_feature_backspace_query();
        return true;
    }
    return false;
}

const char *crazypod_books_feature_query(void)
{
    return search_query;
}

void crazypod_books_feature_append_query(const char *text)
{
    crazypod_ui_text_append(
        search_query, sizeof(search_query), text);
}

void crazypod_books_feature_backspace_query(void)
{
    crazypod_ui_text_backspace(search_query);
}

void crazypod_books_feature_clear_query(void)
{
    search_query[0] = '\0';
}

static bool book_matches(int index, const char *query)
{
    const struct crazypod_book *book = crazypod_book_get(index);

    return book != NULL &&
        (crazypod_collation_contains(book->title, query) ||
         crazypod_collation_contains(book->author, query));
}

static bool audiobook_matches(int index, const char *query)
{
    const struct crazypod_audiobook *book =
        crazypod_audiobook_get(index);

    return book != NULL &&
        (crazypod_collation_contains(book->title, query) ||
         crazypod_collation_contains(book->author, query));
}

bool crazypod_books_feature_search_at(
    const char *query, int index,
    struct crazypod_books_recent_entry *entry)
{
    int seen = 0;
    int i;

    if(index < 0 || query == NULL || query[0] == '\0')
        return false;
    for(i = 0; i < crazypod_books_count(); ++i) {
        if(!book_matches(i, query))
            continue;
        if(seen++ == index) {
            entry->audiobook = false;
            entry->index = i;
            entry->sequence = 0;
            return true;
        }
    }
    ensure_audiobooks();
    for(i = 0; i < crazypod_audiobooks_count(); ++i) {
        if(!audiobook_matches(i, query))
            continue;
        if(seen++ == index) {
            entry->audiobook = true;
            entry->index = i;
            entry->sequence = 0;
            return true;
        }
    }
    return false;
}

int crazypod_books_feature_search_count(const char *query)
{
    int count = 0;
    int i;

    if(query == NULL || query[0] == '\0')
        return 0;
    for(i = 0; i < crazypod_books_count(); ++i)
        count += book_matches(i, query) ? 1 : 0;
    ensure_audiobooks();
    for(i = 0; i < crazypod_audiobooks_count(); ++i)
        count += audiobook_matches(i, query) ? 1 : 0;
    return count;
}

const char *crazypod_books_feature_search_title(
    const char *query, int index)
{
    struct crazypod_books_recent_entry entry;

    if(!crazypod_books_feature_search_at(query, index, &entry))
        return "";
    if(entry.audiobook) {
        const struct crazypod_audiobook *book =
            crazypod_audiobook_get(entry.index);

        return book != NULL ? book->title : "";
    }
    {
        const struct crazypod_book *book =
            crazypod_book_get(entry.index);

        return book != NULL ? book->title : "";
    }
}

const char *crazypod_books_feature_search_subtitle(
    const char *query, int index)
{
    struct crazypod_books_recent_entry entry;
    const char *author = "";

    if(!crazypod_books_feature_search_at(query, index, &entry))
        return "";
    if(entry.audiobook) {
        const struct crazypod_audiobook *book =
            crazypod_audiobook_get(entry.index);

        author = book != NULL ? book->author : "";
        return author[0] != '\0' ? author : CP_TR("Audiobook");
    }
    {
        const struct crazypod_book *book =
            crazypod_book_get(entry.index);

        author = book != NULL ? book->author : "";
    }
    return author[0] != '\0' ? author : CP_TR("Book");
}

int crazypod_books_feature_recent_count(void)
{
    collect_recents();
    return recent_entry_count;
}

bool crazypod_books_feature_recent_at(
    int position, struct crazypod_books_recent_entry *entry)
{
    collect_recents();
    if(position < 0 || position >= recent_entry_count)
        return false;
    *entry = recent_entries[position];
    return true;
}

int crazypod_books_feature_favorite_count(void)
{
    ensure_audiobooks();
    return crazypod_books_favorite_count() +
        crazypod_audiobooks_favorite_count();
}

bool crazypod_books_feature_favorite_at(
    int position, struct crazypod_books_recent_entry *entry)
{
    int text_count;

    if(position < 0)
        return false;
    ensure_audiobooks();
    text_count = crazypod_books_favorite_count();
    entry->sequence = 0;
    if(position < text_count) {
        entry->audiobook = false;
        entry->index = crazypod_books_favorite_at(position);
    }
    else {
        entry->audiobook = true;
        entry->index =
            crazypod_audiobooks_favorite_at(position - text_count);
    }
    return entry->index >= 0;
}

bool crazypod_books_feature_continue_entry(
    struct crazypod_books_recent_entry *entry)
{
    struct crazypod_books_recent_entry newest;

    if(!crazypod_books_feature_recent_at(0, &newest))
        return false;
    if(newest.audiobook) {
        const struct crazypod_audiobook *book =
            crazypod_audiobook_get(newest.index);

        if(book == NULL || book->position_ms == 0)
            return false;
    }
    else {
        const struct crazypod_book *book =
            crazypod_book_get(newest.index);

        if(book == NULL || book->progress == 0)
            return false;
    }
    *entry = newest;
    return true;
}

static bool has_continue(void)
{
    struct crazypod_books_recent_entry entry;

    return crazypod_books_feature_continue_entry(&entry);
}

static bool recent_audiobook_at(int position, int *index)
{
    struct crazypod_books_recent_entry entry;

    if(!crazypod_books_feature_recent_at(position, &entry) ||
       !entry.audiobook)
        return false;
    *index = entry.index;
    return true;
}


static const uint32_t page_colors[] = {
    0xE8D5A4, 0xF8F8F4, 0xDDEFE3, 0x17181D
};

static const uint32_t ink_colors[] = {
    0x302A22, 0x252525, 0x24382D, 0xECECF1
};

static struct {
    bool toolbar_visible;
    long toolbar_hide_tick;
} reader_view;

#define READER_TOOLBAR_VISIBLE_TICKS (2 * HZ)

int crazypod_books_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case BOOKS_ROUTE_MENU:
        return has_continue() ? 8 : 7;
    case BOOKS_ROUTE_AUDIOBOOKS:
        ensure_audiobooks();
        return crazypod_audiobooks_count();
    case BOOKS_ROUTE_RECENTS:
        return crazypod_books_feature_recent_count();
    case BOOKS_ROUTE_LIBRARY:
        return crazypod_books_count();
    case BOOKS_ROUTE_FAVORITES:
        return crazypod_books_feature_favorite_count();
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
    case BOOKS_ROUTE_SEARCH:
        return crazypod_books_feature_search_key_count();
    case BOOKS_ROUTE_SEARCH_RESULTS:
        return crazypod_books_feature_search_count(search_query);
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
    case BOOKS_ROUTE_AUDIOBOOKS:
        return CP_TR("AUDIOBOOKS");
    case BOOKS_ROUTE_SEARCH:
        return CP_TR("SEARCH");
    case BOOKS_ROUTE_SEARCH_RESULTS:
        return CP_TR("RESULTS");
    default:
        return "";
    }
}

static int book_index(
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

static bool favorite_audiobook_at(int position, int *index)
{
    struct crazypod_books_recent_entry entry;

    if(!crazypod_books_feature_favorite_at(position, &entry) ||
       !entry.audiobook)
        return false;
    *index = entry.index;
    return true;
}

bool crazypod_books_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    switch(state->route) {
    case BOOKS_ROUTE_MENU: {
        static const char *const titles[] = {
            CP_TR("Recents"), CP_TR("Books"), CP_TR("Audiobooks"),
            CP_TR("Favorites"), CP_TR("Search"), CP_TR("Stats"),
            CP_TR("Reading")
        };
        bool can_continue = has_continue();
        int logical;

        if(can_continue && index == 0)
            *title = CP_TR("Continue");
        else {
            logical = index - (can_continue ? 1 : 0);
            *title = logical >= 0 &&
                logical < (int)(sizeof(titles) / sizeof(titles[0]))
                ? titles[logical] : "";
        }
        return true;
    }
    case BOOKS_ROUTE_AUDIOBOOKS: {
        const struct crazypod_audiobook *book;

        ensure_audiobooks();
        if(index == state->selected)
            crazypod_audiobook_probe(index);
        book = crazypod_audiobook_get(index);
        *title = book != NULL ? book->title : "";
        return true;
    }
    case BOOKS_ROUTE_RECENTS:
    case BOOKS_ROUTE_FAVORITES: {
        int audiobook;
        bool is_audiobook = state->route == BOOKS_ROUTE_RECENTS
            ? recent_audiobook_at(index, &audiobook)
            : favorite_audiobook_at(index, &audiobook);

        if(is_audiobook) {
            const struct crazypod_audiobook *book;

            if(index == state->selected)
                crazypod_audiobook_probe(audiobook);
            book = crazypod_audiobook_get(audiobook);
            *title = book != NULL ? book->title : "";
            return true;
        }
    }
    /* Fall through: a text book in Recents or Favorites. */
    case BOOKS_ROUTE_LIBRARY: {
        int resolved_index = book_index(state, index);
        const struct crazypod_book *book =
            crazypod_book_get(resolved_index);

        if(index == state->selected) {
            crazypod_book_probe(resolved_index);
            book = crazypod_book_get(resolved_index);
        }

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
    case BOOKS_ROUTE_SEARCH:
        *title = crazypod_books_feature_search_key_title(index);
        return true;
    case BOOKS_ROUTE_SEARCH_RESULTS:
        *title = crazypod_books_feature_search_title(
            search_query, index);
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
        CRAZYPOD_MENU_ICON_PODCAST,
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
    case BOOKS_ROUTE_RECENTS: {
        int audiobook;

        return recent_audiobook_at(index, &audiobook)
            ? CRAZYPOD_MENU_ICON_PODCAST : CRAZYPOD_MENU_ICON_BOOK;
    }
    case BOOKS_ROUTE_LIBRARY:
        return CRAZYPOD_MENU_ICON_BOOK;
    case BOOKS_ROUTE_FAVORITES: {
        int audiobook;

        return favorite_audiobook_at(index, &audiobook)
            ? CRAZYPOD_MENU_ICON_PODCAST : CRAZYPOD_MENU_ICON_FAVORITE;
    }
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
    case BOOKS_ROUTE_AUDIOBOOKS:
        return CRAZYPOD_MENU_ICON_PODCAST;
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
    case CRAZYPOD_BOOKS_ACTION_PLAY_AUDIOBOOK:
        /* The book plays through the ordinary player: Now Playing shows
         * it with the chapter on the album line, and Left/Right skip
         * chapters there and on the lock screen. */
        if(crazypod_audiobook_play(action.book_index))
            host->open_now_playing();
        else
            host->operation_failed();
        break;
    case CRAZYPOD_BOOKS_ACTION_NONE:
    case CRAZYPOD_BOOKS_ACTION_UNHANDLED:
    default:
        break;
    }
    return true;
}

bool crazypod_books_feature_render_search(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font, int item_count,
    const char *(*item_title)(
        const struct route_state *state, int index),
    uint32_t primary_color, uint32_t secondary_color,
    uint32_t panel_color, bool gradient_highlight,
    crazypod_search_panel_factory make_panel)
{
    const struct crazypod_search_screen_context context = {
        .parent = parent,
        .query = search_query,
        .item_count = item_count,
        .primary_color = primary_color,
        .secondary_color = secondary_color,
        .panel_color = panel_color,
        .gradient_highlight = gradient_highlight,
        .metadata_font = metadata_font,
        .item_title = item_title,
        .make_panel = make_panel,
        .result_count = crazypod_books_feature_search_count,
        .result_title = crazypod_books_feature_search_title,
        .result_subtitle = crazypod_books_feature_search_subtitle,
        .empty_hint = CP_TR("No book title or author matched."),
    };

    if(state->route != BOOKS_ROUTE_SEARCH)
        return false;
    crazypod_search_screen_render(state, &context);
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
            page_colors[theme], ink_colors[theme],
            reader_view.toolbar_visible);
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

void crazypod_books_feature_enter_reader(long now)
{
    reader_view.toolbar_visible = true;
    reader_view.toolbar_hide_tick =
        now + READER_TOOLBAR_VISIBLE_TICKS;
}

int crazypod_books_feature_reader_wait_ticks(
    const struct route_state *state, long now, int maximum)
{
    long remaining;

    if(state == NULL || state->route != BOOKS_ROUTE_READER ||
       !reader_view.toolbar_visible ||
       reader_view.toolbar_hide_tick == 0)
        return maximum;
    remaining = reader_view.toolbar_hide_tick - now;
    if(remaining <= 0)
        return 1;
    return remaining < maximum ? (int)remaining : maximum;
}

bool crazypod_books_feature_service_reader(
    const struct route_state *state, long now)
{
    if(state == NULL || state->route != BOOKS_ROUTE_READER ||
       !reader_view.toolbar_visible ||
       reader_view.toolbar_hide_tick == 0 ||
       TIME_BEFORE(now, reader_view.toolbar_hide_tick))
        return false;
    reader_view.toolbar_visible = false;
    reader_view.toolbar_hide_tick = 0;
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

static void show_reader_actions(void)
{
    crazypod_books_feature_enter_reader(book_input_context.now);
    book_input_context.render(false);
    book_input_context.activate();
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

/*
 * Chapters and the saved bookmark, reachable from inside the reader.
 * Both were only on the route the reader is opened from, so changing
 * chapter meant leaving the book and coming back to it.
 */
int crazypod_books_feature_reader_chapter_count(void)
{
    return crazypod_book_chapter_count(crazypod_book_session_index());
}

const char *crazypod_books_feature_reader_chapter_title(int chapter)
{
    static char title[64];

    if(!crazypod_book_chapter_get(
           crazypod_book_session_index(), chapter,
           title, sizeof(title), NULL))
        return "";
    return title;
}

int crazypod_books_feature_reader_current_chapter(void)
{
    int count = crazypod_books_feature_reader_chapter_count();
    uint32_t here = crazypod_book_session_offset();
    int current = -1;
    int chapter;

    for(chapter = 0; chapter < count; ++chapter) {
        uint32_t offset;

        if(crazypod_book_chapter_get(
               crazypod_book_session_index(), chapter,
               NULL, 0, &offset) && offset <= here)
            current = chapter;
    }
    return current;
}

bool crazypod_books_feature_reader_go_to_chapter(int chapter)
{
    int index = crazypod_book_session_index();
    uint32_t offset;

    return crazypod_book_chapter_get(index, chapter, NULL, 0, &offset) &&
        crazypod_book_session_load(index, offset);
}

bool crazypod_books_feature_reader_has_bookmark(void)
{
    const struct crazypod_book *book =
        crazypod_book_get(crazypod_book_session_index());

    return book != NULL && book->bookmark != CRAZYPOD_BOOKMARK_NONE;
}

const char *crazypod_books_feature_reader_bookmark_label(void)
{
    static char label[64];
    const struct crazypod_book *book =
        crazypod_book_get(crazypod_book_session_index());
    unsigned percent;

    if(book == NULL || book->bookmark == CRAZYPOD_BOOKMARK_NONE)
        return CP_TR("No bookmark saved");
    percent = book->content_size > 0
        ? (unsigned)((uint64_t)book->bookmark * 100u / book->content_size)
        : 0;
    snprintf(label, sizeof(label), CP_FMT("Saved position  ·  %u%%"),
             percent > 100u ? 100u : percent);
    return label;
}

bool crazypod_books_feature_reader_go_to_bookmark(void)
{
    int index = crazypod_book_session_index();
    const struct crazypod_book *book = crazypod_book_get(index);

    return book != NULL && book->bookmark != CRAZYPOD_BOOKMARK_NONE &&
        crazypod_book_session_load(index, book->bookmark);
}

bool crazypod_books_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    const struct crazypod_book_reader_input_actions actions = {
        .turn_page = crazypod_book_session_turn,
        .refresh = refresh_reader,
        .show_actions = show_reader_actions,
        .toggle_bookmark = toggle_bookmark,
        .leave = context->pop,
    };

    /*
     * The search editor is a twelve-column grid, so a wheel step has to
     * move a row rather than a key, and MENU has to rub out a character
     * before it leaves. The default menu handling does neither.
     */
    if(state->route == BOOKS_ROUTE_SEARCH) {
        if(event->base == BUTTON_SCROLL_FWD)
            context->move(crazypod_input_wheel_steps(event, 12));
        else if(event->base == BUTTON_SCROLL_BACK)
            context->move(-crazypod_input_wheel_steps(event, 12));
        else if(event->base == BUTTON_RIGHT)
            context->move(1);
        else if(event->base == BUTTON_LEFT)
            context->move(-1);
        else if(event->base == BUTTON_SELECT && !event->repeated)
            context->activate();
        else if(event->base == BUTTON_MENU && !event->repeated) {
            if(search_query[0] != '\0') {
                crazypod_books_feature_backspace_query();
                context->render(false);
            }
            else
                context->pop();
        }
        else if(event->base == BUTTON_PLAY && !event->repeated &&
                search_query[0] != '\0')
            context->push(BOOKS_ROUTE_SEARCH_RESULTS, -1);
        return true;
    }
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
