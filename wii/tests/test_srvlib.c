#include <stdio.h>
#include <string.h>
#include "greatest/greatest.h"
#include "net/http.h"
#include "net/srvlib.h"
#include "testserver.h"

#define WAIT_MS 2000
#define FIXTURE "../../engine/tests/fixtures/root_basic"
#define FILE_BUF 65536
#define INFO_TEXT "version = 1\nname = \"Home PC\"\nrevision = \"1700000000000-1700000000001\"\n"

static char library_text[FILE_BUF];
static char config_text[FILE_BUF];
static char library_head[160];
static char config_head[160];
static char info_head[160];
static char html_head[160];
static ts_reply replies[4];
static ctw_db *db;
static ctw_issues issues;

static void teardown(void *udata)
{
    (void)udata;
    ts_stop();
    ctw_db_free(db);
    db = NULL;
    ctw_issues_free(&issues);
    memset(&issues, 0, sizeof issues);
}

static size_t read_fixture(const char *name, char *out, size_t out_size)
{
    char path[256];
    snprintf(path, sizeof path, FIXTURE "/%s", name);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    size_t n = fread(out, 1, out_size - 1, f);
    fclose(f);
    out[n] = '\0';
    return n;
}

static void ok_head(char *out, size_t out_size, size_t len)
{
    snprintf(out, out_size, "HTTP/1.1 200 OK\r\nContent-Type: application/toml\r\nContent-Length: %zu\r\n\r\n", len);
}

static srv_addr local(uint16_t port)
{
    srv_addr a = { "127.0.0.1", port };
    return a;
}

static bool load_fixtures(size_t *library_len, size_t *config_len)
{
    *library_len = read_fixture("library.toml", library_text, sizeof library_text);
    *config_len = read_fixture("config.toml", config_text, sizeof config_text);
    return *library_len > 0 && *config_len > 0;
}

TEST info_parse_reads_fields(void)
{
    srv_info info;
    ASSERT_EQ(0, srv_info_parse(INFO_TEXT, &info));
    ASSERT_EQ(1, info.version);
    ASSERT_STR_EQ("Home PC", info.name);
    ASSERT_STR_EQ("1700000000000-1700000000001", info.revision);
    PASS();
}

TEST info_parse_rejects_bad_documents(void)
{
    srv_info info;
    ASSERT_EQ(-1, srv_info_parse("version = 2\nname = \"x\"\nrevision = \"1\"\n", &info));
    ASSERT_EQ(-1, srv_info_parse("name = \"x\"\nrevision = \"1\"\n", &info));
    ASSERT_EQ(-1, srv_info_parse("version = 1\nname = \"x\"\n", &info));
    ASSERT_EQ(-1, srv_info_parse("<html>", &info));
    char long_revision[200];
    snprintf(long_revision, sizeof long_revision, "version = 1\nrevision = \"%0100d\"\n", 7);
    ASSERT_EQ(-1, srv_info_parse(long_revision, &info));
    PASS();
}

TEST info_parse_missing_name_is_empty(void)
{
    srv_info info;
    ASSERT_EQ(0, srv_info_parse("version = 1\nrevision = \"1-2\"\n", &info));
    ASSERT_STR_EQ("", info.name);
    PASS();
}

TEST info_parse_truncates_name_at_character_boundary(void)
{
    char text[400] = "version = 1\nrevision = \"1\"\nname = \"";
    size_t n = strlen(text);
    for (int i = 0; i < 31; i++) {
        memcpy(text + n, "\xC3\xA9", 2);
        n += 2;
    }
    memcpy(text + n, "\xC3\xA9\xC3\xA9\"\n", 6);
    srv_info info;
    ASSERT_EQ(0, srv_info_parse(text, &info));
    ASSERT_EQ(62, (int)strlen(info.name));
    PASS();
}

TEST load_db_parses_fixture(void)
{
    size_t library_len, config_len;
    ASSERT(load_fixtures(&library_len, &config_len));
    ok_head(info_head, sizeof info_head, strlen(INFO_TEXT));
    ok_head(library_head, sizeof library_head, library_len);
    ok_head(config_head, sizeof config_head, config_len);
    replies[0] = (ts_reply){ info_head, INFO_TEXT, strlen(INFO_TEXT), false };
    replies[1] = (ts_reply){ library_head, library_text, library_len, false };
    replies[2] = (ts_reply){ config_head, config_text, config_len, false };
    uint16_t port = ts_start(replies, 3);
    ASSERT(port != 0);
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_OK, srv_load_db(&a, WAIT_MS, &db, &issues, &info));
    ASSERT(db != NULL);
    ASSERT_STR_EQ("Home PC", info.name);

    ctw_db *expected = NULL;
    ctw_issues expected_issues = { 0 };
    ASSERT_EQ(0, ctw_db_load(FIXTURE, &expected, &expected_issues));
    ASSERT_EQ(ctw_db_channel_count(expected), ctw_db_channel_count(db));
    ASSERT(ctw_db_channel_count(db) > 0);
    ctw_db_free(expected);
    ctw_issues_free(&expected_issues);
    ASSERT_EQ(0, strncmp(ts_request(0), "GET /wii/v1/info ", 17));
    ASSERT_EQ(0, strncmp(ts_request(1), "GET /wii/v1/files/library.toml ", 31));
    ASSERT_EQ(0, strncmp(ts_request(2), "GET /wii/v1/files/config.toml ", 30));
    PASS();
}

TEST info_wrong_version_is_not_cable_twii(void)
{
    static const char text[] = "version = 2\nname = \"x\"\nrevision = \"1\"\n";
    ok_head(info_head, sizeof info_head, strlen(text));
    replies[0] = (ts_reply){ info_head, text, strlen(text), false };
    uint16_t port = ts_start(replies, 1);
    ASSERT(port != 0);
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_NOT_CABLE_TWII, srv_fetch_info(&a, WAIT_MS, &info));
    PASS();
}

TEST html_page_is_not_cable_twii(void)
{
    static const char text[] = "<html><body>It works</body></html>";
    snprintf(html_head, sizeof html_head, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n", strlen(text));
    replies[0] = (ts_reply){ html_head, text, strlen(text), false };
    uint16_t port = ts_start(replies, 1);
    ASSERT(port != 0);
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_NOT_CABLE_TWII, srv_fetch_info(&a, WAIT_MS, &info));
    PASS();
}

TEST not_found_info_is_not_cable_twii(void)
{
    static const char head[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    replies[0] = (ts_reply){ head, "", 0, false };
    uint16_t port = ts_start(replies, 1);
    ASSERT(port != 0);
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_NOT_CABLE_TWII, srv_fetch_info(&a, WAIT_MS, &info));
    PASS();
}

TEST bad_config_is_bad_library(void)
{
    static const char bad_config[] = "version = 1\n[[channel]]\nnumber = \"one\"\n";
    size_t library_len, config_len;
    ASSERT(load_fixtures(&library_len, &config_len));
    ok_head(info_head, sizeof info_head, strlen(INFO_TEXT));
    ok_head(library_head, sizeof library_head, library_len);
    ok_head(config_head, sizeof config_head, strlen(bad_config));
    replies[0] = (ts_reply){ info_head, INFO_TEXT, strlen(INFO_TEXT), false };
    replies[1] = (ts_reply){ library_head, library_text, library_len, false };
    replies[2] = (ts_reply){ config_head, bad_config, strlen(bad_config), false };
    uint16_t port = ts_start(replies, 3);
    ASSERT(port != 0);
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_BAD_LIBRARY, srv_load_db(&a, WAIT_MS, &db, &issues, &info));
    ASSERT_EQ(NULL, db);
    ASSERT(ctw_issues_has_errors(&issues));
    PASS();
}

TEST missing_library_is_bad_library(void)
{
    static const char missing[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    ok_head(info_head, sizeof info_head, strlen(INFO_TEXT));
    replies[0] = (ts_reply){ info_head, INFO_TEXT, strlen(INFO_TEXT), false };
    replies[1] = (ts_reply){ missing, "", 0, false };
    uint16_t port = ts_start(replies, 2);
    ASSERT(port != 0);
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_BAD_LIBRARY, srv_load_db(&a, WAIT_MS, &db, &issues, &info));
    ASSERT_EQ(NULL, db);
    PASS();
}

TEST no_server_is_no_answer(void)
{
    uint16_t port = ts_start(replies, 0);
    ASSERT(port != 0);
    ts_stop();
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_NO_ANSWER, srv_fetch_info(&a, WAIT_MS, &info));
    ASSERT_EQ(SRV_NO_ANSWER, srv_load_db(&a, WAIT_MS, &db, &issues, &info));
    ASSERT_EQ(NULL, db);
    PASS();
}

TEST info_retries_once_after_dropped_connection(void)
{
    ok_head(info_head, sizeof info_head, strlen(INFO_TEXT));
    replies[0] = (ts_reply){ "", "", 0, true };
    replies[1] = (ts_reply){ info_head, INFO_TEXT, strlen(INFO_TEXT), false };
    uint16_t port = ts_start(replies, 2);
    ASSERT(port != 0);
    srv_addr a = local(port);
    srv_info info;
    ASSERT_EQ(SRV_OK, srv_fetch_info(&a, WAIT_MS, &info));
    ASSERT_STR_EQ("Home PC", info.name);
    ASSERT(ts_conn(1) > ts_conn(0));
    PASS();
}

TEST test_message_texts(void)
{
    char out[256];
    srv_test_message(SRV_OK, "192.168.1.20:4747", "Home PC", out, sizeof out);
    ASSERT_STR_EQ("Connected to \"Home PC\".", out);
    srv_test_message(SRV_NO_ANSWER, "192.168.1.20:4747", "Home PC", out, sizeof out);
    ASSERT_STR_EQ("No answer from 192.168.1.20:4747. Check that the server is running, the address is right and the PC's firewall allows it.", out);
    srv_test_message(SRV_NOT_CABLE_TWII, "192.168.1.20:4747", "Home PC", out, sizeof out);
    ASSERT_STR_EQ("192.168.1.20:4747 answered, but it isn't a cable-twii server.", out);
    srv_test_message(SRV_BAD_LIBRARY, "192.168.1.20:4747", "Home PC", out, sizeof out);
    ASSERT_STR_EQ("The server's channels have a problem. Open cable-twii Studio on the server to fix it.", out);
    srv_test_message(SRV_NO_NETWORK, "192.168.1.20:4747", "Home PC", out, sizeof out);
    ASSERT_STR_EQ("Can't reach the network. Check the Wii's internet settings, or start cable-twii again.", out);
    PASS();
}

SUITE(srvlib_suite)
{
    SET_TEARDOWN(teardown, NULL);
    RUN_TEST(info_parse_reads_fields);
    RUN_TEST(info_parse_rejects_bad_documents);
    RUN_TEST(info_parse_missing_name_is_empty);
    RUN_TEST(info_parse_truncates_name_at_character_boundary);
    RUN_TEST(load_db_parses_fixture);
    RUN_TEST(info_wrong_version_is_not_cable_twii);
    RUN_TEST(html_page_is_not_cable_twii);
    RUN_TEST(not_found_info_is_not_cable_twii);
    RUN_TEST(bad_config_is_bad_library);
    RUN_TEST(missing_library_is_bad_library);
    RUN_TEST(no_server_is_no_answer);
    RUN_TEST(info_retries_once_after_dropped_connection);
    RUN_TEST(test_message_texts);
    SET_TEARDOWN(NULL, NULL);
}
