#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"

#define CTW_DB_MAX_FILE_BYTES (16u * 1024u * 1024u)
#define CTW_DB_PATH_MAX 1024u

typedef enum { READ_OK, READ_MISSING, READ_ERROR, READ_TOO_LARGE, READ_NUL } read_result;

static int join_path(char *out, size_t n, const char *root, const char *name)
{
    size_t len = strlen(root);
    const char *sep = (len > 0 && root[len - 1] == '/') ? "" : "/";
    int w = snprintf(out, n, "%s%s%s", root, sep, name);
    return (w < 0 || (size_t)w >= n) ? -1 : 0;
}

static read_result read_all(const char *path, char **text)
{
    FILE *f = fopen(path, "rb");
    size_t cap = 4096;
    size_t len = 0;
    char *buf;
    read_result rr = READ_OK;
    if (!f) return READ_MISSING;
    buf = malloc(cap);
    while (buf) {
        size_t got;
        if (len + 1 >= cap) {
            char *grown = realloc(buf, cap * 2);
            if (!grown) {
                free(buf);
                buf = NULL;
                rr = READ_ERROR;
                break;
            }
            buf = grown;
            cap *= 2;
        }
        got = fread(buf + len, 1, cap - len - 1, f);
        len += got;
        if (len > CTW_DB_MAX_FILE_BYTES) {
            free(buf);
            buf = NULL;
            rr = READ_TOO_LARGE;
            break;
        }
        if (got == 0) {
            if (ferror(f)) {
                free(buf);
                buf = NULL;
                rr = READ_ERROR;
            }
            break;
        }
    }
    fclose(f);
    if (!buf) return rr == READ_OK ? READ_ERROR : rr;
    buf[len] = '\0';
    if (strlen(buf) != len) {
        free(buf);
        return READ_NUL;
    }
    *text = buf;
    return READ_OK;
}

static int load_file(const char *root, const char *name, const char *label, char **text,
                     ctw_issues *issues)
{
    char path[CTW_DB_PATH_MAX];
    read_result rr;
    if (join_path(path, sizeof path, root, name)) {
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, label,
                       "path too long: %s", name);
        return -1;
    }
    rr = read_all(path, text);
    switch (rr) {
    case READ_OK:
        return 0;
    case READ_MISSING:
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, label,
                       "%s not found", name);
        break;
    case READ_TOO_LARGE:
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, label,
                       "%s is too large", name);
        break;
    case READ_NUL:
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, label,
                       "%s contains a NUL byte", name);
        break;
    default:
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, label,
                       "cannot read %s", name);
        break;
    }
    return -1;
}

int ctw_db_load(const char *root, ctw_db **out, ctw_issues *issues)
{
    ctw_db *db = ctw_db_new();
    char *text = NULL;
    int rc;
    *out = db;
    if (!db) {
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, "library",
                       "out of memory");
        return -1;
    }
    if (load_file(root, "library.toml", "library", &text, issues)) return -1;
    rc = ctw_library_parse(db, text, issues);
    free(text);
    if (rc) return -1;
    text = NULL;
    if (load_file(root, "config.toml", "config", &text, issues)) return -1;
    rc = ctw_config_parse(db, text, issues);
    free(text);
    return rc ? -1 : 0;
}

static int check_one(const char *root, const char *rel, ctw_issues *issues)
{
    char path[CTW_DB_PATH_MAX];
    FILE *f;
    if (join_path(path, sizeof path, root, rel)) {
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_MISSING_FILE, -1, -1, -1, 0, "library",
                       "path too long: %s", rel);
        return -1;
    }
    f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 0;
    }
    ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_MISSING_FILE, -1, -1, -1, 0, "library",
                   "file not found: %s", rel);
    return -1;
}

int ctw_db_check_files(const ctw_db *db, const char *root, ctw_issues *issues)
{
    size_t i, j;
    int bad = 0;
    for (i = 0; i < db->show_count; i++) {
        const ctw_show *s = &db->shows[i];
        for (j = 0; j < s->episode_count; j++)
            if (check_one(root, s->episodes[j].path, issues)) bad = 1;
    }
    for (i = 0; i < db->ad_count; i++)
        if (check_one(root, db->ads[i].path, issues)) bad = 1;
    return bad ? -1 : 0;
}
