#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <string.h>

#include "crazypod_l10n.h"

struct crazypod_l10n_entry {
    const char *source;
    const char *translations[CRAZYPOD_LANGUAGE_COUNT - 1];
};

#include "crazypod_l10n_data.inc"

static enum crazypod_language current_language =
    CRAZYPOD_LANGUAGE_ENGLISH;

static const char *const native_names[CRAZYPOD_LANGUAGE_COUNT] = {
    "English",
    "简体中文",
    "繁體中文",
    "日本語",
    "한국어",
    "Deutsch",
    "Français",
    "Español",
    "Português (Brasil)",
};

static const char *const language_codes[CRAZYPOD_LANGUAGE_COUNT] = {
    "en", "zh-Hans", "zh-Hant", "ja", "ko",
    "de", "fr", "es", "pt-BR",
};

bool crazypod_language_valid(int language)
{
    return language >= CRAZYPOD_LANGUAGE_ENGLISH &&
        language < CRAZYPOD_LANGUAGE_COUNT;
}

enum crazypod_language crazypod_language_current(void)
{
    return current_language;
}

void crazypod_language_set(enum crazypod_language language)
{
    if(crazypod_language_valid(language))
        current_language = language;
}

const char *crazypod_language_native_name(enum crazypod_language language)
{
    return crazypod_language_valid(language)
        ? native_names[language] : native_names[CRAZYPOD_LANGUAGE_ENGLISH];
}

const char *crazypod_language_code(enum crazypod_language language)
{
    return crazypod_language_valid(language)
        ? language_codes[language] : language_codes[CRAZYPOD_LANGUAGE_ENGLISH];
}

const char *crazypod_l10n_text(const char *text)
{
    const char *source;
    int low;
    int high;

    if(text == NULL || text[0] != CRAZYPOD_L10N_MARKER[0])
        return text;

    source = text + 1;
    if(current_language == CRAZYPOD_LANGUAGE_ENGLISH)
        return source;

    low = 0;
    high = CRAZYPOD_L10N_ENTRY_COUNT - 1;
    while(low <= high) {
        int middle = low + (high - low) / 2;
        int comparison = strcmp(source,
                                crazypod_l10n_entries[middle].source);

        if(comparison < 0)
            high = middle - 1;
        else if(comparison > 0)
            low = middle + 1;
        else
            return crazypod_l10n_entries[middle]
                .translations[current_language - 1];
    }

    /* A tagged key missing from the generated table is a build defect. */
    return "[MISSING L10N]";
}

#endif
