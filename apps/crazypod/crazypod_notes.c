#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_checksum.h"
#include "crazypod_notes.h"

#define NOTES_DIRECTORY "/.crazypod/notes"
#define NOTES_INDEX_PATH NOTES_DIRECTORY "/index.bin"
#define NOTES_INDEX_TEMP NOTES_DIRECTORY "/index.tmp"
#define NOTES_TRANSACTION_PATH NOTES_DIRECTORY "/transaction.bin"
#define NOTES_TRANSACTION_TEMP NOTES_DIRECTORY "/transaction.tmp"
#define NOTES_DRAFT_PATH NOTES_DIRECTORY "/draft.bin"
#define NOTES_DRAFT_TEMP NOTES_DIRECTORY "/draft.tmp"
#define NOTES_MAGIC 0x43504e54u
#define NOTES_VERSION 1u
#define NOTES_TRANSACTION_MAGIC 0x43505458u
#define NOTES_TRANSACTION_VERSION 1u
#define NOTES_DRAFT_MAGIC 0x43504452u
#define NOTES_MAX 64

struct note_disk {
    uint32_t id;
    uint32_t updated_sequence;
    uint32_t pinned;
    uint32_t deleted;
    char title[CRAZYPOD_NOTE_TITLE_SIZE];
};

struct notes_index_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t count;
    uint32_t next_id;
    uint32_t next_sequence;
    struct note_disk notes[NOTES_MAX];
    uint32_t checksum;
};

struct note_draft_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t source_id;
    char title[CRAZYPOD_NOTE_TITLE_SIZE];
    char body[CRAZYPOD_NOTE_BODY_SIZE];
    uint32_t checksum;
};

struct notes_transaction_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t id;
    uint32_t old_body_exists;
    struct notes_index_disk old_index;
    struct notes_index_disk new_index;
    uint32_t checksum;
};

static struct notes_index_disk index_state;
static struct note_draft_disk draft_work;
static char body_work[CRAZYPOD_NOTE_BODY_SIZE];

static uint32_t index_checksum(const struct notes_index_disk *index)
{
    return crazypod_checksum_with_zeroed_u32(
        index, sizeof(*index),
        offsetof(struct notes_index_disk, checksum));
}

static uint32_t draft_checksum(const struct note_draft_disk *draft)
{
    return crazypod_checksum_with_zeroed_u32(
        draft, sizeof(*draft),
        offsetof(struct note_draft_disk, checksum));
}

static uint32_t transaction_checksum(
    const struct notes_transaction_disk *transaction)
{
    return crazypod_checksum_with_zeroed_u32(
        transaction, sizeof(*transaction),
        offsetof(struct notes_transaction_disk, checksum));
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *cursor = buffer;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const unsigned char *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static void copy_text(char *destination, size_t size, const char *source)
{
    if(size == 0)
        return;
    snprintf(destination, size, "%s", source != NULL ? source : "");
}

static void notes_reset(void)
{
    memset(&index_state, 0, sizeof(index_state));
    index_state.magic = NOTES_MAGIC;
    index_state.version = NOTES_VERSION;
    index_state.size = sizeof(index_state);
    index_state.next_id = 1;
    index_state.next_sequence = 1;
}

static void notes_index_prepare(struct notes_index_disk *index)
{
    index->magic = NOTES_MAGIC;
    index->version = NOTES_VERSION;
    index->size = sizeof(*index);
    index->checksum = index_checksum(index);
}

static bool notes_index_write_file(
    const char *path, const struct notes_index_disk *source)
{
    int fd;
    bool success;
    struct notes_index_disk index = *source;

    mkdir("/.crazypod");
    mkdir(NOTES_DIRECTORY);
    notes_index_prepare(&index);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &index, sizeof(index));
    if(fsync(fd) < 0)
        success = false;
    if(close(fd) < 0)
        success = false;
    if(!success) {
        remove(path);
        return false;
    }
    return true;
}

static bool notes_index_write(
    const char *temporary, const char *path,
    const struct notes_index_disk *source)
{
    if(!notes_index_write_file(temporary, source) ||
       rename(temporary, path) < 0) {
        remove(temporary);
        return false;
    }
    return true;
}

static bool notes_index_save(void)
{
    notes_index_prepare(&index_state);
    return notes_index_write(
        NOTES_INDEX_TEMP, NOTES_INDEX_PATH, &index_state);
}

static bool notes_index_read(
    const char *path, struct notes_index_disk *index)
{
    int fd = open(path, O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid = read_exact(fd, index, sizeof(*index)) &&
        index->magic == NOTES_MAGIC &&
        index->version == NOTES_VERSION &&
        index->size == sizeof(*index) &&
        index->count <= NOTES_MAX &&
        index->checksum == index_checksum(index);
    if(close(fd) < 0)
        valid = false;
    return valid;
}

static void note_path(char *path, size_t size, uint32_t id,
                      const char *suffix)
{
    snprintf(path, size, NOTES_DIRECTORY "/%lu.%s",
             (unsigned long)id, suffix);
}

static bool note_body_exists(uint32_t id)
{
    char path[MAX_PATH];
    int fd;

    note_path(path, sizeof(path), id, "txt");
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    if(close(fd) < 0)
        return false;
    return true;
}

static bool note_body_write_temp(uint32_t id, const char *body)
{
    char temporary[MAX_PATH];
    size_t length = strlen(body != NULL ? body : "");
    int fd;
    bool success;

    mkdir("/.crazypod");
    mkdir(NOTES_DIRECTORY);
    note_path(temporary, sizeof(temporary), id, "tmp");
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, body != NULL ? body : "", length);
    if(fsync(fd) < 0)
        success = false;
    if(close(fd) < 0)
        success = false;
    if(!success) {
        remove(temporary);
        return false;
    }
    return true;
}

static bool note_body_install(uint32_t id)
{
    char path[MAX_PATH];
    char temporary[MAX_PATH];

    note_path(path, sizeof(path), id, "txt");
    note_path(temporary, sizeof(temporary), id, "tmp");
    return rename(temporary, path) == 0;
}

static bool note_body_backup(uint32_t id, bool old_body_exists)
{
    char path[MAX_PATH];
    char backup[MAX_PATH];

    if(!old_body_exists)
        return true;
    note_path(path, sizeof(path), id, "txt");
    note_path(backup, sizeof(backup), id, "bak");
    remove(backup);
    return rename(path, backup) == 0;
}

static bool note_body_restore(uint32_t id, bool old_body_exists)
{
    char path[MAX_PATH];
    char backup[MAX_PATH];

    note_path(path, sizeof(path), id, "txt");
    note_path(backup, sizeof(backup), id, "bak");
    if(!old_body_exists) {
        remove(path);
        return true;
    }
    if(!file_exists(backup))
        return file_exists(path);
    remove(path);
    return rename(backup, path) == 0;
}

static void note_body_cleanup(uint32_t id)
{
    char temporary[MAX_PATH];
    char backup[MAX_PATH];

    note_path(temporary, sizeof(temporary), id, "tmp");
    note_path(backup, sizeof(backup), id, "bak");
    remove(temporary);
    remove(backup);
}

static bool notes_transaction_write(
    uint32_t id, bool old_body_exists,
    const struct notes_index_disk *old_index,
    const struct notes_index_disk *new_index)
{
    struct notes_transaction_disk transaction;
    int fd;
    bool success;

    memset(&transaction, 0, sizeof(transaction));
    transaction.magic = NOTES_TRANSACTION_MAGIC;
    transaction.version = NOTES_TRANSACTION_VERSION;
    transaction.size = sizeof(transaction);
    transaction.id = id;
    transaction.old_body_exists = old_body_exists ? 1u : 0u;
    transaction.old_index = *old_index;
    transaction.new_index = *new_index;
    notes_index_prepare(&transaction.old_index);
    notes_index_prepare(&transaction.new_index);
    transaction.checksum = transaction_checksum(&transaction);
    mkdir("/.crazypod");
    mkdir(NOTES_DIRECTORY);
    fd = open(NOTES_TRANSACTION_TEMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &transaction, sizeof(transaction));
    if(fsync(fd) < 0)
        success = false;
    if(close(fd) < 0)
        success = false;
    if(!success || rename(NOTES_TRANSACTION_TEMP,
                          NOTES_TRANSACTION_PATH) < 0) {
        remove(NOTES_TRANSACTION_TEMP);
        return false;
    }
    return true;
}

static bool notes_transaction_read(
    struct notes_transaction_disk *transaction)
{
    int fd = open(NOTES_TRANSACTION_PATH, O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid = read_exact(fd, transaction, sizeof(*transaction)) &&
        transaction->magic == NOTES_TRANSACTION_MAGIC &&
        transaction->version == NOTES_TRANSACTION_VERSION &&
        transaction->size == sizeof(*transaction) &&
        transaction->old_body_exists <= 1u &&
        transaction->old_index.checksum ==
            index_checksum(&transaction->old_index) &&
        transaction->new_index.checksum ==
            index_checksum(&transaction->new_index) &&
        transaction->checksum == transaction_checksum(transaction);
    if(close(fd) < 0)
        valid = false;
    return valid;
}

static bool notes_index_matches(
    const struct notes_index_disk *expected)
{
    struct notes_index_disk current;

    return notes_index_read(NOTES_INDEX_PATH, &current) &&
        memcmp(&current, expected, sizeof(current)) == 0;
}

static void notes_transaction_recover(void)
{
    struct notes_transaction_disk transaction;
    bool committed;
    bool body_ready;

    if(!file_exists(NOTES_TRANSACTION_PATH)) {
        remove(NOTES_TRANSACTION_TEMP);
        return;
    }
    if(!notes_transaction_read(&transaction)) {
        remove(NOTES_TRANSACTION_TEMP);
        remove(NOTES_TRANSACTION_PATH);
        return;
    }
    committed = notes_index_matches(&transaction.new_index);
    body_ready = note_body_exists(transaction.id);
    if(committed && !body_ready && note_body_install(transaction.id))
        body_ready = note_body_exists(transaction.id);
    if(committed && body_ready) {
        note_body_cleanup(transaction.id);
        remove(NOTES_TRANSACTION_PATH);
        return;
    }
    if(!note_body_restore(
           transaction.id, transaction.old_body_exists != 0) ||
       !notes_index_write(
           NOTES_INDEX_TEMP, NOTES_INDEX_PATH, &transaction.old_index))
        return;
    note_body_cleanup(transaction.id);
    remove(NOTES_TRANSACTION_PATH);
}

static int note_slot(uint32_t id)
{
    uint32_t i;

    for(i = 0; i < index_state.count; ++i) {
        if(index_state.notes[i].id == id)
            return (int)i;
    }
    return -1;
}

void crazypod_notes_init(void)
{
    struct notes_index_disk loaded;

    notes_transaction_recover();
    notes_reset();
    if(!notes_index_read(NOTES_INDEX_PATH, &loaded))
        return;
    index_state = loaded;
    if(index_state.next_id == 0)
        index_state.next_id = 1;
    if(index_state.next_sequence == 0)
        index_state.next_sequence = 1;
}

static bool note_before(const struct note_disk *left,
                        const struct note_disk *right)
{
    if(left->pinned != right->pinned)
        return left->pinned > right->pinned;
    return left->updated_sequence > right->updated_sequence;
}

int crazypod_notes_count(bool deleted)
{
    int count = 0;
    uint32_t i;

    for(i = 0; i < index_state.count; ++i) {
        if((index_state.notes[i].deleted != 0) == deleted)
            ++count;
    }
    return count;
}

const struct crazypod_note *crazypod_note_get(bool deleted, int index)
{
    static struct crazypod_note result;
    const struct note_disk *best = NULL;
    int rank;

    if(index < 0)
        return NULL;
    for(rank = 0; rank <= index; ++rank) {
        const struct note_disk *candidate = NULL;
        uint32_t i;
        for(i = 0; i < index_state.count; ++i) {
            const struct note_disk *note = &index_state.notes[i];
            int earlier = 0;
            uint32_t j;
            if((note->deleted != 0) != deleted)
                continue;
            for(j = 0; j < index_state.count; ++j) {
                const struct note_disk *other = &index_state.notes[j];
                if((other->deleted != 0) != deleted)
                    continue;
                if(note_before(other, note))
                    ++earlier;
            }
            if(earlier == rank) {
                candidate = note;
                break;
            }
        }
        best = candidate;
    }
    if(best == NULL)
        return NULL;
    result.id = best->id;
    result.updated_sequence = best->updated_sequence;
    result.pinned = best->pinned != 0;
    result.deleted = best->deleted != 0;
    copy_text(result.title, sizeof(result.title), best->title);
    return &result;
}

const struct crazypod_note *crazypod_note_find(uint32_t id)
{
    static struct crazypod_note result;
    int slot = note_slot(id);
    const struct note_disk *note;

    if(slot < 0)
        return NULL;
    note = &index_state.notes[slot];
    result.id = note->id;
    result.updated_sequence = note->updated_sequence;
    result.pinned = note->pinned != 0;
    result.deleted = note->deleted != 0;
    copy_text(result.title, sizeof(result.title), note->title);
    return &result;
}

static char fold_ascii(char value)
{
    return value >= 'A' && value <= 'Z'
        ? (char)(value - 'A' + 'a') : value;
}

static bool contains_text(const char *text, const char *query)
{
    const char *start;

    if(query == NULL || query[0] == '\0')
        return true;
    for(start = text; start != NULL && *start != '\0'; ++start) {
        const char *left = start;
        const char *right = query;
        while(*left != '\0' && *right != '\0' &&
              fold_ascii(*left) == fold_ascii(*right)) {
            ++left;
            ++right;
        }
        if(*right == '\0')
            return true;
    }
    return false;
}

static bool note_matches(const struct crazypod_note *note,
                         const char *query)
{
    if(note == NULL || note->deleted)
        return false;
    if(contains_text(note->title, query))
        return true;
    return crazypod_note_read_body(
               note->id, body_work, sizeof(body_work)) &&
           contains_text(body_work, query);
}

int crazypod_notes_search_count(const char *query)
{
    int count = 0;
    int i;
    for(i = 0; i < crazypod_notes_count(false); ++i) {
        const struct crazypod_note *note =
            crazypod_note_get(false, i);
        if(note_matches(note, query))
            ++count;
    }
    return count;
}

const struct crazypod_note *crazypod_notes_search_get(
    const char *query, int index)
{
    int visible = 0;
    int i;
    for(i = 0; i < crazypod_notes_count(false); ++i) {
        const struct crazypod_note *note =
            crazypod_note_get(false, i);
        uint32_t id = note != NULL ? note->id : 0;
        if(note_matches(note, query) && visible++ == index)
            return crazypod_note_find(id);
    }
    return NULL;
}

bool crazypod_note_read_body(uint32_t id, char *body, size_t size)
{
    char path[MAX_PATH];
    ssize_t count;
    int fd;

    if(body == NULL || size == 0 || note_slot(id) < 0)
        return false;
    body[0] = '\0';
    note_path(path, sizeof(path), id, "txt");
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    count = read(fd, body, size - 1);
    close(fd);
    if(count < 0)
        return false;
    body[count] = '\0';
    return true;
}

uint32_t crazypod_note_save(uint32_t id, const char *title,
                            const char *body)
{
    struct notes_index_disk old_index;
    struct notes_index_disk new_index;
    int slot;
    bool old_body_exists;
    bool transaction_written = false;

    notes_transaction_recover();
    slot = note_slot(id);
    if((title == NULL || title[0] == '\0') &&
       (body == NULL || body[0] == '\0'))
        return 0;
    old_index = index_state;
    if(slot < 0) {
        if(index_state.count >= NOTES_MAX)
            return 0;
        slot = (int)index_state.count++;
        memset(&index_state.notes[slot], 0,
               sizeof(index_state.notes[slot]));
        id = index_state.next_id++;
        if(id == 0)
            id = index_state.next_id++;
        index_state.notes[slot].id = id;
    }
    if(title == NULL || title[0] == '\0') {
        char generated[CRAZYPOD_NOTE_TITLE_SIZE];
        const char *newline;
        size_t length;
        copy_text(generated, sizeof(generated), body);
        newline = strchr(generated, '\n');
        length = newline != NULL
            ? (size_t)(newline - generated) : strlen(generated);
        if(length >= sizeof(generated))
            length = sizeof(generated) - 1;
        generated[length] = '\0';
        copy_text(index_state.notes[slot].title,
                  sizeof(index_state.notes[slot].title), generated);
    }
    else {
        copy_text(index_state.notes[slot].title,
                  sizeof(index_state.notes[slot].title), title);
    }
    index_state.notes[slot].updated_sequence =
        index_state.next_sequence++;
    index_state.notes[slot].deleted = 0;
    new_index = index_state;
    notes_index_prepare(&new_index);
    old_body_exists = note_body_exists(id);
    if(!note_body_write_temp(id, body != NULL ? body : "") ||
       !notes_index_write_file(NOTES_INDEX_TEMP, &new_index))
        goto save_failed;
    if(!notes_transaction_write(
           id, old_body_exists, &old_index, &new_index))
        goto save_failed;
    transaction_written = true;
    if(!note_body_backup(id, old_body_exists) ||
       !note_body_install(id) ||
       rename(NOTES_INDEX_TEMP, NOTES_INDEX_PATH) < 0)
        goto save_failed;
    note_body_cleanup(id);
    remove(NOTES_TRANSACTION_PATH);
    index_state = new_index;
    return id;

save_failed:
    remove(NOTES_INDEX_TEMP);
    if(transaction_written) {
        if(!note_body_restore(id, old_body_exists) ||
           !notes_index_write(
               NOTES_INDEX_TEMP, NOTES_INDEX_PATH, &old_index))
            transaction_written = true;
        else {
            note_body_cleanup(id);
            remove(NOTES_TRANSACTION_PATH);
            transaction_written = false;
        }
    }
    else {
        note_body_cleanup(id);
    }
    index_state = old_index;
    return 0;
}

uint32_t crazypod_note_duplicate(uint32_t id)
{
    const struct crazypod_note *source = crazypod_note_find(id);
    char title[CRAZYPOD_NOTE_TITLE_SIZE];
    uint32_t duplicate;

    if(source == NULL ||
       !crazypod_note_read_body(id, body_work, sizeof(body_work)))
        return 0;
    snprintf(title, sizeof(title), CP_FMT("%.89s Copy"), source->title);
    duplicate = crazypod_note_save(0, title, body_work);
    if(duplicate != 0 && source->pinned)
        crazypod_note_set_pinned(duplicate, true);
    return duplicate;
}

bool crazypod_note_set_pinned(uint32_t id, bool pinned)
{
    int slot = note_slot(id);
    if(slot < 0)
        return false;
    index_state.notes[slot].pinned = pinned ? 1 : 0;
    index_state.notes[slot].updated_sequence =
        index_state.next_sequence++;
    return notes_index_save();
}

bool crazypod_note_move_to_trash(uint32_t id)
{
    int slot = note_slot(id);
    if(slot < 0)
        return false;
    index_state.notes[slot].deleted = 1;
    index_state.notes[slot].pinned = 0;
    index_state.notes[slot].updated_sequence =
        index_state.next_sequence++;
    return notes_index_save();
}

bool crazypod_note_restore(uint32_t id)
{
    int slot = note_slot(id);
    if(slot < 0)
        return false;
    index_state.notes[slot].deleted = 0;
    index_state.notes[slot].updated_sequence =
        index_state.next_sequence++;
    return notes_index_save();
}

bool crazypod_note_delete_forever(uint32_t id)
{
    char path[MAX_PATH];
    int slot = note_slot(id);

    if(slot < 0)
        return false;
    note_path(path, sizeof(path), id, "txt");
    remove(path);
    if(slot + 1 < (int)index_state.count) {
        memmove(&index_state.notes[slot], &index_state.notes[slot + 1],
                (index_state.count - (uint32_t)slot - 1) *
                    sizeof(index_state.notes[0]));
    }
    --index_state.count;
    memset(&index_state.notes[index_state.count], 0,
           sizeof(index_state.notes[0]));
    return notes_index_save();
}

bool crazypod_notes_empty_trash(void)
{
    int i = (int)index_state.count - 1;
    bool changed = false;

    while(i >= 0) {
        if(index_state.notes[i].deleted != 0) {
            char path[MAX_PATH];
            note_path(path, sizeof(path), index_state.notes[i].id, "txt");
            remove(path);
            if(i + 1 < (int)index_state.count) {
                memmove(&index_state.notes[i], &index_state.notes[i + 1],
                        (index_state.count - (uint32_t)i - 1) *
                            sizeof(index_state.notes[0]));
            }
            --index_state.count;
            changed = true;
        }
        --i;
    }
    return !changed || notes_index_save();
}

bool crazypod_note_draft_load(struct crazypod_note_draft *draft)
{
    int fd;
    bool valid;

    if(draft == NULL)
        return false;
    fd = open(NOTES_DRAFT_PATH, O_RDONLY);
    if(fd < 0)
        return false;
    valid = read_exact(fd, &draft_work, sizeof(draft_work)) &&
            draft_work.magic == NOTES_DRAFT_MAGIC &&
            draft_work.version == NOTES_VERSION &&
            draft_work.size == sizeof(draft_work) &&
            draft_work.checksum == draft_checksum(&draft_work);
    close(fd);
    if(!valid)
        return false;
    draft->source_id = draft_work.source_id;
    copy_text(draft->title, sizeof(draft->title), draft_work.title);
    copy_text(draft->body, sizeof(draft->body), draft_work.body);
    return true;
}

bool crazypod_note_draft_save(const struct crazypod_note_draft *draft)
{
    int fd;
    bool success;

    if(draft == NULL)
        return false;
    mkdir("/.crazypod");
    mkdir(NOTES_DIRECTORY);
    memset(&draft_work, 0, sizeof(draft_work));
    draft_work.magic = NOTES_DRAFT_MAGIC;
    draft_work.version = NOTES_VERSION;
    draft_work.size = sizeof(draft_work);
    draft_work.source_id = draft->source_id;
    copy_text(
        draft_work.title, sizeof(draft_work.title), draft->title);
    copy_text(
        draft_work.body, sizeof(draft_work.body), draft->body);
    draft_work.checksum = draft_checksum(&draft_work);
    fd = open(NOTES_DRAFT_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &draft_work, sizeof(draft_work));
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    if(!success || rename(NOTES_DRAFT_TEMP, NOTES_DRAFT_PATH) < 0) {
        remove(NOTES_DRAFT_TEMP);
        return false;
    }
    return true;
}

void crazypod_note_draft_clear(void)
{
    remove(NOTES_DRAFT_PATH);
    remove(NOTES_DRAFT_TEMP);
}

#endif
