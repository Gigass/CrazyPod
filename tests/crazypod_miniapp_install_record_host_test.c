#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "crazypod_cpk_reader.h"
#include "crazypod_miniapp_install_record.h"
#include "crazypod_miniapps.h"

int main(int argc, char **argv)
{
    struct cpk_reader reader;
    struct install_record record;
    char path[512];
    unsigned char value;
    int fd;

    assert(argc == 3);
    assert(crazypod_cpk_open(argv[1], &reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(crazypod_cpk_validate_local_headers(&reader) ==
           CRAZYPOD_MINIAPP_OK);
    assert(crazypod_miniapp_install_record_write(
        argv[2], &reader, 9));
    assert(crazypod_miniapp_install_record_read(argv[2], &record));
    assert(record.version_code == 9);
    assert(record.files[CPK_ICON].size == 102454);
    crazypod_cpk_close(&reader);

    snprintf(path, sizeof(path), "%s/.install.bin", argv[2]);
    fd = open(path, O_RDWR);
    assert(fd >= 0);
    assert(read(fd, &value, 1) == 1);
    value ^= 1;
    assert(lseek(fd, 0, SEEK_SET) == 0);
    assert(write(fd, &value, 1) == 1);
    close(fd);
    assert(!crazypod_miniapp_install_record_read(argv[2], &record));
    return 0;
}
