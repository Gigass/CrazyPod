#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_books.h"
#include "../../../crazypod_state.h"
#include "crazypod_book_session.h"

static int selected_index = -1;
static uint32_t page_offset;
static uint32_t next_offset;
static uint32_t history[64];
static int history_count;
static char page_text[2048];

void crazypod_book_session_reset(void)
{
    selected_index = -1;
    page_offset = 0;
    next_offset = 0;
    history_count = 0;
    page_text[0] = '\0';
}

void crazypod_book_session_begin(int book_index)
{
    selected_index = book_index;
    history_count = 0;
}

bool crazypod_book_session_load(int book_index, uint32_t offset)
{
    page_text[0] = '\0';
    next_offset = offset;
    if(!crazypod_book_read_page(
           book_index, offset, page_text, sizeof(page_text),
           &next_offset))
        return false;
    selected_index = book_index;
    page_offset = offset;
    return true;
}

void crazypod_book_session_set_error(const char *message)
{
    snprintf(page_text, sizeof(page_text), "%s",
             message != NULL ? message : "");
}

void crazypod_book_session_turn(int direction)
{
    const struct crazypod_book *book =
        crazypod_book_get(selected_index);
    uint32_t target;

    if(book == NULL)
        return;
    if(direction > 0) {
        if(next_offset <= page_offset ||
           (book->content_size > 0 &&
            next_offset >= book->content_size))
            return;
        if(history_count <
           (int)(sizeof(history) / sizeof(history[0])))
            history[history_count++] = page_offset;
        target = next_offset;
    }
    else {
        if(history_count <= 0)
            target = 0;
        else
            target = history[--history_count];
        if(target == page_offset)
            return;
    }
    if(!crazypod_book_session_load(selected_index, target))
        return;
    crazypod_book_set_progress(selected_index, target);
    crazypod_state_mark_dirty();
}

int crazypod_book_session_index(void)
{
    return selected_index;
}

uint32_t crazypod_book_session_offset(void)
{
    return page_offset;
}

const char *crazypod_book_session_text(void)
{
    return page_text;
}

bool crazypod_book_session_has_text(void)
{
    return page_text[0] != '\0';
}

#endif
