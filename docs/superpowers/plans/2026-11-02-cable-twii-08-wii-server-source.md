# cable-twii Wii Server Source Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Wii app can play from a cable-twii server on the home network, as an alternative to the drive:
- the source and address are set in a settings file on the SD card or on a new Settings screen with an on-screen keyboard;
- it falls back to the drive when the server can't be reached;
- it rides out weak Wi-Fi and picks up library changes.

**Architecture:**
- Pure, host-tested modules hold every decision: `settings`, `net/addr`, `net/httpparse`, `net/http` (over a socket shim), `net/srvlib`, `media/netring`, `keyboard`, `settings_layout`, plus small pure helpers in `volume`, `playback` and `srcpick`.
- Wii-only glue, checked in Dolphin, connects them to the app:
  - the SD config mount in `storage.c`
  - the loader thread
  - the network source in `media/netsrc.c`
  - the player's callback path
  - the screens
- In server mode `a->root` becomes `http://host:port/wii/v1/files`, so every existing `"%s/%s"` media and icon path turns into a URL. `pl_open` and the icon loader branch on `http://`.
- Drive mode keeps today's code paths: `fopen` and `plm_create_with_file`.

**Tech Stack:** C11 on devkitPPC/libogc/GRRLIB, libogc sockets (`if_config`, `net_*`), zlib (already linked), tomlc99, pl_mpeg buffer callbacks, greatest host tests with POSIX sockets and pthreads, Dolphin with `make -C wii DEBUG_SCRIPT=1`.

**Spec:** `docs/superpowers/specs/2026-11-02-server-source-design.md` (sections 2, 6, 7, 9). Server side: plan 07, `/wii/v1/` contract in `docs/formats.md`. Streaming numbers: `docs/decisions/0004-server-streaming.md` (plan 06).

## Global Constraints

- **Follows decision 0004 (accepted 2026-11-18):** server libraries use the lower-bitrate profile (plan 07 Task 9); the client streams continuously (one open-ended `Range: bytes=<pos>-` request per position) into a 2 MiB ring with a read-ahead target of about 10 s; getting online retries from scratch; no seek index.
- **Drive mode is unchanged.**
  - With `source = "drive"`, or no settings file, every existing flow behaves as today.
  - All existing host tests pass, and the drive checklist items still pass in Dolphin.
- **Writes:** only the root partition (as today), and `settings.toml`, `state.toml` and their `.tmp` files in `sd:/apps/cable-twii/`. Those are allowed only when the SD card has no `wbfs/` or `games/` folder. USB partitions without `/cable-twii/` are never written.
- **Wii interface (plan 07):**
  - `GET /wii/v1/info` returns TOML with `version = 1`, `name`, `revision`.
  - `GET /wii/v1/files/<percent-encoded path>` supports `Range`, and gzip for `.toml` when asked.
  - Every response has a `Content-Length`.
  - Plain HTTP, default port 4747.
- **Settings file:** `sd:/apps/cable-twii/settings.toml`. Keys `source` (`"drive"` or `"server"`, default `"drive"`) and `server` (the address as typed, at most 127 bytes).
- **Timeouts:**
  - network up: up to 3 attempts of `net_init`, each capped at 10000 ms, with `net_deinit` between attempts
  - first connection to the server: one immediate retry
  - each server request while loading: 5000 ms
  - stream read: 5000 ms before one reconnect
  - weak-signal note after 1000 ms waiting
  - rejoin when more than 2000 ms behind the schedule after a stall
- **Copy, exactly:**
  - Connecting: `Connecting to <address>…`
  - Fallback notice: `Server not reachable, playing from USB` / `Server not reachable, playing from the SD card`
  - Error title: `Can't reach the server`
  - Error body: `cable-twii couldn't connect to <address>. Check that the server is running and the Wii is online, or change the address in Settings.`
  - Weak signal: `Weak signal`
  - Reload notice: `Channels updated`
  - No network: `Can't reach the network. Check the Wii's internet settings, or start cable-twii again.`
  - No SD card: `No SD card. These settings last until cable-twii closes.`
  - Test results:
    - `Connected to "<name>".`
    - `No answer from <address>. Check that the server is running, the address is right and the PC's firewall allows it.`
    - `<address> answered, but it isn't a cable-twii server.`
    - `The server's channels have a problem. Open cable-twii Studio on the server to fix it.`
- **Code style:**
  - No comments in source, no trailing whitespace.
  - Module prefixes: `settings_`, `addr_`/`url_`, `hp_`, `http_`/`hs_`, `sk_`, `srv_`, `nr_`, `ns_`, `ld_`, `sp_`, `kb_`, `setl_`.
  - Header guards `CTW_<DIR>_<NAME>_H`.
  - The host build (`-std=c11 -Wall -Wextra -Werror -pedantic`) and `make -C wii` stay clean.
- **Dolphin** takes over the Mac: ask the owner before every run. Dolphin uses the Mac's network, so the server is the Mac (`npm run serve`, plan 07) at `ipconfig getifaddr en0`.
- **Commits:** `wip | …`, `test | …`, `fix | …`, each ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

- **An address typed loosely:** `http://192.168.1.20:4747/`, ` 192.168.1.20 ` and `PC-NAME` all connect. `https://…` and `192.168.1.20:99999` are refused with no crash (Task 2 `addr_accepts_loose_forms`, `addr_refuses_https_and_bad_port`).
- **Library names with spaces and accents:** `Pokémon ✨/S01E02 - Café über alles.mpg` must be requested percent-encoded (Task 2 `url_encodes_unicode_path`).
- **The server restarts mid-show and drops the kept-alive connection:** the next range read reconnects once and returns the bytes (Task 3 `stream_reconnects_after_server_close`).
- **Wi-Fi drops for 20 s mid-episode:** the frame holds with `Weak signal`, then the channel rejoins at the live point, not where it stopped (Task 7 `stall_rejoins_when_far_behind`, checklist item 60).
- **No SD card, or an SD card with a `wbfs/` folder:** settings are kept only for the session and nothing is written (Task 1 `config_write_needs_clean_sd`, checklist item 64).

---

### Task 1: Settings file and the SD config mount

**Files:**
- Create: `wii/source/settings.c`, `wii/source/settings.h`, `wii/tests/test_settings.c`
- Modify: `wii/source/volume.c`, `wii/source/volume.h`, `wii/tests/test_volume.c`, `wii/source/storage.c`, `wii/source/storage.h`, `wii/source/app.h`, `wii/source/main.c`, `wii/tests/Makefile`, `wii/tests/main.c`

**Interfaces:**
- Produces:
  - `settings.h`:
    - `#define SETTINGS_DIR "sd:/apps/cable-twii"`
    - `typedef enum { SRC_DRIVE, SRC_SERVER } ctw_source;`
    - `typedef struct { ctw_source source; char server[128]; } ctw_settings;`
    - `int settings_parse(const char *toml_text, ctw_settings *out);`
    - `size_t settings_format(const ctw_settings *s, char *out, size_t out_size);`
    - `void settings_load(const char *dir, ctw_settings *out);`
    - `int settings_save(const char *dir, const ctw_settings *s);`: tmp file then rename, same as `state_save`.
  - `volume.h`: `bool vol_config_write_allowed(bool has_games, bool scan_error);`, which is `!has_games && !scan_error`.
  - `storage.h`:
    - `int st_config_mount(void);`: mounts the SD card's first FAT partition as `sd`. Writes are allowed, limited to that partition's sector range, only when `vol_config_write_allowed`. Returns 0 when the card is mounted, -1 when there is no card.
    - `bool st_config_writable(void);`
    - `void st_config_release(void);`
    - `st_find_root` reuses this mount instead of mounting the SD card twice. `release_unchosen` doesn't shut the SD card down while the config mount holds it.
  - `app.h`: `ctw_settings settings; bool settings_saveable; char state_dir[64];`.
    - `state_dir` is `a->root` in drive mode and `SETTINGS_DIR` in server mode.
    - `save_state` uses `state_dir`, and its "can save" check uses `st_config_writable()` in server mode.

- [ ] **Step 1: Write the failing tests**
  - `test_settings.c`:
    - `settings_round_trip`: `{SRC_SERVER, "192.168.1.20:4747"}` formats to `source = "server"\nserver = "192.168.1.20:4747"\n` and parses back.
    - `settings_defaults_when_missing`: `settings_load("build/state/none", &s)` gives `SRC_DRIVE` and `""`.
    - `settings_unknown_source_is_drive`.
    - `settings_save_then_load` in `build/state`.
  - `test_volume.c`, `config_write_needs_clean_sd`:
    - `ASSERT(vol_config_write_allowed(false, false))`
    - `ASSERT(!vol_config_write_allowed(true, false))`
    - `ASSERT(!vol_config_write_allowed(false, true))`
- [ ] **Step 2: Run and see them fail**
  Run: `make -C wii/tests test`
  Expected: FAIL, because `settings.h` is missing.
- [ ] **Step 3: Implement** `settings.c` (tomlc99, like `state.c`) and `vol_config_write_allowed`, and add `../source/settings.c` to `SRC`.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: PASS.
- [ ] **Step 5: Implement the storage and boot changes**:
  - `boot()` calls `st_config_mount()` first, then `settings_load(SETTINGS_DIR, &a->settings)`.
  - Drive mode continues as today.
  - `settings_saveable = st_config_writable()`.
- [ ] **Step 6: Build and check in Dolphin** (ask first):
  - `make -C wii DEBUG_SCRIPT=1`
  - With the sample SD folder and no `settings.toml`, the guide appears as today.
  - Then add `apps/cable-twii/settings.toml` with `source = "drive"` and check that nothing changes.
- [ ] **Step 7: Commit** `wip | wii settings file and sd config mount`

---

### Task 2: Address and URL parsing

**Files:**
- Create: `wii/source/net/addr.c`, `wii/source/net/addr.h`, `wii/tests/test_addr.c`
- Modify: `wii/tests/Makefile`, `wii/tests/main.c`, `wii/Makefile` (`SOURCES += source/net`)

**Interfaces:**
- Produces:
  - `#define ADDR_DEFAULT_PORT 4747`
  - `typedef struct { char host[64]; uint16_t port; } srv_addr;`
  - `int addr_parse(const char *typed, srv_addr *out);`: accepts an IP, `ip:port`, a hostname, or `http://host[:port][/]`. Trims spaces. Refuses `https://`, an empty host, a port outside 1–65535, inner spaces, and `/` after the host other than one trailing slash.
  - `void addr_format(const srv_addr *a, char *out, size_t out_size);`: `host:port`.
  - `int url_encode_path(const char *rel, char *out, size_t out_size);`: keeps `A–Z a–z 0–9 - . _ ~ /` and writes every other byte as uppercase `%XX`.
  - `int url_split(const char *url, srv_addr *addr, char *path, size_t path_size);`: takes `http://host:port/rest` and returns the encoded `/rest`.
  - `void srv_root(const srv_addr *a, char *out, size_t out_size);`: `http://host:port/wii/v1/files`.

- [ ] **Step 1: Write the failing tests**:
  - `addr_accepts_loose_forms`:
    - `"http://192.168.1.20:4747/"` → host `192.168.1.20`, port 4747
    - `" 192.168.1.20 "` → port 4747
    - `"PC-NAME:8080"` → port 8080
  - `addr_refuses_https_and_bad_port`: `"https://x"`, `"x:0"`, `"x:99999"`, `""`, `"a b"` and `"x/y"` all return -1.
  - `url_encodes_unicode_path`: `"Cartoon Network/Pokémon ✨/S01E02 - Café über alles.mpg"` → `"Cartoon%20Network/Pok%C3%A9mon%20%E2%9C%A8/S01E02%20-%20Caf%C3%A9%20%C3%BCber%20alles.mpg"`.
  - `url_split_round_trip`: `srv_root` + `"/a b.mpg"` splits back to the same address and `/wii/v1/files/a%20b.mpg`.
- [ ] **Step 2: Run and see them fail** `make -C wii/tests test`. Expected: FAIL.
- [ ] **Step 3: Implement**, and add `../source/net/addr.c` to `SRC`.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | wii server address parsing`

---

### Task 3: HTTP client

**Files:**
- Create:
  - `wii/source/net/httpparse.c`, `httpparse.h`, `wii/source/net/http.c`, `http.h`
  - `wii/source/net/sock.h`, `wii/source/net/sock_wii.c`
  - `wii/tests/sock_posix.c`, `wii/tests/testserver.c`, `wii/tests/testserver.h`
  - `wii/tests/test_httpparse.c`, `wii/tests/test_http.c`
- Modify: `wii/tests/Makefile` (add `-pthread`, `-lz`), `wii/tests/main.c`

**Interfaces:**
- Consumes: Task 2 `srv_addr`.
- Produces:
  - `sock.h`. The Wii version uses `if_config`, `net_gethostbyname`, `net_socket`, `net_connect`, `net_send`, `net_recv` and `net_close` with polling timeouts. The host version uses POSIX.
    - `int sk_up(uint32_t timeout_ms);`: one attempt.
    - `void sk_down(void);`: `net_deinit` on the Wii, nothing on the host.
    - `int sk_connect(const char *host, uint16_t port, uint32_t timeout_ms);`
    - `int sk_send_all(int fd, const void *buf, size_t len, uint32_t timeout_ms);`
    - `long sk_recv(int fd, void *buf, size_t len, uint32_t timeout_ms);`
    - `void sk_close(int fd);`
  - `httpparse.h`:
    - `typedef struct { int status; int64_t content_length; int64_t range_start; int64_t range_end; int64_t total; bool gzip; bool keep_alive; } hp_response;`
    - `long hp_parse_head(const char *buf, size_t len, hp_response *out);`: the header length, 0 when incomplete, -1 when malformed.
  - `http.h`:
    - Errors: `#define HTTP_E_CONNECT -1`, `HTTP_E_TIMEOUT -2`, `HTTP_E_STATUS -3`, `HTTP_E_PROTO -4`, `HTTP_E_MEM -5`, `HTTP_E_TOO_LARGE -6`
    - `typedef struct { char *data; size_t len; int status; } http_body;`
    - `int http_get(const srv_addr *a, const char *path, size_t max_bytes, uint32_t timeout_ms, http_body *out);`: sends `Accept-Encoding: gzip`, inflates gzip with zlib, and NUL-terminates the body.
    - `void http_body_free(http_body *b);`
    - `typedef struct { srv_addr addr; char path[512]; int fd; uint64_t length; uint64_t remaining; uint32_t timeout_ms; } http_stream;`
    - `int hs_open(http_stream *s, const srv_addr *a, const char *path, uint32_t timeout_ms);`: requests `bytes=0-0` and learns `length` from `Content-Range`.
    - `int hs_start(http_stream *s, uint64_t offset);`: sends one open-ended `Range: bytes=<offset>-` request. If the previous response still has unread body bytes, it closes the connection and opens a new one first. Reconnects once on a socket error or a closed connection. A non-206 status gives `HTTP_E_STATUS`.
    - `long hs_read(http_stream *s, void *dst, size_t len);`: returns the next body bytes as they arrive (up to `len`, waiting at most `timeout_ms`), 0 when the response is complete, negative on error. The caller restarts with `hs_start` at its own position after an error.
    - `void hs_close(http_stream *s);`
  - `testserver.h` (host only):
    - `typedef struct { const char *head; const void *body; size_t body_len; bool close_after; } ts_reply;`
    - `uint16_t ts_start(const ts_reply *replies, size_t n);`
    - `const char *ts_request(size_t i);`
    - `void ts_stop(void);`

- [ ] **Step 1: Write the failing tests**:
  - `test_httpparse.c`:
    - `head_206_range`: a 206 with `Content-Range: bytes 0-9/1000` and `Content-Length: 10` → `status 206`, `range_start 0`, `range_end 9`, `total 1000`, `content_length 10`.
    - `head_incomplete_returns_0`.
    - `head_gzip_and_close`: `Content-Encoding: gzip` and `Connection: close` → `gzip` and `!keep_alive`.
    - `head_malformed_returns_minus_1`.
  - `test_http.c`, against `testserver`:
    - `get_inflates_gzip`: gzip `version = 1\n` comes back as plain text, and `ts_request(0)` contains `Accept-Encoding: gzip`.
    - `get_404_is_status_error`: `HTTP_E_STATUS` with `status 404`.
    - `get_too_large`: `max_bytes` 4 against a 10-byte body gives `HTTP_E_TOO_LARGE`.
    - `stream_reads_open_ended_range`: after `hs_open`, `hs_start(s, 100)` sends `Range: bytes=100-` on the same connection, and repeated `hs_read` calls return the exact remaining bytes, then 0.
    - `stream_restart_drops_unfinished_body`: `hs_start(s, 500)` while the previous body is unread opens a new connection and the next `hs_read` returns bytes from 500.
    - `stream_reconnects_after_server_close`: the reply has `close_after`, and the next `hs_start` still succeeds and `hs_read` returns the bytes.
    - `connect_refused_is_connect_error`.
- [ ] **Step 2: Run and see them fail** `make -C wii/tests test`. Expected: FAIL.
- [ ] **Step 3: Implement**. Add `../source/net/httpparse.c ../source/net/http.c` to `SRC`. `sock_wii.c` is Wii-only and is not in the host `SRC`.
- [ ] **Step 4: Run** `make -C wii/tests test` and `make -C wii`. Expected: both pass.
- [ ] **Step 5: Commit** `wip | wii http client`

---

### Task 4: Loading the library from the server

**Files:**
- Create: `wii/source/net/srvlib.c`, `srvlib.h`, `wii/source/srcpick.c`, `srcpick.h`, `wii/tests/test_srvlib.c`, `wii/tests/test_srcpick.c`
- Modify: `wii/tests/Makefile`, `wii/tests/main.c`

**Interfaces:**
- Consumes: Task 3 `http_get`, Task 2 `srv_addr`, engine `ctw_library_parse` / `ctw_config_parse`.
- Produces:
  - `srvlib.h`:
    - `typedef struct { int version; char name[64]; char revision[64]; } srv_info;`
    - `int srv_info_parse(const char *toml, srv_info *out);`: -1 unless `version == 1`.
    - `typedef enum { SRV_OK, SRV_NO_NETWORK, SRV_NO_ANSWER, SRV_NOT_CABLE_TWII, SRV_BAD_LIBRARY } srv_result;`
    - `srv_result srv_fetch_info(const srv_addr *a, uint32_t timeout_ms, srv_info *out);`
    - `srv_result srv_load_db(const srv_addr *a, uint32_t timeout_ms, ctw_db **out, ctw_issues *issues, srv_info *info);`: fetches `info`, then `library.toml`, then `config.toml`, each at most 16 MiB. Parses the library first, then the config, like `ctw_db_load`.
    - `void srv_test_message(srv_result r, const char *address, const char *name, char *out, size_t out_size);`: the four Global Constraints test texts, and the No network copy for `SRV_NO_NETWORK`.
  - `srcpick.h`:
    - `typedef enum { SP_SERVER, SP_DRIVE_WITH_NOTICE, SP_FAIL } sp_outcome;`
    - `bool sp_try_server(const ctw_settings *s);`: true when the source is server and `addr_parse` succeeds.
    - `sp_outcome sp_after_server(srv_result r, bool drive_found);`

- [ ] **Step 1: Write the failing tests**:
  - `test_srvlib.c`, with `testserver` serving the `engine/tests/fixtures/root_basic` files:
    - `load_db_parses_fixture`: `SRV_OK` and `ctw_db_channel_count(db)` equals what `ctw_db_load` gives on the same fixture.
    - `info_wrong_version_is_not_cable_twii`.
    - `html_page_is_not_cable_twii`.
    - `bad_config_is_bad_library`: issues come back with errors.
    - `no_server_is_no_answer`.
    - `test_message_texts`: each of the four strings is exact, with `address 192.168.1.20:4747` and name `Home PC`.
  - `test_srcpick.c`:
    - `sp_after_server(SRV_OK, false) == SP_SERVER`
    - `sp_after_server(SRV_NO_ANSWER, true) == SP_DRIVE_WITH_NOTICE`
    - `sp_after_server(SRV_NO_ANSWER, false) == SP_FAIL`
    - `sp_after_server(SRV_BAD_LIBRARY, true) == SP_DRIVE_WITH_NOTICE`
    - `sp_after_server(SRV_NO_NETWORK, true) == SP_DRIVE_WITH_NOTICE` and `sp_after_server(SRV_NO_NETWORK, false) == SP_FAIL`
    - `sp_try_server` is false for `SRC_DRIVE`, and false for `SRC_SERVER` with `"https://x"`.
- [ ] **Step 2: Run and see them fail** `make -C wii/tests test`. Expected: FAIL.
- [ ] **Step 3: Implement**, adding both files to `SRC`.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | wii server library loading`

---

### Task 5: Streaming playback

**Files:**
- Create: `wii/source/media/netring.c`, `netring.h`, `wii/source/media/netsrc.c`, `netsrc.h`, `wii/tests/test_netring.c`
- Modify: `wii/source/media/player.c`, `wii/source/media/player.h`, `wii/tests/Makefile`, `wii/tests/main.c`

**Interfaces:**
- Consumes: Task 3 `hs_*`, Task 2 `url_split`, decision 0004 sizes.
- Produces:
  - `netring.h`, a window `[start, start+len)` over a circular buffer:
    - `#define NR_KEEP_BEHIND (256u * 1024u)`
    - `typedef struct { uint8_t *mem; size_t cap; uint64_t start; size_t len; } nr_ring;`
    - `void nr_init(nr_ring *r, uint8_t *mem, size_t cap);`
    - `void nr_reset(nr_ring *r, uint64_t pos);`
    - `bool nr_contains(const nr_ring *r, uint64_t pos);`
    - `uint64_t nr_end(const nr_ring *r);`
    - `size_t nr_space(const nr_ring *r);`
    - `void nr_append(nr_ring *r, const uint8_t *src, size_t n);`
    - `size_t nr_copy(const nr_ring *r, uint64_t pos, uint8_t *dst, size_t n);`
    - `void nr_drop_before(nr_ring *r, uint64_t pos);`: keeps `NR_KEEP_BEHIND` bytes before `pos`.
  - `netsrc.h` (Wii-only). One fetch thread (`LWP`) per source keeps one open-ended response (`hs_start` at `nr_end`) streaming into the ring with `hs_read`, pausing while the ring is full or about 10 s ahead of the reader. A read outside the window resets the ring and restarts the response at the new position. The ring lives in MEM2. The debug overlay shows the live download rate.
    - `typedef struct ns_source ns_source;`
    - `ns_source *ns_open(const char *url, uint32_t timeout_ms);`
    - `size_t ns_read(ns_source *s, uint8_t *dst, size_t len);`
    - `void ns_seek(ns_source *s, uint64_t pos);`
    - `uint64_t ns_tell(const ns_source *s);`
    - `uint64_t ns_length(const ns_source *s);`
    - `bool ns_failed(const ns_source *s);`
    - `uint32_t ns_waiting_ms(const ns_source *s);`
    - `void ns_close(ns_source *s);`
  - `player.h`: `uint32_t pl_waiting_ms(void);`, which is 0 for files.
  - `pl_open` with an `http://` path builds the buffer with `plm_buffer_create_with_callbacks(load, seek, tell, ns_length, src)` and `plm_create_with_buffer`.
    - `load` follows pl_mpeg's file load callback: discard read bytes, then fill up to capacity, and set `has_ended` on 0 bytes.
    - End and error classification uses `ns_failed`.
    - The file path code is unchanged.

- [ ] **Step 1: Write the failing tests** in `test_netring.c`:
  - `append_and_copy_across_wrap`: cap 16, append 12, drop to 8, append 10 → copy from 8 returns those 14 bytes in order.
  - `contains_window_edges`: `nr_contains(start)`, `!nr_contains(nr_end)`.
  - `drop_keeps_behind`: with a big cap, `nr_drop_before(r, start + NR_KEEP_BEHIND + 10)` → `start` moves by 10.
  - `reset_empties_at_pos`.
- [ ] **Step 2: Run and see them fail** `make -C wii/tests test`. Expected: FAIL.
- [ ] **Step 3: Implement** `netring.c` (added to `SRC`), then `netsrc.c` and the player branch.
- [ ] **Step 4: Run** `make -C wii/tests test` and `make -C wii`. Expected: both pass.
- [ ] **Step 5: Dolphin check** (ask first): a `DEBUG_SCRIPT` build with `a->root` temporarily set to the Mac server's `srv_root` plays a channel, and the Studio log shows range requests. Revert the temporary root before committing.
- [ ] **Step 6: Commit** `wip | wii streaming playback`

---

### Task 6: Starting from the server, with drive fallback

**Files:**
- Create: `wii/source/loader.c`, `loader.h`, `wii/source/screens/notice.c`, `notice.h`
- Modify: `wii/source/main.c`, `wii/source/app.h`, `wii/source/screens/logo.c`, `wii/source/live.c`, `wii/source/ui/icons.c`, `wii/source/ui/draw.c`, `wii/source/screens/guide.c`

**Interfaces:**
- Consumes: Task 1 settings, `st_config_*` and `state_dir`. Task 4 `srv_load_db`, `sp_*` and `srv_root`. Task 5 player.
- Produces:
  - `loader.h` (Wii-only). One background thread at a time. Before its first request it brings the network up with up to 3 attempts of `sk_up(10000)`, calling `sk_down()` between them; after the third failure the job ends with `SRV_NO_NETWORK`.
    - `typedef enum { LD_IDLE, LD_RUNNING, LD_DONE } ld_state;`
    - `void ld_load(const srv_addr *a);`
    - `void ld_info(const srv_addr *a);`
    - `ld_state ld_poll(void);`
    - `srv_result ld_take_db(ctw_db **db, ctw_issues *issues, srv_info *info);`
    - `srv_result ld_take_info(srv_info *info);`
  - `app.h`: `bool from_server; bool loading; srv_addr server; char revision[64]; char notice[96]; uint32_t notice_until_ms;`
  - `notice.h`: `void notice_show(app *a, const char *text);` (4000 ms) and `void notice_draw(const app *a);`, drawn on the guide and in full screen.
- Built note: the logo holds after the shine (1400 ms), not at the animation's end, because the slide into the guide needs the channels.
  - `boot()`:
    - When `sp_try_server`, it sets `loading`, calls `ld_load`, and goes to `SCR_LOGO`. Otherwise it boots from the drive as today.
    - When the logo animation ends while `loading`, it holds and shows `Connecting to <address>…`.
    - On `LD_DONE`, `sp_after_server` decides:
      - `SP_SERVER`: `a->root = srv_root`, `from_server = true`, `state_dir = SETTINGS_DIR`, then `tn_init`, `state_load`, `guide_prepare`.
      - `SP_DRIVE_WITH_NOTICE`: the drive boot, then the matching fallback notice.
      - `SP_FAIL`: `app_fail` with the exact error copy, or the No network copy when the result is `SRV_NO_NETWORK`.
  - `live.c`: no `drive_lost` check when `from_server`.
  - Icons with `http://` paths are fetched by one worker thread (a queue of 8). The main thread decodes at most `ICON_LOADS_PER_FRAME` fetched PNGs per frame and draws the placeholder until then.

- [ ] **Step 1: Implement** the loader, notice, boot, live and icon changes.
- [ ] **Step 2: Build** `make -C wii` and `make -C wii/tests test`. Expected: both pass.
- [ ] **Step 3: Dolphin checks** (ask first), with `settings.toml` set to `source = "server"` and the Mac server's address:
  - The logo is followed by `Connecting to …`, then the guide with the server's channels and icons. Tuning works.
  - Stop the server, then relaunch: the drive fallback shows `Server not reachable, playing from the SD card`.
  - With no `cable-twii/` on the SD card and the server stopped: the `Can't reach the server` error.
- [ ] **Step 4: Full-size library** (spec section 2, Library size):
  - Write `tools/make-big-library.py <dir>`. It writes only `cable-twii/config.toml` and `cable-twii/library.toml`, with 50 channels and 5,000 half-hour episodes; no video files are needed for loading.
  - Check it: `engine/build/cable-twii-sched validate <dir>/cable-twii` exits 0.
  - Serve it with plan 07's server, then boot a `DEBUG_OVERLAY=1` build against it in Dolphin (ask first).
  - Record the load time and free MEM1/MEM2 in `docs/decisions/0004-server-streaming.md`. The real-Wii numbers are checklist item 66.
- [ ] **Step 5: Commit** `wip | wii starts from the server with drive fallback`

---

### Task 7: Weak signal and rejoining live

**Files:**
- Modify: `wii/source/playback.c`, `wii/source/playback.h`, `wii/tests/test_playback.c`, `wii/source/live.c`, `wii/source/screens/full.c`, `wii/source/screens/guide_info.c`

**Interfaces:**
- Consumes: Task 5 `pl_waiting_ms`.
- Produces:
  - `#define PB_NOTE_MS 1000u` and `#define PB_REJOIN_LAG_MS 2000u`
  - `typedef enum { PB_STALL_NONE, PB_STALL_NOTE, PB_STALL_REJOIN } pb_stall;`
  - `pb_stall pb_stall_action(uint32_t waiting_ms, bool was_stalled, uint32_t position_ms, uint32_t expected_ms);`
    - `PB_STALL_NOTE` while `waiting_ms >= PB_NOTE_MS`.
    - `PB_STALL_REJOIN` when `was_stalled`, data flows again, and `expected_ms - position_ms > PB_REJOIN_LAG_MS`.
    - Otherwise `PB_STALL_NONE`.
  - `live_update` (server mode only) re-tunes on `PB_STALL_REJOIN` with `play_airing` at the current offset. `full.c` and `guide_info.c` draw `Weak signal` over the video while `PB_STALL_NOTE`.

- [ ] **Step 1: Write the failing tests** in `test_playback.c`:
  - `stall_note_after_a_second`: `(1000, false, 0, 0)` → `PB_STALL_NOTE`; `(999, false, 0, 0)` → `PB_STALL_NONE`.
  - `stall_rejoins_when_far_behind`: `(0, true, 10000, 12500)` → `PB_STALL_REJOIN`.
  - `stall_no_rejoin_when_close`: `(0, true, 10000, 11500)` → `PB_STALL_NONE`.
- [ ] **Step 2: Run and see them fail** `make -C wii/tests test`. Expected: FAIL.
- [ ] **Step 3: Implement**.
- [ ] **Step 4: Run** `make -C wii/tests test` and `make -C wii`. Expected: both pass.
- [ ] **Step 5: Dolphin check** (ask first): pause the Mac server process for 10 s (`kill -STOP`, then `kill -CONT`). `Weak signal` shows, then the channel rejoins at the live point.
- [ ] **Step 6: Commit** `wip | wii weak signal and rejoin`

---

### Task 8: Settings screen mockup (owner approval)

**Files:**
- Create: `docs/superpowers/specs/mockups/wii-settings-and-keyboard.html`

- [ ] **Step 1: Draw the mockup** in the Wii redesign style (tokens from `wii/source/ui/theme.h`, 640×480, the same build as `wii-guide-and-fullscreen.html`). It shows:
  - **The Settings screen:**
    - a Drive / Server segmented control
    - the address field with an Edit pill
    - a Test connection pill with its result line
    - Save and Cancel pills
    - the no-SD-card line
  - **The keyboard:**
    - rows `1234567890`, `qwertyuiop`, `asdfghjkl.`, `zxcvbnm:/-`
    - a bottom row with `http://`, backspace and Done
    - the typed text above it
  - **The HOME menu** with Settings and Reload channels pills added.
  - **The error and empty cards** with a Settings pill next to Back to Homebrew Channel.
- [ ] **Step 2: Owner approves.** Record the approved pixel numbers (rects for every pill, key and field, at `pixel_x` 1.0 and 1.333) in a table at the bottom of the mockup.
- Built note: `pixel_x` is 0.75 on 16:9 (`ui_pixel_x()`), not 1.333.
- [ ] **Step 3: Commit** `wip | wii settings and keyboard mockup`

---

### Task 9: Keyboard and settings layout models

**Files:**
- Create: `wii/source/keyboard.c`, `keyboard.h`, `wii/source/settings_layout.c`, `settings_layout.h`, `wii/tests/test_keyboard.c`, `wii/tests/test_settings_layout.c`
- Modify: `wii/source/home_layout.c`, `wii/source/home_layout.h`, `wii/tests/test_home_layout.c`, `wii/tests/Makefile`, `wii/tests/main.c`

**Interfaces:**
- Consumes: Task 8 approved numbers.
- Produces:
  - `keyboard.h`:
    - `#define KB_ROWS 5`
    - `#define KB_MAX_TEXT 127`
    - `typedef enum { KB_KEY_CHAR, KB_KEY_HTTP, KB_KEY_BACK, KB_KEY_DONE } kb_kind;`
    - `typedef struct { kb_kind kind; char ch; } kb_key;`
    - `typedef struct { char text[KB_MAX_TEXT + 1]; size_t len; int row; int col; } kb_state;`
    - `void kb_init(kb_state *k, const char *text);`
    - `int kb_row_len(int row);`
    - `kb_key kb_key_at(int row, int col);`
    - `void kb_move(kb_state *k, int dx, int dy);`: clamps, and keeps the column within shorter rows.
    - `bool kb_press(kb_state *k, kb_key key);`: true on Done. `http://` is inserted only when the text is empty. Back removes one byte. Text stops at `KB_MAX_TEXT`.
  - `settings_layout.h`:
    - `typedef enum { SET_BTN_NONE, SET_BTN_DRIVE, SET_BTN_SERVER, SET_BTN_EDIT, SET_BTN_TEST, SET_BTN_SAVE, SET_BTN_CANCEL } set_btn;`
    - `typedef struct { asp_rect drive, server, field, edit, test, result, save, cancel; asp_rect keys[KB_ROWS][10]; asp_rect text; } settings_layout;`
    - `settings_layout setl_make(float pixel_x);`
    - `set_btn setl_hit(const settings_layout *l, float x, float y);`
    - `bool setl_key_hit(const settings_layout *l, float x, float y, int *row, int *col);`
  - `home_layout.h`:
    - `HOME_BTN_SETTINGS` and `HOME_BTN_RELOAD`, with rects `settings` and `reload` in `home_layout`.
    - `home_layout home_layout_make(float pixel_x, bool from_server);`: `reload` is empty when `!from_server`.

- [ ] **Step 1: Write the failing tests**:
  - `test_keyboard.c`:
    - `kb_types_and_backs`: press `1`, `.`, Back → `"1"`.
    - `kb_http_only_when_empty`: on `""` → `"http://"`; on `"1"` → unchanged.
    - `kb_stops_at_max`.
    - `kb_move_clamps_to_short_row`: from the last column of row 1, moving down to the bottom row clamps to its last key.
    - `kb_done_returns_true`.
  - `test_settings_layout.c` and `test_home_layout.c`: `*_match_the_mockup` asserts the Task 8 numbers, and `hit_finds_each_*_at_its_centre`. Update `layout_pills_match_the_deck` for the new HOME rects.
- [ ] **Step 2: Run and see them fail** `make -C wii/tests test`. Expected: FAIL.
- [ ] **Step 3: Implement**, adding both files to `SRC`.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | wii keyboard and settings layout`

---

### Task 10: Reload channels and the revision check

**Files:**
- Modify: `wii/source/main.c`, `wii/source/app.h`, `wii/source/screens/homemenu.c`, `wii/source/screens/guide.c`, `wii/source/screens/full.c`

**Interfaces:**
- Consumes: Task 6 `ld_info` and `revision`, Task 9 `HOME_BTN_RELOAD`.
- Produces:
  - `void app_reload(app *a);`: `pl_close`, `tn_free`, `ctw_db_free`, `st_release` when the drive was in use, then `boot(a)`. It keeps the settings, the config mount and the volume.
  - The HOME menu's Reload channels pill (server mode only) calls `app_reload` and shows `Channels updated`.
  - When the guide opens, and when the day changes, a server-mode app runs `ld_info`. If `info.revision` differs from `a->revision`, it calls `app_reload` and shows `Channels updated`.

- [ ] **Step 1: Implement**.
- [ ] **Step 2: Build** `make -C wii` and `make -C wii/tests test`. Expected: both pass.
- [ ] **Step 3: Dolphin check** (ask first):
  - Rename a channel in Studio on the Mac, press B back to the guide, and the new name appears with `Channels updated`.
  - HOME → Reload channels works the same way.
- [ ] **Step 4: Commit** `wip | wii reload channels`

---

### Task 11: Settings screen, HOME entries and error-screen Settings

**Files:**
- Create: `wii/source/screens/settings.c`, `settings.h`
- Modify: `wii/source/app.h` (`SCR_SETTINGS`), `wii/source/main.c`, `wii/source/screens/homemenu.c`, `wii/source/screens/error.c`, `wii/source/screens/empty.c`

**Interfaces:**
- Consumes: Task 9 models, Task 1 `settings_save` and `settings_saveable`, Task 6 `ld_info`, Task 4 `srv_test_message`.
- Produces:
  - `screens/settings.h`: `void settings_enter(app *a); void settings_update(app *a); void settings_draw(app *a);`
    - The screen edits a copy of `a->settings`.
    - Edit opens the keyboard: pointer and A, or the D-pad and A; B closes the keyboard without changes.
    - Test runs `ld_info` and shows `srv_test_message`.
    - Save writes with `settings_save` when `settings_saveable`, otherwise keeps the settings in memory and shows the no-SD line. When the source or the address changed, it calls `app_reload` (Task 10).
    - Cancel or B returns to the previous screen.
  - `app.h`: `void app_open_settings(app *a);`
  - The HOME menu's Settings pill, and a new Settings pill on the error and empty cards, call `app_open_settings`.

- [ ] **Step 1: Implement**, following the mockup, `ps_update` press states and the existing sounds.
- [ ] **Step 2: Build** `make -C wii` and `make -C wii/tests test`. Expected: both pass.
- [ ] **Step 3: Dolphin checks** (ask first):
  - From the error screen with no drive: Settings → Server → type the Mac's address on the keyboard → Test shows `Connected to "<name>".` → Save → the guide loads from the server, and `settings.toml` appears in the SD sync folder.
  - With a `wbfs/` folder on the SD card: Save shows the no-SD line and no file is written.
- [ ] **Step 4: Commit** `wip | wii settings screen`

---

### Task 12: Docs and checklist

**Files:**
- Modify: `docs/USER-GUIDE.md`, `docs/testing/wii-checklist.md`, `docs/formats.md`, `docs/STATUS.md`, `CHANGELOG.md`

- [ ] **Step 1: Write the docs**:
  - **USER-GUIDE:** a "Play from a server" section covering `settings.toml`, the Settings screen, the fallback, Reload channels, and a link to `docs/SERVER.md`.
  - **formats.md:** a `settings.toml` section with the keys, the location and the write rule.
  - **CHANGELOG:** an Unreleased entry.
  - **STATUS:** the plan 08 row.
- [ ] **Step 2: Add checklist items 56–66**, each with its expected result:
  - 56: server boot
  - 57: tune-in time from the server
  - 58: 30-minute stream
  - 59: server stopped, so drive fallback
  - 60: Wi-Fi off for 20 s, so `Weak signal` then rejoin live
  - 61: server restarted mid-show
  - 62: settings from scratch on a Wii with no drive
  - 63: Test connection with the firewall blocking
  - 64: no SD card, or an SD card with `wbfs/`, so nothing is saved
  - 65: Reload channels and the revision check
  - 66: the 5,000-episode library from `tools/make-big-library.py` loads within the timeouts, with the free memory recorded
- [ ] **Step 3: Dolphin pass** (ask first) over the drive items that touch boot, state and the HOME menu, and items 56, 59, 62 and 65. Note in STATUS that 57, 58, 60, 61, 63 and 66 need the real Wii at home.
- [ ] **Step 4: Commit** `wip | wii server source docs and checklist`
