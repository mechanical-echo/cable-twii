# cable-twii file formats (frozen)

This is the contract between the C schedule engine, cable-twii Studio (on the Mac) and the Wii app. If this file and the code disagree, the code is wrong.

Sources: design spec sections 5, 6.5, 6.7 and 8.1a; engine plan 01 Task 9 and the `ctw_code` list; Studio plan 02 (shared types, Task 4, Task 5); Wii app plan 03 (`state.toml`).

## Rules for every file

- **Versioning.** Everything is `version = 1`. Any change to any format bumps `version` and updates the engine and Studio in the same commit.
- **Encoding.** UTF-8. The engine accepts an optional UTF-8 BOM at the start and CRLF (`\r\n`) line endings, and treats them exactly like a clean file. Studio writes no BOM and uses LF.
- **Syntax.** TOML. Parsed with `tomlc99` in the engine and `smol-toml` in Studio.
- **Nesting.** Brackets `[` and braces `{` outside strings and comments may nest at most 64 deep (a table header such as `[[channel.slot]]` counts only while it is open). Deeper input is `parse` with the message `nesting too deep` and `line` set to the line where the limit was passed; the file is not parsed further. This keeps the parser's recursion small enough for the Wii's stack.
- **Paths** are relative to the `/cable-twii/` root and use `/` as the separator, on every platform. Never a leading `/`, never `..`.
- **Names** (show, folder, channel) are matched byte for byte, including accents (`Pokémon`).
- **Time** is written `HH:MM`, 24-hour, two digits each, from `00:00` to `23:59`. `24:00`, `7:00` and `07:00:00` are all `bad_time`.
- **Slot window.** A slot runs from its `at` until the next slot's `at`. The last slot runs until the first slot's `at` on the next day.
- **`season = 0`.** On an episode (in `library.toml`) it means a show without seasons.
- **Season on items.** On an `episode` item, `season` must equal the library episode's season exactly, so `season = 0` there targets an episode of a seasonless show (library season 0). On `random`, `random-run` and `fill` items, `season = 0` means all seasons, the same as leaving `season` out. These two meanings are the whole rule.
- **Unknown keys** are ignored by the engine.
- Hidden macOS files (`._*`, `.DS_Store`) are ignored everywhere.

## Files at a glance

| File | Location (under `/cable-twii/`) | Written by | Read by |
|---|---|---|---|
| `config.toml` | `config.toml` | Studio (the owner may hand-edit) | engine (Wii and CLI), Studio |
| `library.toml` | `library.toml` | Studio | engine (Wii and CLI), Studio |
| `show.toml` | `<channel folder>/<show folder>/show.toml` | Studio | Studio only (folded into `library.toml`) |
| `state.toml` | `state.toml` | Wii app | Wii app |
| `sources.toml` | `.studio/sources.toml` | Studio | Studio only (the Wii ignores `.studio/`) |
| `queue.toml` | `.studio/queue.toml` | Studio | Studio only |

## Validation codes

Codes appear in the CLI JSON as lowercase names, from the engine's `ctw_code` names without the `CTW_E_` or `CTW_W_` prefix. Errors make `validate` exit 1 and Studio refuses to save the config. Warnings never block.

| Code | Level | Meaning |
|---|---|---|
| `parse` | error | File missing, not valid TOML, or a required key is missing. `line` is set for syntax errors, 0 for field errors. |
| `version` | error | `version` is not 1. |
| `bad_value` | error | A value is out of range or not allowed: unknown `type` or `mode`, `loop` on a timed channel, `slot` on a looping channel, a looping channel with no or an empty `loop`, or a number outside its range (`number`, `season`, `episode`, `count`, `ads_between`, `duration_ms`). |
| `bad_time` | error | A slot `at` is not a valid `HH:MM`. |
| `dup_channel` | error | Two channels have the same `number`. |
| `dup_slot` | error | Two slots in one channel have the same `at`. |
| `unknown_show` | error | An item's `show` (with `from` if given) is not in the library. |
| `missing_episode` | error | An `episode` item names a season and episode that the show does not have. |
| `unknown_ad` | error | An `ad` item's `path` is not an ad in the library. |
| `fill_in_loop` | error | A `fill` item in a looping channel. |
| `no_slots` | error | A timed channel without any slot. |
| `empty_pool` | error | A `random`, `random-run` or `fill` item whose show and season have no episodes. |
| `missing_file` | error | A file listed in the library does not exist on the drive. Only checked with `validate --check-files`. |
| `cut` | warning | A fixed episode or ad was cut at the next slot's start. |
| `partial` | warning | A `random`/`random-run` item placed fewer than `count` episodes, or a fixed item could not start because its slot was already full, or the lineup reached its cap of 20000 airings (slot and item `-1`, message `lineup reached 20000 airings`). |
| `loop_empty` | warning | A looping channel placed nothing in a full pass (for example only `ads` items on a channel with `ads = false`). The day is one "Up next" card. |

Each issue carries `channel` (channel number), `slot` and `item` (zero-based), `line`, the `file` and a human message. `file` is `"config"`, `"library"` or `"lineup"`; warnings from building a lineup use `"lineup"`, and their `slot` and `item` refer to the slots and items in the channel's `config.toml` entry (for a looping channel `slot` is `-1` and `item` is the loop item index). When an issue is not tied to a channel, slot or item, those fields are `-1`. `line` is `0` when unknown. Slot indexes in issues refer to the slots sorted by `at`; Studio always writes slots sorted.

A key with the wrong TOML type is `parse`; a value of the right type outside its allowed range is `bad_value`.

---

## `config.toml`

**Purpose:** the channels and their schedules.
**Location:** `config.toml`. **Writer:** Studio (the owner may hand-edit; Studio does not keep hand-written comments when it saves). **Readers:** engine, Studio.

### Top level

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `version` | integer | yes | none | `1` |
| `seed` | integer | yes | none | 0 to 4294967295. Studio generates a random 31-bit value. Changing it reshuffles every random pick. |
| `channel` | array of tables (`[[channel]]`) | no | none | zero or more |

### `[[channel]]`

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `number` | integer | yes | none | 1 to 999, unique |
| `name` | string | yes | none | any text |
| `category` | string | yes | none | any text |
| `folder` | string | no | `name` | the channel's folder name under `/cable-twii/` |
| `icon` | string | no | `<folder>/channel.png` | a file name relative to `<folder>/` |
| `ads` | boolean | yes | none | `true` or `false` |
| `mode` | string | yes | none | `"timed"` or `"looping"` |
| `slot` | array of tables (`[[channel.slot]]`) | timed channels | none | at least one for `timed` (`no_slots` otherwise) |
| `loop` | array of inline tables (items) | looping channels | none | the items played in order, repeated all day |

Channels are sorted by `number` by the engine and by Studio when it writes.

### `[[channel.slot]]`

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `at` | string | yes | none | `HH:MM`, unique within the channel |
| `items` | array of inline tables | yes | none | items, in play order |

Slots are sorted by `at` by the engine and by Studio when it writes.

### Items

Every item has a `type`. Other fields depend on the type. A field marked `?` is optional.

| `type` | Fields | Meaning |
|---|---|---|
| `episode` | `show`, `season`, `episode`, `from`? | One specific episode (`season` must match exactly, see the season rule above). Cut off at the window end if it does not fit (`cut` warning). |
| `random` | `show`, `season`?, `count`?, `from`? | `count` random episodes, no repeats within the item. Only episodes that fit the remaining time are chosen. |
| `random-run` | `show`, `season`?, `count`?, `from`? | A random starting episode, then the following episodes in order. The start is chosen only among episodes that fit the remaining time (if none fits, nothing is placed and the item gets `partial`). If the full run does not fit, the longest run that fits is used. |
| `ads` | `count`? | `count` random ads. Skipped when the channel has `ads = false`. |
| `ad` | `path` | One specific ad, path relative to `/cable-twii/`. It always airs, even on a channel with `ads = false`, because it was placed deliberately. |
| `fill` | `show`, `season`?, `ads_between`?, `from`? | Random episodes until the slot is full. Timed channels only (`fill_in_loop` in a loop). |

| Field | Type | Default | Allowed |
|---|---|---|---|
| `show` | string | none | the show's folder name inside the channel's folder (or inside `from`) |
| `season` | integer | `0` (all seasons) | 0 to 65535. Required on `episode`, where it must match the library season exactly; on `random`, `random-run` and `fill`, 0 means all seasons (see the season rule above). |
| `episode` | integer | none | 1 to 65535, `episode` type only |
| `count` | integer | `1` | 1 to 65535 |
| `ads_between` | integer | `0` | 0 to 65535, number of ads between episodes (ignored on channels with `ads = false`) |
| `path` | string | none | `ad` type only; an ad listed in `library.toml` |
| `from` | string | the channel's own folder | another channel's folder name. Lets a show that lives in another channel's folder air here without copying files. Allowed on any item that has a `show`. |

### Ads

Random ads come from the channel's own `ads/` folder if it has any, otherwise from the shared `/cable-twii/ads/`. The same ad never plays twice in a row.

### Validation

`parse`, `version`, `bad_value` (unknown `type` or `mode`; `loop` on a timed channel; `slot` on a looping channel; a looping channel with no `loop` or an empty `loop`; any numeric field out of range: `number`, `season`, `episode`, `count`, `ads_between`, `duration_ms`), `bad_time`, `dup_channel`, `dup_slot`, `unknown_show`, `missing_episode`, `unknown_ad`, `fill_in_loop`, `no_slots`, `empty_pool`. Warnings from building lineups: `cut`, `partial`, `loop_empty`.

### Complete example

```toml
version = 1
seed = 1234

[[channel]]
number   = 1
name     = "Cartoon Network"
category = "Kids"
folder   = "Cartoon Network"
icon     = "channel.png"
ads      = true
mode     = "timed"

  [[channel.slot]]
  at = "07:00"
  items = [
    { type = "random-run", show = "Gumball", count = 3 },
    { type = "ads", count = 1 },
    { type = "random", show = "Adventure Time", season = 5, count = 5 },
    { type = "ads", count = 2 },
    { type = "episode", show = "Ben 10", season = 1, episode = 5 },
    { type = "random", show = "Gumball", count = 2 },
    { type = "ad", path = "ads/soda-commercial.mpg" },
  ]

  [[channel.slot]]
  at = "19:00"
  items = [
    { type = "fill", show = "Adventure Time", ads_between = 2 },
  ]

[[channel]]
number   = 2
name     = "MTV"
category = "Music"
folder   = "mtv-00s"
ads      = false
mode     = "looping"
loop = [
  { type = "random", show = "Music Videos", count = 10 },
  { type = "random", show = "Gumball", from = "Cartoon Network", count = 1 },
]
```

---

## `library.toml`

**Purpose:** the index of every video and ad with its duration. The Wii app trusts it and never scans the drive or opens a video to learn a duration.
**Location:** `library.toml`. **Writer:** Studio (machine-generated, atomic write). **Readers:** engine, Studio.

### Top level

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `version` | integer | yes | none | `1` |
| `generated` | TOML local date-time | no | none | informational, ignored by the engine |
| `ad` | array of tables (`[[ad]]`) | no | none | |
| `show` | array of tables (`[[show]]`) | no | none | |

### `[[ad]]`

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `path` | string | yes | none | relative to `/cable-twii/`, for example `ads/soda-commercial.mpg` |
| `channel` | string | yes | none | `""` for a shared ad, otherwise the channel folder name that owns it |
| `duration_ms` | integer | yes | none | 1000 to 4294967295, so at least one second (outside the range is `bad_value`) |

### `[[show]]`

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `channel` | string | yes | none | the channel folder name that holds the show |
| `folder` | string | yes | none | the show's folder name |
| `name` | string | yes | none | display name; from `show.toml`, else the folder name |
| `icon` | string | yes | none | path to the show icon, relative to `/cable-twii/`; `""` if none |
| `episode` | array of tables (`[[show.episode]]`) | yes | none | the episodes; the engine sorts them by season then episode |

A show is identified by the pair (`channel`, `folder`).

### `[[show.episode]]`

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `season` | integer | yes | none | `0` for a show without seasons, otherwise 1 or more |
| `episode` | integer | yes | none | 1 or more. In a show without seasons it is the 1-based position in filename order. |
| `title` | string | no | `""` | |
| `description` | string | no | `""` | |
| `path` | string | yes | none | relative to `/cable-twii/`, `/` separators, FAT-safe |
| `duration_ms` | integer | yes | none | 1000 to 4294967295, so at least one second (outside the range is `bad_value`, a missing value is `parse`) |

### Validation

`parse` (including a missing `duration_ms`), `version`, `bad_value` (`duration_ms` outside 1000 to 4294967295), `missing_file` (with `--check-files`).

### Complete example

```toml
version = 1
generated = 2026-10-20T14:00:00

[[ad]]
path = "ads/soda-commercial.mpg"
channel = ""
duration_ms = 30030

[[show]]
channel = "Cartoon Network"
folder = "Gumball"
name = "The Amazing World of Gumball"
icon = "Cartoon Network/Gumball/show.png"

  [[show.episode]]
  season = 1
  episode = 1
  title = "The DVD"
  description = "Gumball and Darwin make a TV ad for the video store."
  path = "Cartoon Network/Gumball/Season 1/S01E01 - The DVD.mpg"
  duration_ms = 665120

[[show]]
channel = "mtv-00s"
folder = "Music Videos"
name = "Music Videos"
icon = ""

  [[show.episode]]
  season = 0
  episode = 1
  title = "Artist - Song"
  description = ""
  path = "mtv-00s/Music Videos/Artist - Song.mpg"
  duration_ms = 215000
```

---

## `show.toml`

**Purpose:** optional display name and episode descriptions for one show. Studio folds them into `library.toml`; the Wii never reads it.
**Location:** `<channel folder>/<show folder>/show.toml`. **Writer:** Studio. **Reader:** Studio.

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `name` | string | no | the show's folder name | display name |
| `[episodes]` | table | no | empty | keys are episode keys, values are descriptions (strings) |

Episode keys: `SxxEyy` (for example `S01E01`, two digits at least) for shows with seasons, or the **file title** for shows without seasons (the file name without the extension, for example `Artist - Song`).

There is no `version` key in this file (spec section 5.3); it is version 1 by definition and the next format change adds `version`.

### Complete example

```toml
name = "The Amazing World of Gumball"

[episodes]
"S01E01" = "Gumball and Darwin make a TV ad for the video store."
"S01E02" = "Gumball tries to be responsible."
```

Seasonless show:

```toml
name = "Music Videos"

[episodes]
"Artist - Song" = "The one with the cat."
```

---

## `state.toml`

**Purpose:** remembers the last channel and the volume between sessions.
**Location:** `state.toml`. **Writer:** the Wii app (the only file it writes; it writes `state.toml.tmp` and renames). **Reader:** the Wii app.

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `last_channel` | integer | no | `0` (guide opens on the first channel) | a channel `number` |
| `volume` | integer | no | `80` | 0 to 100 (values outside are clamped) |

A missing or broken file means the defaults. There is no `version` key (spec section 5.4).

### Complete example

```toml
last_channel = 1
volume = 80
```

---

## `.studio/sources.toml`

**Purpose:** maps every converted file on the drive to its original file on the Mac, so unchanged sources are not converted again.
**Location:** `.studio/sources.toml`. **Writer and reader:** Studio only (atomic write: temp file, then rename). The Wii ignores `.studio/`.

Top level: `version` (integer, required, `1`) and `source` (array of tables `[[source]]`, one per converted file).

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `target` | string | yes | none | converted file path relative to `/cable-twii/` |
| `source` | string | yes | none | absolute path of the original on the Mac |
| `size` | integer | yes | none | size of the original in bytes |
| `mtimeMs` | integer | yes | none | modified time of the original, milliseconds since 1970 |
| `durationMs` | integer | yes | none | duration of the converted file in milliseconds |

A source needs converting again when there is no record, or the original's `size` or `mtimeMs` changed, or the target file is missing.

### Complete example

```toml
version = 1

[[source]]
target = "Cartoon Network/Gumball/Season 1/S01E01 - The DVD.mpg"
source = "/Users/owner/Movies/Gumball/S01E01.mkv"
size = 412345678
mtimeMs = 1760000000000
durationMs = 665120
```

---

## `.studio/queue.toml`

**Purpose:** the conversion queue, so an interrupted batch resumes after Studio restarts or the drive is replugged.
**Location:** `.studio/queue.toml`. **Writer and reader:** Studio only.

Top level: `version` (integer, required, `1`) and `job` (array of tables `[[job]]`). Only queued and running jobs are stored. On load every job comes back as `queued`.

| Key | Type | Required | Default | Allowed |
|---|---|---|---|---|
| `id` | string | yes | none | unique job id |
| `source` | string | yes | none | absolute path of the original on the Mac |
| `target` | string | yes | none | output path relative to `/cable-twii/` (written as `<target>.part`, renamed on success) |
| `aspect` | string | yes | none | `"16:9"` or `"4:3"` |

`state`, `progress` and `error` are runtime only and not stored.

### Complete example

```toml
version = 1

[[job]]
id = "j-0001"
source = "/Users/owner/Movies/Gumball/S01E02.mkv"
target = "Cartoon Network/Gumball/Season 1/S01E02 - The Responsible.mpg"
aspect = "16:9"
```

---

## CLI `cable-twii-sched`

Built natively on the Mac (`engine/build/cable-twii-sched`; Studio can override the path with `CTW_SCHED_BIN`). Output is JSON on stdout, strings escaped per RFC 8259. `<root>` is the folder that holds `config.toml` and `library.toml`.

Exit codes and output, for every command:

| Exit | Meaning | stdout | stderr |
|---|---|---|---|
| 0 | Success. | The command's JSON (`hash`: the hash). | empty |
| 1 | The root failed to load, or `validate` found errors. | `{"ok":false,"issues":[...]}`, the same JSON as `validate`, for every command (`lineup`, `now` and `hash` too). | empty |
| 2 | Usage error: unknown command, missing or malformed `--channel`, `--date` or `--at`, or a channel number not in the config. | nothing | plain text message |

Malformed arguments are checked before the root is loaded, so they exit 2 even when the root has errors; only an unknown channel number is resolved after loading (exit 1 wins there).

If `now` asks for a time that no airing covers, the output is `{"airing":null,"offset_ms":0}` with exit 0.

Issue fields are `-1` when not tied to a channel, slot or item, and `line` is `0` when unknown.

### `validate`

```
cable-twii-sched validate <root> [--check-files]
```

`--check-files` also opens every episode and ad path and reports `missing_file`. Exit 0 if there are no errors, 1 if there are errors, 2 on a usage error.

```json
{"ok":false,"issues":[
  {"level":"error","code":"unknown_show","channel":1,"slot":0,"item":2,"line":0,"file":"config","message":"show \"Nope\" not found in Cartoon Network"},
  {"level":"error","code":"parse","channel":-1,"slot":-1,"item":-1,"line":7,"file":"library","message":"syntax error"}
]}
```

Issue fields: `level` (`"error"` or `"warning"`), `code` (see the table above), `channel` (channel number), `slot` and `item` (zero-based indexes); these three are `-1` when the issue is not tied to a channel, slot or item; `line` (line in the file for syntax errors, otherwise 0; 0 also means unknown), `file` (`"config"`, `"library"` or `"lineup"`; lineup warnings use `"lineup"`, with `slot` and `item` referring to the config's slots and items), `message`. `ok` is true when there are no errors.

### `lineup`

```
cable-twii-sched lineup <root> --channel N --date YYYY-MM-DD
```

```json
{"channel":1,"date":"2026-10-20","hash":"0123456789abcdef","airings":[
  {"start_ms":25200000,"duration_ms":600000,"kind":"episode",
   "path":"Cartoon Network/Gumball/Season 1/S01E02 - Test Two.mpg","show":"Gumball","season":1,"episode":2,
   "title":"Test Two","slot":0,"item":0},
  {"start_ms":25800000,"duration_ms":30000,"kind":"ad",
   "path":"ads/ad-a.mpg","show":null,"season":null,"episode":null,
   "title":null,"slot":0,"item":1},
  {"start_ms":25830000,"duration_ms":120000,"kind":"upnext",
   "path":null,"show":null,"season":null,"episode":null,
   "title":null,"slot":-1,"item":-1}
],"warnings":[]}
```

- `start_ms`: milliseconds since local midnight (no gaps). A timed channel's lineup covers [first slot `at`, first slot `at` + 86400000) and spills past midnight into the early hours of the next date; a looping channel's lineup covers [0, 86400000).
- `kind`: `"episode"`, `"ad"` or `"upnext"`.
- Size cap: one lineup holds at most 20000 airings. When placing would go past that, the engine stops placing, covers the rest of the lineup (to the first slot `at` + 86400000 on a timed channel, to 86400000 on a looping channel) with one `upnext` airing (`slot` and `item` `-1`), and adds one `partial` warning with `slot` and `item` `-1` and the message `lineup reached 20000 airings`. Coverage stays contiguous. With the 1000 ms minimum `duration_ms`, a normal schedule stays far below the cap.
- `hash`: 16 lowercase hex digits. Exact encoding: FNV-1a 64 (offset basis `0xcbf29ce484222325`, prime `0x100000001b3`) over every airing in order: `start_ms` as 8 big-endian bytes, `duration_ms` as 4 big-endian bytes, `kind` as 1 byte (episode 0, ad 1, upnext 2), then for an episode or ad its `path` as UTF-8 bytes followed by one 0 byte (up-next adds nothing). Same inputs give the same hash on the Mac and the Wii. Studio treats the hash as opaque and only compares it with values the engine produced.
- `path`, `show`, `season`, `episode`, `title` are `null` for up-next airings. `slot` and `item` are `-1` for filler airings. In a looping channel `slot` is `-1` and `item` is the loop item index.
- `warnings`: same shape as `issues` in `validate`. Every warning here has `file` `"lineup"`, `level` `"warning"` and `line` `0`; `slot` and `item` refer to the config's slots and items (`-1` for a warning not tied to one, such as `loop_empty` or the 20000-airing cap).

### `now`

```
cable-twii-sched now <root> --channel N --at "YYYY-MM-DD HH:MM:SS"
```

```json
{"airing":{"start_ms":25200000,"duration_ms":600000,"kind":"episode","path":"Cartoon Network/Gumball/Season 1/S01E02 - Test Two.mpg","show":"Gumball","season":1,"episode":2,"title":"Test Two","slot":0,"item":0},"offset_ms":5000}
```

Example when nothing covers the time: `{"airing":null,"offset_ms":0}`.

`airing` has the same shape as one lineup airing. `offset_ms` is how far into the airing the given time is. The engine looks at today's lineup and, for a time before today's first airing, at the tail of yesterday's.

### `hash`

```
cable-twii-sched hash <root> --channel N --date YYYY-MM-DD
```

Prints only the 16-digit hash (same encoding as above), for example `0123456789abcdef`, and a newline. It equals `hash` in the `lineup` output.
