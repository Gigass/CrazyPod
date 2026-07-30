#ifndef CRAZYPOD_L10N_H
#define CRAZYPOD_L10N_H

#include <stdbool.h>

enum crazypod_language {
    CRAZYPOD_LANGUAGE_ENGLISH = 0,
    CRAZYPOD_LANGUAGE_CHINESE_SIMPLIFIED,
    CRAZYPOD_LANGUAGE_CHINESE_TRADITIONAL,
    CRAZYPOD_LANGUAGE_JAPANESE,
    CRAZYPOD_LANGUAGE_KOREAN,
    CRAZYPOD_LANGUAGE_GERMAN,
    CRAZYPOD_LANGUAGE_FRENCH,
    CRAZYPOD_LANGUAGE_SPANISH,
    CRAZYPOD_LANGUAGE_PORTUGUESE_BRAZIL,
    CRAZYPOD_LANGUAGE_COUNT
};

#define CRAZYPOD_L10N_MARKER "\x1f"
#define CP_TR(text) CRAZYPOD_L10N_MARKER text
#define CP_FMT(text) crazypod_l10n_text(CP_TR(text))
#define CP_LV_LABEL_SET_TEXT(label, text) \
    crazypod_l10n_label_set_text((label), (text))

bool crazypod_language_valid(int language);
enum crazypod_language crazypod_language_current(void);
void crazypod_language_set(enum crazypod_language language);
const char *crazypod_language_native_name(enum crazypod_language language);
const char *crazypod_language_code(enum crazypod_language language);
const char *crazypod_l10n_text(const char *text);
void crazypod_l10n_label_set_text(void *label, const char *text);

#endif
