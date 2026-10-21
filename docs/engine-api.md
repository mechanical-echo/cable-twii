# cable-twii engine C API

This page is for the Wii app and for anyone else who calls the schedule engine from C. The engine is a portable C11 library in `engine/`. Its only public header is `engine/include/ctw/ctw.h`. The file formats it reads are described in [formats.md](formats.md), which is the binding contract; this page explains how to use the functions safely.

## The short version

1. Load the root folder with `ctw_db_load`. Always free the db with `ctw_db_free`, even when loading failed.
2. If `ctw_db_load` returns `-1`, show the issues to the owner. The db is only partly filled.
3. Build a lineup per channel and date with `ctw_lineup_build`. Free it with `ctw_lineup_free`. Free the db only after every lineup is gone.
4. To find what is on now, build today's and yesterday's lineups and call `ctw_lineup_at`.
5. To move on when an airing ends, use `ctw_lineup_next`; when it returns `NULL`, call `ctw_lineup_at` again at the airing's end time.

```c
ctw_db *db = NULL;
ctw_issues issues = {0};
if (ctw_db_load("sd:/cable-twii/", &db, &issues)) {
    show_issues(&issues);
}
for (size_t i = 0; i < ctw_db_channel_count(db); i++) {
    const ctw_channel *ch = ctw_db_channel_at(db, i);
    ctw_lineup l = {0};
    if (ctw_lineup_build(db, ch, (ctw_date){2026, 10, 20}, &l) == 0) {
        use_lineup(ch, &l);
        ctw_lineup_free(&l);
    }
}
ctw_issues_free(&issues);
ctw_db_free(db);
```

## Rules that are easy to get wrong

### `ctw_db_load` always sets `*out`

`ctw_db_load(root, &db, &issues)` always writes `*out`, on success and on failure. The caller must call `ctw_db_free(db)` in both cases. `*out` is `NULL` only when memory for the db itself could not be allocated, and `ctw_db_free(NULL)` is safe.

### `-1` means the db is partial

`ctw_db_load`, `ctw_library_parse` and `ctw_config_parse` return `0` when there are no errors and `-1` when there is at least one error. On `-1` the db is still usable but incomplete:

- channels, slots and items that had an error are left out, and so are shows, episodes and ads that had an error
- if `library.toml` is missing or broken, `config.toml` is not read at all, so there are no channels
- a timed channel or a loop can lose items, so its lineup can differ from what the owner wrote

So honour `-1`: show the issues (see "Issues" below) and do not present the result as the owner's real schedule without saying something is wrong.

### Lineups point into the db

Every `ctw_airing` holds pointers to a `ctw_show`, `ctw_episode` or `ctw_ad` that live inside the db, and every string (paths, titles, names) is owned by the db's arena. The db must outlive every lineup built from it and every pointer or string you took from it. Free lineups first, then the db.

### `ctw_lineup_build` overwrites `out`

`ctw_lineup_build(db, ch, day, &l)` clears `l` before it starts and does not free what was there. To rebuild into the same variable, free it first:

```c
ctw_lineup_free(&l);
if (ctw_lineup_build(db, ch, day, &l) != 0) {
    handle_error();
}
```

On failure it returns `-1`, and anything it built is already freed (calling `ctw_lineup_free` again is safe). It fails for a `NULL` argument, a timed channel with no slots, a `fill` item in a loop, or when memory runs out. A db that loaded without errors never gives the first three.

### `start_ms` can pass midnight

`start_ms` is milliseconds since local midnight of the lineup's date, and coverage has no gaps:

- a looping channel covers `[0, 86400000)`
- a timed channel covers `[first slot at, first slot at + 86400000)`, so airings in the last slot can start at or after `86400000`; they air in the early hours of the next date

To show a time, use `start_ms % 86400000`. An airing whose `start_ms >= 86400000` belongs to the next calendar date.

```c
uint32_t clock_ms = (uint32_t)(a->start_ms % CTW_DAY_MS);
int next_day = a->start_ms >= CTW_DAY_MS;
```

### Size limits

- A lineup holds at most 20000 airings. If placing would go past that, the engine stops, covers the rest of the lineup with one "Up next" airing (`kind` `CTW_AIR_UPNEXT`, slot and item `-1`) and adds one `partial` warning with the message `lineup reached 20000 airings`. The lineup still has no gaps.
- Every episode and ad needs `duration_ms` of at least 1000 (one second). Smaller values are `bad_value` when the library loads. With normal video lengths, lineups stay far below the cap.
- Arrays and inline tables in the TOML files may nest at most 64 deep (`parse`, `nesting too deep`), so the parser stays small enough for the Wii's stack.

## Finding what is on now

A timed channel's lineup for yesterday spills into today's early hours, before today's first slot. So keep two lineups per channel: today's and yesterday's.

```c
ctw_date today = local_date();
uint32_t ms_of_day = local_ms_since_midnight();
ctw_lineup l_today = {0};
ctw_lineup l_yesterday = {0};
uint32_t offset = 0;
const ctw_airing *a;

ctw_lineup_build(db, ch, today, &l_today);
ctw_lineup_build(db, ch, ctw_date_prev(today), &l_yesterday);
a = ctw_lineup_at(&l_today, &l_yesterday, ms_of_day, &offset);
if (a) {
    play_from(a, offset);
}
```

`ctw_lineup_at(today, yesterday, ms_of_day, &offset)`:

- looks in `today` first; if nothing there covers `ms_of_day`, it looks in `yesterday` at `ms_of_day + 86400000`
- returns a pointer into `today` or `yesterday`, or `NULL` if nothing covers the time, if `today` is `NULL`, or if `ms_of_day >= 86400000`
- sets `*offset` to how far into the airing the time is (0 when it returns `NULL`); `offset` may be `NULL`
- accepts `yesterday == NULL` (looping channels never need it)

To know which lineup the airing came from, compare pointers:

```c
int from_today = a >= l_today.items && a < l_today.items + l_today.count;
```

## Moving to the next airing

`ctw_lineup_next(l, a)` returns the airing after `a` in the same lineup `l`. It returns `NULL` when `a` is the last one, or when `a` is not inside `l`. Pass the lineup that `a` came from.

At the end of a lineup, `NULL` does not mean the channel stops. Call `ctw_lineup_at` again at the moment the airing ends. This works across midnight and across the yesterday/today boundary:

```c
static const ctw_airing *after(const ctw_airing *a, const ctw_lineup *from,
                               const ctw_lineup *l_today, const ctw_lineup *l_yesterday)
{
    const ctw_airing *n = ctw_lineup_next(from, a);
    int64_t end;
    if (n) return n;
    end = a->start_ms + (int64_t)a->duration_ms;
    if (from == l_yesterday) end -= CTW_DAY_MS;
    if (end >= CTW_DAY_MS) {
        roll_to_next_day();
        end -= CTW_DAY_MS;
    }
    return ctw_lineup_at(l_today, l_yesterday, (uint32_t)end, NULL);
}
```

`roll_to_next_day()` stands for: free yesterday's lineup, make today's lineup the new yesterday, build the new date's lineup as today. After a roll, pass the new lineups to `ctw_lineup_at`. In a real player it is simpler to always ask `ctw_lineup_at` with the current clock time when an airing ends; the result is the same.

## Issues

`ctw_issues` is a growable list. Start it zeroed and free it with `ctw_issues_free` when you are done, whether or not anything was added:

```c
ctw_issues issues = {0};
ctw_db_load(root, &db, &issues);
for (size_t i = 0; i < issues.count; i++) {
    const ctw_issue *is = &issues.items[i];
    printf("%s %s %s: %s\n", ctw_level_name(is->level), ctw_code_name(is->code), is->file,
           is->message);
}
ctw_issues_free(&issues);
```

Each `ctw_issue` has `level`, `code`, `channel`, `slot`, `item` (`-1` when not tied to one), `line` (`0` when unknown), `file` (`"config"`, `"library"` or `"lineup"`) and `message`. Lineup warnings live in `ctw_lineup.warnings` and are freed by `ctw_lineup_free`; do not free them separately. Their `file` is `"lineup"`, and `slot` and `item` refer to the slots and items in the channel's `config.toml` entry.

## Thread safety

None. The engine has no global state, but nothing in it is locked. Use one db per thread, or put a lock around every call that touches a shared db and the lineups built from it.

## Memory

- The db owns all strings and all show, episode, ad and channel records, in one arena. They stay valid until `ctw_db_free`. Do not free them yourself and do not keep them after the db is freed.
- `ctw_library_parse` and `ctw_config_parse` copy what they need, so you may free the TOML text right after the call.
- A lineup owns only its `items` array and its `warnings`; `ctw_lineup_free` releases both and leaves the lineup empty, so it is safe to call twice.
- `ctw_issues_add` drops the issue silently if memory runs out.

## Function reference

### Random numbers and hashing

| Function | Returns |
|---|---|
| `ctw_rng ctw_rng_seed(uint64_t initstate, uint64_t initseq)` | A PCG32 generator seeded from the two values. |
| `uint32_t ctw_rng_next(ctw_rng *r)` | The next 32-bit random number. |
| `uint32_t ctw_rng_below(ctw_rng *r, uint32_t bound)` | A uniform number in `[0, bound)`; `0` when `bound` is 0 (and the generator does not move). |
| `void ctw_shuffle(ctw_rng *r, uint32_t *items, size_t n)` | Shuffles `items` in place (Fisher-Yates). |
| `uint64_t ctw_fnv1a(uint64_t h, const void *data, size_t len)` | FNV-1a 64 of `data`, continuing from `h` (start with `CTW_FNV_OFFSET`). |
| `uint64_t ctw_day_seed(uint32_t seed, int channel_number, ctw_date day)` | The per-channel, per-day seed the lineup builder uses. |

### Time and dates

| Function | Returns |
|---|---|
| `int ctw_parse_hhmm(const char *s, uint32_t *out_ms)` | `0` and `*out_ms` for a valid `HH:MM` (00:00 to 23:59), else `-1`. |
| `uint32_t ctw_date_key(ctw_date d)` | `YYYYMMDD` as a number. |
| `ctw_date ctw_date_prev(ctw_date d)` / `ctw_date_next(ctw_date d)` | The day before or after, with leap years. |
| `int ctw_date_parse(const char *s, ctw_date *out)` | `0` for an exact `YYYY-MM-DD` that is a real date, else `-1` (`*out` unchanged). |

### Loading

| Function | Returns |
|---|---|
| `ctw_db *ctw_db_new(void)` | An empty db, or `NULL` when out of memory. |
| `void ctw_db_free(ctw_db *db)` | Frees the db and everything it owns. `NULL` is safe. |
| `int ctw_db_load(const char *root, ctw_db **out, ctw_issues *issues)` | Reads `library.toml` then `config.toml` from `root`. `0` without errors, `-1` with errors. Always sets `*out` (see above). Each file may be at most 16 MiB and must not contain a NUL byte. |
| `int ctw_library_parse(ctw_db *db, const char *toml_text, ctw_issues *issues)` | Parses library text into an empty db. `0` or `-1`. |
| `int ctw_config_parse(ctw_db *db, const char *toml_text, ctw_issues *issues)` | Parses config text into a db that already has its library. `0` or `-1`. |
| `int ctw_db_check_files(const ctw_db *db, const char *root, ctw_issues *issues)` | Opens every episode and ad path under `root`; `0` if all exist, `-1` and one `missing_file` error per missing file otherwise. |

### Reading the db

| Function | Returns |
|---|---|
| `uint32_t ctw_db_seed(const ctw_db *db)` | The config's `seed`. |
| `size_t ctw_db_channel_count(const ctw_db *db)` | Number of loaded channels, sorted by number. |
| `const ctw_channel *ctw_db_channel_at(const ctw_db *db, size_t i)` | Channel `i`, or `NULL` when out of range. |
| `const ctw_channel *ctw_db_channel(const ctw_db *db, int number)` | The channel with that number, or `NULL`. |
| `size_t ctw_db_show_count(const ctw_db *db)` / `const ctw_show *ctw_db_show_at(const ctw_db *db, size_t i)` | Shows in library order; `NULL` when out of range. |
| `const ctw_show *ctw_db_show(const ctw_db *db, const char *channel_folder, const char *folder)` | The show with that channel folder and folder, matched byte for byte, or `NULL`. |
| `size_t ctw_db_ad_count(const ctw_db *db)` / `const ctw_ad *ctw_db_ad_at(const ctw_db *db, size_t i)` | Ads in library order; `NULL` when out of range. |

The db argument must not be `NULL` for these. A show's episodes are sorted by season, then episode.

### Issues

| Function | Returns |
|---|---|
| `void ctw_issues_add(ctw_issues *l, ctw_level lv, ctw_code c, int channel, int slot, int item, int line, const char *file, const char *fmt, ...)` | Appends one issue (printf-style message, cut to 199 bytes; `file` cut to 15). |
| `bool ctw_issues_has_errors(const ctw_issues *l)` | `true` if any issue is an error. `NULL` is `false`. |
| `void ctw_issues_free(ctw_issues *l)` | Frees the list and leaves it empty. `NULL` is safe. |
| `const char *ctw_code_name(ctw_code c)` | The lowercase name from formats.md, for example `"bad_value"` or `"partial"`. |
| `const char *ctw_level_name(ctw_level l)` | `"error"` or `"warning"`. |

### Lineups

| Function | Returns |
|---|---|
| `int ctw_lineup_build(const ctw_db *db, const ctw_channel *ch, ctw_date day, ctw_lineup *out)` | `0` and a full lineup in `*out`, or `-1` and an empty `*out`. Same db, channel and date always give the same lineup, on the Mac and on the Wii. |
| `void ctw_lineup_free(ctw_lineup *l)` | Frees the airings and warnings. `NULL` is safe. |
| `uint64_t ctw_lineup_hash(const ctw_lineup *l)` | The lineup hash defined in formats.md (`CTW_FNV_OFFSET` for `NULL` or empty). |
| `const ctw_airing *ctw_lineup_at(const ctw_lineup *today, const ctw_lineup *yesterday, uint32_t ms_of_day, uint32_t *offset_ms)` | The airing on at that time, or `NULL` (see "Finding what is on now"). |
| `const ctw_airing *ctw_lineup_next(const ctw_lineup *l, const ctw_airing *a)` | The following airing in `l`, or `NULL` at the end or when `a` is not in `l`. |
