#include "config.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "dir.h"
#include "kernel.h"
#include "metadata.h"
#include "pcmbuf.h"
#include "storage.h"

#include "crazypod_audiobook_chapters.h"
#include "crazypod_audiobooks.h"
#include "crazypod_books.h"
#include "crazypod_checksum.h"
#include "crazypod_music.h"
#include "crazypod_playlist.h"

#define AUDIOBOOKS_DIRECTORY "/Audiobooks"
#define BOOKS_DIRECTORY "/Books"
#define AUDIOBOOKS_STATE_DIRECTORY "/.crazypod/books"
#define AUDIOBOOKS_STATE_PATH AUDIOBOOKS_STATE_DIRECTORY "/audiobooks.bin"
#define AUDIOBOOKS_STATE_TEMP AUDIOBOOKS_STATE_DIRECTORY "/audiobooks.tmp"
#define AUDIOBOOKS_MAGIC 0x4B424141u /* "AABK" */
#define AUDIOBOOKS_VERSION 1u
#define AUDIOBOOKS_SCAN_DEPTH 4
#define TICK_INTERVAL (HZ / 2)
#define PERIODIC_SAVE_INTERVAL (30 * HZ)
/* A book within this much of its end restarts from the beginning. */
#define FINISHED_MARGIN_MS 5000u

struct progress_disk {
    uint32_t path_hash;
    uint32_t position_ms;
    uint32_t length_ms;
    uint32_t sequence;
};

struct state_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t count;
    uint32_t next_sequence;
    struct progress_disk entries[CRAZYPOD_AUDIOBOOKS_MAX];
    uint32_t checksum;
};

static struct crazypod_audiobook books[CRAZYPOD_AUDIOBOOKS_MAX];
static int book_count;
static bool scan_done;
static struct state_disk persisted;

static struct crazypod_audiobook_chapter chapters[
    CRAZYPOD_AUDIOBOOK_CHAPTERS_MAX];
static int chapter_count;
static int chapters_index = -1;

static struct {
    int index;              /* book the queue is playing, or -1 */
    bool was_playing;
    long last_tick;
    long last_save;
    uint32_t last_saved_ms;
    int published_chapter;  /* chapter shown as the album line */
    long seek_started;      /* tick of the pending chapter seek, or 0 */
    uint32_t seek_target_ms;
    uint32_t last_seek_ms;
} live = { .index = -1, .published_chapter = -2 };

static struct mp3entry probe_entry;

/* ---- Catalog ------------------------------------------------------------ */

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t i;

    for(i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t path_hash(const char *path)
{
    return hash_bytes(2166136261u, path, strlen(path));
}

static bool text_equal_ignore_case(const char *a, const char *b)
{
    while(*a != '\0' && *b != '\0') {
        char x = *a >= 'A' && *a <= 'Z' ? *a + ('a' - 'A') : *a;
        char y = *b >= 'A' && *b <= 'Z' ? *b + ('a' - 'A') : *b;

        if(x != y)
            return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

static bool is_audiobook_file(const char *path, bool books_tree)
{
    const char *dot = strrchr(path, '.');

    if(dot == NULL)
        return false;
    if(text_equal_ignore_case(dot, ".m4b"))
        return true;
    if(books_tree)
        return false;
    return text_equal_ignore_case(dot, ".m4a") ||
           text_equal_ignore_case(dot, ".mp3");
}

static void title_from_path(char *title, size_t size, const char *path)
{
    const char *name = strrchr(path, '/');
    const char *dot;
    size_t length;

    name = name != NULL ? name + 1 : path;
    dot = strrchr(name, '.');
    length = dot != NULL ? (size_t)(dot - name) : strlen(name);
    if(length >= size)
        length = size - 1;
    memcpy(title, name, length);
    title[length] = '\0';
}

static struct progress_disk *saved_progress(uint32_t hash)
{
    uint32_t i;

    for(i = 0; i < persisted.count; ++i)
        if(persisted.entries[i].path_hash == hash)
            return &persisted.entries[i];
    return NULL;
}

static void add_book(const char *path, const struct dirinfo *info)
{
    struct crazypod_audiobook *book;
    const struct progress_disk *saved;

    if(book_count >= CRAZYPOD_AUDIOBOOKS_MAX)
        return;
    book = &books[book_count];
    memset(book, 0, sizeof(*book));
    snprintf(book->path, sizeof(book->path), "%s", path);
    title_from_path(book->title, sizeof(book->title), path);
    book->size = (uint32_t)info->size;
    book->mtime = (uint32_t)info->mtime;
    saved = saved_progress(path_hash(path));
    if(saved != NULL) {
        book->position_ms = saved->position_ms;
        book->length_ms = saved->length_ms;
    }
    ++book_count;
}

static bool append_path(char *buffer, size_t size,
                        const char *directory, const char *name)
{
    int written = snprintf(buffer, size, "%s/%s", directory, name);

    return written > 0 && (size_t)written < size;
}

static void scan_directory(const char *path, int depth, bool books_tree)
{
    DIR *directory;
    struct DIRENT *entry;

    if(depth > AUDIOBOOKS_SCAN_DEPTH ||
       book_count >= CRAZYPOD_AUDIOBOOKS_MAX)
        return;
    directory = opendir(path);
    if(directory == NULL)
        return;
    while((entry = readdir(directory)) != NULL &&
          book_count < CRAZYPOD_AUDIOBOOKS_MAX) {
        struct dirinfo info;
        char child[MAX_PATH];

        if(entry->d_name[0] == '.' ||
           !append_path(child, sizeof(child), path, entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY)
            scan_directory(child, depth + 1, books_tree);
        else if(is_audiobook_file(child, books_tree))
            add_book(child, &info);
    }
    closedir(directory);
}

static int compare_titles(const void *a, const void *b)
{
    return strcmp(((const struct crazypod_audiobook *)a)->title,
                  ((const struct crazypod_audiobook *)b)->title);
}

static void sort_books(void)
{
    /* Insertion sort: the catalog is small and qsort is not guaranteed in
     * the firmware libc. */
    int i;

    for(i = 1; i < book_count; ++i) {
        struct crazypod_audiobook key = books[i];
        int j = i - 1;

        while(j >= 0 && compare_titles(&books[j], &key) > 0) {
            books[j + 1] = books[j];
            --j;
        }
        books[j + 1] = key;
    }
}

/* ---- Persistence -------------------------------------------------------- */

static uint32_t state_checksum(const struct state_disk *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state), offsetof(struct state_disk, checksum));
}

static bool read_exact(int fd, void *data, size_t size)
{
    return read(fd, data, size) == (ssize_t)size;
}

static bool write_exact(int fd, const void *data, size_t size)
{
    return write(fd, data, size) == (ssize_t)size;
}

static bool state_save(void)
{
    int fd;
    bool success;

    mkdir("/.crazypod");
    mkdir(AUDIOBOOKS_STATE_DIRECTORY);
    persisted.magic = AUDIOBOOKS_MAGIC;
    persisted.version = AUDIOBOOKS_VERSION;
    persisted.size = sizeof(persisted);
    persisted.checksum = state_checksum(&persisted);
    fd = open(AUDIOBOOKS_STATE_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &persisted, sizeof(persisted));
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    if(!success ||
       rename(AUDIOBOOKS_STATE_TEMP, AUDIOBOOKS_STATE_PATH) < 0) {
        remove(AUDIOBOOKS_STATE_TEMP);
        return false;
    }
    return true;
}

static void state_load(void)
{
    static struct state_disk loaded;
    int fd;

    memset(&persisted, 0, sizeof(persisted));
    persisted.next_sequence = 1;
    fd = open(AUDIOBOOKS_STATE_PATH, O_RDONLY);
    if(fd < 0)
        return;
    if(read_exact(fd, &loaded, sizeof(loaded)) &&
       loaded.magic == AUDIOBOOKS_MAGIC &&
       loaded.version == AUDIOBOOKS_VERSION &&
       loaded.size == sizeof(loaded) &&
       loaded.count <= CRAZYPOD_AUDIOBOOKS_MAX &&
       loaded.checksum == state_checksum(&loaded)) {
        persisted = loaded;
        if(persisted.next_sequence == 0)
            persisted.next_sequence = 1;
    }
    close(fd);
}

static struct progress_disk *progress_slot(uint32_t hash)
{
    struct progress_disk *entry = saved_progress(hash);
    uint32_t oldest = 0;
    uint32_t i;

    if(entry != NULL)
        return entry;
    if(persisted.count < CRAZYPOD_AUDIOBOOKS_MAX) {
        entry = &persisted.entries[persisted.count++];
    }
    else {
        /* Recycle the entry that was listened to least recently. */
        for(i = 1; i < persisted.count; ++i)
            if(persisted.entries[i].sequence <
               persisted.entries[oldest].sequence)
                oldest = i;
        entry = &persisted.entries[oldest];
    }
    memset(entry, 0, sizeof(*entry));
    entry->path_hash = hash;
    return entry;
}

static bool remember_position(int index, uint32_t position_ms, bool touch)
{
    struct crazypod_audiobook *book;
    struct progress_disk *entry;

    if(index < 0 || index >= book_count)
        return false;
    book = &books[index];
    entry = progress_slot(path_hash(book->path));
    book->position_ms = position_ms;
    entry->position_ms = position_ms;
    entry->length_ms = book->length_ms;
    if(touch)
        entry->sequence = crazypod_books_take_recent_sequence();
    return state_save();
}

/* ---- Public catalog API ------------------------------------------------- */

void crazypod_audiobooks_init(void)
{
    book_count = 0;
    scan_done = false;
    chapter_count = 0;
    chapters_index = -1;
    live.index = -1;
    live.was_playing = false;
    live.last_tick = 0;
    live.last_save = 0;
    state_load();
}

void crazypod_audiobooks_scan(void)
{
    book_count = 0;
    chapter_count = 0;
    chapters_index = -1;
    live.index = -1;
    mkdir(AUDIOBOOKS_DIRECTORY);
    scan_directory(AUDIOBOOKS_DIRECTORY, 0, false);
    scan_directory(BOOKS_DIRECTORY, 0, true);
    sort_books();
    scan_done = true;
}

bool crazypod_audiobooks_scan_needed(void)
{
    return !scan_done;
}

void crazypod_audiobooks_invalidate_scan(void)
{
    scan_done = false;
}

int crazypod_audiobooks_count(void)
{
    return book_count;
}

const struct crazypod_audiobook *crazypod_audiobook_get(int index)
{
    return index >= 0 && index < book_count ? &books[index] : NULL;
}

bool crazypod_audiobook_probe(int index)
{
    struct crazypod_audiobook *book =
        index >= 0 && index < book_count ? &books[index] : NULL;
    int fd;
    bool ok;

    if(book == NULL)
        return false;
    if(book->details_loaded)
        return true;
    fd = open(book->path, O_RDONLY);
    if(fd < 0)
        return false;
    memset(&probe_entry, 0, sizeof(probe_entry));
    ok = get_metadata(&probe_entry, fd, book->path);
    close(fd);
    book->details_loaded = true;
    if(!ok)
        return true;
    if(probe_entry.title != NULL && probe_entry.title[0] != '\0')
        snprintf(book->title, sizeof(book->title), "%s",
                 probe_entry.title);
    if(probe_entry.artist != NULL && probe_entry.artist[0] != '\0')
        snprintf(book->author, sizeof(book->author), "%s",
                 probe_entry.artist);
    else if(probe_entry.albumartist != NULL &&
            probe_entry.albumartist[0] != '\0')
        snprintf(book->author, sizeof(book->author), "%s",
                 probe_entry.albumartist);
    if(probe_entry.length > 0)
        book->length_ms = (uint32_t)probe_entry.length;
    return true;
}

uint32_t crazypod_audiobook_recent_sequence(int index)
{
    const struct progress_disk *entry;

    if(index < 0 || index >= book_count)
        return 0;
    entry = saved_progress(path_hash(books[index].path));
    return entry != NULL ? entry->sequence : 0;
}

int crazypod_audiobooks_recent_index(void)
{
    uint32_t best_sequence = 0;
    int best = -1;
    int i;

    for(i = 0; i < book_count; ++i) {
        const struct progress_disk *entry =
            saved_progress(path_hash(books[i].path));

        if(entry != NULL && entry->sequence > best_sequence) {
            best_sequence = entry->sequence;
            best = i;
        }
    }
    return best;
}

/* ---- Chapters ----------------------------------------------------------- */

struct file_reader {
    int fd;
};

static bool read_file_at(
    void *context, uint32_t offset, void *buffer, uint32_t size)
{
    struct file_reader *reader = context;

    if(lseek(reader->fd, (off_t)offset, SEEK_SET) != (off_t)offset)
        return false;
    return read(reader->fd, buffer, size) == (ssize_t)size;
}

static bool load_chapters(int index)
{
    struct file_reader reader;
    const char *dot;
    int count;

    if(chapters_index == index)
        return true;
    chapter_count = 0;
    chapters_index = index;
    if(index < 0 || index >= book_count)
        return false;
    dot = strrchr(books[index].path, '.');
    if(dot == NULL ||
       (!text_equal_ignore_case(dot, ".m4b") &&
        !text_equal_ignore_case(dot, ".m4a")))
        return true;
    reader.fd = open(books[index].path, O_RDONLY);
    if(reader.fd < 0)
        return false;
    count = crazypod_audiobook_parse_chapters(
        read_file_at, &reader, (uint32_t)filesize(reader.fd),
        chapters, CRAZYPOD_AUDIOBOOK_CHAPTERS_MAX);
    close(reader.fd);
    chapter_count = count > 0 ? count : 0;
    return count >= 0;
}

int crazypod_audiobook_chapter_count(int index)
{
    load_chapters(index);
    return chapters_index == index ? chapter_count : 0;
}

const struct crazypod_audiobook_chapter *crazypod_audiobook_chapter_get(
    int index, int chapter)
{
    if(crazypod_audiobook_chapter_count(index) <= chapter || chapter < 0)
        return NULL;
    return &chapters[chapter];
}

int crazypod_audiobook_chapter_at(int index, uint32_t position_ms)
{
    int count = crazypod_audiobook_chapter_count(index);
    int i;

    for(i = count - 1; i >= 0; --i)
        if(chapters[i].start_ms <= position_ms)
            return i;
    return count > 0 ? 0 : -1;
}

/* ---- Playback ----------------------------------------------------------- */

static const struct mp3entry *current_entry(void)
{
    if((audio_status() & AUDIO_STATUS_PLAY) == 0)
        return NULL;
    return audio_current_track();
}

static int index_of_path(const char *path)
{
    int i;

    if(path == NULL)
        return -1;
    for(i = 0; i < book_count; ++i)
        if(strcmp(books[i].path, path) == 0)
            return i;
    return -1;
}

int crazypod_audiobooks_current_index(void)
{
    const struct mp3entry *entry = current_entry();

    return entry != NULL ? index_of_path(entry->path) : -1;
}

/* Registers the book with the music layer so every player surface shows
 * its title and author, with the current chapter on the album line. */
static void publish_transient(int index, uint32_t position_ms)
{
    const struct crazypod_audiobook *book = &books[index];
    const struct crazypod_audiobook_chapter *chapter = NULL;
    int chapter_index = -1;
    char album[80];

    if(crazypod_audiobook_chapter_count(index) > 0) {
        chapter_index = crazypod_audiobook_chapter_at(index, position_ms);
        chapter = crazypod_audiobook_chapter_get(index, chapter_index);
    }
    if(live.published_chapter == chapter_index && live.index == index)
        return;
    live.published_chapter = chapter_index;
    if(chapter != NULL)
        snprintf(album, sizeof(album), "%d/%d  %s",
                 chapter_index + 1, chapter_count, chapter->title);
    else
        album[0] = '\0';
    crazypod_music_set_transient_track(
        book->path, book->title, book->author, album);
}

/*
 * After a seek the PCM buffer normally refills to its two-second
 * watermark before sound resumes; the low-latency mode Rockbox uses for
 * scrubbing resumes at a quarter second. Hold it until the seek lands.
 */
static void begin_seek_measure(uint32_t target_ms)
{
    live.seek_started = current_tick != 0 ? current_tick : 1;
    live.seek_target_ms = target_ms;
    pcmbuf_set_low_latency(true);
}

static void end_seek_measure(long now, bool landed)
{
    if(landed)
        live.last_seek_ms = (uint32_t)
            ((now - live.seek_started) * 1000 / HZ);
    live.seek_started = 0;
    pcmbuf_set_low_latency(false);
}

uint32_t crazypod_audiobooks_last_seek_ms(void)
{
    return live.last_seek_ms;
}

bool crazypod_audiobook_is_current(int index)
{
    const struct mp3entry *entry = current_entry();

    return entry != NULL && index >= 0 && index < book_count &&
        strcmp(entry->path, books[index].path) == 0;
}

bool crazypod_audiobook_is_playing(int index)
{
    return crazypod_audiobook_is_current(index) &&
        (audio_status() & AUDIO_STATUS_PAUSE) == 0;
}

uint32_t crazypod_audiobook_position_ms(int index)
{
    const struct mp3entry *entry;

    if(index < 0 || index >= book_count)
        return 0;
    entry = current_entry();
    if(entry != NULL && strcmp(entry->path, books[index].path) == 0)
        return (uint32_t)entry->elapsed;
    return books[index].position_ms;
}

bool crazypod_audiobook_play(int index)
{
    struct crazypod_audiobook *book;
    uint32_t start_ms;

    if(index < 0 || index >= book_count)
        return false;
    book = &books[index];
    if(crazypod_audiobook_is_current(index)) {
        if(audio_status() & AUDIO_STATUS_PAUSE)
            audio_resume();
        return true;
    }
    if(live.index >= 0 && live.index != index)
        remember_position(live.index, live.last_saved_ms, false);
    crazypod_audiobook_probe(index);
    start_ms = book->position_ms;
    if(book->length_ms > 0 &&
       start_ms + FINISHED_MARGIN_MS >= book->length_ms)
        start_ms = 0;
    if(!crazypod_queue_replace_resume(book->path, start_ms))
        return false;
    live.index = index;
    live.was_playing = true;
    live.last_saved_ms = start_ms;
    live.last_save = current_tick;
    live.published_chapter = -2;
    publish_transient(index, start_ms);
    begin_seek_measure(start_ms);
    remember_position(index, start_ms, true);
    return true;
}

bool crazypod_audiobook_toggle(int index)
{
    if(!crazypod_audiobook_is_current(index))
        return crazypod_audiobook_play(index);
    if(audio_status() & AUDIO_STATUS_PAUSE)
        audio_resume();
    else
        audio_pause();
    return true;
}

bool crazypod_audiobook_seek_chapter(int index, int chapter)
{
    const struct crazypod_audiobook_chapter *target;

    if(!crazypod_audiobook_is_current(index)) {
        if(!crazypod_audiobook_play(index))
            return false;
    }
    target = crazypod_audiobook_chapter_get(index, chapter);
    if(target == NULL)
        return false;
    audio_ff_rewind((long)target->start_ms);
    books[index].position_ms = target->start_ms;
    publish_transient(index, target->start_ms);
    begin_seek_measure(target->start_ms);
    return true;
}

bool crazypod_audiobook_skip_chapter(int index, int direction)
{
    uint32_t position;
    uint32_t length;
    int count;

    if(index < 0 || index >= book_count || direction == 0)
        return false;
    if(!crazypod_audiobook_is_current(index))
        return crazypod_audiobook_play(index);
    position = crazypod_audiobook_position_ms(index);
    length = books[index].length_ms;
    count = crazypod_audiobook_chapter_count(index);
    if(count > 0) {
        int current = crazypod_audiobook_chapter_at(index, position);

        /* Going back inside the first seconds of a chapter jumps to the
         * previous one; later it restarts the current chapter. */
        if(direction < 0 && current >= 0 &&
           position > chapters[current].start_ms + 3000u)
            return crazypod_audiobook_seek_chapter(index, current);
        current += direction < 0 ? -1 : 1;
        if(current < 0)
            current = 0;
        if(current >= count)
            return false;
        return crazypod_audiobook_seek_chapter(index, current);
    }
    if(direction < 0)
        position = position > CRAZYPOD_AUDIOBOOK_SKIP_MS
            ? position - CRAZYPOD_AUDIOBOOK_SKIP_MS : 0;
    else {
        position += CRAZYPOD_AUDIOBOOK_SKIP_MS;
        if(length > 0 && position >= length)
            return false;
    }
    audio_ff_rewind((long)position);
    books[index].position_ms = position;
    begin_seek_measure(position);
    return true;
}

void crazypod_audiobooks_tick(long now)
{
    const struct mp3entry *entry;
    int status;
    int index;
    bool playing;
    uint32_t position;

    if(book_count == 0 || !TIME_AFTER(now, live.last_tick + TICK_INTERVAL))
        return;
    live.last_tick = now;
    status = audio_status();
    entry = current_entry();
    index = entry != NULL ? index_of_path(entry->path) : -1;
    if(index < 0 && live.index >= 0 && status & AUDIO_STATUS_PLAY &&
       entry != NULL && live.index < book_count &&
       strcmp(entry->path, books[live.index].path) != 0) {
        /* The queue moved on to something else: keep the book's last
         * known position. */
        remember_position(live.index, live.last_saved_ms, false);
        live.index = -1;
    }
    if(index < 0) {
        if(live.index >= 0 && (status & AUDIO_STATUS_PLAY) == 0) {
            /* Stopped: the last saved position stands. */
            live.index = -1;
            live.was_playing = false;
            live.published_chapter = -2;
            crazypod_music_clear_transient_track();
        }
        return;
    }
    if(live.index != index) {
        live.index = index;
        live.was_playing = false;
        live.last_save = now;
        live.published_chapter = -2;
    }
    playing = (status & AUDIO_STATUS_PAUSE) == 0;
    position = (uint32_t)entry->elapsed;
    if(entry->length > 0)
        books[index].length_ms = (uint32_t)entry->length;
    books[index].position_ms = position;
    publish_transient(index, position);
    if(live.seek_started != 0) {
        uint32_t distance = position > live.seek_target_ms
            ? position - live.seek_target_ms
            : live.seek_target_ms - position;

        if(distance < 3000u)
            end_seek_measure(now, true);
        else if(now - live.seek_started > 30 * HZ)
            end_seek_measure(now, false);
    }
    if(live.was_playing && !playing) {
        /* Paused by the user: this is the moment they expect saved. */
        remember_position(index, position, true);
        live.last_saved_ms = position;
        live.last_save = now;
    }
    else if(playing && TIME_AFTER(now, live.last_save + PERIODIC_SAVE_INTERVAL) &&
            storage_disk_is_active()) {
        remember_position(index, position, true);
        live.last_saved_ms = position;
        live.last_save = now;
    }
    else if(playing)
        live.last_saved_ms = position;
    live.was_playing = playing;
}
