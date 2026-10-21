#include "storage.h"

#include <string.h>
#include <sys/stat.h>
#include <fat.h>

static const char *CANDIDATES[] = { "usb:/cable-twii", "sd:/cable-twii" };
static const size_t CANDIDATE_COUNT = sizeof(CANDIDATES) / sizeof(CANDIDATES[0]);

int st_find_root(char *out, size_t out_size)
{
    if (!fatInitDefault()) return -1;

    for (size_t i = 0; i < CANDIDATE_COUNT; i++) {
        struct stat st;
        if (stat(CANDIDATES[i], &st) != 0) continue;
        if (!S_ISDIR(st.st_mode)) continue;
        size_t len = strlen(CANDIDATES[i]);
        if (len >= out_size) continue;
        memcpy(out, CANDIDATES[i], len + 1);
        return 0;
    }
    return -1;
}
