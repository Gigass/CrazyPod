#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "rbunicode.h"

#include "crazypod_books.h"
#include "crazypod_epub.h"

#define BOOKS_DIRECTORY "/Books"
#define BOOKS_STATE_DIRECTORY "/.crazypod/books"
#define BOOKS_STATE_PATH BOOKS_STATE_DIRECTORY "/library.bin"
#define BOOKS_STATE_TEMP BOOKS_STATE_DIRECTORY "/library.tmp"
#define BOOKS_MAGIC 0x4350424bu
#define BOOKS_VERSION 1u
#define BOOKS_MAX 64
#define BOOKS_SCAN_DEPTH 6
#define BOOK_PAGE_INPUT_SIZE 1536
#define BOOK_ENCODING_SAMPLE_SIZE 4096

enum book_text_encoding {
    BOOK_TEXT_ENCODING_UNKNOWN,
    BOOK_TEXT_ENCODING_UTF8,
    BOOK_TEXT_ENCODING_GBK,
};

struct book_progress_disk {
    uint32_t path_hash;
    uint32_t progress;
    uint32_t bookmark;
    uint32_t favorite;
    uint32_t recent_sequence;
};

struct books_state_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t count;
    uint32_t next_sequence;
    uint32_t font_size;
    uint32_t theme;
    struct book_progress_disk entries[BOOKS_MAX];
    uint32_t checksum;
};

static struct crazypod_book books[BOOKS_MAX];
static uint32_t recent_sequences[BOOKS_MAX];
static unsigned char book_text_encodings[BOOKS_MAX];
static bool book_utf8_bom[BOOKS_MAX];
static int book_count;
static bool books_scan_loaded;
static bool books_scan_dirty;
static struct books_state_disk persisted;
static unsigned char page_input[BOOK_PAGE_INPUT_SIZE];
static unsigned char page_utf8[BOOK_PAGE_INPUT_SIZE * 3 + 1];
static unsigned char encoding_sample[BOOK_ENCODING_SAMPLE_SIZE];

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

static uint32_t state_checksum(const struct books_state_disk *state)
{
    static const uint32_t zero = 0;
    uint32_t hash = hash_bytes(
        2166136261u, state,
        offsetof(struct books_state_disk, checksum));

    return hash_bytes(hash, &zero, sizeof(zero));
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

static bool utf8_valid(const unsigned char *data, size_t count,
                       bool allow_truncated_tail)
{
    size_t i = 0;

    while(i < count) {
        unsigned char first = data[i];
        size_t needed;

        if(first < 0x80) {
            ++i;
            continue;
        }
        if(first >= 0xc2 && first <= 0xdf)
            needed = 2;
        else if(first >= 0xe0 && first <= 0xef)
            needed = 3;
        else if(first >= 0xf0 && first <= 0xf4)
            needed = 4;
        else
            return false;
        if(i + needed > count)
            return allow_truncated_tail;
        if((data[i + 1] & 0xc0) != 0x80)
            return false;
        if(first == 0xe0 && data[i + 1] < 0xa0)
            return false;
        if(first == 0xed && data[i + 1] >= 0xa0)
            return false;
        if(first == 0xf0 && data[i + 1] < 0x90)
            return false;
        if(first == 0xf4 && data[i + 1] >= 0x90)
            return false;
        if(needed >= 3 && (data[i + 2] & 0xc0) != 0x80)
            return false;
        if(needed == 4 && (data[i + 3] & 0xc0) != 0x80)
            return false;
        i += needed;
    }
    return true;
}

static bool gbk_lead(unsigned char value)
{
    return value >= 0x81 && value <= 0xfe;
}

static bool gbk_trail(unsigned char value)
{
    return value >= 0x40 && value <= 0xfe && value != 0x7f;
}

static bool gbk_valid(const unsigned char *data, size_t count,
                      bool allow_truncated_tail)
{
    size_t i = 0;

    while(i < count) {
        if(data[i] < 0x80) {
            ++i;
            continue;
        }
        if(!gbk_lead(data[i]))
            return false;
        if(i + 1 >= count)
            return allow_truncated_tail;
        if(!gbk_trail(data[i + 1]))
            return false;
        i += 2;
    }
    return true;
}

static enum book_text_encoding detect_text_encoding(int fd, bool *has_bom)
{
    size_t sample_count = 0;
    bool found_high_byte = false;
    bool at_file_start = true;

    *has_bom = false;
    if(lseek(fd, 0, SEEK_SET) < 0)
        return BOOK_TEXT_ENCODING_UTF8;
    while(sample_count < sizeof(encoding_sample)) {
        ssize_t count = read(fd, encoding_sample + sample_count,
                             sizeof(encoding_sample) - sample_count);
        size_t i;

        if(count <= 0)
            break;
        if(at_file_start && count >= 3 &&
           encoding_sample[0] == 0xef &&
           encoding_sample[1] == 0xbb &&
           encoding_sample[2] == 0xbf) {
            *has_bom = true;
            return BOOK_TEXT_ENCODING_UTF8;
        }
        at_file_start = false;
        if(!found_high_byte) {
            for(i = 0; i < (size_t)count; ++i) {
                if(encoding_sample[i] >= 0x80)
                    break;
            }
            if(i == (size_t)count) {
                sample_count = 0;
                continue;
            }
            memmove(encoding_sample, encoding_sample + i,
                    (size_t)count - i);
            sample_count = (size_t)count - i;
            found_high_byte = true;
        } else {
            sample_count += (size_t)count;
        }
    }
    if(!found_high_byte)
        return BOOK_TEXT_ENCODING_UTF8;
    if(!utf8_valid(encoding_sample, sample_count, true) &&
       gbk_valid(encoding_sample, sample_count, true))
        return BOOK_TEXT_ENCODING_GBK;
    return BOOK_TEXT_ENCODING_UTF8;
}

static uint32_t align_gbk_offset(int fd, uint32_t target)
{
    uint32_t boundary = 0;
    uint32_t cursor = target;
    uint32_t position;

    while(cursor > 0) {
        uint32_t start = cursor > sizeof(page_input)
            ? cursor - sizeof(page_input) : 0;
        size_t wanted = cursor - start;
        ssize_t count;
        size_t i;

        if(lseek(fd, start, SEEK_SET) < 0)
            return 0;
        count = read(fd, page_input, wanted);
        if(count <= 0)
            return 0;
        for(i = (size_t)count; i > 0; --i) {
            if(page_input[i - 1] == '\n' ||
               page_input[i - 1] == '\r') {
                boundary = start + (uint32_t)i;
                cursor = 0;
                break;
            }
        }
        if(cursor != 0)
            cursor = start;
    }

    position = boundary;
    while(position < target) {
        size_t wanted = target - position;
        ssize_t count;
        size_t i = 0;

        if(wanted > sizeof(page_input))
            wanted = sizeof(page_input);
        if(lseek(fd, position, SEEK_SET) < 0)
            return boundary;
        count = read(fd, page_input, wanted);
        if(count <= 0)
            return boundary;
        while(i < (size_t)count && position < target) {
            uint32_t character_start = position;

            if(page_input[i] < 0x80) {
                ++i;
                ++position;
                continue;
            }
            if(i + 1 >= (size_t)count) {
                if(position + 1 >= target)
                    return character_start;
                break;
            }
            if(gbk_lead(page_input[i]) &&
               gbk_trail(page_input[i + 1])) {
                if(position + 2 > target)
                    return character_start;
                i += 2;
                position += 2;
            } else {
                ++i;
                ++position;
            }
        }
    }
    return position;
}

static size_t utf8_character_bytes(const unsigned char *data,
                                   size_t count, size_t offset)
{
    unsigned char value = data[offset];
    size_t bytes = 1;

    if((value & 0xe0) == 0xc0)
        bytes = 2;
    else if((value & 0xf0) == 0xe0)
        bytes = 3;
    else if((value & 0xf8) == 0xf0)
        bytes = 4;
    return offset + bytes <= count ? bytes : 0;
}

static size_t complete_gbk_bytes(const unsigned char *data, size_t count)
{
    size_t i = 0;

    while(i < count) {
        if(data[i] < 0x80 || !gbk_lead(data[i])) {
            ++i;
        } else if(i + 1 < count && gbk_trail(data[i + 1])) {
            i += 2;
        } else {
            break;
        }
    }
    return i;
}

static size_t paginate_text(const unsigned char *utf8, size_t utf8_count,
                            const unsigned char *source, size_t source_count,
                            bool source_is_gbk, bool markdown,
                            char *text, size_t size,
                            int max_lines, int max_line_units)
{
    size_t input = 0;
    size_t source_input = 0;
    size_t output = 0;
    int visual_lines = 1;
    int line_units = 0;

    while(input < utf8_count && source_input < source_count &&
          output + 1 < size) {
        unsigned char value = utf8[input];
        size_t character_bytes =
            utf8_character_bytes(utf8, utf8_count, input);
        size_t source_bytes = character_bytes;
        int units;

        if(character_bytes == 0)
            break;
        if(source_is_gbk) {
            source_bytes = source[source_input] < 0x80 ? 1 : 2;
            if(source_input + source_bytes > source_count)
                break;
        }
        if(markdown && (value == '#' || value == '*' || value == '`')) {
            input += character_bytes;
            source_input += source_bytes;
            continue;
        }
        if(value == '\r') {
            input += character_bytes;
            source_input += source_bytes;
            continue;
        }
        if(value == '\n') {
            if(visual_lines >= max_lines)
                break;
            text[output++] = '\n';
            input += character_bytes;
            source_input += source_bytes;
            ++visual_lines;
            line_units = 0;
            continue;
        }
        if(output + character_bytes >= size)
            break;
        units = value < 0x80 ? 1 : 2;
        if(line_units + units > max_line_units) {
            if(visual_lines >= max_lines)
                break;
            ++visual_lines;
            line_units = 0;
        }
        memcpy(text + output, utf8 + input, character_bytes);
        output += character_bytes;
        input += character_bytes;
        source_input += source_bytes;
        line_units += units;
    }
    text[output] = '\0';
    return source_input;
}

static bool state_save(void)
{
    int fd;
    bool success;

    mkdir("/.crazypod");
    mkdir(BOOKS_STATE_DIRECTORY);
    persisted.magic = BOOKS_MAGIC;
    persisted.version = BOOKS_VERSION;
    persisted.size = sizeof(persisted);
    persisted.checksum = state_checksum(&persisted);
    fd = open(BOOKS_STATE_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, &persisted, sizeof(persisted));
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    if(!success || rename(BOOKS_STATE_TEMP, BOOKS_STATE_PATH) < 0) {
        remove(BOOKS_STATE_TEMP);
        return false;
    }
    return true;
}

static const char *extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot != NULL ? dot : "";
}

static bool text_equal_ignore_case(const char *left, const char *right)
{
    while(*left != '\0' && *right != '\0') {
        char a = *left++;
        char b = *right++;
        if(a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if(b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if(a != b)
            return false;
    }
    return *left == *right;
}

static bool book_format(const char *path, enum crazypod_book_format *format)
{
    const char *suffix = extension(path);

    if(text_equal_ignore_case(suffix, ".txt")) {
        *format = CRAZYPOD_BOOK_TXT;
        return true;
    }
    if(text_equal_ignore_case(suffix, ".md") ||
       text_equal_ignore_case(suffix, ".markdown")) {
        *format = CRAZYPOD_BOOK_MARKDOWN;
        return true;
    }
    if(text_equal_ignore_case(suffix, ".epub")) {
        *format = CRAZYPOD_BOOK_EPUB;
        return true;
    }
    return false;
}

static bool append_path(char *output, size_t size, const char *directory,
                        const char *name)
{
    int result = snprintf(output, size, "%s/%s", directory, name);
    return result > 0 && (size_t)result < size;
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

static const struct book_progress_disk *saved_progress(uint32_t hash)
{
    uint32_t i;
    for(i = 0; i < persisted.count; ++i) {
        if(persisted.entries[i].path_hash == hash)
            return &persisted.entries[i];
    }
    return NULL;
}

static void add_book(const char *path, const struct dirinfo *info,
                     enum crazypod_book_format format)
{
    const struct book_progress_disk *saved;
    struct crazypod_book *book;
    uint32_t hash;

    if(book_count >= BOOKS_MAX)
        return;
    book = &books[book_count];
    memset(book, 0, sizeof(*book));
    snprintf(book->path, sizeof(book->path), "%s", path);
    title_from_path(book->title, sizeof(book->title), path);
    book->format = format;
    book->size = info->size > 0 ? (uint32_t)info->size : 0;
    book->content_size =
        format == CRAZYPOD_BOOK_EPUB ? 0 : book->size;
    book->mtime = info->mtime > 0 ? (uint32_t)info->mtime : 0;
    hash = path_hash(path);
    saved = saved_progress(hash);
    if(saved != NULL) {
        book->progress = format == CRAZYPOD_BOOK_EPUB ||
                         saved->progress < book->content_size
            ? saved->progress : 0;
        book->bookmark = format == CRAZYPOD_BOOK_EPUB ||
                         saved->bookmark < book->content_size
            ? saved->bookmark : 0;
        book->favorite = saved->favorite != 0;
        recent_sequences[book_count] = saved->recent_sequence;
    }
    ++book_count;
}

static void scan_directory(const char *path, int depth)
{
    DIR *directory;
    struct DIRENT *entry;

    if(depth > BOOKS_SCAN_DEPTH || book_count >= BOOKS_MAX)
        return;
    directory = opendir(path);
    if(directory == NULL)
        return;
    while((entry = readdir(directory)) != NULL &&
          book_count < BOOKS_MAX) {
        struct dirinfo info;
        enum crazypod_book_format format;
        char child[MAX_PATH];

        if(entry->d_name[0] == '.' ||
           !append_path(child, sizeof(child), path, entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY)
            scan_directory(child, depth + 1);
        else if(book_format(child, &format))
            add_book(child, &info, format);
    }
    closedir(directory);
}

void crazypod_books_init(void)
{
    static struct books_state_disk loaded;
    int fd;

    mkdir(BOOKS_DIRECTORY);
    memset(books, 0, sizeof(books));
    memset(recent_sequences, 0, sizeof(recent_sequences));
    memset(book_text_encodings, 0, sizeof(book_text_encodings));
    memset(book_utf8_bom, 0, sizeof(book_utf8_bom));
    book_count = 0;
    books_scan_loaded = false;
    books_scan_dirty = true;
    memset(&persisted, 0, sizeof(persisted));
    persisted.magic = BOOKS_MAGIC;
    persisted.version = BOOKS_VERSION;
    persisted.size = sizeof(persisted);
    persisted.next_sequence = 1;
    persisted.font_size = 1;
    fd = open(BOOKS_STATE_PATH, O_RDONLY);
    if(fd >= 0) {
        if(read_exact(fd, &loaded, sizeof(loaded)) &&
           loaded.magic == BOOKS_MAGIC &&
           loaded.version == BOOKS_VERSION &&
           loaded.size == sizeof(loaded) &&
           loaded.count <= BOOKS_MAX &&
           loaded.checksum == state_checksum(&loaded)) {
            persisted = loaded;
            if(persisted.next_sequence == 0)
                persisted.next_sequence = 1;
        }
        close(fd);
    }
}

void crazypod_books_scan(void)
{
    memset(books, 0, sizeof(books));
    memset(recent_sequences, 0, sizeof(recent_sequences));
    memset(book_text_encodings, 0, sizeof(book_text_encodings));
    memset(book_utf8_bom, 0, sizeof(book_utf8_bom));
    book_count = 0;
    scan_directory(BOOKS_DIRECTORY, 0);
    books_scan_loaded = true;
    books_scan_dirty = false;
}

bool crazypod_books_scan_needed(void)
{
    return !books_scan_loaded || books_scan_dirty;
}

void crazypod_books_invalidate_scan(void)
{
    books_scan_dirty = true;
}

int crazypod_books_count(void)
{
    return book_count;
}

const struct crazypod_book *crazypod_book_get(int index)
{
    return index >= 0 && index < book_count ? &books[index] : NULL;
}

int crazypod_book_index(const struct crazypod_book *book)
{
    int i;

    if(book == NULL)
        return -1;
    for(i = 0; i < book_count; ++i) {
        if(book == &books[i])
            return i;
    }
    return -1;
}

int crazypod_books_recent_index(void)
{
    uint32_t best = 0;
    int result = -1;
    int i;

    for(i = 0; i < book_count; ++i) {
        if(recent_sequences[i] > best) {
            best = recent_sequences[i];
            result = i;
        }
    }
    return result;
}

int crazypod_books_recent_count(void)
{
    int count = 0;
    int i;
    for(i = 0; i < book_count; ++i) {
        if(recent_sequences[i] > 0)
            ++count;
    }
    return count;
}

int crazypod_books_recent_at(int position)
{
    uint32_t ceiling = UINT32_MAX;
    int rank;
    int result = -1;

    for(rank = 0; rank <= position; ++rank) {
        uint32_t best = 0;
        int i;
        result = -1;
        for(i = 0; i < book_count; ++i) {
            if(recent_sequences[i] < ceiling &&
               recent_sequences[i] > best) {
                best = recent_sequences[i];
                result = i;
            }
        }
        if(result < 0)
            return -1;
        ceiling = best;
    }
    return result;
}

int crazypod_books_favorite_count(void)
{
    int count = 0;
    int i;
    for(i = 0; i < book_count; ++i) {
        if(books[i].favorite)
            ++count;
    }
    return count;
}

int crazypod_books_favorite_at(int position)
{
    int visible = 0;
    int i;
    for(i = 0; i < book_count; ++i) {
        if(books[i].favorite && visible++ == position)
            return i;
    }
    return -1;
}

static struct book_progress_disk *progress_entry(int index)
{
    uint32_t hash;
    uint32_t i;

    if(index < 0 || index >= book_count)
        return NULL;
    hash = path_hash(books[index].path);
    for(i = 0; i < persisted.count; ++i) {
        if(persisted.entries[i].path_hash == hash)
            return &persisted.entries[i];
    }
    if(persisted.count >= BOOKS_MAX)
        return NULL;
    i = persisted.count++;
    memset(&persisted.entries[i], 0, sizeof(persisted.entries[i]));
    persisted.entries[i].path_hash = hash;
    return &persisted.entries[i];
}

bool crazypod_book_read_page(int index, uint32_t offset,
                             char *text, size_t size,
                             uint32_t *next_offset)
{
    struct crazypod_book *book =
        index >= 0 && index < book_count ? &books[index] : NULL;
    const char *source_path;
    static char epub_path[MAX_PATH];
    const unsigned char *utf8 = page_input;
    size_t utf8_count;
    size_t source_count;
    size_t consumed;
    bool source_is_gbk = false;
    ssize_t count;
    int max_lines;
    int max_line_units;
    int fd;

    if(text == NULL || size == 0 || book == NULL)
        return false;
    max_lines = crazypod_books_font_size() == 0 ? 11 :
                crazypod_books_font_size() == 2 ? 6 : 9;
    max_line_units = crazypod_books_font_size() == 0 ? 46 :
                     crazypod_books_font_size() == 2 ? 32 : 41;
    source_path = book->path;
    if(book->format == CRAZYPOD_BOOK_EPUB) {
        if(!crazypod_epub_prepare(
               book->path, book->size, book->mtime,
               epub_path, sizeof(epub_path), &book->content_size))
            return false;
        source_path = epub_path;
    }
    if(offset >= book->content_size)
        offset = 0;
    fd = open(source_path, O_RDONLY);
    if(fd < 0)
        return false;
    if(book->format != CRAZYPOD_BOOK_EPUB) {
        if(book_text_encodings[index] == BOOK_TEXT_ENCODING_UNKNOWN) {
            book_text_encodings[index] =
                detect_text_encoding(fd, &book_utf8_bom[index]);
            if(book_text_encodings[index] == BOOK_TEXT_ENCODING_GBK &&
               offset > 0)
                offset = align_gbk_offset(fd, offset);
        }
        source_is_gbk =
            book_text_encodings[index] == BOOK_TEXT_ENCODING_GBK;
        if(!source_is_gbk && book_utf8_bom[index] && offset < 3)
            offset = 3;
    }
    if(lseek(fd, offset, SEEK_SET) < 0) {
        if(fd >= 0)
            close(fd);
        return false;
    }
    count = read(fd, page_input, sizeof(page_input));
    close(fd);
    if(count <= 0)
        return false;
    source_count = (size_t)count;
    if(source_is_gbk) {
        unsigned char *end;

        source_count = complete_gbk_bytes(page_input, source_count);
        if(source_count == 0)
            return false;
        end = iso_decode_ex(page_input, page_utf8, GB_2312,
                            (int)source_count,
                            (int)(sizeof(page_utf8) - 1));
        utf8 = page_utf8;
        utf8_count = (size_t)(end - page_utf8);
    } else {
        utf8_count = source_count;
    }
    consumed = paginate_text(
        utf8, utf8_count, page_input, source_count, source_is_gbk,
        book->format == CRAZYPOD_BOOK_MARKDOWN, text, size,
        max_lines, max_line_units);
    if(next_offset != NULL)
        *next_offset = offset + (uint32_t)consumed;
    return true;
}

bool crazypod_book_set_progress(int index, uint32_t offset)
{
    struct book_progress_disk *entry = progress_entry(index);
    if(entry == NULL)
        return false;
    if(books[index].content_size > 0 &&
       offset >= books[index].content_size)
        offset = books[index].content_size - 1;
    books[index].progress = offset;
    recent_sequences[index] = persisted.next_sequence++;
    entry->progress = offset;
    entry->recent_sequence = recent_sequences[index];
    return state_save();
}

bool crazypod_book_toggle_bookmark(int index, uint32_t offset)
{
    struct book_progress_disk *entry = progress_entry(index);
    if(entry == NULL)
        return false;
    books[index].bookmark =
        books[index].bookmark == offset ? 0 : offset;
    entry->bookmark = books[index].bookmark;
    return state_save();
}

bool crazypod_book_toggle_favorite(int index)
{
    struct book_progress_disk *entry = progress_entry(index);
    if(entry == NULL)
        return false;
    books[index].favorite = !books[index].favorite;
    entry->favorite = books[index].favorite ? 1 : 0;
    return state_save();
}

static bool prepare_epub_book(int index)
{
    struct crazypod_book *book =
        index >= 0 && index < book_count ? &books[index] : NULL;
    char text_path[MAX_PATH];
    char title[96];

    if(book == NULL || book->format != CRAZYPOD_BOOK_EPUB)
        return false;
    if(!crazypod_epub_prepare(
           book->path, book->size, book->mtime,
           text_path, sizeof(text_path), &book->content_size))
        return false;
    title[0] = '\0';
    crazypod_epub_book_info(
        title, sizeof(title),
        book->author, sizeof(book->author),
        book->cover_path, sizeof(book->cover_path));
    if(title[0] != '\0')
        snprintf(book->title, sizeof(book->title), "%s", title);
    book->details_loaded = true;
    return true;
}

bool crazypod_book_probe(int index)
{
    struct crazypod_book *book =
        index >= 0 && index < book_count ? &books[index] : NULL;
    char title[96];

    if(book == NULL)
        return false;
    if(book->details_loaded)
        return true;
    if(book->format != CRAZYPOD_BOOK_EPUB) {
        book->details_loaded = true;
        return true;
    }
    title[0] = '\0';
    if(!crazypod_epub_probe(
           book->path, book->size, book->mtime,
           title, sizeof(title),
           book->author, sizeof(book->author),
           book->cover_path, sizeof(book->cover_path)))
        return false;
    if(title[0] != '\0')
        snprintf(book->title, sizeof(book->title), "%s", title);
    book->details_loaded = true;
    return true;
}

bool crazypod_book_prepare(int index)
{
    return crazypod_book_prepare_with_progress(index, NULL, NULL);
}

struct book_prepare_progress_bridge {
    crazypod_book_progress_callback callback;
    void *context;
};

static void book_prepare_epub_progress(
    int percent, const char *stage, void *context)
{
    struct book_prepare_progress_bridge *bridge = context;

    if(bridge == NULL || bridge->callback == NULL)
        return;
    if(percent < 0)
        percent = 0;
    if(percent > 100)
        percent = 100;
    bridge->callback(percent * 92 / 100, stage, bridge->context);
}

bool crazypod_book_prepare_with_progress(
    int index, crazypod_book_progress_callback callback, void *context)
{
    struct crazypod_book *book =
        index >= 0 && index < book_count ? &books[index] : NULL;
    struct book_prepare_progress_bridge bridge = {
        .callback = callback,
        .context = context,
    };
    bool result;

    if(book == NULL)
        return false;
    if(book->format != CRAZYPOD_BOOK_EPUB)
        return crazypod_book_probe(index);
    crazypod_epub_set_progress_callback(
        callback != NULL ? book_prepare_epub_progress : NULL,
        callback != NULL ? &bridge : NULL);
    result = prepare_epub_book(index);
    crazypod_epub_set_progress_callback(NULL, NULL);
    return result;
}

int crazypod_book_chapter_count(int index)
{
    const struct crazypod_book *book = crazypod_book_get(index);

    if(book == NULL)
        return 0;
    if(book->format != CRAZYPOD_BOOK_EPUB)
        return 1;
    return prepare_epub_book(index)
        ? crazypod_epub_chapter_count() : 0;
}

bool crazypod_book_chapter_get(int index, int chapter,
                               char *title, size_t title_size,
                               uint32_t *offset)
{
    const struct crazypod_book *book = crazypod_book_get(index);

    if(book == NULL || chapter < 0)
        return false;
    if(book->format != CRAZYPOD_BOOK_EPUB) {
        if(chapter != 0)
            return false;
        if(title != NULL && title_size > 0)
            snprintf(title, title_size, "%s", book->title);
        if(offset != NULL)
            *offset = 0;
        return true;
    }
    return prepare_epub_book(index) &&
           crazypod_epub_chapter_get(
               chapter, title, title_size, offset);
}

bool crazypod_book_delete(int index)
{
    char path[MAX_PATH];
    uint32_t hash;
    uint32_t i;

    if(index < 0 || index >= book_count ||
       strncmp(books[index].path, BOOKS_DIRECTORY "/",
               sizeof(BOOKS_DIRECTORY)) != 0)
        return false;
    snprintf(path, sizeof(path), "%s", books[index].path);
    hash = path_hash(path);
    if(remove(path) < 0)
        return false;
    if(books[index].format == CRAZYPOD_BOOK_EPUB)
        crazypod_epub_remove_cache(path);
    for(i = 0; i < persisted.count; ++i) {
        if(persisted.entries[i].path_hash == hash) {
            uint32_t tail = persisted.count - i - 1;
            if(tail > 0)
                memmove(&persisted.entries[i],
                        &persisted.entries[i + 1],
                        tail * sizeof(persisted.entries[0]));
            --persisted.count;
            memset(&persisted.entries[persisted.count], 0,
                   sizeof(persisted.entries[0]));
            break;
        }
    }
    crazypod_books_scan();
    return state_save();
}

int crazypod_books_font_size(void)
{
    return persisted.font_size <= 2 ? (int)persisted.font_size : 1;
}

int crazypod_books_theme(void)
{
    return persisted.theme <= 3 ? (int)persisted.theme : 0;
}

bool crazypod_books_set_font_size(int value)
{
    if(value < 0 || value > 2)
        return false;
    persisted.font_size = (uint32_t)value;
    return state_save();
}

bool crazypod_books_set_theme(int value)
{
    if(value < 0 || value > 3)
        return false;
    persisted.theme = (uint32_t)value;
    return state_save();
}

#endif
