#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ctw/ctw.h"
#include "json.h"

#define EXIT_OK 0
#define EXIT_INVALID 1
#define EXIT_USAGE 2

typedef struct {
    const char *root;
    int check_files;
    const char *channel;
    const char *date;
    const char *at;
} cli_args;

static void write_issue(json_w *w, const ctw_issue *i)
{
    json_begin_obj(w);
    json_key(w, "level");
    json_str(w, ctw_level_name(i->level));
    json_key(w, "code");
    json_str(w, ctw_code_name(i->code));
    json_key(w, "channel");
    json_int(w, i->channel);
    json_key(w, "slot");
    json_int(w, i->slot);
    json_key(w, "item");
    json_int(w, i->item);
    json_key(w, "line");
    json_int(w, i->line);
    json_key(w, "file");
    json_str(w, i->file);
    json_key(w, "message");
    json_str(w, i->message);
    json_end_obj(w);
}

static void write_issue_array(json_w *w, const ctw_issues *l)
{
    json_begin_arr(w);
    for (size_t k = 0; k < l->count; k++) write_issue(w, &l->items[k]);
    json_end_arr(w);
}

static void write_validate(const ctw_issues *l)
{
    json_w w;
    json_init(&w, stdout);
    json_begin_obj(&w);
    json_key(&w, "ok");
    json_bool(&w, !ctw_issues_has_errors(l));
    json_key(&w, "issues");
    write_issue_array(&w, l);
    json_end_obj(&w);
    fputc('\n', stdout);
}

static void write_airing(json_w *w, const ctw_airing *a)
{
    const char *path = NULL;
    const char *show = NULL;
    const char *title = NULL;
    int has_episode = 0;

    if (a->kind == CTW_AIR_EPISODE && a->episode) {
        path = a->episode->path;
        title = a->episode->title;
        has_episode = 1;
        if (a->show) show = a->show->folder;
    } else if (a->kind == CTW_AIR_AD && a->ad) {
        path = a->ad->path;
    }

    json_begin_obj(w);
    json_key(w, "start_ms");
    json_int(w, a->start_ms);
    json_key(w, "duration_ms");
    json_int(w, a->duration_ms);
    json_key(w, "kind");
    json_str(w, a->kind == CTW_AIR_EPISODE ? "episode" : a->kind == CTW_AIR_AD ? "ad" : "upnext");
    json_key(w, "path");
    json_str(w, path);
    json_key(w, "show");
    json_str(w, show);
    json_key(w, "season");
    if (has_episode) json_int(w, a->episode->season);
    else json_null(w);
    json_key(w, "episode");
    if (has_episode) json_int(w, a->episode->episode);
    else json_null(w);
    json_key(w, "title");
    json_str(w, title);
    json_key(w, "slot");
    json_int(w, a->slot_index);
    json_key(w, "item");
    json_int(w, a->item_index);
    json_end_obj(w);
}

static void format_date(char *buf, size_t n, ctw_date d)
{
    snprintf(buf, n, "%04d-%02d-%02d", d.year, d.month, d.day);
}

static int digit(char c) { return c >= '0' && c <= '9'; }

static int parse_channel(const char *s, int *out)
{
    char *end = NULL;
    long v;
    if (!s || s[0] < '1' || s[0] > '9') return -1;
    for (const char *p = s; *p; p++) {
        if (!digit(*p)) return -1;
    }
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno || *end || v < 1 || v > INT_MAX) return -1;
    *out = (int)v;
    return 0;
}

static int parse_hhmmss(const char *s, uint32_t *out_ms)
{
    if (!digit(s[0]) || !digit(s[1]) || s[2] != ':' || !digit(s[3]) || !digit(s[4]) ||
        s[5] != ':' || !digit(s[6]) || !digit(s[7]) || s[8] != '\0') {
        return -1;
    }
    uint32_t hh = (uint32_t)((s[0] - '0') * 10 + (s[1] - '0'));
    uint32_t mm = (uint32_t)((s[3] - '0') * 10 + (s[4] - '0'));
    uint32_t ss = (uint32_t)((s[6] - '0') * 10 + (s[7] - '0'));
    if (hh > 23u || mm > 59u || ss > 59u) return -1;
    *out_ms = ((hh * 60u + mm) * 60u + ss) * 1000u;
    return 0;
}

static int parse_at(const char *s, ctw_date *date, uint32_t *ms)
{
    char day[11];
    if (strlen(s) != 19 || s[10] != ' ') return -1;
    memcpy(day, s, 10);
    day[10] = '\0';
    if (ctw_date_parse(day, date)) return -1;
    return parse_hhmmss(s + 11, ms);
}

static int usage(const char *msg)
{
    fprintf(stderr, "cable-twii-sched: %s\n", msg);
    fprintf(stderr,
            "usage: cable-twii-sched validate <root> [--check-files]\n"
            "       cable-twii-sched lineup <root> --channel N --date YYYY-MM-DD\n"
            "       cable-twii-sched now <root> --channel N --at \"YYYY-MM-DD HH:MM:SS\"\n"
            "       cable-twii-sched hash <root> --channel N --date YYYY-MM-DD\n");
    return EXIT_USAGE;
}

static int parse_args(int argc, char **argv, const char *cmd, cli_args *a)
{
    int want_channel = strcmp(cmd, "validate") != 0;
    int want_date = strcmp(cmd, "lineup") == 0 || strcmp(cmd, "hash") == 0;
    int want_at = strcmp(cmd, "now") == 0;

    memset(a, 0, sizeof *a);
    if (argc < 3 || argv[2][0] == '-') return usage("missing <root>");
    a->root = argv[2];
    for (int i = 3; i < argc; i++) {
        const char *o = argv[i];
        const char **slot = NULL;
        if (!strcmp(o, "--check-files") && !want_channel) {
            a->check_files = 1;
            continue;
        }
        if (!strcmp(o, "--channel") && want_channel) slot = &a->channel;
        else if (!strcmp(o, "--date") && want_date) slot = &a->date;
        else if (!strcmp(o, "--at") && want_at) slot = &a->at;
        if (!slot) return usage("unexpected argument");
        if (*slot || i + 1 >= argc) return usage("missing or repeated option value");
        *slot = argv[++i];
    }
    if (want_channel && !a->channel) return usage("missing --channel");
    if (want_date && !a->date) return usage("missing --date");
    if (want_at && !a->at) return usage("missing --at");
    return EXIT_OK;
}

static int build_into(const ctw_db *db, const ctw_channel *ch, ctw_date day, ctw_lineup *l,
                      ctw_issues *issues)
{
    if (ctw_lineup_build(db, ch, day, l) == 0) return 0;
    ctw_issues_add(issues, CTW_LEVEL_ERROR, CTW_E_PARSE, ch->number, -1, -1, 0, "config",
                   "internal error: could not build lineup");
    return -1;
}

static int cmd_lineup(const ctw_db *db, const ctw_channel *ch, ctw_date day, int hash_only,
                      ctw_issues *issues)
{
    ctw_lineup l;
    char date[16];
    json_w w;

    if (build_into(db, ch, day, &l, issues)) {
        write_validate(issues);
        return EXIT_INVALID;
    }
    if (hash_only) {
        printf("%016llx\n", (unsigned long long)ctw_lineup_hash(&l));
        ctw_lineup_free(&l);
        return EXIT_OK;
    }
    format_date(date, sizeof date, day);
    json_init(&w, stdout);
    json_begin_obj(&w);
    json_key(&w, "channel");
    json_int(&w, ch->number);
    json_key(&w, "date");
    json_str(&w, date);
    json_key(&w, "hash");
    {
        char hex[17];
        snprintf(hex, sizeof hex, "%016llx", (unsigned long long)ctw_lineup_hash(&l));
        json_str(&w, hex);
    }
    json_key(&w, "airings");
    json_begin_arr(&w);
    for (size_t k = 0; k < l.count; k++) write_airing(&w, &l.items[k]);
    json_end_arr(&w);
    json_key(&w, "warnings");
    write_issue_array(&w, &l.warnings);
    json_end_obj(&w);
    fputc('\n', stdout);
    ctw_lineup_free(&l);
    return EXIT_OK;
}

static int cmd_now(const ctw_db *db, const ctw_channel *ch, ctw_date day, uint32_t ms,
                   ctw_issues *issues)
{
    ctw_lineup today;
    ctw_lineup yesterday;
    const ctw_airing *a;
    uint32_t offset = 0;
    json_w w;

    if (build_into(db, ch, day, &today, issues)) {
        write_validate(issues);
        return EXIT_INVALID;
    }
    if (build_into(db, ch, ctw_date_prev(day), &yesterday, issues)) {
        ctw_lineup_free(&today);
        write_validate(issues);
        return EXIT_INVALID;
    }
    a = ctw_lineup_at(&today, &yesterday, ms, &offset);
    json_init(&w, stdout);
    json_begin_obj(&w);
    json_key(&w, "airing");
    if (a) write_airing(&w, a);
    else json_null(&w);
    json_key(&w, "offset_ms");
    json_int(&w, a ? (long long)offset : 0);
    json_end_obj(&w);
    fputc('\n', stdout);
    ctw_lineup_free(&today);
    ctw_lineup_free(&yesterday);
    return EXIT_OK;
}

int main(int argc, char **argv)
{
    cli_args a;
    ctw_db *db = NULL;
    ctw_issues issues = {0};
    const ctw_channel *ch = NULL;
    ctw_date day = {0, 0, 0};
    uint32_t ms = 0;
    int channel = 0;
    int rc;
    const char *cmd;

    if (argc < 2) return usage("missing command");
    cmd = argv[1];
    if (strcmp(cmd, "validate") && strcmp(cmd, "lineup") && strcmp(cmd, "now") &&
        strcmp(cmd, "hash")) {
        return usage("unknown command");
    }
    rc = parse_args(argc, argv, cmd, &a);
    if (rc) return rc;

    if (strcmp(cmd, "validate")) {
        if (parse_channel(a.channel, &channel)) return usage("malformed --channel");
        if (a.date && ctw_date_parse(a.date, &day)) return usage("malformed --date");
        if (a.at && parse_at(a.at, &day, &ms)) return usage("malformed --at");
    }

    rc = ctw_db_load(a.root, &db, &issues);
    if (rc == 0 && a.check_files) rc = ctw_db_check_files(db, a.root, &issues);
    if (rc != 0 || !strcmp(cmd, "validate")) {
        write_validate(&issues);
        rc = ctw_issues_has_errors(&issues) || rc != 0 ? EXIT_INVALID : EXIT_OK;
        goto done;
    }

    ch = ctw_db_channel(db, channel);
    if (!ch) {
        rc = usage("channel not in config");
        goto done;
    }
    if (!strcmp(cmd, "now")) rc = cmd_now(db, ch, day, ms, &issues);
    else rc = cmd_lineup(db, ch, day, !strcmp(cmd, "hash"), &issues);

done:
    ctw_issues_free(&issues);
    ctw_db_free(db);
    return rc;
}
