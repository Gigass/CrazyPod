#ifndef CRAZYPOD_NOTES_H
#define CRAZYPOD_NOTES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRAZYPOD_NOTE_TITLE_SIZE 96
#define CRAZYPOD_NOTE_BODY_SIZE 4096

struct crazypod_note {
    uint32_t id;
    uint32_t updated_sequence;
    bool pinned;
    bool deleted;
    char title[CRAZYPOD_NOTE_TITLE_SIZE];
};

struct crazypod_note_draft {
    uint32_t source_id;
    char title[CRAZYPOD_NOTE_TITLE_SIZE];
    char body[CRAZYPOD_NOTE_BODY_SIZE];
};

void crazypod_notes_init(void);
int crazypod_notes_count(bool deleted);
const struct crazypod_note *crazypod_note_get(bool deleted, int index);
const struct crazypod_note *crazypod_note_find(uint32_t id);
int crazypod_notes_search_count(const char *query);
const struct crazypod_note *crazypod_notes_search_get(
    const char *query, int index);
bool crazypod_note_read_body(uint32_t id, char *body, size_t size);
uint32_t crazypod_note_save(uint32_t id, const char *title,
                            const char *body);
uint32_t crazypod_note_duplicate(uint32_t id);
bool crazypod_note_set_pinned(uint32_t id, bool pinned);
bool crazypod_note_move_to_trash(uint32_t id);
bool crazypod_note_restore(uint32_t id);
bool crazypod_note_delete_forever(uint32_t id);
bool crazypod_notes_empty_trash(void);

bool crazypod_note_draft_load(struct crazypod_note_draft *draft);
bool crazypod_note_draft_save(const struct crazypod_note_draft *draft);
void crazypod_note_draft_clear(void);

#endif
