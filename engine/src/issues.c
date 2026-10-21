#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"

#define CTW_ISSUES_INITIAL_CAP 16u

void ctw_issues_add(ctw_issues *l, ctw_level lv, ctw_code c, int channel, int slot, int item,
                    int line, const char *file, const char *fmt, ...)
{
    ctw_issue *it;
    va_list ap;
    size_t flen;
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : CTW_ISSUES_INITIAL_CAP;
        ctw_issue *grown = realloc(l->items, cap * sizeof *grown);
        if (!grown) return;
        l->items = grown;
        l->cap = cap;
    }
    it = &l->items[l->count++];
    memset(it, 0, sizeof *it);
    it->level = lv;
    it->code = c;
    it->channel = channel;
    it->slot = slot;
    it->item = item;
    it->line = line;
    flen = file ? strlen(file) : 0;
    if (flen >= sizeof it->file) flen = sizeof it->file - 1;
    if (flen) memcpy(it->file, file, flen);
    it->file[flen] = '\0';
    va_start(ap, fmt);
    vsnprintf(it->message, sizeof it->message, fmt, ap);
    va_end(ap);
}

bool ctw_issues_has_errors(const ctw_issues *l)
{
    size_t i;
    if (!l) return false;
    for (i = 0; i < l->count; i++)
        if (l->items[i].level == CTW_LEVEL_ERROR) return true;
    return false;
}

void ctw_issues_free(ctw_issues *l)
{
    if (!l) return;
    free(l->items);
    l->items = NULL;
    l->count = 0;
    l->cap = 0;
}

const char *ctw_code_name(ctw_code c)
{
    switch (c) {
    case CTW_E_PARSE: return "parse";
    case CTW_E_VERSION: return "version";
    case CTW_E_BAD_VALUE: return "bad_value";
    case CTW_E_BAD_TIME: return "bad_time";
    case CTW_E_DUP_CHANNEL: return "dup_channel";
    case CTW_E_UNKNOWN_SHOW: return "unknown_show";
    case CTW_E_MISSING_EPISODE: return "missing_episode";
    case CTW_E_UNKNOWN_AD: return "unknown_ad";
    case CTW_E_FILL_IN_LOOP: return "fill_in_loop";
    case CTW_E_NO_SLOTS: return "no_slots";
    case CTW_E_DUP_SLOT: return "dup_slot";
    case CTW_E_MISSING_FILE: return "missing_file";
    case CTW_E_EMPTY_POOL: return "empty_pool";
    case CTW_W_CUT: return "cut";
    case CTW_W_PARTIAL: return "partial";
    case CTW_W_LOOP_EMPTY: return "loop_empty";
    }
    return "unknown";
}

const char *ctw_level_name(ctw_level l)
{
    switch (l) {
    case CTW_LEVEL_ERROR: return "error";
    case CTW_LEVEL_WARNING: return "warning";
    }
    return "unknown";
}
