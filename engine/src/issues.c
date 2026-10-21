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
