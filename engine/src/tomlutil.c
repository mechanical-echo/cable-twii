#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"

#define CTW_TOML_MAX_DEPTH 64

void ctw_pctx_init(ctw_pctx *pc, ctw_db *db, ctw_issues *issues, const char *file,
                   ctw_code type_code)
{
    pc->db = db;
    pc->issues = issues;
    pc->file = file;
    pc->type_code = type_code;
    pc->channel = -1;
    pc->slot = -1;
    pc->item = -1;
    pc->bad = 0;
}

void ctw_pctx_error(ctw_pctx *pc, ctw_code code, int line, const char *fmt, ...)
{
    char msg[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    ctw_issues_add(pc->issues, CTW_LEVEL_ERROR, code, pc->channel, pc->slot, pc->item, line,
                   pc->file, "%s", msg);
    pc->bad++;
}

void ctw_pctx_oom(ctw_pctx *pc)
{
    ctw_pctx_error(pc, CTW_E_PARSE, 0, "out of memory");
}

void ctw_pctx_not_table(ctw_pctx *pc, const char *where)
{
    ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: must be a table", where);
}

void ctw_pctx_wrong_top_level(ctw_pctx *pc, const char *key)
{
    ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s must be an array of tables", key);
}

int ctw_get_int(ctw_pctx *pc, const toml_table_t *t, const char *key, int64_t lo, int64_t hi,
                int64_t *out, const char *where)
{
    toml_datum_t d;
    if (!toml_key_exists(t, key)) {
        ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: missing %s", where, key);
        return -1;
    }
    d = toml_int_in(t, key);
    if (!d.ok) {
        ctw_pctx_error(pc, pc->type_code, 0, "%s: %s must be an integer from %lld to %lld",
                       where, key, (long long)lo, (long long)hi);
        return -1;
    }
    if (d.u.i < lo || d.u.i > hi) {
        ctw_pctx_error(pc, CTW_E_BAD_VALUE, 0, "%s: %s must be an integer from %lld to %lld",
                       where, key, (long long)lo, (long long)hi);
        return -1;
    }
    *out = d.u.i;
    return 0;
}

int ctw_get_str(ctw_pctx *pc, const toml_table_t *t, const char *key, int required,
                const char **out, const char *where)
{
    toml_datum_t d;
    if (!toml_key_exists(t, key)) {
        if (!required) {
            *out = "";
            return 0;
        }
        ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: missing %s", where, key);
        return -1;
    }
    d = toml_string_in(t, key);
    if (!d.ok) {
        ctw_pctx_error(pc, pc->type_code, 0, "%s: %s must be a string", where, key);
        return -1;
    }
    *out = ctw_arena_strdup(&pc->db->arena, d.u.s);
    free(d.u.s);
    if (!*out) {
        ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: out of memory", where);
        return -1;
    }
    return 0;
}

int ctw_get_bool(ctw_pctx *pc, const toml_table_t *t, const char *key, int *out,
                 const char *where)
{
    toml_datum_t d;
    if (!toml_key_exists(t, key)) {
        ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: missing %s", where, key);
        return -1;
    }
    d = toml_bool_in(t, key);
    if (!d.ok) {
        ctw_pctx_error(pc, pc->type_code, 0, "%s: %s must be true or false", where, key);
        return -1;
    }
    *out = d.u.b;
    return 0;
}

static int parse_error_line(const char *errbuf)
{
    int line = 0;
    if (sscanf(errbuf, "line %d:", &line) == 1 && line > 0) return line;
    return 0;
}

static const char *skip_string(const char *p, char quote)
{
    while (*p != '\0' && *p != '\n') {
        if (quote == '"' && *p == '\\' && p[1] != '\0' && p[1] != '\n') {
            p += 2;
            continue;
        }
        if (*p == quote) return p + 1;
        p++;
    }
    return p;
}

static const char *skip_multiline(const char *p, char quote, int *line)
{
    while (*p != '\0') {
        if (quote == '"' && *p == '\\' && p[1] != '\0') {
            if (p[1] == '\n') (*line)++;
            p += 2;
            continue;
        }
        if (p[0] == quote && p[1] == quote && p[2] == quote) {
            p += 3;
            if (*p == quote) p++;
            if (*p == quote) p++;
            return p;
        }
        if (*p == '\n') (*line)++;
        p++;
    }
    return p;
}

static int nesting_too_deep(const char *p, int *out_line)
{
    int depth = 0;
    int line = 1;
    while (*p != '\0') {
        char c = *p;
        if (c == '\n') {
            line++;
            p++;
        } else if (c == '#') {
            while (*p != '\0' && *p != '\n') p++;
        } else if (c == '"' || c == '\'') {
            if (p[1] == c && p[2] == c) p = skip_multiline(p + 3, c, &line);
            else p = skip_string(p + 1, c);
        } else if (c == '[' || c == '{') {
            if (++depth > CTW_TOML_MAX_DEPTH) {
                *out_line = line;
                return 1;
            }
            p++;
        } else {
            if ((c == ']' || c == '}') && depth > 0) depth--;
            p++;
        }
    }
    return 0;
}

toml_table_t *ctw_toml_open(ctw_pctx *pc, const char *text)
{
    char errbuf[200];
    char *copy;
    toml_table_t *root;
    int line = 0;
    if (strncmp(text, "\xEF\xBB\xBF", 3) == 0) text += 3;
    if (nesting_too_deep(text, &line)) {
        ctw_pctx_error(pc, CTW_E_PARSE, line, "nesting too deep");
        return NULL;
    }
    copy = malloc(strlen(text) + 1);
    if (!copy) {
        ctw_pctx_oom(pc);
        return NULL;
    }
    strcpy(copy, text);
    root = toml_parse(copy, errbuf, sizeof errbuf);
    free(copy);
    if (!root) ctw_pctx_error(pc, CTW_E_PARSE, parse_error_line(errbuf), "%s", errbuf);
    return root;
}

void ctw_check_version(ctw_pctx *pc, const toml_table_t *root)
{
    toml_datum_t d;
    if (!toml_key_exists(root, "version")) {
        ctw_pctx_error(pc, CTW_E_PARSE, 0, "missing version");
        return;
    }
    d = toml_int_in(root, "version");
    if (!d.ok || d.u.i != 1) ctw_pctx_error(pc, CTW_E_VERSION, 0, "version must be 1");
}
