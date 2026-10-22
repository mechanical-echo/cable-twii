# cable-twii Schedule Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A portable C11 library that loads `config.toml` + `library.toml` and deterministically computes each channel's daily lineup, plus the `cable-twii-sched` CLI that Studio calls, with identical results on the Mac and the Wii.

**Architecture:** Pure C11 with no platform calls beyond `stdio`/`stdlib`/`string`. Parsing uses vendored tomlc99. All schedule decisions use integer milliseconds and an own PCG32 generator seeded by FNV-1a hashes, so big-endian Wii and little-endian Mac agree. Built natively with a host Makefile for tests and the CLI; the Wii app compiles the same sources.

**Tech Stack:** C11, clang (Xcode command line tools), tomlc99 (MIT, `github.com/cktan/tomlc99`), greatest (ISC, single-header test framework, `github.com/silentbicycle/greatest`), Node.js only for the CLI test script.

**Spec:** `docs/superpowers/specs/2026-10-20-cable-twii-design.md` (§5 file formats, §6 engine)

## Status (2026-10-22)

- Done: all ten tasks, merged to `master` (8f93c1b). Engine tests, CLI tests and the Wii big-endian cross-check pass on the host (`make -C engine test`, `make -C engine cli-test`, `make -C engine wii-check`).
- Left: nothing in this plan. Studio and the Wii app consume the engine as-is.

## Global Constraints

- C11, compiles warning-free with `-std=c11 -Wall -Wextra -Werror` on clang and with devkitPPC `powerpc-eabi-gcc`.
- No `rand()`, no `time()`, no floating point in any scheduling decision, no reliance on host endianness or `struct` layout.
- Times: `uint32_t ms_of_day` from local midnight; `int64_t start_ms` inside lineups. Day length `86400000`.
- Symbol prefix `ctw_`; public header `engine/include/ctw/ctw.h`.
- File format `version = 1` exactly as in spec §5 and `docs/formats.md`.
- No comments in source code. No trailing whitespace.
- Commits: `wip | …`, `test | …`, `fix | …` plus the attribution line.

## Review Focus

- A `config.toml` saved by hand with Windows line endings (`\r\n`) or a UTF-8 BOM must parse exactly like a clean one (Task 3 test `config_crlf_and_bom`).
- Show and season names with non-ASCII characters (`Pokémon`) must match between config and library byte-for-byte (Task 3 test `config_unicode_show_name`).
- A pool where every episode is longer than the slot must produce a warning and ad/up-next filler, never an infinite loop or an empty lineup gap (Task 5 test `timed_nothing_fits`).
- A looping channel whose every item is skipped (only `ads` items on an `ads = false` channel) must terminate with one up-next airing for the day (Task 6 test `loop_all_skipped`).
- An episode with `duration_ms = 0` in the library must be rejected at load, or it causes zero-progress loops (Task 2 test `library_rejects_zero_duration`).

---

## File Structure

```
engine/
├── Makefile                     host build: test, cli, clean
├── include/ctw/ctw.h            public types and functions
├── src/
│   ├── rng.c                    PCG32 + bounded draw + shuffle
│   ├── hash.c                   FNV-1a 64, day seed, lineup hash
│   ├── timeutil.c               HH:MM parsing, date arithmetic
│   ├── arena.c                  string/array arena for the db
│   ├── issues.c                 issue list helpers
│   ├── library.c                library.toml → db shows/ads
│   ├── config.c                 config.toml → db channels (resolves shows)
│   ├── db.c                     ctw_db_load from a root folder, ctw_db_free, lookups
│   ├── pool.c                   episode pools and shuffle bags, ad picking
│   ├── lineup_timed.c           timed resolution
│   ├── lineup_loop.c            looping resolution
│   └── lineup.c                 ctw_lineup_build dispatch, ctw_lineup_at, free
├── vendor/tomlc99/toml.c, toml.h
├── vendor/greatest/greatest.h
├── cli/main.c                   cable-twii-sched
└── tests/
    ├── main.c                   greatest runner
    ├── fixtures/lib_basic.toml
    ├── fixtures/root_basic/     config.toml + library.toml for CLI tests
    ├── test_rng.c  test_hash.c  test_time.c
    ├── test_library.c  test_config.c
    ├── test_timed.c  test_loop.c  test_lineup_at.c
    └── cli_test.sh
```

## Public types (`engine/include/ctw/ctw.h`)

Every later task and plan uses these names exactly.

```c
typedef struct { int year; int month; int day; } ctw_date;

typedef struct { uint64_t state; uint64_t inc; } ctw_rng;

typedef struct {
    const char *path;
    const char *title;
    const char *description;
    uint16_t season;
    uint16_t episode;
    uint32_t duration_ms;
} ctw_episode;

typedef struct {
    const char *channel_folder;
    const char *folder;
    const char *name;
    const char *icon;
    ctw_episode *episodes;
    size_t episode_count;
} ctw_show;

typedef struct {
    const char *path;
    const char *channel_folder;
    uint32_t duration_ms;
} ctw_ad;

typedef enum {
    CTW_ITEM_EPISODE, CTW_ITEM_RANDOM, CTW_ITEM_RANDOM_RUN,
    CTW_ITEM_ADS, CTW_ITEM_AD, CTW_ITEM_FILL
} ctw_item_type;

typedef struct {
    ctw_item_type type;
    const ctw_show *show;
    uint16_t season;
    uint16_t episode;
    uint16_t count;
    uint16_t ads_between;
    const ctw_ad *ad;
} ctw_item;

typedef struct { uint32_t at_ms; ctw_item *items; size_t item_count; } ctw_slot;

typedef enum { CTW_MODE_TIMED, CTW_MODE_LOOPING } ctw_mode;

typedef struct {
    int number;
    const char *name;
    const char *category;
    const char *folder;
    const char *icon;
    bool ads;
    ctw_mode mode;
    ctw_slot *slots;
    size_t slot_count;
    ctw_item *loop;
    size_t loop_count;
} ctw_channel;

typedef struct ctw_db ctw_db;

typedef enum { CTW_LEVEL_ERROR, CTW_LEVEL_WARNING } ctw_level;

typedef enum {
    CTW_E_PARSE, CTW_E_VERSION, CTW_E_BAD_VALUE, CTW_E_BAD_TIME,
    CTW_E_DUP_CHANNEL, CTW_E_UNKNOWN_SHOW, CTW_E_MISSING_EPISODE,
    CTW_E_UNKNOWN_AD, CTW_E_FILL_IN_LOOP, CTW_E_NO_SLOTS, CTW_E_DUP_SLOT,
    CTW_E_MISSING_FILE,
    CTW_W_CUT, CTW_W_PARTIAL, CTW_W_EMPTY_POOL, CTW_W_LOOP_EMPTY
} ctw_code;

typedef struct {
    ctw_level level;
    ctw_code code;
    int channel;
    int slot;
    int item;
    int line;
    char file[16];
    char message[200];
} ctw_issue;

typedef struct { ctw_issue *items; size_t count; size_t cap; } ctw_issues;

typedef enum { CTW_AIR_EPISODE, CTW_AIR_AD, CTW_AIR_UPNEXT } ctw_air_kind;

typedef struct {
    int64_t start_ms;
    uint32_t duration_ms;
    ctw_air_kind kind;
    const ctw_show *show;
    const ctw_episode *episode;
    const ctw_ad *ad;
    int16_t slot_index;
    int16_t item_index;
} ctw_airing;

typedef struct { ctw_airing *items; size_t count; size_t cap; ctw_issues warnings; } ctw_lineup;
```

`season = 0` on an episode means "show without seasons"; `season = 0` on an item means "all seasons". `slot_index`/`item_index` are `-1` for filler airings.

---

### Task 1: Build skeleton, RNG, hashing, time utilities

**Files:**
- Create: `engine/Makefile`, `engine/include/ctw/ctw.h` (all types above plus the declarations below), `engine/src/rng.c`, `engine/src/hash.c`, `engine/src/timeutil.c`, `engine/vendor/greatest/greatest.h`, `engine/tests/main.c`, `engine/tests/test_rng.c`, `engine/tests/test_hash.c`, `engine/tests/test_time.c`

**Interfaces:**
- Produces:
  - `ctw_rng ctw_rng_seed(uint64_t initstate, uint64_t initseq);` (PCG32 `pcg32_srandom_r`)
  - `uint32_t ctw_rng_next(ctw_rng *r);` (PCG32 XSH-RR)
  - `uint32_t ctw_rng_below(ctw_rng *r, uint32_t bound);` (PCG's `pcg32_boundedrand_r`: `threshold = -bound % bound`, redraw while `r < threshold`, return `r % bound`)
  - `void ctw_shuffle(ctw_rng *r, uint32_t *items, size_t n);` (Fisher–Yates from the end: for `i = n-1 … 1`, `j = ctw_rng_below(r, i+1)`, swap)
  - `uint64_t ctw_fnv1a(uint64_t h, const void *data, size_t len);` and `#define CTW_FNV_OFFSET 0xcbf29ce484222325ULL`
  - `uint64_t ctw_day_seed(uint32_t seed, int channel_number, ctw_date day);` (FNV-1a over seed, channel number, `ctw_date_key(day)`, each as 4 big-endian bytes)
  - `int ctw_parse_hhmm(const char *s, uint32_t *out_ms);` (exactly `HH:MM`, `00:00`–`23:59`; returns 0 or -1)
  - `uint32_t ctw_date_key(ctw_date d);` (YYYYMMDD), `ctw_date ctw_date_prev(ctw_date d);`, `ctw_date ctw_date_next(ctw_date d);`, `int ctw_date_parse(const char *s, ctw_date *out);` (`YYYY-MM-DD`)
- Lineup RNG stream constant: `#define CTW_RNG_STREAM 0x63747721ULL`.

- [x] **Step 1: Write the failing tests**

```c
TEST pcg32_reference_sequence(void) {
    ctw_rng r = ctw_rng_seed(42u, 54u);
    uint32_t expected[6] = {0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e};
    for (int i = 0; i < 6; i++) ASSERT_EQ_FMT(expected[i], ctw_rng_next(&r), "%08x");
    PASS();
}
TEST bounded_stays_below(void) {
    ctw_rng r = ctw_rng_seed(1, 2);
    for (int i = 0; i < 10000; i++) ASSERT(ctw_rng_below(&r, 7) < 7);
    PASS();
}
TEST shuffle_is_permutation(void) { /* shuffle 0..99, sort copy, ASSERT equals 0..99 */ }
TEST shuffle_is_deterministic(void) { /* two rngs same seed → identical arrays */ }

TEST fnv_known_values(void) {
    ASSERT_EQ(0xcbf29ce484222325ULL, ctw_fnv1a(CTW_FNV_OFFSET, "", 0));
    ASSERT_EQ(0xaf63dc4c8601ec8cULL, ctw_fnv1a(CTW_FNV_OFFSET, "a", 1));
    PASS();
}
TEST day_seed_varies(void) {
    ctw_date d = {2026, 10, 20}, e = {2026, 10, 21};
    ASSERT(ctw_day_seed(1, 1, d) != ctw_day_seed(1, 1, e));
    ASSERT(ctw_day_seed(1, 1, d) != ctw_day_seed(1, 2, d));
    ASSERT(ctw_day_seed(1, 1, d) != ctw_day_seed(2, 1, d));
    ASSERT_EQ(ctw_day_seed(1, 1, d), ctw_day_seed(1, 1, d));
    PASS();
}

TEST hhmm_parsing(void) {
    uint32_t ms;
    ASSERT_EQ(0, ctw_parse_hhmm("07:00", &ms)); ASSERT_EQ(25200000u, ms);
    ASSERT_EQ(0, ctw_parse_hhmm("23:59", &ms)); ASSERT_EQ(86340000u, ms);
    ASSERT_EQ(0, ctw_parse_hhmm("00:00", &ms)); ASSERT_EQ(0u, ms);
    ASSERT_EQ(-1, ctw_parse_hhmm("24:00", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("07:60", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("7:00", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("07:00 ", &ms));
    PASS();
}
TEST date_arithmetic(void) {
    ctw_date p = ctw_date_prev((ctw_date){2024, 3, 1});
    ASSERT_EQ(2024, p.year); ASSERT_EQ(2, p.month); ASSERT_EQ(29, p.day);
    p = ctw_date_prev((ctw_date){2026, 1, 1});
    ASSERT_EQ(2025, p.year); ASSERT_EQ(12, p.month); ASSERT_EQ(31, p.day);
    p = ctw_date_next((ctw_date){2026, 2, 28});
    ASSERT_EQ(3, p.month); ASSERT_EQ(1, p.day);
    ASSERT_EQ(20261020u, ctw_date_key((ctw_date){2026, 10, 20}));
    PASS();
}
```

(The bracketed bodies above are the assertions to write, not comments to keep.)

- [x] **Step 2: Write `engine/Makefile`** with targets `test` (builds `build/tests` from `src/*.c tests/*.c vendor/tomlc99/toml.c`, `-Iinclude -Ivendor`, runs it), `cli` (builds `build/cable-twii-sched`), `clean`.

- [x] **Step 3: Run tests to verify they fail**

Run: `make -C engine test`
Expected: link errors for `ctw_rng_seed`, `ctw_fnv1a`, `ctw_parse_hhmm`.

- [x] **Step 4: Implement `rng.c`, `hash.c`, `timeutil.c`** with the signatures above.

- [x] **Step 5: Run tests**

Run: `make -C engine test`
Expected: all tests PASS.

- [x] **Step 6: Commit**

```bash
git add engine && git commit -m "wip | engine rng, hashing and time utilities"
```

---

### Task 2: Library parsing

**Files:**
- Create: `engine/vendor/tomlc99/toml.c`, `engine/vendor/tomlc99/toml.h`, `engine/src/arena.c`, `engine/src/issues.c`, `engine/src/library.c`, `engine/tests/fixtures/lib_basic.toml`, `engine/tests/test_library.c`

**Interfaces:**
- Consumes: Task 1.
- Produces:
  - `ctw_db *ctw_db_new(void);` / `void ctw_db_free(ctw_db *db);`
  - `int ctw_library_parse(ctw_db *db, const char *toml_text, ctw_issues *issues);` (0 ok, -1 on any error)
  - `const ctw_show *ctw_db_show(const ctw_db *db, const char *channel_folder, const char *folder);`
  - `size_t ctw_db_show_count(const ctw_db *db); const ctw_show *ctw_db_show_at(const ctw_db *db, size_t i);`
  - `size_t ctw_db_ad_count(const ctw_db *db); const ctw_ad *ctw_db_ad_at(const ctw_db *db, size_t i);`
  - `void ctw_issues_add(ctw_issues *l, ctw_level lv, ctw_code c, int channel, int slot, int item, int line, const char *file, const char *fmt, ...);`, `bool ctw_issues_has_errors(const ctw_issues *l);`, `void ctw_issues_free(ctw_issues *l);`
- `lib_basic.toml` contents (also used by later tasks): channel folder `Cartoon Network` with shows `Gumball` (S1E1–S1E4, S2E1–S2E2, 600000 ms each), `Adventure Time` (S5E1–S5E5, 660000 ms each), `Ben 10` (S1E5, 1320000 ms); channel folder `mtv-00s` with show `Music Videos` (season 0, episodes 1–6, 200000 ms each); shared ads `ads/ad-a.mpg` 30000, `ads/ad-b.mpg` 15000, `ads/ad-c.mpg` 60000. Paths follow spec §4 naming.

- [x] **Step 1: Write the failing tests**

```c
TEST library_basic_counts(void)       { ASSERT_EQ(4, ctw_db_show_count(db)); ASSERT_EQ(3, ctw_db_ad_count(db)); PASS(); }
TEST library_episodes_sorted(void)    { const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
                                        ASSERT_EQ(6, g->episode_count);
                                        ASSERT_EQ(1, g->episodes[0].season); ASSERT_EQ(1, g->episodes[0].episode);
                                        ASSERT_EQ(2, g->episodes[5].season); ASSERT_EQ(2, g->episodes[5].episode);
                                        ASSERT_EQ(600000u, g->episodes[0].duration_ms); PASS(); }
TEST library_unsorted_input_sorted(void) { /* episodes listed S02E01 before S01E01 in input → sorted output */ }
TEST library_missing_duration(void)   { /* episode without duration_ms → -1, issue CTW_E_PARSE, file "library", line > 0 */ }
TEST library_rejects_zero_duration(void) { /* duration_ms = 0 → -1, CTW_E_BAD_VALUE */ }
TEST library_wrong_version(void)      { /* version = 2 → -1, CTW_E_VERSION */ }
TEST library_syntax_error_line(void)  { /* broken TOML on line 7 → CTW_E_PARSE, line == 7 */ }
```

- [x] **Step 2: Run to verify failure**

Run: `make -C engine test`
Expected: FAIL / link errors for `ctw_library_parse`.

- [x] **Step 3: Implement** the arena (all db strings and arrays live in it; `ctw_db_free` releases everything), issue list, and `ctw_library_parse` with tomlc99 (`toml_parse` on a copy of the text; take the line number from its error string `"line N: …"`). Episodes sorted by `(season, episode)` with a stable sort. For field-level errors tomlc99 has no line numbers, so report `line = 0` and put the show/episode in the message.

- [x] **Step 4: Run tests**

Run: `make -C engine test`
Expected: PASS.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | engine library parsing"
```

---

### Task 3: Config parsing, resolution and validation

**Files:**
- Create: `engine/src/config.c`, `engine/tests/test_config.c`

**Interfaces:**
- Consumes: Task 2 (`ctw_db_show`, issues).
- Produces:
  - `int ctw_config_parse(ctw_db *db, const char *toml_text, ctw_issues *issues);` (requires library parsed first; resolves `show` + optional `from` to `const ctw_show *`, `ad` paths to `const ctw_ad *`)
  - `uint32_t ctw_db_seed(const ctw_db *db);`
  - `size_t ctw_db_channel_count(const ctw_db *db); const ctw_channel *ctw_db_channel_at(const ctw_db *db, size_t i);` (sorted by `number`)
  - `const ctw_channel *ctw_db_channel(const ctw_db *db, int number);`
- Rules (spec §5.1, §6.5): `folder` defaults to `name`; `icon` defaults to `<folder>/channel.png`, otherwise is relative to `<folder>/`; slots sorted by `at`; `from` names another channel folder; `count` defaults to 1; `ads_between` defaults to 0; `season` defaults to 0.

- [x] **Step 1: Write the failing tests** (each parses `lib_basic.toml` then an inline config string)

```c
TEST config_defaults(void)            { /* channel without folder/icon: folder == name, icon == "Cartoon Network/channel.png" */ }
TEST config_channels_sorted(void)     { /* numbers 2 then 1 in file → ctw_db_channel_at(db,0)->number == 1 */ }
TEST config_slots_sorted(void)        { /* slots "19:00" then "07:00" → slots[0].at_ms == 25200000 */ }
TEST config_items_resolved(void)      { /* random Adventure Time season 5 count 5 → item.show == ctw_db_show(db,"Cartoon Network","Adventure Time"), season 5, count 5 */ }
TEST config_from_other_channel(void)  { /* channel mtv-00s item { type="random", show="Gumball", from="Cartoon Network" } resolves */ }
TEST config_specific_ad(void)         { /* { type="ad", path="ads/ad-b.mpg" } → item.ad->duration_ms == 15000 */ }
TEST config_dup_channel(void)         { /* two channels number 1 → CTW_E_DUP_CHANNEL */ }
TEST config_dup_slot(void)            { /* two slots "07:00" → CTW_E_DUP_SLOT */ }
TEST config_unknown_show(void)        { /* show = "Nope" → CTW_E_UNKNOWN_SHOW, channel 1, slot 0, item 0 */ }
TEST config_missing_episode(void)     { /* Ben 10 S1E6 → CTW_E_MISSING_EPISODE */ }
TEST config_unknown_ad(void)          { /* ad path not in library → CTW_E_UNKNOWN_AD */ }
TEST config_fill_in_loop(void)        { /* looping channel with fill → CTW_E_FILL_IN_LOOP */ }
TEST config_timed_without_slots(void) { /* mode timed, no slot → CTW_E_NO_SLOTS */ }
TEST config_bad_time(void)            { /* at = "25:00" → CTW_E_BAD_TIME */ }
TEST config_bad_type(void)            { /* type = "movie" → CTW_E_BAD_VALUE */ }
TEST config_bad_mode(void)            { /* mode = "daily" → CTW_E_BAD_VALUE */ }
TEST config_empty_pool_warning(void)  { /* random Gumball season 9 → CTW_W_EMPTY_POOL warning, parse returns 0 */ }
TEST config_crlf_and_bom(void)        { /* same config with "\xEF\xBB\xBF" prefix and \r\n → identical channel/slot/item results */ }
TEST config_unicode_show_name(void)   { /* library show folder "Pokémon" referenced from config → resolves */ }
```

- [x] **Step 2: Run to verify failure**

Run: `make -C engine test`
Expected: FAIL / link errors for `ctw_config_parse`.

- [x] **Step 3: Implement `config.c`.** Strip a leading UTF-8 BOM before `toml_parse`. Collect every error (do not stop at the first) so Studio can show them all; return -1 if any error was added.

- [x] **Step 4: Run tests**

Run: `make -C engine test`
Expected: PASS.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | engine config parsing and validation"
```

---

### Task 4: Pools, shuffle bags and ad picking

**Files:**
- Create: `engine/src/pool.c`, `engine/src/pool.h` (internal), tests in `engine/tests/test_timed.c` (pool tests at the top)

**Interfaces:**
- Consumes: Task 1 RNG, Task 3 items.
- Produces (internal, used by Tasks 5–6):
  - `typedef struct { const ctw_show *show; uint16_t season; uint32_t *order; size_t n; size_t pos; bool shuffled; } ctw_bag;`
  - `typedef struct { ctw_bag *bags; size_t count; size_t cap; uint32_t last_ad; bool has_last_ad; } ctw_bagset;`
  - `ctw_bag *ctw_bagset_get(ctw_bagset *s, const ctw_show *show, uint16_t season);` (pool = the show's episodes filtered by season, in sorted order; created on first use)
  - `const ctw_episode *ctw_bag_draw_fitting(ctw_bag *b, ctw_rng *r, uint32_t max_ms);`
  - `size_t ctw_pool_list(const ctw_show *show, uint16_t season, const ctw_episode **out, size_t cap);`
  - `const ctw_ad *ctw_ads_draw_fitting(const ctw_db *db, const ctw_channel *ch, ctw_bagset *s, ctw_rng *r, uint32_t max_ms);`
  - `void ctw_bagset_free(ctw_bagset *s);`
- Algorithms (determinism depends on these exact rules):
  - Bag: on first draw, `order = 0..n-1`, `ctw_shuffle`. `ctw_bag_draw_fitting` scans `order[pos..n-1]` for the first episode with `duration_ms <= max_ms`, swaps it into `order[pos]`, increments `pos`, returns it. If `pos == n` or nothing fits in the remainder: when `pos == n`, reshuffle (`pos = 0`, `ctw_shuffle`) and scan once more; when items remain but none fit, return NULL without reshuffling. `max_ms = UINT32_MAX` means no limit.
  - Ads: candidates are ads whose `channel_folder` equals `ch->folder`; if there are none, ads with `channel_folder` `""`. Filter by `duration_ms <= max_ms`. If more than one candidate fits, exclude `last_ad`. Pick `candidates[ctw_rng_below(r, k)]` in library order. Update `last_ad`.

- [x] **Step 1: Write the failing tests**

```c
TEST bag_no_repeat_until_empty(void)   { /* Gumball all seasons: 6 draws with max UINT32_MAX → 6 distinct episodes */ }
TEST bag_refills_after_empty(void)     { /* 7th draw returns a non-NULL episode */ }
TEST bag_respects_fit(void)            { /* Ben 10 pool, max 600000 → NULL; max 1320000 → S1E5 */ }
TEST bag_deterministic(void)           { /* two bagsets, same rng seed → same 6-draw sequence */ }
TEST ads_prefer_channel_folder(void)   { /* add ad with channel_folder "Cartoon Network" → only that ad drawn for CN */ }
TEST ads_no_immediate_repeat(void)     { /* 200 draws → no two consecutive identical */ }
TEST ads_fit_limit(void)               { /* max 20000 → only ad-b (15000) */ }
TEST ads_none_fit(void)                { /* max 10000 → NULL */ }
```

- [x] **Step 2: Run to verify failure**

Run: `make -C engine test`
Expected: FAIL.

- [x] **Step 3: Implement `pool.c`.**

- [x] **Step 4: Run tests**

Run: `make -C engine test`
Expected: PASS.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | engine shuffle bags and ad picking"
```

---

### Task 5: Timed lineups

**Files:**
- Create: `engine/src/lineup.c`, `engine/src/lineup_timed.c`
- Modify: `engine/tests/test_timed.c`

**Interfaces:**
- Consumes: Tasks 3–4.
- Produces:
  - `int ctw_lineup_build(const ctw_db *db, const ctw_channel *ch, ctw_date day, ctw_lineup *out);` (0 ok; dispatches on mode)
  - `void ctw_lineup_free(ctw_lineup *l);`
  - `uint64_t ctw_lineup_hash(const ctw_lineup *l);` (FNV-1a over each airing: `start_ms` 8 BE bytes, `duration_ms` 4 BE, `kind` 1 byte, path bytes of episode/ad plus a 0 byte; up-next contributes no path)
- RNG: one `ctw_rng` per build, `ctw_rng_seed(ctw_day_seed(seed, ch->number, day), CTW_RNG_STREAM)`, one `ctw_bagset` per build.
- Timed algorithm (spec §6.3). For slot *i*: `start = at_i`, `end = at_{i+1}` or `at_0 + 86400000` for the last slot; `cursor = start`.
  - `EPISODE`: if `cursor >= end`, add `CTW_W_PARTIAL` and skip. Else `dur = min(ep.duration, end - cursor)`; if cut, add `CTW_W_CUT`.
  - `AD`: same as EPISODE with the ad, kind `CTW_AIR_AD`.
  - `RANDOM`: repeat `count` times `ctw_bag_draw_fitting(bag, rng, end - cursor)`; on NULL add `CTW_W_PARTIAL` and stop the item.
  - `RANDOM_RUN`: list the pool; `n = list length`; if `n < count`, `s = 0` and add `CTW_W_PARTIAL`; else `s = ctw_rng_below(rng, n - count + 1)`. Take episodes from `s` in order while each fits and fewer than `count` are taken; if fewer than `count` are taken, add `CTW_W_PARTIAL`.
  - `FILL`: loop: draw fitting; NULL → stop. After each episode on an ads channel, add up to `ads_between` ads with `ctw_ads_draw_fitting(…, end - cursor)`, stopping at the first NULL.
  - `ADS`: skip on `ads = false`; else up to `count` ads, stopping at the first NULL.
  - Leftover: on an ads channel, add fitting ads until NULL; then if `cursor < end`, add one `CTW_AIR_UPNEXT` airing for `end - cursor`.
  - Lineup covers `[at_0, at_0 + 86400000)` and is contiguous.

- [x] **Step 1: Write the failing tests**

Helper `assert_contiguous(const ctw_lineup *l, int64_t from, int64_t to)`: first start == from, each start == previous start + duration, last end == to.

```c
TEST timed_contiguous_coverage(void)  { /* slots 07:00 + 19:00 mixed items → assert_contiguous(l, 25200000, 25200000 + 86400000) */ }
TEST timed_fixed_episode_cut(void)    { /* 07:00 [episode Ben 10 S1E5], 07:15 [...] → items[0].duration_ms == 900000, warning CTW_W_CUT slot 0 item 0 */ }
TEST timed_random_fits_only(void)     { /* 07:00 [random Adventure Time s5 count 5], 07:30 [...] → exactly 2 CTW_AIR_EPISODE in slot 0, CTW_W_PARTIAL, no airing crosses 27000000 */ }
TEST timed_random_no_repeat(void)     { /* 07:00 [random Gumball count 6], 12:00 → 6 distinct episode pointers */ }
TEST timed_random_run_consecutive(void) { /* random-run Gumball count 3 → episode indexes k, k+1, k+2 in the sorted list */ }
TEST timed_random_run_shortened(void) { /* random-run Adventure Time count 5 in a 30 min slot → 2 consecutive episodes + CTW_W_PARTIAL */ }
TEST timed_fill_with_ads_between(void) { /* fill Gumball ads_between 1, 60 min slot, ads=true → kinds alternate EPISODE, AD … until no episode fits */ }
TEST timed_ads_off(void)              { /* ads=false: ads item yields no AD airings; leftover is exactly one UPNEXT at the end of each slot */ }
TEST timed_specific_ad_on_ads_off(void) { /* ads=false with { type="ad" } → one AD airing */ }
TEST timed_nothing_fits(void)         { /* 07:00 [random Ben 10], 07:10 → no EPISODE in slot 0, CTW_W_PARTIAL, slot fully covered */ }
TEST timed_single_slot(void)          { /* one slot "00:00" → covers [0, 86400000) */ }
TEST timed_deterministic(void)        { /* build twice → same ctw_lineup_hash */ }
TEST timed_seed_changes(void)         { /* seed 1 vs seed 2 → different hash */ }
TEST timed_day_changes(void)          { /* 2026-10-20 vs 2026-10-21 → different hash */ }
```

- [x] **Step 2: Run to verify failure**

Run: `make -C engine test`
Expected: FAIL.

- [x] **Step 3: Implement `lineup.c` and `lineup_timed.c`.** Warnings go into `out->warnings` with channel number, slot and item indexes.

- [x] **Step 4: Run tests**

Run: `make -C engine test`
Expected: PASS.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | engine timed lineups"
```

---

### Task 6: Looping lineups

**Files:**
- Create: `engine/src/lineup_loop.c`, `engine/tests/test_loop.c`

**Interfaces:**
- Consumes: Task 5 `ctw_lineup_build` dispatch.
- Looping algorithm (spec §6.4): `cursor = 0`, `end = 86400000`; walk `loop` items in order, repeating the list, with `max_ms = UINT32_MAX` for draws; `ADS` skipped when `ads = false`; the airing that would cross `end` is cut at `end` (no warning). If a full pass over the list adds no airing, add `CTW_W_LOOP_EMPTY` and one `UPNEXT` covering `[cursor, end)`. `slot_index = -1` for all looping airings; `item_index` is the loop item index.

- [x] **Step 1: Write the failing tests**

```c
TEST loop_covers_day(void)          { /* MTV loop [random Music Videos count 10] → assert_contiguous(l, 0, 86400000) */ }
TEST loop_cut_at_midnight(void)     { /* last airing ends at exactly 86400000 and its duration < 200000 or == 200000 */ }
TEST loop_bag_across_passes(void)   { /* first 6 airings distinct episodes */ }
TEST loop_all_skipped(void)         { /* ads=false, loop [ads count 2] → exactly one UPNEXT [0, 86400000), CTW_W_LOOP_EMPTY */ }
TEST loop_ads_on(void)              { /* ads=true, loop [random count 1, ads count 1] → kinds alternate EPISODE, AD */ }
TEST loop_deterministic(void)       { /* same hash twice */ }
```

- [x] **Step 2: Run to verify failure**

Run: `make -C engine test`
Expected: FAIL.

- [x] **Step 3: Implement `lineup_loop.c`.**

- [x] **Step 4: Run tests**

Run: `make -C engine test`
Expected: PASS.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | engine looping lineups"
```

---

### Task 7: "What's on now" lookup

**Files:**
- Modify: `engine/src/lineup.c`
- Create: `engine/tests/test_lineup_at.c`

**Interfaces:**
- Produces:
  - `const ctw_airing *ctw_lineup_at(const ctw_lineup *today, const ctw_lineup *yesterday, uint32_t ms_of_day, uint32_t *offset_ms);`
    Looks up `ms_of_day` in `today`; if it is before today's first airing, looks up `ms_of_day + 86400000` in `yesterday` (may be NULL for looping channels). Binary search. Returns NULL only if not covered.
  - `const ctw_airing *ctw_lineup_next(const ctw_lineup *l, const ctw_airing *a);` (NULL at the end)

- [x] **Step 1: Write the failing tests**

```c
TEST at_offset_inside(void)        { /* timed 07:00: ms 25205000 → items[0], offset 5000 */ }
TEST at_exact_boundary(void)       { /* ms == items[1].start_ms → items[1], offset 0 */ }
TEST at_spill_from_yesterday(void) { /* slots 07:00 + 19:00; ms 10800000 (03:00) → airing from yesterday's lineup, its start_ms - 86400000 <= 10800000 < end */ }
TEST at_looping_midnight(void)     { /* looping ms 0 → today's items[0], offset 0 */ }
TEST at_next(void)                 { /* ctw_lineup_next(l, &l->items[0]) == &l->items[1]; last → NULL */ }
```

- [x] **Step 2: Run to verify failure**

Run: `make -C engine test`
Expected: FAIL.

- [x] **Step 3: Implement.**

- [x] **Step 4: Run tests**

Run: `make -C engine test`
Expected: PASS.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | engine now lookup"
```

---

### Task 8: Loading a drive folder

**Files:**
- Create: `engine/src/db.c`
- Create: `engine/tests/fixtures/root_basic/config.toml`, `engine/tests/fixtures/root_basic/library.toml` (copy of `lib_basic.toml`)
- Modify: `engine/tests/test_config.c`

**Interfaces:**
- Produces:
  - `int ctw_db_load(const char *root, ctw_db **out, ctw_issues *issues);` reads `<root>/library.toml` then `<root>/config.toml` with `fopen`/`fread`; missing file → `CTW_E_PARSE` with message `"library.toml not found"` / `"config.toml not found"`. On error `*out` is still set (partial db) so callers can show issues; caller frees.
  - `int ctw_db_check_files(const ctw_db *db, const char *root, ctw_issues *issues);` uses `fopen(path, "rb")` on every episode and ad path; missing → `CTW_E_MISSING_FILE`. Used by the CLI, not at Wii boot.
- `root_basic/config.toml`: channel 1 "Cartoon Network" timed with slots `07:00` and `19:00` using every item type; channel 2 "MTV" folder `mtv-00s` looping, `ads = false`.

- [x] **Step 1: Write the failing tests**

```c
TEST load_root_basic(void)       { /* ctw_db_load("tests/fixtures/root_basic") == 0, 2 channels */ }
TEST load_missing_config(void)   { /* root without config.toml → -1, message contains "config.toml not found" */ }
```

- [x] **Step 2–4: Fail, implement, pass** with `make -C engine test`.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | engine load from drive folder"
```

---

### Task 9: `cable-twii-sched` CLI

**Files:**
- Create: `engine/cli/main.c`, `engine/cli/json.c`, `engine/cli/json.h`, `engine/tests/cli_test.sh`

**Interfaces:**
- Consumes: Tasks 5–8.
- Produces (exact JSON shapes, recorded in `docs/formats.md`; Studio plan 02 relies on them):

```
cable-twii-sched validate <root> [--check-files]
  stdout: {"ok":true,"issues":[{"level":"error"|"warning","code":"unknown_show","channel":1,"slot":0,"item":2,"line":0,"file":"config","message":"…"}]}
  exit 0 if no errors, 1 if errors, 2 on usage error

cable-twii-sched lineup <root> --channel N --date YYYY-MM-DD
  stdout: {"channel":1,"date":"2026-10-20","hash":"0123456789abcdef","airings":[
    {"start_ms":25200000,"duration_ms":600000,"kind":"episode"|"ad"|"upnext",
     "path":"Cartoon Network/Gumball/Season 1/S01E02 - Test Two.mpg","show":"Gumball","season":1,"episode":2,
     "title":"Test Two","slot":0,"item":0}],
    "warnings":[ …same shape as issues… ]}

cable-twii-sched now <root> --channel N --at "YYYY-MM-DD HH:MM:SS"
  stdout: {"airing":{…same airing shape…},"offset_ms":5000}

cable-twii-sched hash <root> --channel N --date YYYY-MM-DD
  stdout: 0123456789abcdef
```

Codes are the `ctw_code` names lowercased without the `CTW_E_`/`CTW_W_` prefix. `path`, `show`, `season`, `episode`, `title` are `null` for up-next airings; `slot`/`item` are `-1` for filler. JSON strings escaped per RFC 8259.

- [x] **Step 1: Write `cli_test.sh`** that builds the CLI, runs each command on `tests/fixtures/root_basic`, and checks with `node -e` that: `validate` exits 0 and `ok` is true; `lineup` airings are contiguous and cover 86400000 ms; `now --at "2026-10-20 07:00:05"` gives `offset_ms` 5000; `hash` equals `lineup`'s `hash`; a config with an unknown show exits 1 with code `unknown_show`.

- [x] **Step 2: Run to verify failure**

Run: `bash engine/tests/cli_test.sh`
Expected: FAIL (binary missing).

- [x] **Step 3: Implement the CLI** and a minimal JSON writer (`json_str`, `json_int`, object/array helpers).

- [x] **Step 4: Run**

Run: `make -C engine cli test && bash engine/tests/cli_test.sh`
Expected: all checks print `ok`, exit 0.

- [x] **Step 5: Commit**

```bash
git add engine && git commit -m "wip | cable-twii-sched cli"
```

---

### Task 10: Wii cross-check (big-endian agreement)

**Files:**
- Create: `engine/wii-check/Makefile`, `engine/wii-check/source/main.c`, `engine/wii-check/data/` (none; reads from SD)
- Create: `engine/tests/wii_check.md` (how to run)

**Interfaces:**
- Consumes: engine sources compiled by devkitPPC.

- [x] **Step 1: Build a Wii program** that mounts `sd:`, loads `sd:/cable-twii/` with `ctw_db_load`, and prints `ctw_lineup_hash` for every channel for dates `2026-10-20`, `2026-10-21`, `2026-12-31`, `2027-01-01`.

- [x] **Step 2: Produce the expected hashes on the Mac**

Run: `for d in 2026-10-20 2026-10-21 2026-12-31 2027-01-01; do for c in 1 2; do engine/build/cable-twii-sched hash engine/tests/fixtures/root_basic --channel $c --date $d; done; done`
Expected: 8 hex lines.

- [x] **Step 3: Run the Wii program in Dolphin** with the virtual SD card containing `cable-twii/` = `root_basic`.
Expected: the same 8 hashes in the same order. Any difference is a determinism bug: fix in the engine with a host test that reproduces it.

- [x] **Step 4: Commit**

```bash
git add engine && git commit -m "test | wii big-endian lineup cross-check"
```
