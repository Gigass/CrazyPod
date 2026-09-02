#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

const char *crazypod_l10n_text(const char *text)
{
    return text;
}

#include "../apps/crazypod/crazypod_epub.c"
#include "../apps/crazypod/epub/crazypod_epub_layout.h"

static int progress_calls;
static int progress_last;

static void test_progress_callback(int percent, const char *stage,
                                   void *context)
{
    int *saw_stage = context;

    progress_calls++;
    progress_last = percent;
    if(stage != NULL && stage[0] != '\0')
        *saw_stage = 1;
}

static bool test_remove_tree(const char *root)
{
    char directory[MAX_PATH];
    char nested[MAX_PATH];
    char path[MAX_PATH];
    int fd;
    int i;

    if(snprintf(directory, sizeof(directory),
                "%s/.remove-tree-test", root) >=
       (int)sizeof(directory) ||
       snprintf(nested, sizeof(nested),
                "%s/nested", directory) >=
       (int)sizeof(nested))
        return false;
    mkdir(directory);
    mkdir(nested);
    for(i = 0; i < 32; ++i) {
        if(snprintf(path, sizeof(path), "%s/item-%02d", directory, i) >=
           (int)sizeof(path))
            return false;
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd < 0 || !write_exact(fd, "x", 1)) {
            if(fd >= 0)
                close(fd);
            return false;
        }
        close(fd);
    }
    if(snprintf(path, sizeof(path), "%s/item", nested) >=
       (int)sizeof(path))
        return false;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0 || !write_exact(fd, "nested", 6)) {
        if(fd >= 0)
            close(fd);
        return false;
    }
    close(fd);
    return crazypod_epub_extraction_remove_tree(directory) &&
        !file_exists(directory);
}

static bool test_html_layout(const char *root)
{
    static const char source[] =
        "<html><head><title>Hidden title</title>"
        "<style>hidden css</style></head><body>"
        "<h1>Chapter &amp; One</h1>"
        "<p> First   paragraph. </p>"
        "<script>hidden script</script>"
        "<nav>hidden navigation</nav>"
        "<section><p>Second<br/>line.</p>"
        "<ul><li>Alpha</li><li>Beta</li></ul></section>"
        "</body></html>";
    static const char expected[] =
        "Chapter & One\nFirst paragraph.\nSecond\nline.\nAlpha\nBeta";
    char input_path[MAX_PATH];
    char output_path[MAX_PATH];
    char output[256];
    ssize_t count;
    int input;
    int result;

    if(snprintf(input_path, sizeof(input_path),
                "%s/.layout-test.xhtml", root) >=
       (int)sizeof(input_path) ||
       snprintf(output_path, sizeof(output_path),
                "%s/.layout-test.txt", root) >=
       (int)sizeof(output_path))
        return false;
    input = open(input_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(input < 0 || !write_exact(input, source, sizeof(source) - 1)) {
        if(input >= 0)
            close(input);
        return false;
    }
    close(input);
    result = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(result < 0 ||
       !crazypod_epub_html_append_text(input_path, result)) {
        if(result >= 0)
            close(result);
        remove(input_path);
        return false;
    }
    close(result);
    result = open(output_path, O_RDONLY);
    if(result < 0) {
        remove(input_path);
        return false;
    }
    count = read(result, output, sizeof(output) - 1);
    close(result);
    remove(input_path);
    remove(output_path);
    if(count < 0)
        return false;
    output[count] = '\0';
    return strcmp(output, expected) == 0;
}

static bool test_image_callback(const char *source, uint32_t offset,
                                void *context)
{
    char *seen = context;

    (void)offset;
    snprintf(seen, MAX_PATH, "%s", source);
    return true;
}

static bool test_html_images(const char *root)
{
    static const char source[] =
        "<body><p>Before</p><figure><img SRC=\"../img/pic.jpg\""
        " alt=\"illustration\"/></figure><p>After</p></body>";
    char input_path[MAX_PATH];
    char output_path[MAX_PATH];
    char output[256];
    char image_source[MAX_PATH];
    ssize_t count;
    int input;
    int result;

    if(snprintf(input_path, sizeof(input_path),
                "%s/.image-test.xhtml", root) >=
       (int)sizeof(input_path) ||
       snprintf(output_path, sizeof(output_path),
                "%s/.image-test.txt", root) >=
       (int)sizeof(output_path))
        return false;
    input = open(input_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(input < 0 || !write_exact(input, source, sizeof(source) - 1)) {
        if(input >= 0)
            close(input);
        return false;
    }
    close(input);
    result = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(result < 0 || !crazypod_epub_html_append_text_with_images(
           input_path, result, test_image_callback, image_source)) {
        if(result >= 0)
            close(result);
        remove(input_path);
        return false;
    }
    close(result);
    result = open(output_path, O_RDONLY);
    if(result < 0) {
        remove(input_path);
        return false;
    }
    count = read(result, output, sizeof(output) - 1);
    close(result);
    remove(input_path);
    remove(output_path);
    if(count < 0)
        return false;
    output[count] = '\0';
    return strcmp(image_source, "../img/pic.jpg") == 0 &&
        strstr(output, "Before") != NULL &&
        strstr(output, "After") != NULL &&
        strchr(output, CRAZYPOD_EPUB_IMAGE_MARKER) != NULL;
}

static unsigned test_layout_width(
    uint32_t codepoint, uint32_t next_codepoint, void *context)
{
    (void)next_codepoint;
    (void)context;
    return codepoint == ' ' ? 3 : 7;
}

static bool test_layout_pagination(void)
{
    static const unsigned char english[] = "hello world again";
    static const unsigned char cjk[] =
        "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c";
    static const unsigned char gbk[] = "\xd6\xd0\xce\xc4";
    static const unsigned char converted[] =
        "\xe4\xb8\xad\xe6\x96\x87";
    static const unsigned char image[] = {
        CRAZYPOD_EPUB_IMAGE_MARKER, 'x'
    };
    char output[64];
    size_t consumed;

    consumed = crazypod_epub_layout_page(
        english, sizeof(english) - 1,
        english, sizeof(english) - 1, false, false,
        output, sizeof(output), 2, 10);
    if(strcmp(output, "hello\nworld") != 0 || consumed != 12)
        return false;
    consumed = crazypod_epub_layout_page(
        cjk, sizeof(cjk) - 1,
        cjk, sizeof(cjk) - 1, false, false,
        output, sizeof(output), 2, 4);
    if(strcmp(output, "你好\n世界") != 0 || consumed != 12)
        return false;
    consumed = crazypod_epub_layout_page(
        converted, sizeof(converted) - 1,
        gbk, sizeof(gbk) - 1, true, false,
        output, sizeof(output), 1, 4);
    if(strcmp(output, "中文") != 0 || consumed != sizeof(gbk) - 1)
        return false;
    consumed = crazypod_epub_layout_page(
        image, sizeof(image), image, sizeof(image), false, false,
        output, sizeof(output), 2, 10);
    if(output[0] != '\0' || consumed != 1)
        return false;
    consumed = crazypod_epub_layout_page_with_measure(
        (const unsigned char *)"ab cd", 5,
        (const unsigned char *)"ab cd", 5, false, false,
        output, sizeof(output), 2, 16, test_layout_width, NULL);
    return strcmp(output, "ab\ncd") == 0 && consumed == 5;
}

static bool test_named_entities(void)
{
    char output[4];

    return crazypod_epub_html_decode_entity("copy", output) == 2 &&
        (unsigned char)output[0] == 0xc2 &&
        (unsigned char)output[1] == 0xa9 &&
        crazypod_epub_html_decode_entity("rdquo", output) == 3 &&
        (unsigned char)output[0] == 0xe2 &&
        (unsigned char)output[1] == 0x80 &&
        (unsigned char)output[2] == 0x9d;
}

int main(int argc, char **argv)
{
    char opf_path[MAX_PATH];
    char output_path[MAX_PATH];
    int output;
    off_t output_size;
    int i;

    if(argc != 2 && argc != 3) {
        fprintf(stderr,
                "usage: %s EXTRACTED_EPUB_ROOT [EPUB_ARCHIVE]\n",
                argv[0]);
        return 2;
    }
    if(argc == 2 && strcmp(argv[1], "--layout-only") == 0) {
        if(!test_layout_pagination() || !test_named_entities())
            return 1;
        puts("layout tests passed");
        return 0;
    }
    if(!test_remove_tree(argv[1])) {
        fprintf(stderr, "safe temporary tree cleanup failed\n");
        return 1;
    }
    if(!test_html_layout(argv[1])) {
        fprintf(stderr, "HTML reading layout failed\n");
        return 1;
    }
    if(!test_html_images(argv[1])) {
        fprintf(stderr, "HTML inline image handling failed\n");
        return 1;
    }
    if(!test_layout_pagination()) {
        fprintf(stderr, "Unicode layout pagination failed\n");
        return 1;
    }
    if(!test_named_entities()) {
        fprintf(stderr, "HTML named entity decoding failed\n");
        return 1;
    }
    if(snprintf(output_path, sizeof(output_path),
                "%s/.crazypod-test.txt", argv[1]) >=
       (int)sizeof(output_path))
        return 2;
    if(!parse_container(argv[1], opf_path, sizeof(opf_path))) {
        fprintf(stderr, "container parse failed\n");
        return 1;
    }
    if(!build_epub_text(argv[1], opf_path, output_path)) {
        fprintf(stderr, "package/spine parse failed\n");
        return 1;
    }
    output = open(output_path, O_RDONLY);
    if(output < 0)
        return 1;
    output_size = filesize(output);
    close(output);
    remove(output_path);
    if(output_size <= 0 || epub_chapter_count <= 0) {
        fprintf(stderr, "empty reading output\n");
        return 1;
    }

    printf("title=%s\n", epub_title);
    printf("author=%s\n", epub_author);
    printf("cover=%s\n", epub_cover_source);
    printf("chapters=%d\n", epub_chapter_count);
    printf("text_bytes=%lld\n", (long long)output_size);
    for(i = 0; i < epub_chapter_count && i < 5; ++i)
        printf("chapter[%d]=%s\n", i, epub_chapters[i].title);

    if(argc == 3) {
        struct stat source;
        char probe_title[96];
        char probe_author[96];
        char probe_cover[MAX_PATH];
        char cached_text[MAX_PATH];
        char resolved_text[MAX_PATH];
        uint32_t cached_text_size = 0;
        int saw_stage = 0;

        if(stat(argv[2], &source) != 0 ||
           !crazypod_epub_probe(
               argv[2], (uint32_t)source.st_size,
               (uint32_t)source.st_mtime,
               probe_title, sizeof(probe_title),
               probe_author, sizeof(probe_author),
               probe_cover, sizeof(probe_cover)) ||
           probe_title[0] == '\0' ||
           (probe_cover[0] != '\0' &&
            !file_exists(probe_cover))) {
            fprintf(stderr, "selective metadata probe failed\n");
            return 1;
        }
        printf("probe_title=%s\n", probe_title);
        printf("probe_author=%s\n", probe_author);
        printf("probe_cover=%s\n", probe_cover);
        crazypod_epub_set_progress_callback(
            test_progress_callback, &saw_stage);
        if(!crazypod_epub_prepare(
               argv[2], (uint32_t)source.st_size,
               (uint32_t)source.st_mtime,
               cached_text, sizeof(cached_text),
               &cached_text_size) ||
           cached_text_size == 0 ||
           !file_exists(cached_text) ||
           progress_calls == 0 ||
           progress_last != 100 ||
           !saw_stage) {
            fprintf(stderr, "selective reading cache failed\n");
            return 1;
        }
        crazypod_epub_text_path(
            argv[2], resolved_text, sizeof(resolved_text));
        if(strcmp(cached_text, resolved_text) != 0) {
            fprintf(stderr, "cached text path mismatch\n");
            return 1;
        }
        progress_calls = 0;
        progress_last = 0;
        saw_stage = 0;
        if(!crazypod_epub_prepare(
               argv[2], (uint32_t)source.st_size,
               (uint32_t)source.st_mtime,
               cached_text, sizeof(cached_text),
               &cached_text_size) ||
           progress_calls == 0 ||
           progress_last != 100 ||
           !saw_stage) {
            fprintf(stderr, "cached reading progress failed\n");
            return 1;
        }
        crazypod_epub_set_progress_callback(NULL, NULL);
        printf("cached_text_bytes=%lu\n",
               (unsigned long)cached_text_size);
    }
    return 0;
}
