#ifndef CTW_SETTINGS_H
#define CTW_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include "aspect.h"

#define SETTINGS_DIR "sd:/apps/cable-twii"

typedef enum { SRC_DRIVE, SRC_SERVER } ctw_source;

typedef struct {
    ctw_source source;
    char server[128];
    asp_mode picture;
} ctw_settings;

int settings_parse(const char *toml_text, ctw_settings *out);
size_t settings_format(const ctw_settings *s, char *out, size_t out_size);
void settings_load(const char *dir, ctw_settings *out);
int settings_save(const char *dir, const ctw_settings *s);
bool settings_equal(const ctw_settings *a, const ctw_settings *b);
bool settings_restarts(const ctw_settings *was, const ctw_settings *now);

#endif
