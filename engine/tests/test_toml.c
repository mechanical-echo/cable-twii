#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"

typedef int (*parse_fn)(ctw_db *db, const char *text, ctw_issues *issues);

static char *nested(const char *head, char open, char close, size_t depth, const char *tail)
{
    size_t hl = strlen(head);
    size_t tl = strlen(tail);
    char *buf = malloc(hl + depth * 2u + 2u + tl + 1u);
    size_t n = 0;
    if (!buf) return NULL;
    memcpy(buf, head, hl);
    n += hl;
    memset(buf + n, open, depth);
    n += depth;
    buf[n++] = '1';
    if (close) {
        memset(buf + n, close, depth);
        n += depth;
    }
    buf[n++] = '\n';
    memcpy(buf + n, tail, tl);
    n += tl;
    buf[n] = '\0';
    return buf;
}

static int run(parse_fn fn, const char *text, ctw_issues *is)
{
    ctw_db *db = ctw_db_new();
    int rc;
    if (!db) return -2;
    rc = fn(db, text, is);
    ctw_db_free(db);
    return rc;
}

static enum greatest_test_res assert_too_deep(const ctw_issues *is, int line, const char *file)
{
    ASSERT_EQ(1u, is->count);
    ASSERT_EQ(CTW_LEVEL_ERROR, is->items[0].level);
    ASSERT_EQ(CTW_E_PARSE, is->items[0].code);
    ASSERT_EQ(line, is->items[0].line);
    ASSERT_STR_EQ(file, is->items[0].file);
    ASSERT_STR_EQ("nesting too deep", is->items[0].message);
    PASS();
}

TEST toml_nesting_too_deep_config(void)
{
    ctw_issues is = {0};
    char *text = nested("version = 1\nseed = 1\nx = ", '[', ']', 100, "");
    ASSERT(text);
    ASSERT_EQ(-1, run(ctw_config_parse, text, &is));
    CHECK_CALL(assert_too_deep(&is, 3, "config"));
    ctw_issues_free(&is);
    free(text);
    PASS();
}

TEST toml_nesting_too_deep_library(void)
{
    ctw_issues is = {0};
    char *text = nested("version = 1\n\n\nx = ", '[', ']', 100, "");
    ASSERT(text);
    ASSERT_EQ(-1, run(ctw_library_parse, text, &is));
    CHECK_CALL(assert_too_deep(&is, 4, "library"));
    ctw_issues_free(&is);
    free(text);
    PASS();
}

TEST toml_nesting_too_deep_inline_tables(void)
{
    ctw_issues is = {0};
    size_t depth = 70;
    size_t n = 0;
    char *text = malloc(64 + depth * 8u);
    ASSERT(text);
    n += (size_t)sprintf(text, "version = 1\nseed = 1\nx = ");
    for (size_t i = 0; i < depth; i++) n += (size_t)sprintf(text + n, "{a=");
    text[n++] = '1';
    for (size_t i = 0; i < depth; i++) text[n++] = '}';
    text[n++] = '\n';
    text[n] = '\0';
    ASSERT_EQ(-1, run(ctw_config_parse, text, &is));
    CHECK_CALL(assert_too_deep(&is, 3, "config"));
    ctw_issues_free(&is);
    free(text);
    PASS();
}

TEST toml_nesting_limit_is_64(void)
{
    ctw_issues is = {0};
    char *ok = nested("version = 1\nseed = 1\nx = ", '[', ']', 64, "");
    char *bad = nested("version = 1\nseed = 1\nx = ", '[', ']', 65, "");
    ASSERT(ok && bad);
    ASSERT_EQ(0, run(ctw_config_parse, ok, &is));
    ASSERT_EQ(0u, is.count);
    ASSERT_EQ(-1, run(ctw_config_parse, bad, &is));
    CHECK_CALL(assert_too_deep(&is, 3, "config"));
    ctw_issues_free(&is);
    free(ok);
    free(bad);
    PASS();
}

TEST toml_nesting_ok(void)
{
    static const char text[] =
        "version = 1\n"
        "seed = 1\n"
        "s = \"[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[\"\n"
        "e = \"\\\"[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[\"\n"
        "l = '[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[['\n"
        "m = \"\"\"\n[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{\n[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[\"\"\"\"\n"
        "n = '''\n[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{\n[[[[[[[[[[{{{{{{{{{{[[[[[[[[[['''\n"
        "# [[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[\n"
        "a = [[[[[1]]]]] # [[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{[[[[[[[[[[{{{{{{{{{{\n"
        "t = { a = { b = [ { c = [1] } ] } }\n"
        "[[q]]\n[[q]]\n[r]\n";
    ctw_issues is = {0};
    ASSERT_EQ(0, run(ctw_config_parse, text, &is));
    ASSERT_EQ(0u, is.count);
    ASSERT_EQ(0, run(ctw_library_parse, "version = 1\nx = \"[[[[[[[[[[\" # [[[[[[[[[[\ny = [[[[[1]]]]]\n", &is));
    ASSERT_EQ(0u, is.count);
    ctw_issues_free(&is);
    PASS();
}

TEST toml_nesting_200k_no_crash(void)
{
    ctw_issues is = {0};
    char *closed = nested("version = 1\nseed = 1\nx = ", '[', ']', 200000, "");
    char *open = nested("version = 1\n\nx = ", '[', 0, 200000, "");
    ASSERT(closed && open);
    ASSERT_EQ(-1, run(ctw_config_parse, closed, &is));
    CHECK_CALL(assert_too_deep(&is, 3, "config"));
    ctw_issues_free(&is);
    ASSERT_EQ(-1, run(ctw_library_parse, open, &is));
    CHECK_CALL(assert_too_deep(&is, 3, "library"));
    ctw_issues_free(&is);
    free(closed);
    free(open);
    PASS();
}

TEST toml_db_load_too_deep(void)
{
    char dir[] = "/tmp/ctw-toml-XXXXXX";
    char path[64];
    FILE *f;
    ctw_db *db = NULL;
    ctw_issues is = {0};
    char *text = nested("version = 1\nseed = 1\nx = ", '[', ']', 200000, "");
    ASSERT(text);
    ASSERT(mkdtemp(dir));
    snprintf(path, sizeof path, "%s/config.toml", dir);
    f = fopen(path, "wb");
    ASSERT(f);
    fputs(text, f);
    fclose(f);
    snprintf(path, sizeof path, "%s/library.toml", dir);
    f = fopen(path, "wb");
    ASSERT(f);
    fputs("version = 1\n", f);
    fclose(f);
    ASSERT_EQ(-1, ctw_db_load(dir, &db, &is));
    ASSERT(is.count >= 1);
    ASSERT_STR_EQ("nesting too deep", is.items[is.count - 1].message);
    ctw_db_free(db);
    ctw_issues_free(&is);
    remove(path);
    snprintf(path, sizeof path, "%s/config.toml", dir);
    remove(path);
    rmdir(dir);
    free(text);
    PASS();
}

SUITE(toml_suite)
{
    RUN_TEST(toml_nesting_too_deep_config);
    RUN_TEST(toml_nesting_too_deep_library);
    RUN_TEST(toml_nesting_too_deep_inline_tables);
    RUN_TEST(toml_nesting_limit_is_64);
    RUN_TEST(toml_nesting_ok);
    RUN_TEST(toml_nesting_200k_no_crash);
    RUN_TEST(toml_db_load_too_deep);
}
