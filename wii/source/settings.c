#include "settings.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"
#include "tomlc99/toml.h"

#define SETTINGS_MAX_BYTES 4096
#define FORMAT_BYTES 1024
#define PATH_MAX_BYTES 300

static void set_defaults(ctw_settings *out)
{
    out->source = SRC_DRIVE;
    out->server[0] = '\0';
}

static int read_string(const toml_table_t *t, const char *key, char *out, size_t out_size)
{
    if (!toml_key_exists(t, key)) return 0;
    toml_datum_t d = toml_string_in(t, key);
    if (!d.ok) return -1;
    size_t n = strlen(d.u.s);
    int rc = n < out_size ? 0 : -1;
    if (rc == 0) memcpy(out, d.u.s, n + 1);
    free(d.u.s);
    return rc;
}

int settings_parse(const char *toml_text, ctw_settings *out)
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

    ctw_settings s;
    set_defaults(&s);
    char source[sizeof s.server] = "";
    int rc = 0;
    if (read_string(root, "source", source, sizeof source) != 0) rc = -1;
    if (read_string(root, "server", s.server, sizeof s.server) != 0) rc = -1;
    toml_free(root);
    if (strcmp(source, "server") == 0) s.source = SRC_SERVER;
    if (rc == 0) *out = s;
    return rc;
}

static void escape(const char *in, size_t in_size, char *out)
{
    for (size_t i = 0; i < in_size && in[i] != '\0'; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') *out++ = '\\';
        if (c < 0x20 || c == 0x7F) out += snprintf(out, 7, "\\u%04X", c);
        else *out++ = (char)c;
    }
    *out = '\0';
}

size_t settings_format(const ctw_settings *s, char *out, size_t out_size)
{
    char server[sizeof s->server * 6 + 1];
    escape(s->server, sizeof s->server, server);
    const char *source = s->source == SRC_SERVER ? "server" : "drive";
    int n = snprintf(out, out_size, "source = \"%s\"\nserver = \"%s\"\n", source, server);
    if (n < 0 || (size_t)n >= out_size) {
        if (out_size > 0) out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

static bool join(char *out, const char *dir, const char *name)
{
    int n = snprintf(out, PATH_MAX_BYTES, "%s/%s", dir, name);
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

void settings_load(const char *dir, ctw_settings *out)
{
    set_defaults(out);
    char path[PATH_MAX_BYTES];
    char *text = malloc(SETTINGS_MAX_BYTES + 1);
    if (text == NULL) return;
    bool found = join(path, dir, "settings.toml") && read_text(path, text, SETTINGS_MAX_BYTES + 1);
    if (!found) found = join(path, dir, "settings.toml.tmp") && read_text(path, text, SETTINGS_MAX_BYTES + 1);
    if (found) settings_parse(text, out);
    free(text);
}

int settings_save(const char *dir, const ctw_settings *s)
{
    char text[FORMAT_BYTES];
    char tmp[PATH_MAX_BYTES];
    char dst[PATH_MAX_BYTES];
    size_t len = settings_format(s, text, sizeof text);
    if (len == 0 || !join(tmp, dir, "settings.toml.tmp") || !join(dst, dir, "settings.toml")) return -1;

    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return -1;
    bool ok = fwrite(text, 1, len, f) == len;
    if (fflush(f) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (!ok) {
        remove(tmp);
        return -1;
    }
    errno = 0;
    if (rename(tmp, dst) == 0) return 0;
    if (!state_replace_after_rename_error(errno) || remove(dst) != 0) return -1;
    return rename(tmp, dst) == 0 ? 0 : -1;
}
