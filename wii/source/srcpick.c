#include "srcpick.h"

#include <stdio.h>
#include <string.h>
#include "net/addr.h"

bool sp_try_server(const ctw_settings *s)
{
    srv_addr a;
    return s->source == SRC_SERVER && addr_parse(s->server, &a) == 0;
}

sp_outcome sp_after_server(srv_result r, bool reload, bool drive_found)
{
    if (r == SRV_OK) return SP_SERVER;
    if (reload) return SP_KEEP;
    return drive_found ? SP_DRIVE_WITH_NOTICE : SP_FAIL;
}

const char *sp_reload_notice(srv_result r, bool manual)
{
    if (r == SRV_OK) return "Channels updated";
    return manual ? "Can't reach the server" : NULL;
}

const char *sp_fallback_notice(vol_source source)
{
    return source == VOL_USB ? "Server not reachable, playing from USB" : "Server not reachable, playing from the SD card";
}

void sp_fail_body(srv_result r, const char *address, char *out, size_t out_size)
{
    if (r == SRV_NO_NETWORK) {
        srv_test_message(r, address, "", out, out_size);
        return;
    }
    snprintf(out, out_size, "cable-twii couldn't connect to %s. Check that the server is running and the Wii is online, or change the address in Settings.", address);
}

void sp_connecting(const char *address, char *out, size_t out_size)
{
    snprintf(out, out_size, "Connecting to %s\xE2\x80\xA6", address);
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void trimmed(const char *typed, char *out, size_t out_size)
{
    while (is_space(*typed)) typed++;
    size_t len = strlen(typed);
    while (len > 0 && is_space(typed[len - 1])) len--;
    snprintf(out, out_size, "%.*s", (int)len, typed);
}

sp_line sp_test_start(const char *typed, srv_addr *addr, char *name, size_t name_size, char *out, size_t out_size)
{
    srv_addr parsed;
    if (addr_parse(typed, &parsed) == 0) {
        *addr = parsed;
        addr_format(&parsed, name, name_size);
        sp_connecting(name, out, out_size);
        return SP_LINE_BUSY;
    }
    trimmed(typed, name, name_size);
    if (name[0] == '\0') {
        if (out_size > 0) out[0] = '\0';
        return SP_LINE_NONE;
    }
    srv_test_message(SRV_NO_ANSWER, name, "", out, out_size);
    return SP_LINE_FAIL;
}

sp_line sp_test_result(srv_result r, const char *name, const char *server_name, char *out, size_t out_size)
{
    srv_test_message(r, name, server_name, out, out_size);
    return r == SRV_OK ? SP_LINE_OK : SP_LINE_FAIL;
}

static bool blank(const char *s)
{
    while (is_space(*s)) s++;
    return *s == '\0';
}

sp_save sp_save_plan(const ctw_settings *was, const ctw_settings *now)
{
    if (now->source == SRC_SERVER && !sp_try_server(now)) return blank(now->server) ? SP_SAVE_TYPE : SP_SAVE_BAD;
    if (settings_equal(was, now)) return SP_SAVE_RETURN;
    return settings_restarts(was, now) ? SP_SAVE_SWITCH : SP_SAVE_STORE;
}
