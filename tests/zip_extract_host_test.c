#include <stdio.h>

#include "zip.h"

int main(int argc, char **argv)
{
    struct zip *archive;
    int result;

    if(argc != 3) {
        fprintf(stderr, "usage: %s ARCHIVE OUTPUT_DIRECTORY\n", argv[0]);
        return 2;
    }
    archive = zip_open(argv[1], false);
    if(archive == NULL) {
        fprintf(stderr, "zip open failed\n");
        return 1;
    }
    result = zip_extract(archive, argv[2], NULL, NULL);
    zip_close(archive);
    if(result != 0) {
        fprintf(stderr, "zip extract failed: %d\n", result);
        return 1;
    }
    return 0;
}
