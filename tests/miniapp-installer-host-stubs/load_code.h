#ifndef TEST_MINIAPP_INSTALLER_LOAD_CODE_H
#define TEST_MINIAPP_INSTALLER_LOAD_CODE_H

struct lc_header {
    unsigned long magic;
    unsigned short target_id;
    unsigned short api_version;
    unsigned char *load_addr;
    unsigned char *end_addr;
};

#endif
