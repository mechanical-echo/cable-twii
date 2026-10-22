#ifndef CTW_STATE_H
#define CTW_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STATE_DEFAULT_VOLUME 80
#define STATE_SAVE_GAP_MS 10000u

typedef struct {
    int last_channel;
    int volume;
} ctw_state;

int state_parse(const char *toml_text, ctw_state *out);
size_t state_format(const ctw_state *s, char *out, size_t out_size);
void state_load(const char *root, ctw_state *out);
void state_save(const char *root, const ctw_state *s);
bool state_replace_after_rename_error(int err);
bool state_changed(const ctw_state *a, const ctw_state *b);
bool state_save_due(bool changed, uint32_t last_save_ms, uint32_t now_ms);
int state_start_channel(int last_channel, const int *numbers, size_t count);

#endif
