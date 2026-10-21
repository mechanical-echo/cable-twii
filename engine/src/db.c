#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"

static void join_path(char *out, size_t n, const char *root, const char *name)
{
    size_t len = strlen(root);
    const char *sep = (len > 0 && root[len - 1] == '/') ? "" : "/";
    snprintf(out, n, "%s%s%s", root, sep, name);
}

static int read_all(const char *path, char **text)
{
    FILE *f = fopen(path, "rb");
    size_t cap = 4096;
    size_t len = 0;
    char *buf;
    if (!f) return 1;
    buf = malloc(cap);
    while (buf) {
        size_t got;
        if (len + 1 >= cap) {
            char *grown = realloc(buf, cap * 2);
            if (!grown) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = grown;
            cap *= 2;
        }
        got = fread(buf + len, 1, cap - len - 1, f);
        len += got;
        if (got == 0) {
            if (ferror(f)) {
                free(buf);
                buf = NULL;
            }
            break;
        }
    }
    fclose(f);
    if (!buf) return -1;
    buf[len] = '\0';
    *text = buf;
    return 0;
}

static int load_file(const char *root, const char *name, const char *label, char **text,
                     ctw_issues *issues)
{
    char path[1024];
    int rc;
    join_path(path, sizeof path, root, name);
    rc = read_all(path, text);
    if (rc == 1) {
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, label,
                       "%s not found", name);
    } else if (rc < 0) {
        ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, label,
                       "cannot read %s", name);
    }
    return rc;
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
    char path[1024];
    FILE *f;
    join_path(path, sizeof path, root, rel);
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
