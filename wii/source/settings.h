#ifndef CTW_SETTINGS_H
#define CTW_SETTINGS_H

#include <stddef.h>

#define SETTINGS_DIR "sd:/apps/cable-twii"

typedef enum { SRC_DRIVE, SRC_SERVER } ctw_source;

typedef struct {
    ctw_source source;
    char server[128];
} ctw_settings;

int settings_parse(const char *toml_text, ctw_settings *out);
size_t settings_format(const ctw_settings *s, char *out, size_t out_size);
void settings_load(const char *dir, ctw_settings *out);
int settings_save(const char *dir, const ctw_settings *s);

#endif
