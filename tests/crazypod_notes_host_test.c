#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include "crazypod_notes.h"

const char *notes_test_root;
bool notes_test_fail_index_open;
bool notes_test_fail_index_rename;
bool notes_test_fail_index_rename_persistent;

const char *crazypod_l10n_text(const char *text)
{
    return text;
}

int main(int argc, char **argv)
{
    char body[CRAZYPOD_NOTE_BODY_SIZE];
    const struct crazypod_note *note;
    uint32_t id;

    assert(argc == 2);
    notes_test_root = argv[1];
    assert(mkdir(notes_test_root) == 0);
    crazypod_notes_init();

    id = crazypod_note_save(0, "Old title", "old body");
    assert(id != 0);
    assert(crazypod_note_read_body(id, body, sizeof(body)));
    assert(strcmp(body, "old body") == 0);

    notes_test_fail_index_rename = true;
    notes_test_fail_index_rename_persistent = true;
    assert(crazypod_note_save(id, "New title", "new body") == 0);
    notes_test_fail_index_rename = false;
    notes_test_fail_index_rename_persistent = false;
    crazypod_notes_init();
    note = crazypod_note_find(id);
    assert(note != NULL);
    assert(strcmp(note->title, "Old title") == 0);
    assert(crazypod_note_read_body(id, body, sizeof(body)));
    assert(strcmp(body, "old body") == 0);
    assert(crazypod_notes_count(false) == 1);

    notes_test_fail_index_open = true;
    assert(crazypod_note_save(0, "Not committed", "discard me") == 0);
    notes_test_fail_index_open = false;
    assert(crazypod_notes_count(false) == 1);

    id = crazypod_note_save(id, "New title", "new body");
    assert(id != 0);
    note = crazypod_note_find(id);
    assert(note != NULL);
    assert(strcmp(note->title, "New title") == 0);
    assert(crazypod_note_read_body(id, body, sizeof(body)));
    assert(strcmp(body, "new body") == 0);
    return 0;
}
