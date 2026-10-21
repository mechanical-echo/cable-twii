#include "lineup_internal.h"

int ctw_lineup_build_loop(ctw_build *b)
{
    const ctw_channel *ch = b->ch;
    const int64_t end = (int64_t)CTW_DAY_MS;
    if (ch->loop_count == 0u || !ch->loop) return -1;
    b->cursor = 0;
    b->limit = end;
    b->cover_end = end;
    while (b->cursor < end && !b->capped) {
        size_t before = b->out->count;
        for (size_t i = 0; i < ch->loop_count; i++) {
            if (b->cursor >= end || b->capped) break;
            if (ch->loop[i].type == CTW_ITEM_FILL) return -1;
            if (ctw_place_item(b, &ch->loop[i], INT64_MAX, -1, (int)i)) return -1;
        }
        if (b->out->count == before) {
            ctw_build_warn(b, CTW_W_LOOP_EMPTY, -1, -1, "a full pass of the loop placed nothing");
            return ctw_build_push(b, ctw_build_left(b, end), CTW_AIR_UPNEXT, NULL, NULL, NULL,
                                  -1, -1);
        }
    }
    return 0;
}
