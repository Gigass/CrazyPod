#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../apps/crazypod/crazypod_epub.c"

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
    return remove_tree(directory) && !file_exists(directory);
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
    if(!test_remove_tree(argv[1])) {
        fprintf(stderr, "safe temporary tree cleanup failed\n");
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
