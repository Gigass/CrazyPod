#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_books.h"
#include "crazypod_books_confirmation.h"

struct crazypod_books_confirmation_result
crazypod_books_confirmation_execute(const struct route_state *state)
{
    struct crazypod_books_confirmation_result result = { 0 };

    if(state->route != BOOKS_ROUTE_DELETE_CONFIRM)
        return result;
    result.handled = true;
    result.deleted = crazypod_book_delete(state->group);
    return result;
}

#endif
