#include "net/srvlib.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/http.h"
#include "tomlc99/toml.h"

#define INFO_PATH "/wii/v1/info"
#define LIBRARY_PATH "/wii/v1/files/library.toml"
#define CONFIG_PATH "/wii/v1/files/config.toml"
#define INFO_MAX_BYTES 4096
#define FILE_MAX_BYTES (16u * 1024u * 1024u)
#define NAME_MAX_BYTES 63
#define REVISION_MAX_BYTES 63

static bool is_continuation(char c)
{
    return ((unsigned char)c & 0xC0) == 0x80;
}

static int read_string(const toml_table_t *t, const char *key, char **out)
{
    *out = NULL;
    if (!toml_key_exists(t, key)) return 0;
    toml_datum_t d = toml_string_in(t, key);
    if (!d.ok) return -1;
    *out = d.u.s;
    return 0;
}

int srv_info_parse(const char *toml_text, srv_info *out)
{
    if (strncmp(toml_text, "\xEF\xBB\xBF", 3) == 0) toml_text += 3;
    size_t len = strlen(toml_text);
    char *copy = malloc(len + 1);
    if (copy == NULL) return -1;
    memcpy(copy, toml_text, len + 1);
    char errbuf[200];
    toml_table_t *root = toml_parse(copy, errbuf, sizeof errbuf);
    free(copy);
    if (root == NULL) return -1;

    int rc = -1;
    char *name = NULL;
    char *revision = NULL;
    toml_datum_t version = toml_int_in(root, "version");
    if (version.ok && version.u.i == 1 && read_string(root, "name", &name) == 0 &&
        read_string(root, "revision", &revision) == 0 && revision != NULL && strlen(revision) <= REVISION_MAX_BYTES) {
        memset(out, 0, sizeof *out);
        out->version = 1;
        size_t n = name != NULL ? strlen(name) : 0;
        if (n > NAME_MAX_BYTES) {
            n = NAME_MAX_BYTES;
            while (n > 0 && is_continuation(name[n])) n--;
        }
        if (n > 0) memcpy(out->name, name, n);
        strcpy(out->revision, revision);
        rc = 0;
    }
    free(name);
    free(revision);
    toml_free(root);
    return rc;
}

static int get_with_retry(const srv_addr *a, const char *path, size_t max_bytes, uint32_t timeout_ms, http_body *out)
{
    int e = http_get(a, path, max_bytes, timeout_ms, out);
    if (e == HTTP_E_CONNECT || e == HTTP_E_TIMEOUT) {
        http_body_free(out);
        e = http_get(a, path, max_bytes, timeout_ms, out);
    }
    return e;
}

srv_result srv_fetch_info(const srv_addr *a, uint32_t timeout_ms, srv_info *out)
{
    http_body body = { NULL, 0, 0 };
    int e = get_with_retry(a, INFO_PATH, INFO_MAX_BYTES, timeout_ms, &body);
    if (e == HTTP_E_CONNECT || e == HTTP_E_TIMEOUT) {
        http_body_free(&body);
        return SRV_NO_ANSWER;
    }
    srv_result r = SRV_NOT_CABLE_TWII;
    if (e == 0 && srv_info_parse(body.data, out) == 0) r = SRV_OK;
    http_body_free(&body);
    return r;
}

static srv_result fetch_file(const srv_addr *a, const char *path, uint32_t timeout_ms, http_body *body)
{
    int e = http_get(a, path, FILE_MAX_BYTES, timeout_ms, body);
    if (e == 0) return SRV_OK;
    http_body_free(body);
    return e == HTTP_E_CONNECT || e == HTTP_E_TIMEOUT ? SRV_NO_ANSWER : SRV_BAD_LIBRARY;
}

srv_result srv_load_db(const srv_addr *a, uint32_t timeout_ms, ctw_db **out, ctw_issues *issues, srv_info *info)
{
    *out = NULL;
    srv_result r = srv_fetch_info(a, timeout_ms, info);
    if (r != SRV_OK) return r;

    ctw_db *db = ctw_db_new();
    if (db == NULL) return SRV_BAD_LIBRARY;
    http_body body = { NULL, 0, 0 };
    r = fetch_file(a, LIBRARY_PATH, timeout_ms, &body);
    if (r == SRV_OK) {
        int rc = ctw_library_parse(db, body.data, issues);
        http_body_free(&body);
        if (rc != 0) r = SRV_BAD_LIBRARY;
    }
    if (r == SRV_OK) {
        r = fetch_file(a, CONFIG_PATH, timeout_ms, &body);
        if (r == SRV_OK) {
            int rc = ctw_config_parse(db, body.data, issues);
            http_body_free(&body);
            if (rc != 0) r = SRV_BAD_LIBRARY;
        }
    }
    if (r != SRV_OK) {
        ctw_db_free(db);
        return r;
    }
    *out = db;
    return SRV_OK;
}

void srv_test_message(srv_result r, const char *address, const char *name, char *out, size_t out_size)
{
    switch (r) {
    case SRV_OK:
        snprintf(out, out_size, "Connected to \"%s\".", name);
        break;
    case SRV_NO_NETWORK:
        snprintf(out, out_size, "Can't reach the network. Check the Wii's internet settings, or start cable-twii again.");
        break;
    case SRV_NO_ANSWER:
        snprintf(out, out_size, "No answer from %s. Check that the server is running, the address is right and the PC's firewall allows it.", address);
        break;
    case SRV_NOT_CABLE_TWII:
        snprintf(out, out_size, "%s answered, but it isn't a cable-twii server.", address);
        break;
    case SRV_BAD_LIBRARY:
        snprintf(out, out_size, "The server's channels have a problem. Open cable-twii Studio on the server to fix it.");
        break;
    }
}
