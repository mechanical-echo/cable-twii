#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tomlc99/toml.h"

#define STATE_MAX_BYTES 4096
#define PATH_MAX_BYTES 300

static void set_defaults(ctw_state *out)
{
    out->last_channel = 0;
    out->volume = STATE_DEFAULT_VOLUME;
}

static int read_int(const toml_table_t *t, const char *key, int64_t lo, int64_t hi, int *out)
{
    if (!toml_key_exists(t, key)) return 0;
    toml_datum_t d = toml_int_in(t, key);
    if (!d.ok) return -1;
    int64_t v = d.u.i;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    *out = (int)v;
    return 0;
}

int state_parse(const char *toml_text, ctw_state *out)
{
    set_defaults(out);
    if (strncmp(toml_text, "\xEF\xBB\xBF", 3) == 0) toml_text += 3;
    size_t len = strlen(toml_text);
    char *copy = malloc(len + 1);
    if (copy == NULL) return -1;
    memcpy(copy, toml_text, len + 1);
    char errbuf[200];
    toml_table_t *root = toml_parse(copy, errbuf, sizeof errbuf);
    free(copy);
    if (root == NULL) return -1;

    ctw_state s;
    set_defaults(&s);
    int rc = 0;
    if (read_int(root, "last_channel", INT32_MIN, INT32_MAX, &s.last_channel) != 0) rc = -1;
    if (read_int(root, "volume", 0, 100, &s.volume) != 0) rc = -1;
    toml_free(root);
    if (rc == 0) *out = s;
    return rc;
}

size_t state_format(const ctw_state *s, char *out, size_t out_size)
{
    int n = snprintf(out, out_size, "last_channel = %d\nvolume = %d\n", s->last_channel, s->volume);
    if (n < 0 || (size_t)n >= out_size) {
        if (out_size > 0) out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

static bool join(char *out, const char *root, const char *name)
{
    int n = snprintf(out, PATH_MAX_BYTES, "%s/%s", root, name);
    return n > 0 && n < PATH_MAX_BYTES;
}

static bool read_text(const char *path, char *buf, size_t buf_size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;
    size_t n = fread(buf, 1, buf_size, f);
    bool ok = !ferror(f) && n < buf_size;
    fclose(f);
    if (!ok) return false;
    buf[n] = '\0';
    return memchr(buf, '\0', n) == NULL;
}

void state_load(const char *root, ctw_state *out)
{
    set_defaults(out);
    char path[PATH_MAX_BYTES];
    char *text = malloc(STATE_MAX_BYTES + 1);
    if (text == NULL) return;
    bool found = join(path, root, "state.toml") && read_text(path, text, STATE_MAX_BYTES + 1);
    if (!found) found = join(path, root, "state.toml.tmp") && read_text(path, text, STATE_MAX_BYTES + 1);
    if (found) state_parse(text, out);
    free(text);
}

void state_save(const char *root, const ctw_state *s)
{
    char text[64];
    char tmp[PATH_MAX_BYTES];
    char dst[PATH_MAX_BYTES];
    size_t len = state_format(s, text, sizeof text);
    if (len == 0 || !join(tmp, root, "state.toml.tmp") || !join(dst, root, "state.toml")) return;

    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return;
    bool ok = fwrite(text, 1, len, f) == len;
    if (fflush(f) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (!ok) {
        remove(tmp);
        return;
    }
    if (rename(tmp, dst) == 0) return;
    if (remove(dst) != 0) return;
    rename(tmp, dst);
}

bool state_changed(const ctw_state *a, const ctw_state *b)
{
    return a->last_channel != b->last_channel || a->volume != b->volume;
}

bool state_save_due(bool changed, uint32_t last_save_ms, uint32_t now_ms)
{
    return changed && now_ms - last_save_ms >= STATE_SAVE_GAP_MS;
}

int state_start_channel(int last_channel, const int *numbers, size_t count)
{
    if (count == 0) return 0;
    for (size_t i = 0; i < count; i++) {
        if (numbers[i] == last_channel) return last_channel;
    }
    return numbers[0];
}
