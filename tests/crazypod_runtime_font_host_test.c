#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Compile the production resolver, including its real RB12 coverage reader.
 * Only I/O, font storage, mutexes and LVGL types are host doubles. */
#include "../apps/crazypod/crazypod_runtime_font.c"

static struct {
    char path[MAX_PATH];
    int refs;
    struct font font;
} loaded[MAXFONTS];
static int font_limit = MAXFONTS;
static int open_files;
static off_t cursor;
static const char *fail_path;
static enum crazypod_language language = CRAZYPOD_LANGUAGE_CHINESE_SIMPLIFIED;

/* Minimal valid RB12, one fixed-width glyph and no offset/width tables. */
static const uint8_t rb12[38] = {
    'R', 'B', '1', '2', 1, 0, 1, 0, 1, 0, 0, 0,
    32, 0, 0, 0, 32, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0
};

enum crazypod_language crazypod_language_current(void) { return language; }
int test_font_open(const char *path, int flags)
{
    (void)flags;
    if(fail_path != NULL && strcmp(path, fail_path) == 0)
        return -1;
    assert(open_files == 0);
    ++open_files;
    cursor = 0;
    return 0;
}
int test_font_close(int fd)
{ assert(fd == 0 && open_files == 1); --open_files; return 0; }
int filesize(int fd) { assert(fd == 0); return sizeof(rb12); }
off_t test_font_lseek(int fd, off_t offset, int whence)
{
    assert(fd == 0 && whence == SEEK_SET && offset >= 0);
    cursor = offset;
    return cursor;
}
ssize_t test_font_read(int fd, void *buffer, size_t size)
{
    assert(fd == 0 && cursor >= 0 && (size_t)cursor <= sizeof(rb12));
    if(size > sizeof(rb12) - (size_t)cursor)
        size = sizeof(rb12) - (size_t)cursor;
    memcpy(buffer, rb12 + cursor, size);
    cursor += size;
    return size;
}

int font_load_ex(const char *path, size_t buffer_size, int glyphs)
{
    int available = -1;
    (void)buffer_size;
    (void)glyphs;
    /* Same path sharing and slot limit as firmware/font.c. */
    for(int i = 0; i < font_limit; ++i) {
        if(loaded[i].refs && strcmp(loaded[i].path, path) == 0) {
            ++loaded[i].refs;
            return i;
        }
        if(!loaded[i].refs && available < 0)
            available = i;
    }
    if(available < 0)
        return -1;
    strlcpy(loaded[available].path, path, MAX_PATH);
    loaded[available].refs = 1;
    loaded[available].font.height = 12;
    loaded[available].font.ascent = 12;
    return available;
}
void font_unload(int id)
{ assert(id >= 0 && id < MAXFONTS && loaded[id].refs > 0); --loaded[id].refs; }
void font_lock(int id, bool lock)
{ (void)lock; assert(id >= 0 && id < MAXFONTS && loaded[id].refs > 0); }
struct font *font_get(int id) { return &loaded[id].font; }
int font_get_width(struct font *font, ucschar_t ch)
{ (void)font; (void)ch; return 1; }
const unsigned char *font_get_bits(struct font *font, ucschar_t ch)
{ (void)font; (void)ch; return rb12 + 36; }
const unsigned char *utf8decode(const unsigned char *text, ucschar_t *ch)
{ *ch = *text; return text + 1; }

static int loaded_count(void)
{
    int count = 0;
    for(int i = 0; i < MAXFONTS; ++i)
        count += loaded[i].refs > 0;
    return count;
}

static void shell_fonts(void)
{
    assert(crazypod_runtime_font_init());
    assert(crazypod_runtime_font_at_size_weight(10, 700));
    assert(crazypod_runtime_font_at_size(15));
    assert(crazypod_runtime_font_at_size(12));
    assert(crazypod_runtime_font_at_size(10));
    assert(crazypod_runtime_font_at_size_weight(8, 700));
    assert(loaded_count() == 11);
}

static void now_playing_fonts(void)
{
    const lv_font_t *font;
    assert(crazypod_runtime_font_resolve(CRAZYPOD_FONT_FAMILY_SYSTEM,
        12, 400, CRAZYPOD_FONT_STYLE_NORMAL, 16));
    font = crazypod_runtime_font_resolve(CRAZYPOD_FONT_FAMILY_SYSTEM,
        16, 700, CRAZYPOD_FONT_STYLE_NORMAL, 20);
    assert(font && font->fallback);
    assert(font->line_height == 20);
}

int main(void)
{
    assert(MAXFONTS >= CRAZYPOD_RUNTIME_FONT_MAX + CRAZYPOD_ASSET_FONT_MAX + 12);
    assert(MAX_OPEN_FILES >= MAXFONTS + 31);
    assert(DC_NUM_ENTRIES > MAX_OPEN_FILES + MAX_OPEN_DIRS + AUX_FILEOBJS);
    shell_fonts();
    now_playing_fonts();
    assert(loaded_count() == 13);
    for(int i = 0; i < 100; ++i) {
        crazypod_runtime_asset_fonts_reset();
        assert(loaded_count() == 11);
        now_playing_fonts();
        assert(loaded_count() == 13);
    }
    crazypod_runtime_asset_fonts_reset();

    /* Reproduce the old limit; a failed fallback must release its primary. */
    font_limit = 12;
    assert(!crazypod_runtime_font_resolve(CRAZYPOD_FONT_FAMILY_SYSTEM,
        16, 700, CRAZYPOD_FONT_STYLE_NORMAL, 20));
    assert(loaded_count() == 11);
    assert(strstr(crazypod_runtime_font_last_error(), "fallback load failed"));
    font_limit = MAXFONTS;
    crazypod_runtime_font_error_clear();

    fail_path = FONT_DIR "/crazypod-aot/jp-system-400-16.fnt";
    assert(!crazypod_runtime_font_resolve(CRAZYPOD_FONT_FAMILY_SYSTEM,
        16, 700, CRAZYPOD_FONT_STYLE_NORMAL, 20));
    assert(loaded_count() == 11);
    assert(open_files == 0);
    fail_path = NULL;
    crazypod_runtime_font_error_clear();
    now_playing_fonts();
    assert(loaded_count() == 13);
    assert(!crazypod_runtime_font_last_error()[0]);

    /* Fill the advertised semantic/private pools with Rockbox fonts present. */
    for(int i = 0; i < 12; ++i) {
        char path[32];
        snprintf(path, sizeof(path), "/rockbox-%d.fnt", i);
        assert(font_load_ex(path, 0, 256) >= 0);
    }
    for(unsigned size = 6; size <= CRAZYPOD_FONT_MAX_SIZE; ++size) {
        unsigned used = 0;
        for(unsigned i = 0; i < CRAZYPOD_RUNTIME_FONT_MAX; ++i)
            used += runtime_fonts[i].used;
        if(used == CRAZYPOD_RUNTIME_FONT_MAX)
            break;
        assert(crazypod_runtime_font_resolve(CRAZYPOD_FONT_FAMILY_SERIF,
            size, 400, CRAZYPOD_FONT_STYLE_NORMAL, 0));
    }
    for(unsigned i = 0; i < CRAZYPOD_RUNTIME_FONT_MAX; ++i)
        assert(runtime_fonts[i].used);
    for(unsigned i = 0; i < CRAZYPOD_ASSET_FONT_MAX; ++i) {
        char path[32];
        snprintf(path, sizeof(path), "/private-%u.fnt", i);
        const lv_font_t *font = crazypod_runtime_asset_font(path, path);
        assert(font && font->fallback);
    }
    assert(loaded_count() <= MAXFONTS);
    crazypod_runtime_asset_fonts_reset();
    assert(loaded_count() == 11 + 12);
    puts("CrazyPod runtime font capacity/lifecycle tests passed");
    return 0;
}
